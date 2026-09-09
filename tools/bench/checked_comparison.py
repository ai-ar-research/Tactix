#!/usr/bin/env python3
"""Fail-closed paired comparison: retain every rank and classify before scoring.

Reads atomically published normal benchmark records; never submits or cleans.
An aggregate is available only when every task is independently checked/clean
or has a classified, clean solver timeout. Resource kills need phase evidence;
SIGKILL alone does not prove OOM. Pending checks never count as checked solves.
"""
import argparse
from collections import Counter
import csv
import json
import math
from pathlib import Path
import sys
from fixed_runtime import sha, require

LIVE = {'PENDING', 'RUNNING', 'COMPLETING', 'CONFIGURING', 'SUSPENDED'}

def classify(record, final=False, state=None):
    state = ((state or '').split() or [''])[0].rstrip('+')
    live = not state or state in LIVE
    clean = bool(final and record and record.get('artifact_deleted') and
                 record.get('derived_artifacts_deleted') and not record.get('cleanup_pending'))
    result = dict(status='pending_solver', clean=clean, checked=False, scorable=False)
    if not record:
        if not live:
            result['status'] = 'resource_failure_phase_unknown' if state == 'OUT_OF_MEMORY' else 'missing_solver_record_terminal'
        return result
    solve = record.get('solve')
    if not solve:
        result['status'] = 'harness_failure' if final else 'missing_solver_metrics'
        return result
    seconds = solve.get('wall_seconds')
    require(isinstance(seconds, (int,float)) and math.isfinite(seconds) and seconds >= 0, 'invalid solve time')
    expected = record.get('expected')
    require(expected in ('SAT','UNSAT'), 'unknown expected verdict')
    expected_code = {'SAT':10,'UNSAT':20}[expected]
    solver_ok = (record.get('verdict') == expected and record.get('expected_match') is True and
                 not solve.get('timed_out') and solve.get('exit_code') == expected_code)
    if not final:
        if state == 'OUT_OF_MEMORY':
            result['status'] = 'checker_resource_failure' if solver_ok else 'solver_resource_failure'
        elif not live:
            result['status'] = 'missing_final_terminal'
        elif solver_ok:
            result['status'] = 'pending_validation'
        elif solve.get('timed_out'):
            result['status'] = 'pending_cleanup_solver_timeout'
        else:
            result['status'] = 'pending_invalid_solver_result'
        return result
    if solve.get('timed_out'):
        result['status'] = 'solver_timeout'
        # A reported verdict still needs its certificate classified: do not
        # hide a rejected certificate behind a solver timeout label.
        if record.get('verdict') == 'UNKNOWN' and not record.get('proof_check') and not record.get('model_check'):
            result['scorable'] = clean
        else:
            result['status'] = 'solver_timeout_with_verdict'
        return result
    if not solver_ok:
        if record.get('verdict') in ('SAT','UNSAT') and record['verdict'] != expected:
            result['status'] = 'verdict_mismatch'
        elif state == 'OUT_OF_MEMORY':
            result['status'] = 'solver_resource_failure'
        else:
            result['status'] = 'solver_abnormal_exit'
        return result
    checker = record.get('proof_check' if expected == 'UNSAT' else 'model_check')
    if not checker:
        result['status'] = 'missing_checker_result'
    elif checker.get('timed_out'):
        result['status'] = 'checker_timeout'
    elif checker.get('exit_code') not in (0,1):
        result['status'] = 'checker_abnormal_exit'
    else:
        oracle = checker.get('result') == 'verified' and checker.get('exit_code') == 0
        if expected == 'UNSAT':
            output = checker.get('output_tail','')
            oracle = oracle and 's VERIFIED' in output and 's NOT VERIFIED' not in output
        if not oracle:
            result['status'] = 'proof_rejected_or_status_mismatch' if expected == 'UNSAT' else 'model_rejected'
        elif record.get('valid') is not True:
            result['status'] = 'inconsistent_final_validation'
        elif not clean:
            result['status'] = 'residue'
        else:
            result.update(status='checked_'+expected, checked=True, scorable=True)
    return result

def summarize(matrix, records, final_tasks, states, timeout):
    require([int(r['task']) for r in matrix] == list(range(len(matrix))), 'noncontiguous task matrix')
    ranks = sorted({int(r['rank']) for r in matrix})
    require(ranks == list(range(1,len(ranks)+1)), 'noncontiguous rank matrix')
    require(len(matrix) == 2*len(ranks), 'not a paired matrix')
    require(bool(ranks), 'empty comparison matrix')
    task_rows = {int(r['task']):r for r in matrix}
    require(set(records) <= set(task_rows), 'unknown record task')
    grouped = {}
    counts = {a:Counter() for a in ('tactix','kissat')}
    cells = []
    for row in matrix:
        task,rank,arm = int(row['task']),int(row['rank']),row['arm']
        require(arm in counts, 'unexpected arm')
        key = rank,arm
        require(key not in grouped, 'duplicate paired arm')
        record = records.get(task)
        if record:
            for field in ('instance','expected','arm'):
                require(record[field] == row[field], 'record identity: '+field)
            require(int(record['rank']) == rank and int(record['task']) == task, 'record rank/task identity')
        cell = dict(task=task,rank=rank,arm=arm,**classify(record,task in final_tasks,states.get(task)))
        cell['seconds'] = record.get('solve',{}).get('wall_seconds') if record else None
        cell['peak_rss_kib'] = record.get('solve',{}).get('peak_rss_kb') if record else None
        grouped[key] = cell
        cells.append(cell)
        counts[arm][cell['status']] += 1
    pairs=[]
    for rank in ranks:
        require((rank,'tactix') in grouped and (rank,'kissat') in grouped, 'missing paired arm')
        t,k = grouped[rank,'tactix'],grouped[rank,'kissat']
        a,b = task_rows[t['task']],task_rows[k['task']]
        require((a['instance'],a['expected']) == (b['instance'],b['expected']), 'paired input mismatch')
        both = t['checked'] and k['checked']
        memory = t['peak_rss_kib'] is not None and k['peak_rss_kib'] is not None
        pairs.append(dict(rank=rank,instance=a['instance'],expected=a['expected'],
            tactix=t,kissat=k,ratio=t['seconds']/k['seconds'] if both and k['seconds'] else None,
            delta_seconds=t['seconds']-k['seconds'] if both else None,
            memory_excess_kib=t['peak_rss_kib']-k['peak_rss_kib'] if memory else None,
            tactix_over_16gib=t['peak_rss_kib'] > 16*1024*1024 if t['peak_rss_kib'] is not None else None,
            solve_loss=bool(k['checked'] and t['status'] == 'solver_timeout'),
            unresolved_tactix_vs_checked_kissat=bool(k['checked'] and not t['scorable'])))
    complete = all(c['scorable'] for c in cells)
    totals={arm:math.fsum(c['seconds'] if c['checked'] else 2*timeout for c in cells if c['arm']==arm)
            for arm in counts} if complete else None
    return dict(ranks=len(ranks),records=len(records),final_records=len(final_tasks),
        classes={a:dict(v) for a,v in counts.items()},all_classified_for_scoring=complete,
        all_certificates_checked=all(c['checked'] for c in cells),
        zero_residue=all(c['clean'] for c in cells),
        residue_tasks=[c['task'] for c in cells if c['status']=='residue'],
        cleanup_unverified_tasks=[c['task'] for c in cells if not c['clean']],totals_par2=totals,
        ratio=totals['tactix']/totals['kissat'] if totals and totals['kissat'] else None,
        pairs=pairs,
        limitation='Unresolved checker/harness/resource/cleanup cases retain their ranks and suppress the aggregate. Pending measurements are not checked solves. RSS comparison is not causal memory attribution.')

def load(campaign, states=None):
    manifest=json.loads((campaign/'manifest.json').read_text())
    for rel,digest in manifest['pin_sha256'].items():
        require(sha(campaign/rel)==digest,'changed pin: '+rel)
    with (campaign/'matrix.tsv').open(newline='') as stream:
        matrix=list(csv.DictReader(stream,delimiter='\t'))
    records={};final=set();bindings={}
    for folder in ('provisional','results'):
        for path in sorted((campaign/folder).glob('*.json')):
            record=json.loads(path.read_text());task=int(record['task'])
            require(int(path.stem)==task,'record filename mismatch')
            if folder=='results':
                if 'solve' in record:
                    require(record['binary_sha256']==manifest['arms'][record['arm']]['sha256'],'binary mismatch')
                if task in records and 'solve' in record:
                    for key in ('solve','certificate_sha256','certificate_bytes'):
                        require(record[key]==records[task][key],'provisional/final mismatch: '+key)
                final.add(task)
            records[task]=record;bindings[str(path)]=sha(path)
    output=summarize(matrix,records,final,states or {},manifest['resource_contract']['solve_timeout'])
    output.update(campaign=str(campaign),manifest_sha256=sha(campaign/'manifest.json'),record_hashes=bindings)
    return output

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('campaign',type=Path)
    ap.add_argument('--states',type=Path,help='Frozen JSON object mapping task numbers to SLURM states')
    args=ap.parse_args()
    states={int(k):v for k,v in json.loads(args.states.read_text()).items()} if args.states else {}
    print(json.dumps(load(args.campaign.resolve(),states),indent=2))

if __name__=='__main__':main()

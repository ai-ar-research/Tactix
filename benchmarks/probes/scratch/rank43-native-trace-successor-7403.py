#!/usr/bin/env python3
"""Classify native trace exhaustion, then repeat only the missing native leg."""
from collections import Counter
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools/bench'))
from fixed_runtime import sha
from native_work import primary_statistics
from checked_comparison import classify

OLD=ROOT/'benchmarks/runs/20260911-7401-rank43-analysis-window'
C=ROOT/'benchmarks/runs/20260911-7403-rank43-native-analysis-complete'
CLASS=OLD/'native-truncation-7403.json'


def read(p):return json.loads(p.read_text())


def prepare():
    assert not C.exists() and not CLASS.exists()
    m=read(OLD/'manifest.json');deps=dict(m['dependencies'])
    def bind(p,expected=None):
        d=sha(p);assert expected is None or d==expected,str(p)
        deps[str(p.relative_to(ROOT))]=d
    for rel,d in m['pin_sha256'].items():bind(OLD/rel,d)
    p=OLD/'provisional/001.json';r=read(p);bind(p)
    assert classify(r,False)['status']=='pending_validation'
    oracle=ROOT/'benchmarks/runs/20260911-7397-current-rank43-decisions/provisional/001.json'
    old=read(oracle);bind(oracle)
    assert primary_statistics(r['solver_statistics'])==primary_statistics(old['solver_statistics'])
    for k in ('verdict','certificate_sha256','certificate_bytes'):assert r[k]==old[k]
    search=OLD/'traces/kissat.search.tsv';bind(search);body=search.read_text()
    lines=body.splitlines();rows=[l.split('\t') for l in lines[3:] if not l.startswith('#')]
    assert len(rows)==250000 and '# truncated at 250000 records' in lines
    assert all(len(row)==22 and row[1]==str(i) for i,row in enumerate(rows))
    assert rows[-1][3]=='85'
    strict=Path(__file__).with_name('validate-rank43-analysis-7402.py');bind(strict)
    native_root=ROOT/'tools/solvers/variants/kissat-imply-fast-5414/src'
    for name in ('analyze.c','search.c','eventtrace.c'):bind(native_root/name)
    failure=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        classification='DIAGNOSTIC_TRACE_CAP; complete native work/certificate replay succeeds, full trace acceptance fails',
        source_record=str(p.relative_to(ROOT)),trace=str(search.relative_to(ROOT)),rows=len(rows),last=rows[-1],
        event_counts=dict(Counter(row[2] for row in rows)),
        strict_validator=str(strict.relative_to(ROOT)),strict_validator_sha256=sha(strict),
        additional_contract_issue='The new7402 assumption of one conflict/begin/end/learned per integer conflict is invalid: native OTFS can increment conflict within one analysis (begin29/end30), reused conflicts do not emit learned clauses, and native conflict observer has a4096-byte buffer that can omit a large clause (C84). Preserve the validator; a successor must validate analysis transactions and classify missing conflict payloads explicitly, never silently invent one.',
        successor='Repeat only native with500000 trace rows; candidate7401 remains in flight and must pass its own uncapped replay. Solver1200s/checker16000s/resources unchanged. No source or correctness oracle changed.',
        hashes={str(x.relative_to(ROOT)):sha(x) for x in (p,oracle,search,strict)})
    with CLASS.open('x') as stream:json.dump(failure,stream,indent=2)
    bind(CLASS);bind(Path(__file__));bind(OLD/'manifest.json');bind(OLD/'launch.json')
    for name in ('pins','logs','results','provisional','claims','traces'):(C/name).mkdir(parents=True)
    for p in (OLD/'pins').iterdir():
        if p.is_file():shutil.copy2(p,C/'pins'/p.name)
    arms={'kissat':dict(m['arms']['kissat'])}
    arms['kissat']['environment']={k:v.replace(str(OLD),str(C)) for k,v in arms['kissat']['environment'].items()}
    arms['kissat']['environment']['KISSAT_EVENT_TRACE_LIMIT']='500000'
    matrix=(OLD/'matrix.tsv').read_text().splitlines()
    native=matrix[2].split('\t');assert native[0]=='1' and native[2]=='kissat';native[0]='0'
    with (C/'matrix.tsv').open('x') as stream:stream.write(matrix[0]+'\n'+'\t'.join(native)+'\n')
    batch=(OLD/'array.sbatch').read_text().replace(str(OLD),str(C)).replace('7401-rank43-analysis-window','7403-rank43-native-analysis').replace('--array=0-1','--array=0-0')
    with (C/'array.sbatch').open('x') as stream:stream.write(batch)
    m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),arms=arms,dependencies=deps,row_cap=500000,
        resource_contract=dict(m['resource_contract'],array='0-0'),
        cached_candidate_campaign=str(OLD.relative_to(ROOT)),
        validation='One expected full native UNSAT, exact whole primary statistics/certificate/decisions against7397, native boundary additions accounted. Require complete uncapped trace and exact first250000 previously captured native event rows, ignoring only the cap marker. Combine only with independently validated7401 candidate full replay; native big-conflict payload omissions are explicit diagnostic holes, never formula-equality evidence.',
        hypothesis='Complete the bounded C1..85 analysis tail without repeating the still-running unchanged candidate. Originalcap is diagnostic capacity, not solver effort; preserve all source/solver proof gates.',
        test_consideration='No binary or runner edits. Existing cap/truncation/search/macro tests plus transaction controls for OTFS counter advances, conflict reuse, missing payloads and incomplete analysis pairs in successor validator.')
    m['pin_sha256']={str(p.relative_to(C)):sha(p) for p in [*(C/'pins').iterdir(),C/'matrix.tsv',C/'array.sbatch']}
    with (C/'manifest.json').open('x') as stream:json.dump(m,stream,indent=2)
    for p in (C/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
    print(CLASS);print(C)


def submit():
    assert not (C/'launch.json').exists()
    m=read(C/'manifest.json')
    for rel,d in m['pin_sha256'].items():assert sha(C/rel)==d,rel
    for group in ('dependencies','protected_campaign_hashes'):
        for rel,d in m[group].items():assert sha(ROOT/rel)==d,rel
    since=dt.datetime.now().replace(microsecond=0).isoformat()
    command=['sbatch','--parsable',str(C/'array.sbatch')]
    job=subprocess.check_output(command,text=True).strip().split(';')[0];assert job.isdigit()
    with (C/'launch.json').open('x') as stream:json.dump(dict(job_id=job,accounting_since=since,command=command,
        manifest_sha256=sha(C/'manifest.json'),submit_helper_sha256=sha(Path(__file__))),stream,indent=2)
    print(job,since)


if __name__=='__main__':
    assert sys.argv[1:] in (['--prepare'],['--submit'])
    if sys.argv[1:]==['--prepare']:prepare()
    else:submit()

#!/usr/bin/env python3
"""Validate full observer neutrality and the selected assignment/queue surface."""
import json
import math
from pathlib import Path
import runpy
import sys

columns = 'conflicts decisions level chosen observed value assignment_level heap_position score reason stable queue_stamp search_external search_stamp observed_stamp observed_prev observed_next chosen_stamp'.split()
def state(line):
    assert line.endswith('\n')
    fields = line.rstrip('\n').split('\t')
    assert len(fields) == len(columns)
    r = {k:(v if k == 'reason' else float(v) if k == 'score' else int(v)) for k,v in zip(columns, fields)}
    assert r['conflicts'] == 182093 and r['observed'] == 120438
    assert r['decisions'] > 0 and r['chosen'] and r['level'] > 0
    assert r['value'] in (-1,0,1) and r['stable'] in (0,1) and math.isfinite(r['score'])
    assert all(0 <= r[k] <= 0xffffffff for k in columns[10:])
    if r['value'] == 0:
        assert r['assignment_level'] == 0xffffffff and r['reason'] == 'unassigned'
    else:
        assert 0 <= r['assignment_level'] <= r['level']
        reason = r['reason']
        if reason not in ('decision', 'unit'):
            kind, literals = reason.split(':', 1)
            lits = [int(v) for v in literals.split(',')]
            assert all(lits) and ((kind == 'binary' and len(lits) == 1) or (kind == 'large' and len(lits) >= 3))
    return r

def fixtures():
    good = '182093\t49396661\t7257\t-121103\t120438\t0\t4294967295\t0\t1.5\tunassigned\t0\t500\t121103\t400\t399\t3\t4\t400\n'
    assert state(good)['value'] == 0
    assigned = good.replace('\t0\t4294967295\t0\t1.5\tunassigned', '\t-1\t7000\t0\t1.5\tbinary:3')
    assert state(assigned)['assignment_level'] == 7000
    bad = [good.rstrip('\n'), good + '\t0\n', good.replace('182093', '182092'),
        good.replace('120438', '120439'), good.replace('1.5', 'nan'),
        good.replace('unassigned', 'decision'), assigned.replace('binary:3', 'binary:0'),
        assigned.replace('7000', '8000'), assigned.replace('binary:3', 'large:1,2')]
    for line in bad:
        try: state(line)
        except (AssertionError, ValueError): pass
        else: raise AssertionError('Malformed selected state accepted')
    print('PASS: two positive/nine negative choice-state fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    support = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
    root = support['root']
    c = root / 'benchmarks/runs/20260908-5669-rank33-choice-state'
    if any(not (c / 'results' / (a + '.json')).exists() for a in ('candidate','kissat')):
        print('5669 pending'); return
    evidence = support['validate'](c)
    selected, counts, preceding = {}, {}, {}
    for arm in ('candidate','kissat'):
        rows = [state(line) for line in (c / 'traces' / (arm + '.state.tsv')).open()]
        assert rows and all(b['decisions'] == a['decisions'] + 1 for a,b in zip(rows, rows[1:]))
        actual = []
        with (c / 'traces' / (arm + '.decisions.tsv')).open() as stream:
            for line in stream:
                if line.startswith('#'): continue
                fields = list(map(int, line.split()))
                if fields[0] > 182093: break
                if fields[0] == 182093: actual.append(fields[:4])
        assert actual == [[r[k] for k in columns[:4]] for r in rows]
        matches = [r for r in rows if r['decisions'] == 49396661]
        assert len(matches) == 1 and matches[0]['chosen'] == (-120438 if arm == 'candidate' else -121103)
        selected[arm] = matches[0]
        counts[arm] = len(rows)
        preceding[arm] = [r for r in rows if r['decisions'] < 49396661][-3:]
    result = dict(parent_full_replay=True,parser_fixtures=11,evidence=evidence,
        selected_state=selected,state_rows=counts,preceding_choices=preceding,
        limitation='Observed state is after each decision assignment. Queue stamps/physical heap positions may require normalization; differing availability or order identifies a hypothesis,not its cause or a speedup.')
    p = c / 'summary.json'
    assert not p.exists()
    p.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k:v for k,v in result.items() if k != 'evidence'}, indent=2))

if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Separate optional observer rows from actual candidate-order divergence."""
import importlib.util
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('census', Path(__file__).with_name('validate-rank32-bve-candidates-6192.py'))
census = importlib.util.module_from_spec(spec)
spec.loader.exec_module(census)

def compare(left, right):
    omitted = 0
    portable = ('external', 'conflicts', 'applied', 'value', 'ticks', 'limit')
    for i, (a, b) in enumerate(zip(left, right)):
        for stage in ('begin', 'end'):
            if any(a[stage][k] != b[stage][k] for k in portable):
                return dict(matched_attempts=i, first_stage=stage, candidate=a, kissat=b,
                            verified_noop_postflush_omissions=omitted)
        if 'postflush' not in a and 'postflush' in b:
            assert a['end']['applied'] == 0
            for key in ('roots', 'trail', 'propagate', 'value', 'retired', 'ticks'):
                assert a['begin'][key] == a['end'][key], (i, key)
            assert {k: v for k, v in b['end'].items() if k != 'stage'} == {
                k: v for k, v in b['postflush'].items() if k != 'stage'}, i
            omitted += 1
        elif 'postflush' in a and 'postflush' in b:
            assert all(a['postflush'][k] == b['postflush'][k] for k in portable), i
        else:
            assert 'postflush' not in a and 'postflush' not in b, i
    assert len(left) == len(right), 'Unexplained attempt count mismatch'
    return dict(matched_attempts=len(left), first_stage=None,
                verified_noop_postflush_omissions=omitted)

def fixtures():
    import copy
    row = dict(external=9, conflicts=1, applied=0, value=0, ticks=3, limit=4,
               roots=2, trail=2, propagate=2, retired=0)
    a = {'begin': dict(row, stage='begin'), 'end': dict(row, stage='end')}
    b = dict(a, postflush=dict(row, stage='postflush'))
    assert compare([a], [b])['verified_noop_postflush_omissions'] == 1
    for stage in ('begin', 'end'):
        bad = copy.deepcopy(b); bad[stage]['external'] = 10
        assert compare([a], [bad])['first_stage'] == stage
    for stage, key in (('end', 'roots'), ('end', 'trail'), ('end', 'propagate'), ('end', 'retired')):
        bad = copy.deepcopy(a); bad[stage][key] += 1
        try: compare([bad], [b])
        except AssertionError: pass
        else: raise AssertionError('state-changing omission accepted')
    bad = copy.deepcopy(b); bad['postflush']['ticks'] += 1
    try: compare([a], [bad])
    except AssertionError: pass
    else: raise AssertionError('native state-changing flush accepted')
    print('PASS: optional-row alignment, two stage divergences, five unsafe omissions')

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6188-rank32-bve-candidates'
    target = c / 'attempt-alignment-6193.json'
    assert not target.exists()
    summary = json.loads((c / 'summary.json').read_text())
    assert summary['parent_replay']
    groups = {}
    for arm, solver in (('candidate', 'tactix'), ('kissat', 'kissat')):
        p = c / 'traces' / (arm + '.bve-candidates.tsv')
        assert census.sha(p) == summary['hashes'][str(p.relative_to(root))]
        with p.open() as stream:
            groups[arm] = census.attempts(census.candidates(stream, solver, 386968))
    result = compare(groups['candidate'], groups['kissat'])
    assert result['matched_attempts'] == 673 and result['first_stage'] == 'begin'
    assert result['candidate']['begin']['external'] == 349460
    assert result['kissat']['begin']['external'] == 132302
    result.update(parent_summary_sha256=census.sha(c / 'summary.json'),
                  raw_first_difference=summary['first_portable_difference'],
                  limitation='Only the first673 aligned attempts and their omitted no-op flushes are compared; no later equivalence or causal attribution is inferred.')
    target.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'raw_first_difference'}, indent=2))

if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Classify complete BVE attempts without hiding inactive-pivot discrepancies."""
from collections import Counter
import hashlib
import importlib.util
import itertools
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
CAMPAIGN = ROOT / 'benchmarks/runs/20260908-5937-first-bve-frontier'
FIELDS = ('external', 'stage', 'conflicts', 'applied', 'value', 'ticks', 'limit')

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

def attempts(rows, arm):
    result = []
    current = None
    for row in rows:
        stage = row['stage']
        assert stage in ('begin', 'end', 'postflush')
        if stage == 'begin':
            if current is not None:
                validate(current, arm)
                result.append(current)
            current = [row]
        else:
            assert current is not None
            assert stage == ('end' if len(current) == 1 else 'postflush')
            assert len(current) < 3
            assert all(row[k] == current[0][k] for k in ('external', 'conflicts', 'limit'))
            assert int(row['ticks']) >= int(current[-1]['ticks'])
            current.append(row)
    assert current is not None
    validate(current, arm)
    result.append(current)
    return result

def validate(group, arm):
    assert len(group) in (2, 3)
    begin, end = group[:2]
    assert begin['applied'] == '0'
    if len(group) == 2:
        # Native calls postflush after every non-inconsistent attempt. Tactix's
        # caller continues early only on decline with no newly derived root.
        assert arm == 'candidate' and end['applied'] == '0'
        assert all(begin[k] == end[k] for k in ('roots', 'trail', 'propagate'))
    else:
        assert group[2]['applied'] == end['applied']

def first_difference(a, b):
    for i, (x, y) in enumerate(itertools.zip_longest(a, b)):
        if x is None or y is None or any(u[k] != v[k] for u, v in zip(x[:2], y[:2]) for k in FIELDS):
            return dict(index=i, candidate=x, kissat=y)
    return None

def fixtures():
    def row(stage, **extra):
        return dict(external='7', stage=stage, conflicts='271079', ticks='3',
                    limit='9', applied='0', value='0', roots='4', trail='4',
                    propagate='4', **extra)
    good = [row('begin'), row('end')]
    assert len(attempts(good, 'candidate')) == 1
    assert len(attempts(good + [row('postflush')], 'kissat')) == 1
    bad = [([row('end')], 'candidate'), ([row('begin')], 'candidate'),
           (good, 'kissat'), (good + [row('end')], 'candidate'),
           ([good[0], dict(good[1], external='8')], 'candidate'),
           ([good[0], dict(good[1], ticks='2')], 'candidate'),
           ([good[0], dict(good[1], roots='5')], 'candidate'),
           ([good[0], dict(good[1], applied='1')], 'candidate'),
           ([row('unknown')], 'candidate')]
    for rows, arm in bad:
        try: attempts(rows, arm)
        except AssertionError: pass
        else: raise AssertionError('malformed attempt accepted')
    assert first_difference([good], [good]) is None
    assert first_difference([good], [])['index'] == 0
    assert first_difference([good], [[good[0], dict(good[1], ticks='4')]])['index'] == 0
    print('PASS: attempt grouping, coverage asymmetry, effort and nine negative fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    target = CAMPAIGN / 'attempt-classification.json'
    assert not target.exists()
    summary = json.loads((CAMPAIGN / 'summary.json').read_text())
    old = json.loads((CAMPAIGN / 'bve-frontier.json').read_text())
    assert sha(CAMPAIGN / 'summary.json') == old['hashes'][str((CAMPAIGN/'summary.json').relative_to(ROOT))]
    arms, stages, bindings = {}, {}, {}
    for arm in ('candidate', 'kissat'):
        path = CAMPAIGN / 'traces' / f'{arm}.bve-candidates.tsv'
        digest = sha(path)
        assert digest == summary['evidence'][arm]['trace_sha256'][path.name]
        bindings[str(path.relative_to(ROOT))] = digest
        with path.open() as stream:
            assert next(stream).strip() == '# bve-candidate-observer\t1'
            rows = []
            for line in stream:
                fields = line.rstrip().split('\t')
                assert fields[0] == ('tactix' if arm == 'candidate' else 'kissat')
                row = dict(x.split('=', 1) for x in fields[1:])
                assert len(row) == len(fields)-1 and row['conflicts'] == '271079'
                rows.append(row)
        stages[arm] = dict(Counter(r['stage'] for r in rows))
        arms[arm] = attempts(rows, arm)
    inactive = [dict(index=i, rows=p) for i,p in enumerate(arms['candidate'])
                if p[0]['retired']=='1' or p[0]['value']!='0']
    assert all(p[0]['active']=='1' and p[0]['value']=='0' for p in arms['kissat'])
    active = [p for p in arms['candidate'] if p[0]['retired']=='0' and p[0]['value']=='0']
    parser_path = Path(__file__).with_name('compare-rank32-formula-5785.py')
    spec = importlib.util.spec_from_file_location('formula', parser_path)
    parser = importlib.util.module_from_spec(spec); spec.loader.exec_module(parser)
    def irredundant(arm):
        path = CAMPAIGN/'traces'/f'{arm}.formula.tsv'
        assert sha(path) == summary['evidence'][arm]['trace_sha256'][path.name]
        with path.open() as stream:
            next(stream); next(stream)
            for line in stream:
                if line.startswith('0\t'): yield parser.parse_line(line)
    differences, first, count = Counter(), {}, 0
    for i,(a,b) in enumerate(itertools.zip_longest(irredundant('candidate'),irredundant('kissat'))):
        assert a is not None and b is not None
        count += 1
        for j,key in enumerate(('key','pose','metadata','values')):
            if a[j] != b[j]:
                differences[key] += 1
                first.setdefault(key, dict(index=i,candidate=a,kissat=b))
    sources = [ROOT/'.claude/worktrees/runtime-reduce-root-5900/src/policies/default_policy.cpp',
               ROOT/'tools/solvers/variants/kissat-score-bump-observer-5696/src/eliminate.c']
    bindings.update({str(p.relative_to(ROOT)):sha(p) for p in sources})
    out = dict(stage_rows=stages,attempt_counts={a:len(p) for a,p in arms.items()},
               first_full_attempt_difference=first_difference(*arms.values()),
               inactive_candidate_attempts=inactive,
               first_active_projection_difference=first_difference(active,arms['kissat']),
               compared_fields=FIELDS,
               irredundant_input=dict(copies=count,differences=dict(differences),first=first),
               inherited_binary_snapshots=old['binary_snapshots'],
               inherited_learned_only_differences=all(x['row'][0][0]==1 for k in ('candidate_only','kissat_only') for x in summary[k]),
               hashes=bindings,
               limitation='Active projection exposes later divergence; it does not excuse the nine inactive attempts or prove formula identity. Literal pose already differs in the irredundant input. No timing or correctness acceptance claim.')
    target.write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','inactive_candidate_attempts','irredundant_input')},indent=2))
    print('irredundant',count,dict(differences),'inactive attempts',len(inactive))

if __name__ == '__main__': main()

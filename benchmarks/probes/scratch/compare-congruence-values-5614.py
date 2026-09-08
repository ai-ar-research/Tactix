#!/usr/bin/env python3
"""Strictly validate selected clause values and whole observer-neutral replay."""
import json
from pathlib import Path
import runpy
import sys
s = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root, sha = s['root'], s['sha']
def parse(content):
    lines = content.splitlines()
    assert len(lines) >= 2
    result = []
    for line in lines[2:]:
        f = line.split('\t')
        assert len(f) == 7 and f[4].startswith('pose=') and f[5].startswith('values=')
        assert f[6] in ('garbage=0', 'garbage=1')
        red, literals = s['formula']('\n'.join(lines[:2] + ['\t'.join(f[:4])]))[0]
        pose = tuple(map(int, f[4][5:].split(',')))
        assert tuple(sorted(pose)) == literals
        values = [tuple(map(int, v.split(':'))) for v in f[5][7:].split(',')]
        assert all(len(v) == 2 and v[1] in (-1, 0, 1) for v in values)
        assert tuple(v[0] for v in values) == literals
        result.append(dict(redundant=red, literals=literals, pose=pose, values=values,
                           garbage=int(f[6][-1]), root_satisfied=any(v[1] == 1 for v in values)))
    # Empty selections still need a valid versioned header.
    s['formula']('\n'.join(lines[:2]))
    return result
def fixtures():
    header = '# tactix-formula-set\t2\n# redundant\tsize\thash\tliterals\n'
    row = '0\t3\t0000000000000001\t-3,1,2\tpose=2,-3,1\tvalues=-3:0,1:1,2:-1\tgarbage=0\n'
    good = header + row
    assert parse(good)[0]['root_satisfied']
    assert parse(good.replace('tactix', 'kissat'))[0]['pose'] == (2, -3, 1)
    assert parse(good.replace('garbage=0', 'garbage=1'))[0]['garbage'] == 1
    assert not parse(good.replace('1:1', '1:0'))[0]['root_satisfied']
    assert len(parse(good + row)) == 2
    assert parse(header) == []
    bad = ['', good.replace('pose=2,-3,1', 'pose=2,-3,4'),
           good.replace('1:1', '1:2'), good.replace('1:1', '4:1'),
           good.replace('garbage=0', 'garbage=2'), good.replace('values=', 'value='),
           good.replace('pose=', 'order='), good.replace('\t3\t000', '\t4\t000'),
           good.replace('-3:0,1:1', '1:1,-3:0'), good.replace('\tgarbage=0', '')]
    for text in bad:
        try: parse(text)
        except (AssertionError, ValueError, IndexError): pass
        else: raise AssertionError('Malformed clause state accepted')
    print('PASS: six positive/ten negative clause-state fixtures', flush=True)
fixtures()
if '--self-test' in sys.argv: raise SystemExit(0)
c = root / 'benchmarks/runs/20260908-5613-congruence-clause-values'
m = json.loads((c / 'manifest.json').read_text())
parent = root / m['parent_campaign']
if not (parent / 'clause-localization.json').exists() or len(list((c / 'results').glob('*.json'))) != 2:
    print('5613 or validated5611 pending'); raise SystemExit(0)
assert sha(parent / 'manifest.json') == m['parent_manifest_sha256']
for name, digest in m['hashes'].items(): assert sha(c / name) == digest
assert sha(root / m['input']) == m['input_sha256_compressed']
targets = json.loads((parent / 'clause-localization.json').read_text())['large_clause_difference']
wanted = {tuple(x['literals']) for side in targets.values() for x in side}
states, evidence = {}, {}
for arm in ('candidate', 'kissat'):
    p = c / 'results' / (arm + '.json')
    r = json.loads(p.read_text())
    old = json.loads((parent / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == m['hashes']['pins/' + arm]
    assert r['partial_certificate'] == old['partial_certificate']
    assert set(r['trace_sha256']) == set(old['trace_sha256'])
    for name, digest in r['trace_sha256'].items():
        assert sha(c / 'traces' / name) == digest
        if not name.endswith('.formula.tsv'):
            assert digest == old['trace_sha256'][name] == sha(parent / 'traces' / name)
    states[arm] = [row for row in parse((c / 'traces' / (arm + '.formula.tsv')).read_text())
                   if tuple(row['literals']) in wanted]
    evidence[arm] = dict(result_sha256=sha(p), trace_sha256=r['trace_sha256'])
result = dict(full_parent_replay=True, parser_fixtures=16, evidence=evidence,
    current_targets=targets, selected_states=states,
    limitation='Root values at first9457 output discriminate satisfied cleanup from other explanations. Garbage may already have been collected; absence alone does not prove when or why a clause was deleted.')
p = c / 'value-localization.json'
if p.exists(): assert json.loads(p.read_text()) == json.loads(json.dumps(result))
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k:v for k,v in result.items() if k != 'evidence'}, indent=2))

#!/usr/bin/env python3
"""Validate full observer neutrality and framed Sweep binary multisets."""
from collections import Counter
import json
from pathlib import Path
import runpy
import sys

support = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root, sha = support['root'], support['sha']

def snapshots(content, arm):
    groups, stages, current = [], [], None
    for line in content.splitlines():
        fields = line.split('\t')
        if line.startswith('# begin\t'):
            assert current is None and len(fields) == 7
            assert int(fields[1]) == len(groups) and fields[2] == arm
            assert fields[4] == '120330'
            if arm == 'tactix':
                assert fields[3] == 'slot_boundary'
                assert fields[6] in ('name=Sweep|stage=begin', 'name=Sweep|stage=end')
                stage = fields[6].split('=')[-1]
            else:
                assert fields[3] in ('sweep_begin', 'sweep_end') and fields[6] == '-'
                stage = fields[3].split('_')[-1]
            assert stage == ('begin' if len(groups) % 2 == 0 else 'end')
            current, expected = [], int(fields[5])
            stages.append(stage)
        elif line.startswith('# end\t'):
            assert current is not None and len(fields) == 2
            assert int(fields[1]) == len(groups)
            assert len(current) == expected and current == sorted(current)
            groups.append(current)
            current = None
        elif line and not line.startswith('#'):
            assert current is not None and len(fields) == 3 and int(fields[0]) == len(groups)
            pair = tuple(map(int, fields[1:]))
            assert all(pair) and pair[0] <= pair[1]
            current.append(pair)
    assert current is None and groups and len(groups) % 2 == 0
    return groups, stages

def fixtures():
    begin = '# begin\t0\ttactix\tslot_boundary\t120330\t1\tname=Sweep|stage=begin\n0\t-3\t4\n# end\t0\n'
    end = begin.replace('begin\n', 'end\n').replace('# begin\t0', '# begin\t1').replace('\n0\t', '\n1\t').replace('# end\t0', '# end\t1')
    good = begin + end
    assert snapshots(good, 'tactix') == ([[(-3, 4)], [(-3, 4)]], ['begin', 'end'])
    native = good.replace('tactix', 'kissat').replace('slot_boundary\t120330\t1\tname=Sweep|stage=begin', 'sweep_begin\t120330\t1\t-').replace('slot_boundary\t120330\t1\tname=Sweep|stage=end', 'sweep_end\t120330\t1\t-')
    assert snapshots(native, 'kissat') == snapshots(good, 'tactix')
    bad_cases = ['', begin, good.replace('120330', '120331'), good.replace('Sweep', 'Factor'),
                 good.replace('# end\t1\n', ''), good.replace('\t1\tname=', '\t2\tname='),
                 good.replace('-3\t4', '4\t-3'), good.replace('stage=end', 'stage=begin')]
    for bad in bad_cases:
        try: snapshots(bad, 'tactix')
        except (AssertionError, ValueError): pass
        else: raise AssertionError('Malformed Sweep snapshot accepted')
    print('PASS: two positive/eight negative Sweep snapshot fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260908-5639-rank33-sweep-binaries'
    if any(not (c / 'results' / (a + '.json')).exists() for a in ('candidate', 'kissat')):
        print('5639 pending'); return
    m = json.loads((c / 'manifest.json').read_text())
    parent = root / m['parent_campaign']
    assert sha(parent / 'summary.json') == m['parent_summary_sha256']
    for name, digest in m['hashes'].items(): assert sha(c / name) == digest
    assert sha(root / m['input']) == m['input_sha256_compressed']
    groups, large, records = {}, {}, {}
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json')
        r = json.loads(p.read_text())
        old = json.loads((parent / 'results' / (arm + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        assert r['partial_certificate']['bytes'] > 0 and r['partial_certificate'] == old['partial_certificate']
        assert r['binary_sha256'] == m['hashes']['pins/' + arm]
        for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
        for name, digest in old['trace_sha256'].items():
            assert sha(parent / 'traces' / name) == digest
            if name.endswith(('.formula.tsv', '.binary.tsv')): continue
            assert r['trace_sha256'][name] == digest, (arm, name)
        groups[arm], stages = snapshots((c / 'traces' / (arm + '.binary.tsv')).read_text(),
                                        'tactix' if arm == 'candidate' else arm)
        large[arm] = support['formula']((c / 'traces' / (arm + '.formula.tsv')).read_text())
        assert Counter(row[0] for row in large[arm]) == {0: 515789, 1: 11212}
        records[arm] = dict(result_sha256=sha(p), traces=r['trace_sha256'])
    assert len(groups['candidate']) == len(groups['kissat'])
    comparisons = []
    for i, (a, b) in enumerate(zip(groups['candidate'], groups['kissat'])):
        comparisons.append(dict(snapshot=i, stage=stages[i], candidate_count=len(a), kissat_count=len(b),
            equal_unique_set=set(a) == set(b), equal_multiset=Counter(a) == Counter(b),
            **support['multiset_difference'](a, b)))
    result = dict(parent_full_replay=True, parser_fixtures=10, evidence=records,
        binary_comparisons=comparisons, first_input_large_difference=support['multiset_difference'](large['candidate'], large['kissat']),
        limitation='Binary observer captures all begin/end calls at120330; one-shot large observer captures first input only. Complete parent trace and proof identity validates observer neutrality. No checked full verdict or runtime claim; duplicates are not automatically causal defects.')
    p = c / 'summary.json'
    if p.exists(): assert json.loads(p.read_text()) == json.loads(json.dumps(result))
    else: p.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'evidence'}, indent=2))

if __name__ == '__main__': main()

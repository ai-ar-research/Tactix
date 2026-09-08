#!/usr/bin/env python3
"""Validate companion replays and compare physical Factor input and accepted grids."""
from collections import Counter
import hashlib
import itertools
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

def binary_snapshots(content, arm):
    result, current = [], None
    for line in content.splitlines():
        fields = line.split('\t')
        if line.startswith('# begin\t'):
            assert current is None and len(fields) == 7
            assert int(fields[1]) == len(result) and fields[2] == arm
            assert fields[4] == '19375'
            assert (fields[3], fields[6]) == (
                ('slot_boundary', 'name=Factor|stage=begin') if arm == 'tactix'
                else ('factor_begin', '-'))
            current, expected = [], int(fields[5])
        elif line.startswith('# end\t'):
            assert current is not None and int(fields[1]) == len(result)
            assert len(current) == expected and current == sorted(current)
            result.append(current)
            current = None
        elif line and not line.startswith('#'):
            assert current is not None and len(fields) == 3
            assert int(fields[0]) == len(result)
            pair = tuple(map(int, fields[1:]))
            assert all(pair) and pair[0] <= pair[1]
            current.append(pair)
    assert current is None and result
    return result

def formula(content):
    lines = content.splitlines()
    assert len(lines) >= 2 and lines[0] in (
        '# tactix-formula-set\t2', '# kissat-formula-set\t2')
    assert lines[1] == '# redundant\tsize\thash\tliterals'
    result = []
    for line in lines[2:]:
        fields = line.split('\t')
        assert len(fields) == 4 and fields[0] in ('0', '1')
        literals = tuple(map(int, fields[3].split(',')))
        assert len(literals) == int(fields[1]) >= 3
        assert all(literals) and literals == tuple(sorted(literals))
        assert len(fields[2]) == 16 and 0 <= int(fields[2], 16) < 2**64
        result.append((int(fields[0]), literals))
    return result

def fresh_payload(text):
    fields = text.split('|')
    d = dict(field.split('=', 1) for field in fields)
    assert len(d) == len(fields) == 7
    assert set(d) == {'fresh', 'ordinal', 'seed', 'factors', 'remainders', 'delta', 'members'}
    result = {k:int(v) for k,v in d.items() if k != 'members'}
    result['members'] = list(map(int, d['members'].split(',')))
    assert result['fresh'] > 0 and result['ordinal'] > 0
    assert result['factors'] == len(result['members']) >= 2
    assert result['members'][0] == result['seed'] and all(result['members'])
    assert result['remainders'] > 0 and result['delta'] > 0
    return result

def fresh_events(content):
    assert 'truncat' not in content.lower()
    result, sequence = [], 0
    for line in content.splitlines():
        if not line or line.startswith('#'): continue
        row = line.split('\t')
        assert len(row) == 22 and int(row[1]) == sequence
        sequence += 1
        if row[2] == 'trace_begin':
            assert sequence == 1
            continue
        assert row[2] == 'factor_fresh' and int(row[3]) == 19375
        value = fresh_payload(row[-1])
        if result:
            assert value['ordinal'] == result[-1]['ordinal'] + 1
            assert value['fresh'] == result[-1]['fresh'] + 1
        result.append(value)
    assert result and sequence < 100000
    return result

def fixtures():
    good = '# begin\t0\ttactix\tslot_boundary\t19375\t2\tname=Factor|stage=begin\n0\t-3\t4\n0\t-3\t4\n# end\t0\n'
    assert binary_snapshots(good, 'tactix') == [[(-3, 4), (-3, 4)]]
    native = good.replace('tactix', 'kissat').replace('slot_boundary', 'factor_begin').replace('name=Factor|stage=begin', '-')
    assert binary_snapshots(native, 'kissat') == [[(-3, 4), (-3, 4)]]
    f = '# tactix-formula-set\t2\n# redundant\tsize\thash\tliterals\n0\t3\t0000000000000001\t-3,1,2\n'
    assert formula(f) == [(0, (-3, 1, 2))]
    assert len(formula(f + f.splitlines()[-1] + '\n')) == 2
    g = 'fresh=400|ordinal=15|seed=-3|factors=2|remainders=3|delta=1|members=-3,4'
    assert fresh_payload(g)['ordinal'] == 15
    bad_cases = [(binary_snapshots, (bad, 'tactix')) for bad in (
        '', good.replace('19375', '19374'), good.replace('stage=begin', 'stage=end'),
        good.replace('\t2\tname=', '\t1\tname='), good.replace('# end\t0\n', ''),
        good.replace('# end\t0', '# end\t1'))]
    bad_cases += [(formula, (bad,)) for bad in ('', f.replace('\t3\t000', '\t4\t000'),
        f.replace('-3,1,2', '1,-3,2'), f.replace('0000000000000001', 'xyz'))]
    bad_cases += [(fresh_payload, (bad,)) for bad in (g.replace('factors=2', 'factors=3'),
        g.replace('seed=-3', 'seed=4'), g.replace('delta=1', 'delta=0'), g + '|delta=1')]
    for fn, args in bad_cases:
        try: fn(*args)
        except (AssertionError, ValueError): pass
        else: raise AssertionError('Invalid diagnostic accepted')
    print('PASS: five positive/fourteen negative Factor parser fixtures', flush=True)

def validate(c):
    m = json.loads((c / 'manifest.json').read_text())
    parent = root / m['parent_campaign']
    assert sha(parent / 'summary.json') == m['parent_summary_sha256']
    for name,digest in m['hashes'].items(): assert sha(c / name) == digest
    assert sha(root / m['input']) == m['input_sha256_compressed']
    records = {}
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json')
        r = json.loads(p.read_text())
        old = json.loads((parent / 'results' / (arm + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
        assert r['partial_certificate']['bytes'] > 0 and r['partial_certificate'] == old['partial_certificate']
        assert r['binary_sha256'] == m['hashes']['pins/' + arm]
        for name,digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
        for name,digest in old['trace_sha256'].items():
            assert sha(parent / 'traces' / name) == digest == r['trace_sha256'][name]
        records[arm] = dict(result_sha256=sha(p), traces=r['trace_sha256'])
    return records

def multiset_difference(a, b):
    ac, bc = Counter(a), Counter(b)
    return dict(candidate_only=[dict(item=k, count=v) for k,v in sorted((ac-bc).items())],
                kissat_only=[dict(item=k, count=v) for k,v in sorted((bc-ac).items())])

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    grids = root / 'benchmarks/runs/20260908-5564-rank32-small-factor'
    inputs = root / 'benchmarks/runs/20260908-5565-rank32-factor-input'
    if any(not (c / 'results' / (arm + '.json')).exists()
           for c in (grids, inputs) for arm in ('candidate', 'kissat')):
        print('5564/5565 results pending'); return
    evidence = {c.name:validate(c) for c in (grids, inputs)}
    binaries, formulas, events = {}, {}, {}
    for arm in ('candidate', 'kissat'):
        binaries[arm] = binary_snapshots((inputs / 'traces' / (arm + '.binary.tsv')).read_text(),
            'tactix' if arm == 'candidate' else arm)
        # Two Factor passes share this conflict count. Binary snapshots retain
        # both; the single-shot formula observer defaults to the first pass.
        assert [len(v) for v in binaries[arm]] == [212403, 206128]
        formulas[arm] = formula((inputs / 'traces' / (arm + '.formula.tsv')).read_text())
        assert Counter(r[0] for r in formulas[arm]) == {0:586000, 1:2420}
        events[arm] = fresh_events((grids / 'traces' / (arm + '.factor.tsv')).read_text())
    first = next((dict(index=i, candidate=a, kissat=b)
        for i,(a,b) in enumerate(itertools.zip_longest(events['candidate'], events['kissat'])) if a != b), None)
    result = dict(parent_replay=True, parser_fixtures=19, evidence=evidence,
        binary_difference_by_pass=[multiset_difference(a,b) for a,b in
            zip(binaries['candidate'], binaries['kissat'])],
        first_pass_large_difference=multiset_difference(formulas['candidate'], formulas['kissat']),
        large_physical_order_equal=formulas['candidate'] == formulas['kissat'],
        accepted_counts={a:len(v) for a,v in events.items()}, first_grid_difference=first,
        limitation='Two Factor passes share19375:5565 captures both binary inputs but only the first large input.5569 selects the second large input explicitly. Literal multisets omit watch order, clause pose, flags, heap scores and ties. A grid difference is not proof of a defect or runtime gain.5564 fresh-only mode suppresses input snapshots;5565 supplies them independently.')
    p = grids / 'factor-input-localization.json'
    if p.exists(): assert json.loads(p.read_text()) == json.loads(json.dumps(result))
    else: p.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k:v for k,v in result.items() if k != 'evidence'}, indent=2))

if __name__ == '__main__': main()

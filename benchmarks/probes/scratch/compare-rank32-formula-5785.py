#!/usr/bin/env python3
"""Validate neutral Rank32 snapshots and compare literal sets, pose and roots."""
from collections import Counter
import hashlib
import itertools
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

def parse_line(line):
    f = line.rstrip('\n').split('\t')
    assert len(f) == 12 and f[0] in ('0', '1')
    literals = tuple(map(int, f[3].split(',')))
    assert len(literals) == int(f[1]) >= 3
    assert all(literals) and literals == tuple(sorted(literals))
    assert len(f[2]) == 16 and 0 <= int(f[2], 16) < 2**64
    attrs = dict(field.split('=', 1) for field in f[4:])
    assert set(attrs) == {'pose', 'glue', 'priority', 'searched', 'used', 'reason', 'shrunken', 'values'}
    pose = tuple(map(int, attrs.pop('pose').split(',')))
    assert tuple(sorted(pose)) == literals
    values = tuple(tuple(map(int, v.split(':'))) for v in attrs.pop('values').split(','))
    assert tuple(v[0] for v in values) == literals
    assert all(v[1] in (-1, 0, 1) for v in values)
    meta = tuple((k, int(v)) for k, v in sorted(attrs.items()))
    assert all(v >= 0 for _, v in meta)
    return (int(f[0]), literals), pose, meta, values

def fixtures():
    good = '0\t3\t0000000000000001\t-3,1,2\tpose=1,-3,2\tglue=0\tpriority=0\tsearched=2\tused=0\treason=0\tshrunken=0\tvalues=-3:-1,1:0,2:0'
    assert parse_line(good)[3] == ((-3, -1), (1, 0), (2, 0))
    for before, after in [('values=-3:-1', 'values=-3:2'), ('values=-3:-1', 'values=-4:-1'),
                          ('pose=1,-3,2', 'pose=1,-3,4'), ('\t3\t', '\t4\t'),
                          ('used=0', 'reason=0'), ('0000000000000001', 'not-a-hash')]:
        try: parse_line(good.replace(before, after))
        except (ValueError, AssertionError): pass
        else: raise AssertionError('malformed snapshot accepted')
    assert len(Counter([parse_line(good)[0]] * 2)) == 1
    assert Counter([parse_line(good)[0]] * 2)[parse_line(good)[0]] == 2
    print('PASS snapshot parser: valid pose/roots,duplicate multiplicity,six malformed cases', flush=True)

def read_rows(path):
    with path.open() as stream:
        assert next(stream).rstrip() in ('# tactix-formula-set\t2', '# kissat-formula-set\t2')
        assert next(stream).rstrip() == '# redundant\tsize\thash\tliterals'
        return [parse_line(line) for line in stream]

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    boundary = json.loads((parent / 'boundary-localization.json').read_text())
    stages = {}
    for number, stage in ((5782, 'begin'), (5783, 'end')):
        c = root / f'benchmarks/runs/20260908-{number}-rank32-forward-{stage}-formula'
        target = c / 'summary.json'
        if target.exists():
            stages[stage] = json.loads(target.read_text())
            print(stage, 'already sealed', flush=True)
            continue
        if any(not (c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
            print(stage, 'pending terminal stops', flush=True)
            continue
        m = json.loads((c / 'manifest.json').read_text())
        for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
        data = {}
        evidence = {}
        for arm in ('candidate', 'kissat'):
            rp = c / 'results' / (arm + '.json')
            r = json.loads(rp.read_text())
            old = json.loads((parent / 'results' / (arm + '.json')).read_text())
            assert r['exit_code'] == 0 and r['scratch_deleted']
            assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
            assert r['partial_certificate'] == old['partial_certificate']
            for rel, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / rel) == digest
            for rel, digest in old['trace_sha256'].items():
                assert r['trace_sha256'][rel] == digest == sha(parent / 'traces' / rel)
            assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.formula.tsv'}
            data[arm] = read_rows(c / 'traces' / (arm + '.formula.tsv'))
            expected = boundary['selected_boundary_pairs'][arm][0 if stage == 'begin' else 1]
            assert Counter(row[0][0] for row in data[arm]) == {0: int(expected['irred']), 1: int(expected['red'])}
            evidence[arm] = dict(result_sha256=sha(rp), traces=r['trace_sha256'])
        a, b = data['candidate'], data['kissat']
        ac, bc = Counter(row[0] for row in a), Counter(row[0] for row in b)
        a_only, b_only = ac - bc, bc - ac
        keys_equal = [r[0] for r in a] == [r[0] for r in b]
        first = next((dict(index=i, candidate=x, kissat=y) for i, (x, y) in enumerate(itertools.zip_longest(a, b)) if x != y), None)
        def witness(counts, rows):
            wanted = set(counts)
            return [dict(copies=counts[row[0]], row=row) for row in rows if row[0] in wanted][:30]
        out = dict(stage=stage, observer_parent_trace_and_proof_equal=True, evidence=evidence,
                   large_clause_multisets_equal=ac == bc, physical_clause_order_equal=keys_equal,
                   candidate_only_copies=sum(a_only.values()), kissat_only_copies=sum(b_only.values()),
                   candidate_only=witness(a_only, a), kissat_only=witness(b_only, b),
                   first_physical_difference=first,
                   pose_differences=sum(x[1] != y[1] for x, y in zip(a, b)) if keys_equal else None,
                   metadata_differences=sum(x[2] != y[2] for x, y in zip(a, b)) if keys_equal else None,
                   root_value_differences=sum(x[3] != y[3] for x, y in zip(a, b)) if keys_equal else None,
                   limitation='Complete large-clause snapshots only;binary watch order and scheduling flags are not included. Difference witnesses capped at30 rows;copy counts cover complete multisets. Metadata differences are recorded,not all fields assumed semantically portable.')
        target.write_text(json.dumps(out, indent=2) + '\n')
        stages[stage] = out
        print(json.dumps({k: v for k, v in out.items() if k != 'evidence'}, indent=2), flush=True)
    print('Sealed stages:', ','.join(stages))

if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Validate framed complete binary multisets and the selected subsumer."""
from collections import Counter
import hashlib
import io
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5794-rank32-forward-binaries'
parent = root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

def parse(stream):
    header = next(stream).rstrip('\n')
    assert header in ('# tactix-binary-set-snapshot\t1', '# kissat-binary-set-snapshot\t1')
    assert next(stream).rstrip('\n') == '# snapshot\ta\tb'
    snapshots, current = [], None
    for line in stream:
        f = line.rstrip('\n').split('\t')
        if f[0] == '# begin':
            assert current is None and len(f) == 7 and int(f[1]) == len(snapshots)
            assert f[2] in ('tactix', 'kissat') and f[3] == 'slot_boundary'
            assert f[6] in ('name=Strengthen|stage=begin', 'name=Strengthen|stage=end')
            current = dict(index=int(f[1]), conflict=int(f[4]), count=int(f[5]),
                           stage=f[6].split('=')[-1], pairs=[])
        elif f[0] == '# end':
            assert current is not None and len(f) == 2 and int(f[1]) == current['index']
            assert len(current['pairs']) == current['count']
            assert current['pairs'] == sorted(current['pairs'])
            snapshots.append(current)
            current = None
        else:
            assert current is not None and len(f) == 3 and int(f[0]) == current['index']
            pair = tuple(map(int, f[1:]))
            assert pair[0] and pair[1] and pair[0] < pair[1]
            current['pairs'].append(pair)
    assert current is None and snapshots
    return snapshots

def fixtures():
    good = '# tactix-binary-set-snapshot\t1\n# snapshot\ta\tb\n# begin\t0\ttactix\tslot_boundary\t7\t2\tname=Strengthen|stage=begin\n0\t-1\t2\n0\t-1\t2\n# end\t0\n'
    # The production framing has seven fields, including the begin marker.
    got = parse(io.StringIO(good))
    assert Counter(got[0]['pairs']) == {(-1, 2): 2}
    for before, after in [('# end\t0\n', ''), ('\t7\t2\t', '\t7\t3\t'),
                          ('# end\t0', '# end\t1'), ('0\t-1\t2', '1\t-1\t2'),
                          ('0\t-1\t2', '0\t0\t2'), ('stage=begin', 'stage=wrong')]:
        try: parse(io.StringIO(good.replace(before, after)))
        except (AssertionError, ValueError, StopIteration): pass
        else: raise AssertionError((before, after))
    print('PASS binary framing: duplicate preservation and six malformed controls', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    target = c / 'summary.json'
    if target.exists():
        print('Already sealed:', target)
        return
    if any(not (c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
        print('Pending terminal results; no validation claim.')
        return
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
    boundary = json.loads((parent / 'boundary-localization.json').read_text())
    snapshots, evidence = {}, {}
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
        assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.binaries.tsv'}
        with (c / 'traces' / (arm + '.binaries.tsv')).open() as stream: snapshots[arm] = parse(stream)
        expected = [row for row in boundary['selected_boundary_pairs'][arm] if row['name'] == 'strengthen']
        assert len(snapshots[arm]) == len(expected) == 4
        for snap, row in zip(snapshots[arm], expected):
            assert (snap['stage'], snap['conflict'], snap['count']) == (row['stage'], int(row['conflicts']), int(row['binary']))
        evidence[arm] = dict(result_sha256=sha(rp), trace_sha256=r['trace_sha256'])
    pairs = []
    subsumer = (217824, 217939)
    for a, b in zip(snapshots['candidate'], snapshots['kissat']):
        ac, bc = Counter(a['pairs']), Counter(b['pairs'])
        pairs.append(dict(index=a['index'], stage=a['stage'], equal=ac == bc,
                          candidate_only_copies=sum((ac-bc).values()), kissat_only_copies=sum((bc-ac).values()),
                          candidate_subsumer_copies=ac[subsumer], kissat_subsumer_copies=bc[subsumer]))
    witness = root / 'benchmarks/runs/20260908-5787-rank32-native-forward-end-formula/summary.json'
    extra = json.loads(witness.read_text())['kissat_only']
    covered = all(set(subsumer) <= set(row['row'][0][1]) for row in extra)
    out = dict(observer_parent_trace_and_proof_equal=True, evidence=evidence, snapshots=pairs,
               subsumer=subsumer, all_eight_extra_native_clauses_contain_subsumer=covered,
               limitation='Physical binary multiplicities, not watch order or scheduling flags. Deletion equivalence does not prove downstream time neutrality.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps({k:v for k,v in out.items() if k != 'evidence'}, indent=2))

if __name__ == '__main__': main()

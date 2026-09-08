#!/usr/bin/env python3
"""Validate selected-clause frames and exact boundary coverage before localization."""
import csv
import hashlib
import io
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
C = ROOT / 'benchmarks/runs/20260908-6008-clause-pose-series'
KEY = {-246596, -5409, -47, 49, 5415, 148944, 238853}
NAMES = {'congruence', 'substitute', 'backbone', 'vivify', 'sweep', 'factor', 'strengthen', 'bve', 'sparsecollect'}
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

def parse(stream, key=KEY):
    assert next(stream).rstrip() == '# clause-pose-series\t1'
    frames, current = [], None
    for line in stream:
        row = line.rstrip('\n').split('\t')
        if row[0] == '# begin':
            assert current is None and len(row) == 6 and int(row[1]) == len(frames)
            assert row[5] in ('begin', 'end') and row[4]
            current = dict(ordinal=int(row[1]), conflicts=int(row[2]), decisions=int(row[3]),
                           name=row[4], stage=row[5], clauses=[])
            assert current['conflicts'] >= 0 and current['decisions'] >= 0
        elif row[0] == '# end':
            assert current is not None and len(row) == 3
            assert int(row[1]) == current['ordinal'] and int(row[2]) == len(current['clauses'])
            frames.append(current); current = None
        else:
            assert current is not None and len(row) == 6 and int(row[0]) == current['ordinal']
            pose = list(map(int, row[4].split(','))); values = list(map(int, row[5].split(',')))
            assert len(pose) == len(key) and set(pose) == key and len(values) == len(pose)
            assert all(v in (-1, 0, 1) for v in values)
            assert int(row[1]) >= 0 and row[2] in ('0', '1') and 2 <= int(row[3]) < len(pose)
            current['clauses'].append(dict(ref=int(row[1]), redundant=int(row[2]), searched=int(row[3]), pose=pose, values=values))
    assert current is None and frames
    return frames

def fixtures():
    good = '# clause-pose-series\t1\n# begin\t0\t1\t0\tvivify\tbegin\n0\t4\t0\t2\t1,-2,3\t0,0,0\n0\t8\t0\t2\t-2,1,3\t0,0,0\n# end\t0\t2\n# begin\t1\t1\t0\tvivify\tend\n# end\t1\t0\n'
    assert [len(f['clauses']) for f in parse(io.StringIO(good), {1, -2, 3})] == [2, 0]
    for old, new in [('# end\t1\t0\n', ''), ('# end\t0\t2', '# end\t0\t1'),
                     ('# begin\t1', '# begin\t3'), ('1,-2,3', '1,1,3'),
                     ('0,0,0', '0,2,0'), ('\t0\t2\t1,-2,3', '\t0\t3\t1,-2,3'),
                     ('\tvivify\tbegin', '\tvivify\tinvalid'), ('# clause-pose-series\t1', '# clause-pose-series\t2')]:
        try: parse(io.StringIO(good.replace(old, new)), {1, -2, 3})
        except (AssertionError, ValueError, StopIteration): pass
        else: raise AssertionError('Malformed series accepted')
    print('PASS: selected pose framing, duplicate multiplicity, disappearance and eight rejection controls', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    target = C / 'summary.json'; assert not target.exists()
    m = json.loads((C / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(C / rel) == digest, rel
    arms, evidence = {}, {}
    for arm in ('candidate', 'kissat'):
        p = C / 'results' / (arm + '.json'); r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['binary_sha256'] == sha(C / 'pins' / arm)
        binding = m['cached_controls'][arm]; oldp = ROOT / binding['record']
        assert sha(oldp) == binding['sha256']; old = json.loads(oldp.read_text())
        assert r['partial_certificate'] == old['partial_certificate']
        for name, digest in old['trace_sha256'].items(): assert r['trace_sha256'][name] == digest, name
        assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.bve-candidates.tsv', arm + '.clause-pose-series.tsv'}
        parent = json.loads((ROOT / m['parent_campaign'] / 'results' / (arm + '.json')).read_text())
        assert r['trace_sha256'][arm + '.bve-candidates.tsv'] == parent['trace_sha256'][arm + '.bve-candidates.tsv']
        for name, digest in r['trace_sha256'].items(): assert sha(C / 'traces' / name) == digest, name
        with (C / 'traces' / (arm + '.clause-pose-series.tsv')).open() as stream: frames = parse(stream)
        with (C / 'traces' / (arm + '.boundaries.tsv')).open() as stream:
            next(stream); header = next(stream).removeprefix('# ').rstrip().split('\t')
            rows = list(csv.DictReader(stream, fieldnames=header, delimiter='\t'))
        expected = [(int(x['conflicts']), int(x['decisions']), x['name'].lower(), x['stage']) for x in rows
                    if x['name'].lower() in NAMES and 0 <= int(x['conflicts']) <= 271079]
        actual = [(x['conflicts'], x['decisions'], x['name'], x['stage']) for x in frames]
        assert actual == expected, (arm, len(actual), len(expected))
        arms[arm] = frames
        evidence[arm] = dict(result_sha256=sha(p), trace_sha256=r['trace_sha256'], frame_count=len(frames), boundary_coverage=True)
    # Align common boundary occurrences, preserving repeated names at one conflict.
    def keyed(frames):
        seen, out = {}, {}
        for f in frames:
            key = (f['conflicts'], f['name'], f['stage']); ordinal = seen.get(key, 0); seen[key] = ordinal + 1
            out[key + (ordinal,)] = f
        return out
    a, b = keyed(arms['candidate']), keyed(arms['kissat'])
    changes = {}
    for arm, frames in arms.items():
        changes[arm] = []; prev = None
        for f in frames:
            state = [(c['redundant'], c['searched'], c['pose']) for c in f['clauses']]
            if state != prev: changes[arm].append(f); prev = state
    first = None
    for key, x in a.items():
        if key not in b: continue
        y = b[key]
        state = lambda f: [(c['redundant'], c['searched'], c['pose']) for c in f['clauses']]
        if state(x) != state(y):
            first = dict(key=key, candidate=x, kissat=y); break
    out = dict(full_replay=True, evidence=evidence, first_common_boundary_pose_difference=first,
               pose_changes=changes, one_sided_boundaries=dict(candidate=len(a.keys()-b.keys()), kissat=len(b.keys()-a.keys())),
               limitation='Selected canonical clause only; absence can precede creation or follow deletion. Common phase boundaries localize an interval, not an individual propagator mutation. Runtime and final proof acceptance unchanged.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps({k: v for k, v in out.items() if k not in ('evidence', 'pose_changes')}, indent=2))

if __name__ == '__main__': main()

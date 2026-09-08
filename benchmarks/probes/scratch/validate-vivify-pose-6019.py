#!/usr/bin/env python3
"""Strict candidate-pose framing and full parent replay before interpretation."""
import csv
import hashlib
import io
import itertools
import json
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
C = ROOT / 'benchmarks/runs/20260908-6020-vivify-pose'
KEY = {-246596, -5409, -47, 49, 5415, 148944, 238853}
FIELDS = 'seq event conflicts scope ordinal ticks limit selected_ref searched pose values candidate_ref redundant garbage candidate'.split()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for b in iter(lambda: stream.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
def portable_scope(scope, arm):
    # Native tier IDs are not Tactix public scope IDs. Preserve raw IDs.
    if arm == 'candidate':
        assert scope in (2, 3, 4, 5), 'Unscoped/legacy modes have no one-tier native equivalent'
        return {2: 0, 3: 1, 4: 2, 5: 3}[scope]
    assert arm == 'kissat' and 0 <= scope <= 3
    return scope
def parse(stream, key=KEY, conflict=270978, arm=None):
    assert next(stream).rstrip() == '# vivify-pose\t1'
    assert next(stream).removeprefix('# ').rstrip().split('\t') == FIELDS
    rows = []; current = None; rounds = {}; next_ordinal = 0
    for fields in csv.reader(stream, delimiter='\t'):
        assert len(fields) == len(FIELDS)
        r = dict(zip(FIELDS, fields))
        for name in FIELDS:
            if name not in ('event', 'pose', 'values', 'candidate'): r[name] = int(r[name])
        assert r['seq'] == len(rows) and r['conflicts'] == conflict
        assert 0 <= r['scope'] <= (5 if arm != 'kissat' else 3) and all(r[n] >= 0 for n in ('ordinal', 'ticks', 'limit', 'candidate_ref'))
        r['portable_scope'] = portable_scope(r['scope'], arm) if arm else r['scope']
        assert r['redundant'] in (0, 1) and r['garbage'] in (0, 1)
        assert r['event'] in ('vivify_candidate', 'vivify_candidate_end', 'vivify_budget_stop')
        if r['selected_ref'] == 4294967295:
            assert r['searched'] == 0 and r['pose'] == '-' and r['values'] == '-'
            r['pose'], r['values'] = [], []
        else:
            r['pose'] = list(map(int, r['pose'].split(','))); r['values'] = list(map(int, r['values'].split(',')))
            assert r['selected_ref'] >= 0 and set(r['pose']) == key and len(r['pose']) == len(key)
            assert len(r['values']) == len(key) and all(x in (-1, 0, 1) for x in r['values'])
            assert 2 <= r['searched'] < len(key)
        r['candidate'] = list(map(int, r['candidate'].split(',')))
        assert len(r['candidate']) >= 3 and all(r['candidate'])
        assert len(set(map(abs, r['candidate']))) == len(r['candidate'])
        if r['event'] != 'vivify_candidate_end' and r['ordinal'] == 0:
            assert current is None
            rounds[r['scope']] = rounds.get(r['scope'], -1) + 1; next_ordinal = 0
        r['scope_occurrence'] = rounds.get(r['scope'], -1); assert r['scope_occurrence'] >= 0
        if r['event'] == 'vivify_candidate':
            assert current is None and r['ordinal'] == next_ordinal
            current = r
        elif r['event'] == 'vivify_candidate_end':
            assert current is not None
            assert all(r[n] == current[n] for n in ('scope', 'scope_occurrence', 'ordinal', 'candidate_ref', 'limit'))
            assert r['ticks'] >= current['ticks']; next_ordinal += 1; current = None
        else:
            assert current is None and r['ordinal'] == next_ordinal and r['ticks'] > r['limit']
        rows.append(r)
    # A stopped fixed control may have a final begin; the full run requires closure separately.
    assert rows
    return rows, current
def fixtures():
    assert [portable_scope(s, 'candidate') for s in (2, 3, 4, 5)] == list(range(4))
    assert [portable_scope(s, 'kissat') for s in range(4)] == list(range(4))
    for scope, arm in ((0, 'candidate'), (1, 'candidate'), (4, 'kissat'), (6, 'candidate')):
        try: portable_scope(scope, arm)
        except AssertionError: pass
        else: raise AssertionError('Invalid portable scope accepted')
    head = '# vivify-pose\t1\n# ' + '\t'.join(FIELDS) + '\n'
    good = head + '0\tvivify_candidate\t1\t1\t0\t4\t20\t1\t2\t1,-2,3\t0,0,0\t7\t0\t0\t1,2,4\n1\tvivify_candidate_end\t1\t1\t0\t5\t20\t1\t2\t-2,1,3\t0,0,0\t7\t0\t0\t1,2,4\n'
    assert parse(io.StringIO(good), {1,-2,3}, 1)[1] is None
    for raw, portable in ((2, 0), (3, 1), (4, 2), (5, 3)):
        mapped = good.replace('\t1\t1\t0\t', f'\t1\t{raw}\t0\t')
        rows, pending = parse(io.StringIO(mapped), {1,-2,3}, 1, arm='candidate')
        assert pending is None and all(r['scope'] == raw and r['portable_scope'] == portable for r in rows)
    for old, new in [('1,-2,3', '1,1,3'), ('0,0,0', '0,2,0'),
                     ('\t1\t1\t0\t5\t20', '\t1\t1\t1\t5\t20'),
                     ('\t5\t20', '\t3\t20'), ('\t7\t0\t0', '\t7\t2\t0'),
                     ('1\tvivify_candidate_end', '3\tvivify_candidate_end'),
                     ('vivify_candidate_end', 'bad-event'), ('\t1\t2\t1,-2,3', '\t1\t3\t1,-2,3')]:
        try: parse(io.StringIO(good.replace(old, new)), {1,-2,3}, 1)
        except (AssertionError, ValueError, StopIteration): pass
        else: raise AssertionError('Malformed candidate row accepted')
    print('PASS: candidate/scope/ordinal framing, all four Tactix/native tier mappings, eight malformed-row controls', flush=True)
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
        parentp = ROOT / m['parent_campaign'] / 'results' / (arm + '.json'); parent = json.loads(parentp.read_text())
        assert r['partial_certificate'] == parent['partial_certificate']
        for name, digest in parent['trace_sha256'].items(): assert r['trace_sha256'][name] == digest, name
        assert set(r['trace_sha256']) - set(parent['trace_sha256']) == {arm + '.vivify-pose.tsv'}
        for name, digest in r['trace_sha256'].items(): assert sha(C / 'traces' / name) == digest, name
        with (C / 'traces' / (arm + '.vivify-pose.tsv')).open() as stream: rows, pending = parse(stream, arm=arm)
        assert pending is None, (arm, pending)
        arms[arm] = rows
        evidence[arm] = dict(result_sha256=sha(p), parent_result_sha256=sha(parentp), trace_sha256=r['trace_sha256'], rows=len(rows))
    first = None
    for i, (a, b) in enumerate(itertools.zip_longest(arms['candidate'], arms['kissat'])):
        fields = ('event', 'portable_scope', 'scope_occurrence', 'ordinal', 'redundant', 'garbage', 'pose', 'searched')
        if a is None or b is None or any(a[k] != b[k] for k in fields) or sorted(a['candidate']) != sorted(b['candidate']):
            first = dict(index=i, candidate=a, kissat=b); break
    changes = {}
    for arm, rows in arms.items():
        changes[arm] = []; previous = None
        for r in rows:
            state = (r['pose'], r['searched'])
            if state != previous: changes[arm].append(r); previous = state
    out = dict(full_parent_replay=True, evidence=evidence, first_portable_row_difference=first, selected_pose_changes=changes,
               limitation='Candidate scope occurrence and ordinal are preserved, not assumed equivalent if processing diverges. Absolute tick origins differ; inspect per-candidate deltas and budgets separately. Observer caches the first live exact-clause copy; phase history establishes one copy at its boundaries, not universal transient uniqueness. No runtime or final-proof acceptance claim.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps({k:v for k,v in out.items() if k not in ('evidence','selected_pose_changes')}, indent=2))
if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Seal observer replay and the no-gates, nonzero-merge early-return witness."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from search_window import read_search
c = root / 'benchmarks/runs/20260909-6651-rank49-congruence-stages'
target = c / 'summary-6653.json'
assert not target.exists()
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['hashes'].items(): bind(c / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for rel in ('manifest.json', 'launch.json', 'results/candidate.json'): bind(c / rel)
parent = root / m['cached_controls']
r = json.loads((c / 'results/candidate.json').read_text())
old = json.loads((parent / 'results/candidate.json').read_text())
assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == 'candidate'
assert r['binary_sha256'] == old['binary_sha256'] == m['hashes']['pins/candidate']
assert r['partial_certificate'] == old['partial_certificate']
for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
for name, digest in old['trace_sha256'].items(): assert r['trace_sha256'][name] == digest, name
assert set(r['trace_sha256']) - set(old['trace_sha256']) == {
    'candidate.congruence-memory.tsv', 'candidate.congruence-binaries.tsv', 'candidate.slot-memory.tsv'}
def fields(line, start):
    values = [x.split('=') for x in line.split('\t')[start:]]
    assert all(len(x) == 2 and x[1].isdecimal() for x in values)
    assert len({x[0] for x in values}) == len(values)
    return {key: int(value) for key, value in values}
memory = (c / 'traces/candidate.congruence-memory.tsv').read_text()
assert memory.endswith('\n')
rows = [dict(stage=line.split('\t')[0], **fields(line, 1)) for line in memory.splitlines()]
starts = [i for i, row in enumerate(rows) if row['stage'] == 'root-end']
assert len(starts) == 4
rounds = [rows[a:b] for a, b in zip(starts, starts[1:] + [len(rows)])]
selected = rounds[1]
assert all(row['vars'] == 2202 for row in selected)
def stage(name): return [r for r in selected if r['stage'] == name]
assert len(stage('gates-end')) == 1 and stage('gates-end')[0]['first'] == 0
assert len(stage('binary-import-merge-added')) == 1 and stage('binary-import-merge-added')[0]['first'] == 1
assert selected[-1]['stage'] == 'binary-import-scratch-released'
assert not stage('closure-forward-begin')
slots = []
for line in (c / 'traces/candidate.slot-memory.tsv').read_text().splitlines():
    name, phase = line.split('\t')[:2]
    row = dict(name=name, phase=phase, **fields(line, 2))
    if row['memory_conflicts'] == 2988 and name == 'congruence-dense-resume': slots.append(row)
assert [r['phase'] for r in slots] == ['entry', 'first']
native_path = parent / 'traces/kissat.search.tsv'
bind(native_path)
native = [r for r in read_search(native_path.read_text(), solver='kissat', window=(9268,9275), cap=250000)
          if r['conflicts'] == 2988 and r['event'].startswith('congruence_dense_')]
assert [r['event'] for r in native] == ['congruence_dense_entry', 'congruence_dense_first', 'congruence_dense_second_entry', 'congruence_dense_second']
assert [r['binary'] for r in native] == [451,317,319,313]
for rel in ('.claude/worktrees/runtime-material-schedule-6579/src/rules/congruence/congruence.cpp',
            'tools/solvers/variants/kissat-macro-heap-6090/src/congruence.c',
            'tools/solvers/variants/kissat-macro-heap-6090/src/dense.c'):
    bind(root / rel)
out = dict(rank=49, full_own_replay=True, candidate_round=selected, candidate_dense_boundaries=slots,
           native_dense_boundaries=native, hashes=hashes,
           conclusion='Current has zero gates but one binary equivalence merge, then returns at gates.empty before the second dense transition. Native performs that transition at2988 and reduces319 to313 binaries. Combined6645/6650 evidence accounts Probe population/deadline shift; actual performance effect requires experimental repair.',
           limitations='Stage trace lacks conflict IDs; selected second actual apply is joined to slot memory by unique2202-variable round and2988 boundaries. No memory timing claim or native rerun. No source change or correctness failure.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(out['conclusion'])

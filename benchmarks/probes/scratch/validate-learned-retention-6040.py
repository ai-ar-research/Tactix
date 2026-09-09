#!/usr/bin/env python3
"""Require full parent replay, then compare all post-learning reductions."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6038-learned-retention'
target = c / 'summary.json'; assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
subprocess.run(['python3', 'tests/test_compare_reduce_trace.py'], cwd=root, check=True)
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
evidence = {}
modulepath = root / 'tools/bench/compare_reduce_trace.py'
spec = importlib.util.spec_from_file_location('reduce_compare', modulepath)
mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
tables = {}
for arm in ('candidate', 'kissat'):
    p = c / 'results' / (arm + '.json'); r = json.loads(p.read_text())
    parentp = root / m['parent_campaign'] / 'results' / (arm + '.json'); parent = json.loads(parentp.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == parent['binary_sha256'] == sha(c / 'pins' / arm)
    assert r['partial_certificate'] == parent['partial_certificate']
    assert set(r['trace_sha256']) - set(parent['trace_sha256']) == {arm + '.reduce.tsv'}
    for name, digest in parent['trace_sha256'].items(): assert r['trace_sha256'][name] == digest, name
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest, name
    evidence[arm] = dict(result_sha256=sha(p), parent_result_sha256=sha(parentp), trace_sha256=r['trace_sha256'])
    tables[arm] = mod.load(c / 'traces' / (arm + '.reduce.tsv'))
assert set(tables['candidate'][0]) == set(tables['kissat'][0]) and tables['candidate'][0]
comparisons = []
for reduction in sorted(tables['candidate'][0]):
    x, y = (tables[arm][0][reduction] for arm in ('candidate','kissat'))
    assert x['conflict'] == y['conflict'] and 245473 <= x['conflict'] <= 270978
    comparisons.append(mod.compare(c / 'traces/candidate.reduce.tsv', c / 'traces/kissat.reduce.tsv', reduction))
key = tuple(sorted((53,51,-47,-93630,-4166,-4349,4172)))
selected = {arm: [row for row in clauses if row['literals'] == key] for arm, (_,clauses) in tables.items()}
out = dict(full_parent_replay=True, evidence=evidence, comparator_sha256=sha(modulepath),
    reductions=comparisons, selected_clause_rows=selected,
    limitation='Observer snapshots preserve each source callback phase. Missing selected clause before a reduction does not prove that reduction deleted it. Same reduction number must also match conflict/mode; no runtime or final-proof claim.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps({k:v for k,v in out.items() if k != 'evidence'}, indent=2))

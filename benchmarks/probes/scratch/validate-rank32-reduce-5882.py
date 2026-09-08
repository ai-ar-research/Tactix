#!/usr/bin/env python3
"""Validate observer neutrality, then compare complete reduction state."""
import hashlib
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5869-rank32-reduce-frontier'
target = c / 'reduce-validation.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
bindings = {str((c / 'manifest.json').relative_to(root)): sha(c / 'manifest.json')}
for arm in ('candidate', 'kissat'):
    p = c / 'results' / (arm + '.json')
    r = json.loads(p.read_text())
    control = m['cached_controls'][arm]
    oldpath = root / control['record']
    assert sha(oldpath) == control['sha256']
    old = json.loads(oldpath.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
    assert r['partial_certificate'] == old['partial_certificate'], arm
    for name, digest in r['trace_sha256'].items():
        assert sha(c / 'traces' / name) == digest, name
        if not name.endswith('.reduce.tsv'): assert digest == old['trace_sha256'][name], name
    bindings[str(p.relative_to(root))] = sha(p)
    bindings[str(oldpath.relative_to(root))] = sha(oldpath)
modulepath = root / 'tools/bench/compare_reduce_trace.py'
spec = importlib.util.spec_from_file_location('reduce_compare', modulepath)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
result = mod.compare(c / 'traces/candidate.reduce.tsv', c / 'traces/kissat.reduce.tsv', 49)
out = dict(observer_neutrality='Both arms exactly replay all ordinary trace hashes and partial proof bytes from frozen controls through300K. Not a checked final proof or timing claim.',
           comparison=result, hashes=bindings, comparator_sha256=sha(modulepath))
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(result, indent=2))

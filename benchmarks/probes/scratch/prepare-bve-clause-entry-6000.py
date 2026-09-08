#!/usr/bin/env python3
"""Bracket whether the causal clause pose already differs before forward."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5937-first-bve-frontier'
c = root / 'benchmarks/runs/20260908-6000-bve-clause-entry'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
assert not c.exists()
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(parent / rel) == digest, rel
evidence = root / 'benchmarks/runs/20260908-5996-bve-reinsert-order/heap-validation.json'
v = json.loads(evidence.read_text())
assert v['full_ordinary_trace_and_partial_proof_replay']
assert v['completed_insertions']['candidate'][0]['subject'] == '5409'
assert v['completed_insertions']['kissat'][0]['subject'] == '5415'
c.mkdir()
for name in ('pins', 'logs', 'results', 'traces'):
    (c / name).mkdir()
for p in (parent / 'pins').iterdir():
    shutil.copy2(p, c / 'pins' / p.name)
runner = (parent / 'run.py').read_text().replace('tactix-5937-', 'tactix-6000-')
old = "SAT_EVENT_TRACE_FORMULA_SET_MATCH='name=Strengthen|stage=end'"
assert runner.count(old) == 1
runner = runner.replace(old, "SAT_EVENT_TRACE_FORMULA_SET_MATCH='name=Strengthen|stage=begin'")
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('5937-first-bve-frontier', '6000-bve-clause-entry'))
m.update(parent_campaign=str(parent.relative_to(root)), formula_stage='begin', formula_ordinal=0,
    causal_evidence=dict(path=str(evidence.relative_to(root)), sha256=sha(evidence)),
    hypothesis='The seven-literal clause responsible for opposite5409/5415 reinsertion order may already have different physical pose before first forward. Inspect exact canonical set -246596,-5409,-47,49,5415,148944,238853 at firstStrengthen begin271079 and compare frozen5937 exit. If already different, investigate earlier sparse watch/phase representation; if not, inspect forward transformation.',
    test_consideration='No source change. Reuse frozen5937 pins, full ordinary trace/proof replay, strict full-formula parser and explicit ordinal0 boundary controls. New configuration is syntax-checked and hash-bound.',
    native_rerun_reason='Missing firstStrengthen entry formula pose only; fixed runtime denominator remains6810.066s.',
    limitation='Diagnostic on retained5900; use5962 for any implementation. No runtime claim, promotion, or fuzz activity.',
    created_at=__import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat())
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir()]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)

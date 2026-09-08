#!/usr/bin/env python3
"""Reuse frozen tested pins to trace the actual native-only Vivify candidate."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-6020-vivify-pose'
c = root / 'benchmarks/runs/20260908-6024-vivify-lemma-history'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert not c.exists()
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest, rel
assert json.loads((parent / 'summary.json').read_text())['full_parent_replay']
analysis = json.loads((parent / 'candidate-analysis.json').read_text())
last = analysis['within_candidate_pose_mutations']['kissat'][-1]['begin']
assert (last['scope'], last['scope_occurrence'], last['ordinal']) == (2, 1, 2249)
assert set(last['candidate']) == {53, 51, -47, -93630, -4166, -4349, 4172}
c.mkdir()
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
runner = (parent / 'run.py').read_text().replace('tactix-6020-', 'tactix-6024-')
old = "env['SAT_CLAUSE_POSE_SERIES_LITS'] = '-246596,-5409,-47,49,5415,148944,238853'"
assert runner.count(old) == 1
runner = runner.replace(old, "env['SAT_CLAUSE_POSE_SERIES_LITS'] = '53,51,-47,-93630,-4166,-4349,4172'")
runner = runner.replace("'congruence,substitute,backbone,vivify,sweep,factor,strengthen,bve,sparsecollect'", "'congruence,substitute,backbone,vivify,sweep,factor,strengthen,bve,sparsecollect,reduce'")
compile(runner, str(c / 'run.py'), 'exec'); (c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('6020-vivify-pose', '6024-vivify-lemma-history'))
m.update(parent_campaign=str(parent.relative_to(root)),
    hypothesis='The exact native tier2 candidate2249 mutates selected BVE-triggering pose in second Vivify270978; it never appears in either Tactix captured schedule and is native-only at5937. Trace its presence at macro boundaries including Reduce to distinguish creation/retention/simplification origin.',
    validation='Exact6020 partial certificate and every trace except deliberately changed clause-pose-series; retain original selected-pose candidate trace unchanged. Require complete macro/Reduce boundary coverage.',
    native_rerun_reason='Missing birth/deletion history of the now-proven causal native-only learned clause; not a timing refresh.',
    test_consideration='No new solver code or rebuild: reuse99/99-tested6016 pins and native controls. Existing strict frame parser has eight malformed-row tests; new validation compares unchanged6020 traces and selected-clause identity.',
    causal_evidence=dict(path=str((parent / 'candidate-analysis.json').relative_to(root)), sha256=sha(parent / 'candidate-analysis.json')),
    created_at=__import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat())
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir()]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

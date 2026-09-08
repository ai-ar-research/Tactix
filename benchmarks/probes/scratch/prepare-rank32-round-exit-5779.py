#!/usr/bin/env python3
"""Reuse the neutral5760 observer on the newly localized Rank32 boundary."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
parent = runs / '20260908-5771-reuse-target-rank32-extended'
observer = runs / '20260908-5764-rank31-forward-exit'
c = runs / '20260908-5779-rank32-forward-round'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
summary = json.loads((parent / 'summary.json').read_text())
assert summary['first_decision_difference']['candidate'][0] == '179463'
om = json.loads((observer / 'manifest.json').read_text())
for rel, digest in om['hashes'].items(): assert sha(observer / rel) == digest
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
shutil.copy2(observer / 'pins/candidate', c / 'pins/candidate')
runner = (parent / 'run.py').read_text().replace('tactix-5771-', 'tactix-5779-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "env['TACTIX_FORWARD_ROUND_TRACE'] = str(c / 'traces/candidate.forward-round.tsv')\nenv['SAT_FORWARD_ROUND_CONFLICT'] = '179463'\n" + anchor)
(c / 'run.py').write_text(runner)
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c))
assert '#SBATCH --array=0-1' in batch
(c / 'array.sbatch').write_text(batch.replace('#SBATCH --array=0-1', '#SBATCH --array=0-0'))
m.update(parent_campaign=str(parent.relative_to(root)), candidate_authority=om['candidate_authority'],
         candidate_source=om['candidate_source'], source_patch_sha256=om['source_patch_sha256'],
         hypothesis='Rank32 first differs after Strengthen179463: equal incoming counts but eight fewer outgoing large clauses. Test unit exit versus full or quota-limited pass using existing observer5760; do not assume Rank31 cause transfers.',
         validation='All ordinary300K traces and partial proof must match5771 candidate. Round observations are diagnostic only.',
         parent_summary=dict(path=str((parent / 'summary.json').relative_to(root)), sha256=sha(parent / 'summary.json')),
         test_consideration='Unchanged91-test observer5760 already neutral on Rank31. New conflict/input configuration only; exact-parent full trace/proof replay required, no new solver unit test.',
         resource_contract=dict(partition='cpu-dense-hwulab-q', cpus_per_task=12, memory_mib=48000,
                                array='0-0', throttle=None, node_constraint=None),
         contract='One candidate-only proof-on diagnostic;12logicalCPUs/48000MiB,full-width,no native rerun.',
         cached_native_campaign=str(parent.relative_to(root)))
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in (c / 'run.py', c / 'array.sbatch', c / 'pins/candidate')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)

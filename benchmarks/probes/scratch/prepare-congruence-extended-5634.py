#!/usr/bin/env python3
"""Continue past the old182093 split only after full common100K replay closes."""
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
parent=root/'benchmarks/runs/20260908-5627-congruence-index-rank33-prefix'
if not (parent/'summary.json').exists():
    print('Validated5627 prefix pending');raise SystemExit(0)
prior=json.loads((parent/'summary.json').read_text())
if not prior['complete_prefix_identical']:
    print('New100K divergence requires diagnosis before extension:',prior['first_decision_difference']);raise SystemExit(0)
source=Path(__file__).with_name('prepare-rank33-extended-5567.py').read_text()
source=source.replace('20260908-5558-rank33-small-prefix','20260908-5627-congruence-index-rank33-prefix')
source=source.replace('20260908-5567-rank33-small-extended','20260908-5634-congruence-index-extended')
source=source.replace('tactix-5558-','tactix-5627-').replace('tactix-5567-','tactix-5634-')
source=source.replace('All31494584 decisions through100K agree but full Rank33 has24.46% extra conflicts and22.88% extra propagations. Locate the later first split before selecting a general fidelity repair.',
    'After validated common100K decisions, determine whether visited satisfied-index cleanup closes or moves the previous182093 Rank33 split. Preserve native and shared-prefix identity; no runtime claim.')
exec(compile(source,__file__,'exec'))
command=['sbatch','--parsable',str((c/'array.sbatch').relative_to(root))]
job=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip()
assert job.isdecimal()
(c/'submission.json').write_text(json.dumps(dict(job=job,command=command,manifest_sha256=sha(c/'manifest.json')),indent=2)+'\n')
print('Submitted extended causal replay',job)

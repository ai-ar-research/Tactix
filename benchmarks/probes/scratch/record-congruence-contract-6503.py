#!/usr/bin/env python3
"""Record the actual two-task launch without rewriting a submitted manifest."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6501-rank33-congruence-replay'
target=c/'contract-6503.json'; assert not target.exists()
m=json.loads((c/'manifest.json').read_text()); launch=json.loads((c/'launch.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest,rel
assert launch['manifest_sha256']==sha(c/'manifest.json')
assert m['resource_contract']['array']=='0-2'
assert '#SBATCH --array=0-1\n' in (c/'array.sbatch').read_text()
assert "arm = ('candidate', 'kissat')[int(sys.argv[1])]" in (c/'run.py').read_text()
command=['sacct','--array','-n','-X','-j',launch['job_id'],'-S',launch['accounting_since'],'-P','-o','JobID,State,ExitCode,Submit']
output=subprocess.check_output(command,text=True)
tasks={int(row.split('|')[0].split('_')[1]) for row in output.splitlines() if '_' in row.split('|')[0]}
assert tasks=={0,1},output
target.write_text(json.dumps(dict(
    manifest_sha256=sha(c/'manifest.json'),launch_sha256=sha(c/'launch.json'),
    actual_resource_contract=dict(m['resource_contract'],array='0-1'),
    actual_arms={'0':'candidate','1':'kissat'},accounting=dict(command=command,stdout=output),
    correction='The immutable manifest inherited its parent three-arm array label0-2. The hashed batch and runner, original launch and exact accounting all use only tasks0-1/candidate,kissat. Preserve original files; no task missing, no resubmission or excluded measurement. This sidecar owns the corrected array metadata.'),indent=2)+'\n')
print(target)

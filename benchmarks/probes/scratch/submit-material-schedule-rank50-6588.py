#!/usr/bin/env python3
"""Submit only after the full prerequisite solver gate, not its proof wait."""
import json, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[3]
subprocess.run(['python3',str(Path(__file__).with_name('validate-material-schedule-BC-6586.py'))],check=True)
gate=root/'benchmarks/runs/20260909-6584-material-schedule-BC-gate/summary-6586.json'
s=json.loads(gate.read_text()); assert s['solver_records']==66 and len(s['pairs'])==33
source=Path(__file__).with_name('submit-rank32-extension-6184.py')
code=source.read_text().replace('20260909-6183-rank32-current-prefix','20260909-6587-rank50-material-schedule')
anchor="since = dt.datetime.now().replace(microsecond=0).isoformat()"
assert code.count(anchor)==1
code=code.replace(anchor,"dependency_hashes = {rel: sha(root/rel) for rel in m['dependencies']}\n"+anchor)
anchor="manifest_sha256=sha(c / 'manifest.json')"
assert code.count(anchor)==1
code=code.replace(anchor,anchor+", dependencies=dependency_hashes")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

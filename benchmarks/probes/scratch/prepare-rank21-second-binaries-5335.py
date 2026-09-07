#!/usr/bin/env python3
"""Second Sweep at the same conflict; full checked solves avoid early stop."""
import csv
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5333-rank21-standalone'
new = root / 'benchmarks/runs/20260907-5335-rank21-second-binaries'
assert not (new/'manifest.json').exists(), 'Do not overwrite prepared campaigns'
new.mkdir(exist_ok=True)
for name in ('pins', 'logs', 'traces', 'results', 'provisional', 'claims'):
    (new/name).mkdir(exist_ok=True)
for p in (old/'pins').iterdir():
    if p.is_file() and p.name != 'kissat':
        if (new/'pins'/p.name).exists():
            (new/'pins'/p.name).chmod(0o644)
        shutil.copy2(p, new/'pins'/p.name)
runner = new/'pins/runtime_task.py'
runner.chmod(0o644)
source = runner.read_text()
anchor = '        affinity = sorted(os.sched_getaffinity(0))'
assert source.count(anchor) == 1
source = source.replace(anchor, '''        os.environ.update({
            "SAT_EVENT_TRACE_BINARY_SET_PATH": str(CAMPAIGN / "traces" / (row["arm"] + ".binaries.tsv")),
            "SAT_EVENT_TRACE_BINARY_SET_MATCH": "name=Sweep|stage=end",
            "SAT_EVENT_TRACE_BINARY_SET_CONFLICT": "23889",
            "SAT_EVENT_TRACE_BINARY_SET_ORDINAL": "1",
            "SAT_EVENT_TRACE_BINARY_SET_ONCE": "1"})
''' + anchor)
runner.write_text(source)
with (old/'matrix.tsv').open(newline='') as f:
    reader = csv.DictReader(f, delimiter='\t')
    fields = reader.fieldnames
    rows = [r for r in reader if r['arm'] != 'kissat']
with (new/'matrix.tsv').open('w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=fields, delimiter='\t')
    writer.writeheader()
    writer.writerows(rows)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(new)).replace('--array=0-2', '--array=0-1'))
m = json.loads((old/'manifest.json').read_text())
m.pop('slurm_job_id', None)
m['arms'].pop('kissat')
m.update(hypothesis='Capture the four extra copies at the SECOND Sweep end at conflict 23889, not the first unchanged Sweep captured by 5334.',
         comparison='Two Tactix diagnostic pins; complete proof-on checked solves, standalone boundaries and one complete binary multiset.',
         decision='Require two verified models and cleanup, then exact multiset counts 621085 and 621089. Instrumented times are not timing acceptance.',
         parent_campaign=str(old.relative_to(root)),
         runner_delta='Adds binary export ordinal 1; no SET_STOP. Existing standalone boundary logger, solver invocation and proof/model oracle unchanged.')
m['resource_contract']['array'] = '0-1'
m['pin_sha256'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in [*(new/'pins').iterdir(), new/'matrix.tsv', new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)

#!/usr/bin/env python3
"""Trace the four exact binary producers without opening global EventTrace."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5335-rank21-second-binaries'
new = root/'benchmarks/runs/20260907-5336-rank21-producers'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results', 'provisional', 'claims'):
    (new/name).mkdir()
for p in (old/'pins').iterdir():
    if p.is_file():
        shutil.copy2(p, new/'pins'/p.name)
runner = new/'pins/runtime_task.py'
runner.chmod(0o644)
source = runner.read_text()
begin = source.index('        os.environ.update({\n            "SAT_EVENT_TRACE_BINARY_SET_PATH"')
end = source.index('        affinity = sorted(os.sched_getaffinity(0))', begin)
source = source[:begin]+'''        os.environ.update({
            "TACTIX_BINARY_LIFECYCLE_TRACE": str(CAMPAIGN / "traces" / (row["arm"] + ".lifecycle.tsv")),
            "TACTIX_BINARY_LIFECYCLE_PAIRS": "-364071,212244;-364068,212244;-306948,211419;-306945,211419"})
'''+source[end:]
runner.write_text(source)
shutil.copy2(old/'matrix.tsv', new/'matrix.tsv')
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(new)))
m = json.loads((old/'manifest.json').read_text())
m.pop('slurm_job_id', None)
m.update(hypothesis='Identify the producers of the four extra copies exported at the second Sweep at conflict 23889; distinguish large collapse from binary rewrite or bridge birth.',
         comparison='Core-only versus sequential Tactix, full checked solves and exact-pair standalone lifecycle observer.',
         decision='Require two verified models and cleanup; require standalone boundary traces unchanged from 5333; inspect selected producer/source rows. No timing acceptance.',
         parent_campaign=str(old.relative_to(root)),
         runner_delta='Replaces standalone binary-set exporter with four-pair lifecycle observer. No global EventTrace. All solver/model/proof invocation and cleanup logic unchanged.')
m['pin_sha256'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in [*(new/'pins').iterdir(), new/'matrix.tsv', new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)

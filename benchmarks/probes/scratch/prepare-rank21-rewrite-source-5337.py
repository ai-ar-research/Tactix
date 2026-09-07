#!/usr/bin/env python3
"""Pinned source/target and emitted-pair observer; no solver semantics change."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5336-rank21-producers'
new = root/'benchmarks/runs/20260907-5337-rank21-rewrite-source'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results', 'provisional', 'claims'):
    (new/name).mkdir()
for p in (old/'pins').iterdir():
    if p.is_file():
        shutil.copy2(p, new/'pins'/p.name)
sequential = new/'pins/sequential'
sequential.chmod(0o644)
shutil.copy2(root/'build/pin-5337-rewrite-source/tactix', sequential)
runner = new/'pins/runtime_task.py'
runner.chmod(0o644)
source = runner.read_text()
anchor = '        affinity = sorted(os.sched_getaffinity(0))'
assert source.count(anchor) == 1
source = source.replace(anchor, '''        os.environ["TACTIX_SWEEP_EMISSION_TRACE"] = str(CAMPAIGN / "traces" / (row["arm"] + ".emission.tsv"))
        os.environ["TACTIX_SWEEP_EMISSION_CONFLICT"] = "23889"
''' + anchor)
runner.write_text(source)
shutil.copy2(old/'matrix.tsv', new/'matrix.tsv')
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(new)))
patch = ''
for rel in ('src/rules/substitute/substitute.cpp', 'src/rules/sweep/sweep.cpp'):
    before = root/'.claude/worktrees/runtime-sequential-trace-5325'/rel
    after = root/'.claude/worktrees/runtime-rewrite-source-5337'/rel
    result = subprocess.run(['diff', '-u', '--label', 'a/'+rel, '--label', 'b/'+rel, before, after], text=True, capture_output=True)
    assert result.returncode in (0, 1)
    patch += result.stdout
(new/'pins/source-observer.patch').write_text(patch)
m = json.loads((old/'manifest.json').read_text())
m.pop('slurm_job_id', None)
m['arms']['sequential']['sha256'] = hashlib.sha256(sequential.read_bytes()).hexdigest()
m['arms']['sequential']['source_provenance'] = '5336 sequential source plus pins/source-observer.patch; fresh Release EVENT_TRACE build'
m.update(hypothesis='Identify exact source pairs and emitted equivalence order for the four reconstructed duplicates.',
         comparison='Unchanged core diagnostic control versus fresh sequential observer; full proof-on checked solves.',
         decision='Require two verified models, cleanup and full boundary traces byte-identical to 5333. Target payload records source and target external literals plus publication flags; independent pair-order log at conflict 23889. No runtime acceptance.',
         parent_campaign=str(old.relative_to(root)),
         runner_delta='Adds emission observer path/conflict; all solver/oracle/cleanup invocation unchanged.')
m['pin_sha256'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in [*(new/'pins').iterdir(), new/'matrix.tsv', new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m, indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)

#!/usr/bin/env python3
"""Checked solves with the existing standalone boundary logger, no global trace."""
import csv
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5330-ordering-ablation'
trace = root / 'benchmarks/runs/20260907-5332-rank21-sweep-boundary'
new = root / 'benchmarks/runs/20260907-5333-rank21-standalone'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results','provisional','claims'):
    (new / name).mkdir()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for arm in ('core','sequential','kissat'):
    shutil.copy2(trace / 'pins' / arm, new / 'pins' / arm)
for path in trace.glob('*.patch'):
    shutil.copy2(path, new / 'pins' / path.name)
for name in ('checkmodel','drat-trim','modelcheck.py'):
    shutil.copy2(old / 'pins' / name, new / 'pins' / name)
runner = (old/'pins/runtime_task.py').read_text()
anchor = '        affinity = sorted(os.sched_getaffinity(0))'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''        # Diagnostic-only: leave the global EventTrace unopened.
        prefix = "KISSAT" if row["arm"] == "kissat" else "TACTIX"
        os.environ[prefix + "_INPROCESS_BOUNDARY_TRACE"] = str(
            CAMPAIGN / "traces" / (row["arm"] + ".tsv"))
''' + anchor)
(new/'pins/runtime_task.py').write_text(runner)
with (old/'matrix.tsv').open(newline='') as source:
    reader = csv.DictReader(source,delimiter='\t'); fields=reader.fieldnames; row=next(reader)
with (new/'matrix.tsv').open('w',newline='') as target:
    writer=csv.DictWriter(target,fieldnames=fields,delimiter='\t'); writer.writeheader()
    for i,arm in enumerate(('core','sequential','kissat')):
        writer.writerow(dict(row,task=i,arm=arm))
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)).replace('--array=0-5','--array=0-2'))
manifest=json.loads((old/'manifest.json').read_text()); manifest.pop('slurm_job_id',None)
manifest.update(
    arms={arm:dict(path='pins/'+arm,sha256=sha(new/'pins'/arm),arguments=[],
                   source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',
                   source_provenance=str(trace.relative_to(root))+'/manifest.json')
          for arm in ('core','sequential','kissat')},
    hypothesis='Find the first changed inprocessing boundary without paying global EventTrace hot-path and restart-snapshot cost.',
    comparison='Core-only, sequential rewrite, pristine Kissat with diagnostic overlay; unchanged Rank-21 input. Full proof-on solves, independently checked models.',
    decision='Require all three SAT models verified and scratch clean; compare ordered boundary events and counters. Instrumented runtimes are not performance evidence.',
    collect_runtime_context=False,
    parent_campaign=str(trace.relative_to(root)),
    runner_delta='Only inject the solver-specific standalone boundary output path after environment sanitation. Solver invocation, model/proof oracle and cleanup logic unchanged.')
manifest['resource_contract']['array']='0-2'
manifest['pin_sha256']={str(p.relative_to(new)):sha(p) for p in (new/'pins').iterdir()}
for name in ('matrix.tsv','array.sbatch'):
    manifest['pin_sha256'][name]=sha(new/name)
(new/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)

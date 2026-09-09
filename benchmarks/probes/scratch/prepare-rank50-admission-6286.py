#!/usr/bin/env python3
"""Pin a three-arm observer-neutrality/admission replay, never a timing run."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6211-rank50-extended-prefix'
build = root / 'benchmarks/runs/20260909-6283-admission-observer-build'
b = json.loads((build / 'manifest.json').read_text())
assert b['release_exact_parent']
for source, files in b['files'].items():
    for rel, digest in files.items(): assert sha(root / source / rel) == digest, (source, rel)
for rel, digest in b['binaries'].items(): assert sha(root / rel) == digest, rel
c = root / 'benchmarks/runs/20260909-6286-rank50-admission'
assert not c.exists()
for name in ('pins', 'traces', 'results', 'logs'): (c / name).mkdir(parents=True)
sources = dict(candidate='build/pin-6283-admission-trace/tactix',
               candidate_control='build/pin-6269-vivify-binary-retry-trace/tactix',
               kissat='tools/solvers/variants/kissat-admission-observer-6283/build/kissat')
assert sha(root / sources['candidate_control']) == 'ae974849b0c49db7e4fe54ef2a1cedc00c0ca399cfbc13c39519100be4a1b5ea'
for arm, source in sources.items(): shutil.copy2(root / source, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('tactix-6211-', 'tactix-6286-')
assert runner.count("('candidate', 'kissat')") == 1
runner = runner.replace("('candidate', 'kissat')", "('candidate', 'candidate_control', 'kissat')")
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "if arm != 'candidate_control':\n    env[prefix + '_ELIMINATE_ADMISSION_TRACE'] = str(c / 'traces' / (arm + '.admission.tsv'))\n    env['SAT_ELIMINATE_ADMISSION_CONFLICT'] = str(m['admission_conflict'])\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6211-rank50-prefix', '6286-rank50-admission').replace('--array=0-1', '--array=0-2')
assert '--array=0-2' in batch and '--cpus-per-task=12' in batch and '--mem=48000M' in batch
(c / 'array.sbatch').write_text(batch)
m = json.loads((old / 'manifest.json').read_text())
m.update(admission_conflict=823562, observer_build=dict(path=str((build / 'manifest.json').relative_to(root)), sha256=sha(build / 'manifest.json')),
    sources=sources, purpose='Record actual Rank50 Eliminate admission operands at first known split using current6269 plus same-source control and native6090 successor.',
    hypothesis='The missed823562 Eliminate boundary is an actual admission/deadline/watermark difference; distinguish these before changing a policy.',
    validation='Require complete candidate/control decision,boundary,macro,prefix and partial-proof identity; require complete native replay of6211. Verify inherited139633-row hash236dcfa1a362af4d and parse all guard operands before inference. No timing claim or final-proof acceptance.',
    test_consideration='New trace-only Tactix fixture and actual native guard fixture pass,105 Release/104 trace tests; release binary exactly6269. Three independent full-width tasks with immutable pins, same12CPU/48000MiB and1200solver-only seconds. Partial certificates hash/delete.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c/'run.py', c/'array.sbatch', *sorted((c/'pins').iterdir())]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

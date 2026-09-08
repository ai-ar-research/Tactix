#!/usr/bin/env python3
"""Freeze the tested candidate-level observer in a separate campaign."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-6008-clause-pose-series'
c = root / 'benchmarks/runs/20260908-6020-vivify-pose'
controls = root / 'benchmarks/runs/20260908-6016-vivify-pose-controls'
tactix = root / '.claude/worktrees/runtime-vivify-pose-6016'
native = root / 'tools/solvers/variants/kissat-vivify-pose-6016'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
assert not c.exists()
assert '100% tests passed out of 99' in (controls / 'logs/tactix-ctest.log').read_text()
control = json.loads((controls / 'summary.json').read_text())
for rel, digest in control['hashes'].items(): assert sha(root / rel) == digest, rel
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest, rel
assert json.loads((parent / 'summary.json').read_text())['full_replay']
c.mkdir()
for name in ('pins', 'logs', 'traces', 'results', 'source'): (c / name).mkdir()
shutil.copy2(root / 'build/pin-6016-vivify-pose-trace/tactix', c / 'pins/candidate')
shutil.copy2(native / 'build/kissat', c / 'pins/kissat')
runner = (parent / 'run.py').read_text().replace('tactix-6008-', 'tactix-6020-')
anchor = "assert not any(key.endswith('_EVENT_TRACE')"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env['SAT_VIVIFY_POSE_TRACE'] = str(c / 'traces' / (arm + '.vivify-pose.tsv'))
env['SAT_VIVIFY_POSE_LITS'] = '-246596,-5409,-47,49,5415,148944,238853'
env['SAT_VIVIFY_POSE_CONFLICT'] = '270978'
""" + anchor)
compile(runner, str(c / 'run.py'), 'exec'); (c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('6008-clause-pose-series', '6020-vivify-pose'))
sources = []
for arm, old, new, names in [
    ('tactix', root / m['candidate_source'], tactix, ['src/diagnostics/event_trace.cpp', 'tests/CMakeLists.txt']),
    ('kissat', root / m['native_source'], native, ['src/eventtrace.c', 'src/eventtrace.h', 'src/vivify.c'])]:
    patch = ''
    for name in names:
        r = subprocess.run(['diff', '-U0', '--label', 'a/' + name, '--label', 'b/' + name, old / name, new / name], capture_output=True, text=True)
        assert r.returncode == 1; patch += r.stdout
        shutil.copy2(new / name, c / 'source' / (arm + '-' + Path(name).name))
        if arm == 'tactix': sources.append(new / name)
    (c / (arm + '-observer.patch')).write_text(patch)
for p in [tactix / 'tests/test_vivify_pose.cpp', tactix / 'src/diagnostics/vivify-pose-6016.h']:
    shutil.copy2(p, c / 'source' / p.name); sources.append(p)
assert sha(tactix / 'src/diagnostics/vivify-pose-6016.h') == sha(native / 'src/vivify-pose-6016.h')
m.update(parent_campaign=str(parent.relative_to(root)), candidate_source=str(tactix.relative_to(root)), native_source=str(native.relative_to(root)),
    candidate_parent=m['candidate_source'], native_parent=m['native_source'],
    hypothesis='The selected clause first differs at second Vivify270978 exit. Observe candidate scope/ordinal, selected pose, candidate identity and ticks around each candidate to identify the actual first differing operation.',
    validation='Full ordinary300K traces, partial certificate, BVE journals and selected phase history must replay6008 exactly before new candidate-level evidence is interpreted.',
    observer_controls=dict(path=str((controls / 'summary.json').relative_to(root)), sha256=sha(controls / 'summary.json')),
    test_consideration=control['test_consideration'],
    prior_entry_evidence=dict(path=str((parent / 'summary.json').relative_to(root)), sha256=sha(parent / 'summary.json')),
    native_source_files={str(p.relative_to(native)): sha(p) for p in (native / 'src').iterdir() if p.is_file()},
    diagnostic_source_files={str(p.relative_to(tactix)): sha(p) for p in sources},
    created_at=__import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat())
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir(), *(c / 'source').iterdir(), *c.glob('*.patch')]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

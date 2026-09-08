#!/usr/bin/env python3
"""Freeze the tested selected-clause series without changing any old campaign."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5996-bve-reinsert-order'
c = root / 'benchmarks/runs/20260908-6008-clause-pose-series'
controls = root / 'benchmarks/runs/20260908-6004-clause-pose-series-controls'
tactix = root / '.claude/worktrees/runtime-clause-pose-series-6004'
native = root / 'tools/solvers/variants/kissat-clause-pose-series-6004'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
assert not c.exists()
assert '100% tests passed out of 98' in (controls / 'logs/tactix-ctest.log').read_text()
control = json.loads((controls / 'summary.json').read_text())
for rel, digest in control['hashes'].items(): assert sha(root / rel) == digest, rel
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest, rel
c.mkdir()
for name in ('pins', 'logs', 'traces', 'results', 'source'): (c / name).mkdir()
shutil.copy2(root / 'build/pin-6004-clause-pose-series-trace/tactix', c / 'pins/candidate')
shutil.copy2(native / 'build/kissat', c / 'pins/kissat')
runner = (parent / 'run.py').read_text().replace('tactix-5996-', 'tactix-6008-')
begin = runner.index("env[prefix + '_BVE_HEAP_TRACE']")
end = runner.index("assert not any(key.endswith('_EVENT_TRACE')", begin)
runner = runner[:begin] + """env['SAT_CLAUSE_POSE_SERIES_PATH'] = str(c / 'traces' / (arm + '.clause-pose-series.tsv'))
env['SAT_CLAUSE_POSE_SERIES_LITS'] = '-246596,-5409,-47,49,5415,148944,238853'
env['SAT_CLAUSE_POSE_SERIES_FROM'] = '0'
env['SAT_CLAUSE_POSE_SERIES_TO'] = '271079'
env['SAT_CLAUSE_POSE_SERIES_NAMES'] = 'congruence,substitute,backbone,vivify,sweep,factor,strengthen,bve,sparsecollect'
""" + runner[end:]
compile(runner, str(c / 'run.py'), 'exec'); (c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('5996-bve-reinsert-order', '6008-clause-pose-series'))
for arm, old, new, names in [
    ('tactix', root / '.claude/worktrees/runtime-bve-repeat-heap-5985', tactix, ['src/diagnostics/event_trace.cpp', 'tests/CMakeLists.txt']),
    ('kissat', root / 'tools/solvers/variants/kissat-bve-repeat-heap-5985', native, ['src/eventtrace.c'])]:
    patch = ''
    for name in names:
        r = subprocess.run(['diff', '-U3', '--label', 'a/' + name, '--label', 'b/' + name, old / name, new / name], capture_output=True, text=True)
        assert r.returncode == 1; patch += r.stdout
        shutil.copy2(new / name, c / 'source' / (arm + '-' + Path(name).name))
    (c / (arm + '-observer.patch')).write_text(patch)
for p in [tactix / 'tests/test_clause_pose_series.cpp', tactix / 'src/diagnostics/clause-pose-series-6004.h']:
    shutil.copy2(p, c / 'source' / p.name)
assert sha(tactix / 'src/diagnostics/clause-pose-series-6004.h') == sha(native / 'src/clause-pose-series-6004.h')
entry = root / 'benchmarks/runs/20260908-6000-bve-clause-entry/summary.json'
assert json.loads(entry.read_text())['observer_parent_trace_and_proof_equal']
m.update(parent_campaign=str(parent.relative_to(root)), candidate_source=str(tactix.relative_to(root)), native_source=str(native.relative_to(root)),
    candidate_parent='.claude/worktrees/runtime-bve-repeat-heap-5985', native_parent='tools/solvers/variants/kissat-bve-repeat-heap-5985',
    hypothesis='Validated6000 already has differing selected seven-literal pose before first forward, and5996 proves opposite5409/5415 reinsertions. Record exact selected clause at all common macro boundaries to locate its first different producer interval.',
    validation='Full ordinary300K traces,partial certificate andcomplete BVE journals replay5996; every selected phase boundary must have a framed snapshot including explicit zero copies. Preserve repeated boundary occurrence ordinals.',
    native_rerun_reason='Previously missing selected-clause history only; fixed6810.066s timing denominator unchanged.',
    observer_controls=dict(path=str((controls / 'summary.json').relative_to(root)), sha256=sha(controls / 'summary.json')),
    test_consideration=control['test_consideration'],
    prior_entry_evidence=dict(path=str(entry.relative_to(root)), sha256=sha(entry)),
    limitation='Diagnostic sources only,descend from5900. Implement on5962 after identifying general cause. No proof acceptance,runtime gain,promotion or fuzz claim.',
    native_source_files={str(p.relative_to(native)): sha(p) for p in (native / 'src').iterdir() if p.is_file()},
    diagnostic_source_files={str(p.relative_to(tactix)): sha(p) for p in [tactix / 'src/diagnostics/event_trace.cpp', tactix / 'src/diagnostics/clause-pose-series-6004.h', tactix / 'tests/test_clause_pose_series.cpp', tactix / 'tests/CMakeLists.txt']},
    created_at=__import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat())
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir(), *(c / 'source').iterdir(), *c.glob('*.patch')]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

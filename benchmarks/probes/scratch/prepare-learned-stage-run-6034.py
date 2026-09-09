#!/usr/bin/env python3
"""Freeze tested learning observers without touching existing campaigns."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-6024-vivify-lemma-history'
c = root / 'benchmarks/runs/20260908-6035-learned-stage'
controls = root / 'benchmarks/runs/20260908-6030-learned-stage-controls'
tactix = root / '.claude/worktrees/runtime-learned-stage-6030'
native = root / 'tools/solvers/variants/kissat-learned-stage-6030'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert not c.exists()
assert '100% tests passed out of 100' in (controls / 'logs/tactix-ctest.log').read_text()
control = json.loads((controls / 'summary.json').read_text())
for rel, digest in control['hashes'].items(): assert sha(root / rel) == digest, rel
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest, rel
assert json.loads((parent / 'summary.json').read_text())['full_replay']
c.mkdir()
for name in ('pins', 'logs', 'traces', 'results', 'source'): (c / name).mkdir()
shutil.copy2(root / 'build/pin-6030-learned-stage-trace/tactix', c / 'pins/candidate')
shutil.copy2(native / 'build/kissat', c / 'pins/kissat')
runner = (parent / 'run.py').read_text().replace('tactix-6024-', 'tactix-6035-')
anchor = "assert not any(key.endswith('_EVENT_TRACE')"; assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env['SAT_LEARNED_STAGE_TRACE'] = str(c / 'traces' / (arm + '.learned-stage.tsv'))
env['SAT_LEARNED_STAGE_FROM'] = '240062'
env['SAT_LEARNED_STAGE_TO'] = '247206'
""" + anchor)
compile(runner, str(c / 'run.py'), 'exec'); (c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('6024-vivify-lemma-history', '6035-learned-stage'))
for arm, old, new, names in [
    ('tactix', root / m['candidate_source'], tactix, ['src/diagnostics/event_trace.cpp', 'tests/CMakeLists.txt']),
    ('kissat', root / m['native_source'], native, ['src/eventtrace.c'])]:
    patch = ''
    for name in names:
        r = subprocess.run(['diff', '-U0', '--label', 'a/' + name, '--label', 'b/' + name, old / name, new / name], capture_output=True, text=True)
        assert r.returncode == 1; patch += r.stdout
        shutil.copy2(new / name, c / 'source' / (arm + '-' + Path(name).name))
    (c / (arm + '-observer.patch')).write_text(patch)
for name in ('tests/test_learned_stage.cpp', 'src/diagnostics/learned-stage-6030.h'):
    shutil.copy2(tactix / name, c / 'source' / Path(name).name)
assert sha(tactix / 'src/diagnostics/learned-stage-6030.h') == sha(native / 'src/learned-stage-6030.h')
diagnostic = set(m['diagnostic_source_files']) | {'tests/test_learned_stage.cpp', 'src/diagnostics/learned-stage-6030.h'}
m.update(parent_campaign=str(parent.relative_to(root)), candidate_source=str(tactix.relative_to(root)), native_source=str(native.relative_to(root)),
    candidate_parent=m['candidate_source'], native_parent=m['native_source'],
    hypothesis='Native-only Vivify candidate first appears between240062 Reduce end and247206 Reduce begin. Observe raw/sorted/shrunk/final learned clauses in that interval to identify its actual learning conflict and earlier stage difference.',
    validation='Every6024 trace and partial certificate must replay exactly before interpreting new standalone learning-stage rows. No global event trace enabled.',
    native_rerun_reason='Missing learning-conflict/stage origin of now-proven causal native-only candidate; fixed timing denominator unchanged.',
    observer_controls=dict(path=str((controls / 'summary.json').relative_to(root)), sha256=sha(controls / 'summary.json')),
    test_consideration=control['test_consideration'],
    prior_entry_evidence=dict(path=str((parent / 'presence-analysis.json').relative_to(root)), sha256=sha(parent / 'presence-analysis.json')),
    native_source_files={str(p.relative_to(native)): sha(p) for p in (native / 'src').iterdir() if p.is_file()},
    diagnostic_source_files={name: sha(tactix / name) for name in sorted(diagnostic)},
    created_at=__import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat())
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir(), *(c / 'source').iterdir(), *c.glob('*.patch')]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

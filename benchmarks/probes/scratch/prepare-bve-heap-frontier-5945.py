#!/usr/bin/env python3
"""Freeze a selected-heap diagnostic; preserve all existing campaign scripts."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
parent = root/'benchmarks/runs/20260908-5937-first-bve-frontier'
c = root/'benchmarks/runs/20260908-5945-bve-heap-frontier'
native = root/'tools/solvers/variants/kissat-bve-heap-observer-5944'
base = root/'tools/solvers/variants/kissat-imply-fast-5414'
controls = root/'benchmarks/runs/20260908-5944-bve-heap-observer-controls/repaired/summary.json'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
tests = json.loads(controls.read_text())
for rel,digest in tests['hashes'].items(): assert sha(root/rel)==digest
m = json.loads((parent/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(parent/rel)==digest
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results','native-source'): (c/name).mkdir()
shutil.copy2(parent/'pins/candidate',c/'pins/candidate')
shutil.copy2(native/'build/kissat',c/'pins/kissat')
patch = ''
for name in ('eventtrace.c','eliminate.c'):
    p = subprocess.run(['diff','-u','--label','a/src/'+name,'--label','b/src/'+name,
                        str(base/'src'/name),str(native/'src'/name)],capture_output=True,text=True)
    assert p.returncode==1
    patch += p.stdout
    shutil.copy2(native/'src'/name,c/'native-source'/name)
(c/'native-observer.patch').write_text(patch)
runner = (parent/'run.py').read_text().replace('tactix-5937-','tactix-5945-')
start = runner.index('env.update(SAT_EVENT_TRACE_FORMULA_SET_PATH=')
end = runner.index("assert not any(key.endswith('_EVENT_TRACE')",start)
runner = runner[:start]+'''env[prefix + '_BVE_CANDIDATE_TRACE'] = str(c / 'traces' / (arm + '.bve-candidates.tsv'))
env['SAT_BVE_CANDIDATE_CONFLICT'] = '271079'
env[prefix + '_BVE_HEAP_TRACE'] = str(c / 'traces' / (arm + '.bve-heap.tsv'))
env['SAT_BVE_HEAP_CONFLICT'] = '271079'
env['SAT_BVE_HEAP_SUBJECTS'] = '225614,225641,225620,89339'
if arm == 'candidate':
    env['TACTIX_EVENT_TRACE_BVE_HEAP'] = env['SAT_BVE_HEAP_SUBJECTS']
'''+runner[end:]
compile(runner,str(c/'run.py'),'exec')
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)).replace('5937-first-bve-frontier','5945-bve-heap-frontier'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),parent_campaign=str(parent.relative_to(root)),
    hypothesis='Before firstBVE the irredundant clause sequence agrees but816 literal poses differ. Complete attempts expose first active order split225614/225641 after120872 common active attempts. Selected heap updates should localize an earlier score/physical-topology discrepancy;inactive89339 reprocessing remains a separate measured discrepancy.',
    validation='All cached ordinary300K trace hashes and partial proof bytes must replay per arm. Candidate binary unchanged; native changes only read-only selected heap journal plus post-push callback. Compare selected state at actual caller boundaries, preserving native update-before-push versus Tactix after-update stage asymmetry.',
    native_rerun_reason='No cached selected heap update journal at271079 exists. Native observer clone of exact5414 lineage adds standalone sink; runtime reference remains fixed.',
    test_consideration='Nine malformed attempt controls PASS. Actual native heap/remapping/conflict-filter controls plus eight bad selector or missing-initial controls PASS after repairing fixture allocation before heap import. No production solver/unit change.',
    observer_controls=dict(path=str(controls.relative_to(root)),sha256=sha(controls)),
    native_source=str(native.relative_to(root)),native_parent=str(base.relative_to(root)),
    native_source_files={str(p.relative_to(native)):sha(p) for p in (native/'src').iterdir() if p.is_file()},
    native_parent_source_files={str(p.relative_to(base)):sha(p) for p in (base/'src').iterdir() if p.is_file()},
    formula_stage=None,formula_ordinal=None,
    limitation='Diagnostic only, no timing or proof acceptance. Tactix5900 unchanged;5920 remains rejected. Fuzz denied/pending. All existing jobs and pins immutable.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c/'run.py',c/'array.sbatch',c/'pins/candidate',c/'pins/kissat',c/'native-observer.patch',*(c/'native-source').iterdir()]}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

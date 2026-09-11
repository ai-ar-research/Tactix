#!/usr/bin/env python3
"""Observe the selected BVE candidate without new solver instrumentation."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
old=root/'benchmarks/runs/20260911-7353-root-phase-trace'
c=root/'benchmarks/runs/20260911-7356-bve-heap'
assert not c.exists()
m=json.loads((old/'manifest.json').read_text());deps=dict(m['dependencies'])
def bind(path,expected=None):
    digest=sha(path);assert expected is None or digest==expected,path
    deps[str(path.relative_to(root))]=digest
for rel,digest in list(deps.items()):bind(root/rel,digest)
for rel,digest in m['pin_sha256'].items():bind(old/rel,digest)
bind(old/'manifest.json');bind(old/'launch.json')
for task in range(2):
    path=old/f'results/{task:03d}.json';r=json.loads(path.read_text())
    assert classify(r,True)['checked'];bind(path)
    for artifact in r['trace_artifacts'].values():bind(old/artifact['path'],artifact['sha256'])
for folder in ('pins','logs','claims','provisional','results','traces'):(c/folder).mkdir(parents=True)
for path in (old/'pins').iterdir():
    if path.name!='runtime_task.py':shutil.copy2(path,c/'pins'/path.name)
shutil.copy2(old/'matrix.tsv',c/'matrix.tsv')
runner=(old/'pins/runtime_task.py').read_text()
replacements={
    "('decisions', 'macro', 'active', 'inprocess')":"('decisions', 'macro', 'active', 'inprocess', 'heap')",
    "        solve = measured(command, stdout, stderr,":
        "        os.environ['TACTIX_EVENT_TRACE_BVE_HEAP'] = '22861'\n        os.environ['TACTIX_BVE_HEAP_TRACE'] = str(trace_files['heap'])\n        os.environ['SAT_BVE_HEAP_CONFLICT'] = '26295'\n        solve = measured(command, stdout, stderr,",
    "        verdict, statistics = solver_summary(stdout)":
        "        for name in ('TACTIX_EVENT_TRACE_BVE_HEAP','TACTIX_BVE_HEAP_TRACE','SAT_BVE_HEAP_CONFLICT'): del os.environ[name]\n        verdict, statistics = solver_summary(stdout)",
}
for before,after in replacements.items():assert runner.count(before)==1,before;runner=runner.replace(before,after)
compile(runner,str(c/'pins/runtime_task.py'),'exec')
with (c/'pins/runtime_task.py').open('x') as out:out.write(runner)
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('7353-root-phase-trace','7356-bve-heap')
with (c/'array.sbatch').open('x') as out:out.write(batch)
for path in [Path(__file__),Path(__file__).with_name('check-submit-bve-heap-7356.py'),
             root/'benchmarks/reports/2026-09-11-root-phase-analysis-7354.json']:bind(path)
m.update(schema='tactix-bve-heap-7356-v1',dependencies=deps,
    hypothesis='Determine whether external22861 has different incoming score/membership, subsequent reinsertion or heap position at BVE C26295. Its candidate-only retirement is the first exposed difference; later restart split is downstream. Existing selected-subject sink only, no new solver code or global event trace.',
    acceptance='Both expected original proofs checked/clean; exact full own Release work/certificate AND entire7353 decisions/macro/active/inprocess artifacts. Only selected heap output may be added. No runtime claim, source promotion or arbitrary field exclusions.',
    test_consideration='Configuration-only extension of immutable7353 runner. Selected heap rows must have exact schema and subject22861; compare all rows, preserve absent-member state, trigger identities and hexadecimal scores. Existing identity/proof gates unchanged.')
m['pin_sha256']={str(path.relative_to(c)):sha(path) for path in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out:json.dump(m,out,indent=2)
for path in (c/'pins').iterdir():path.chmod(path.stat().st_mode & ~0o222)
print(c)

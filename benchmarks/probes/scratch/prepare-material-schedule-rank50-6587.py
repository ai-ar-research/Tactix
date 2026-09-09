#!/usr/bin/env python3
"""Approved missing-invitation test, reusing the closed native semantic trace."""
import json, shutil, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6562-rank50-current-material-pair'
c=root/'benchmarks/runs/20260909-6587-rank50-material-schedule'
assert not c.exists()
build=root/'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json'
b=json.loads(build.read_text())
for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest,rel
binary=root/'build/pin-6579-material-schedule-trace/tactix'
assert sha(binary)==b['binaries'][str(binary.relative_to(root))]
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest,rel
for d in ('pins','traces','results','logs'): (c/d).mkdir(parents=True)
for arm in ('candidate','candidate_control'): shutil.copy2(binary,c/'pins'/arm)
(c/'run.py').write_text((old/'run.py').read_text().replace('tactix-6562-','tactix-6587-'))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6562-current-material-pair','6587-material-schedule'))
del m['preparation_failure']
m.update(sources={arm:str(binary.relative_to(root)) for arm in ('candidate','candidate_control')},
    source_manifest=str(build.relative_to(root)),
    dependencies=['benchmarks/runs/20260909-6584-material-schedule-BC-gate/summary-6586.json'],
    purpose='Test the approved canonical material scheduling repair at the closed Rank50 missing invitation, with independent same-binary lifecycle-observer control.',
    hypothesis='Recover native Eliminate/Strengthen at823562 after the material-producing binary at780400, without instance-specific policy. Explain any earlier decision split before attributing gain.',
    validation='Both candidate arms must match all ordinary traces and partial certificate bytes/hashes. Compare the complete candidate/native decision streams and pair lifecycle against cached6556 and parent6562. Changed common prefix is evidence requiring explanation, not something to discard. No native timing refreshed.',
    test_consideration=b['test_consideration'],
    limitation='Bounded diagnostic, not independent final-proof acceptance, runtime gain or promotion. Checkers for prerequisite expected solves continue asynchronously. Any invalidity pauses dependent work.')
m['earlier_closure'].update({str(p.relative_to(root)):sha(p) for p in (build,old/'summary-6564.json',root/'benchmarks/runs/20260909-6582-material-proof-control/summary.json')})
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c/'run.py',c/'array.sbatch',*sorted((c/'pins').iterdir())]}
with (c/'manifest.json').open('x') as out: json.dump(m,out,indent=2)
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Bounded three-arm localization of the largest new full50 regression."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6402-rank45-stamp-prefix'
c=root/'benchmarks/runs/20260909-6470-rank33-activation-prefix'; assert not c.exists()
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest
selection=root/'benchmarks/reports/2026-09-09-activation-full50-6441/summary.json'
full=json.loads(selection.read_text()); assert len(full['rows'])==50
row=max(full['rows'],key=lambda r:r['delta_parent_seconds'])
assert row['rank']==33 and row['delta_parent_seconds']>56 and row['expected']=='UNSAT'
gate=root/'benchmarks/runs/20260909-6427-activation-stamp-BC-gate/solver-checkpoint-6429.json'
assert json.loads(gate.read_text())['solver_records']==66
pins={}
for arm,stem in (('candidate','6424-activation-stamp'),('previous','6383-vivify-count-cache')):
    p=root/f'benchmarks/runs/20260909-{stem}-build/manifest.json'; b=json.loads(p.read_text())
    for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest
    pin=root/f'build/pin-{stem}-trace/tactix'; assert sha(pin)==b['binaries'][str(pin.relative_to(root))]
    pins[arm]=pin; m['authorities'][arm]={'path':str(p.relative_to(root)),'sha256':sha(p)}
pins['kissat']=old/'pins/kissat'
for folder in ('pins','logs','traces','results'): (c/folder).mkdir(parents=True)
for arm,p in pins.items(): shutil.copy2(p,c/'pins'/arm)
(c/'run.py').write_text((old/'run.py').read_text().replace('tactix-6402-','tactix-6470-'))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6402-rank45-stamp-prefix','6470-rank33-activation-prefix'))
fm=json.loads((root/'benchmarks/runs/20260909-6439-activation-ranks1-50/manifest.json').read_text())
m.update(rank=33,input='benchmarks/cache/'+row['instance'],input_sha256_compressed=fm['input_sha256'][row['instance']],
    selection={'path':str(selection.relative_to(root)),'sha256':sha(selection)},
    provisional_gate={'path':str(gate.relative_to(root)),'sha256':sha(gate)},
    purpose='Rank33 is the largest new activation parent runtime loss in complete6441. Compare current6424, parent6383 and cached clean native diagnostic pin through100K; do not assume which trajectory agrees with native.',
    validation='Exact pins/input, complete full-file trace hashes, prefix framing/row accounting, then all three complete decision-row comparisons. Diagnostic native run does not refresh frozen timings. Own partial-proof deletion is not final acceptance.',
    test_consideration='Unchanged three-arm runner and observer. Reuse65benchmark13watcher tests plus maintained derived-counter and complete-row controls. Selection asserts the actual maximum full-table parent loss. No parser/source/checker edit.',
    limitation='Bounded first-choice localization, not formula identity, a cause, full-solve proof or timing acceptance; native reference vector unchanged.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

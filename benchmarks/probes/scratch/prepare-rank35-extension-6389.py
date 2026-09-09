#!/usr/bin/env python3
"""Extend the unchanged6357/6305 adverse control after its proved100K prefix."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6376-rank35-stamp-prefix'
c=root/'benchmarks/runs/20260909-6389-rank35-stamp-extension'
assert not c.exists()
m=json.loads((old/'manifest.json').read_text())
s=json.loads((old/'summary.json').read_text())
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest
assert s['current_previous_decisions']['complete_equal']
assert s['current_previous_decisions']['matching_rows']==40636523
for arm in ('candidate','previous'):
    row=list(map(int,subprocess.check_output(['tail','-n','1',str(old/'traces'/f'{arm}.decisions.tsv')],text=True).split()))
    assert row[0]==99999 and row[1]==s['prefixes'][arm]['end_decisions']
for folder in ('pins','logs','traces','results'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir(): shutil.copy2(p,c/'pins'/p.name)
(c/'run.py').write_text((old/'run.py').read_text().replace('tactix-6376-','tactix-6389-'))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6376-rank35-stamp-prefix','6389-rank35-extension'))
m.update(stop_conflict=1000000,stream_from=100000,common_decisions=40636523,prior_common_hash='6a3ebeb3a46d7ad5',
    prior_closure=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')),
    purpose='Find earliest Rank35 changed choice after the validated100K prefix of6357 versus6305. Repeat timing confirms~1.519x adverse runtime.',
    validation='Both inherited40636523-decision hashes must match. Cached streams end at99999, so suffix has zero overlap; first decision counter must immediately follow cached end. Bind every full stream and compare complete suffix. No new native run.',
    test_consideration='Same runner and existing complete-prefix/stream tests; additional artifact assertions check zero-overlap and exact counter adjacency. No solver or trace instrumentation edit.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(c)

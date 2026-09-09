#!/usr/bin/env python3
"""Identify the four actual binary clauses behind the first Probe population gap."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6643-rank49-probe-input'
c=root/'benchmarks/runs/20260909-6648-rank49-binary-life';assert not c.exists()
s=json.loads((old/'summary-6645.json').read_text());assert s['full_own_replay'] and s['full_large_rows_equal']
pairs=[d['row'] for d in s['candidate_only_binary']]
assert pairs==[[-1090,2201],[-546,2202],[546,2202],[1090,2201]] and not s['kissat_only_binary']
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
m=json.loads((old/'manifest.json').read_text())
runner=(old/'run.py').read_text().replace('tactix-6643-','tactix-6648-')
anchor='\ndef interrupted(signum, frame):';assert runner.count(anchor)==1
spec=';'.join(','.join(map(str,p)) for p in pairs)
runner=runner.replace(anchor,"\nenv[prefix + '_BINARY_LIFECYCLE_TRACE'] = str(c/'traces'/(arm+'.lifecycle.tsv'))\nenv[prefix + '_BINARY_LIFECYCLE_PAIRS'] = " + repr(spec) + '\n' + anchor)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'):(c/folder).mkdir(parents=True)
for arm in ('candidate','kissat'):
    shutil.copy2(old/'pins'/arm,c/'pins'/arm)
    p=old/f'results/{arm}.json';m['dependencies'][str(p.relative_to(root))]=sha(p)
    r=json.loads(p.read_text())
    for name,digest in r['trace_sha256'].items():m['dependencies'][str((old/'traces'/name).relative_to(root))]=digest
p=old/'summary-6645.json';m['dependencies'][str(p.relative_to(root))]=sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6643-probe-input','6648-binary-life'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),cached_controls=str(old.relative_to(root)),selected_pairs=pairs,
    purpose='FirstProbe exit large rows are fully identical, but current has four unique extra binaries involving2201/2202, not duplicates. Physical populations8373/8369 predict exact next deadlines9272/9273. Trace producers/deletions and values for allfour before2988; do not change counting to ignore actual live clauses.',
    validation='Require all6643 outputs and partial proof byte/hash exactly. Parse both existing v3 lifecycle schemas, permitting no records in an arm only when the full frame is valid; absent pair provenance is evidence, not invented deletion. Compare portable events/values/copies and rule metadata when available.',
    test_consideration='Configuration only; existing5 binary_lifecycle and8 binary_snapshot tests plus190 benchmark tests apply. Any causal payload interpretation or solver fix needs focused fixtures.',
    limitation='Diagnostic provenance; no rank-specific policy, arithmetic compensation, source promotion or native timing refresh. Preserve all prior closures.')
m['hashes']={name:sha(c/name) for name in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as stream:json.dump(m,stream,indent=2);stream.write('\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Extend the Rank46 bound with exact100K decision/proof-prefix controls."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old = root/'benchmarks/runs/20260909-6669-rank46-current-prefix'
c = root/'benchmarks/runs/20260909-6673-rank46-long-prefix'; assert not c.exists()
s = json.loads((old/'summary-6671.json').read_text()); assert s['bounded_stops_clean']
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
m = json.loads((old/'manifest.json').read_text()); controls = {}
for arm in ('candidate','kissat'):
    r = json.loads((old/f'results/{arm}.json').read_text())
    controls[arm] = dict(prefix=s['prefixes'][arm],certificate=r['partial_certificate'])
    for p in (old/f'results/{arm}.json',old/'summary-6671.json'):
        m['dependencies'][str(p.relative_to(root))] = sha(p)
    for name,digest in r['trace_sha256'].items(): m['dependencies'][str((old/'traces'/name).relative_to(root))] = digest
runner = (old/'run.py').read_text().replace('tactix-6669-','tactix-6673-')
anchor = "str(m['common_decisions'] if arm == 'candidate' else 1)"; assert runner.count(anchor) == 1
runner = runner.replace(anchor,"str(m['controls'][arm]['prefix']['count'])")
anchor = "    r['trace_sha256'] ="; assert runner.count(anchor) == 1
runner = runner.replace(anchor,"    sys.path.insert(0, str(c/'pins'))\n    from certificate_prefix import hash_prefix\n    r['prior_certificate_prefix'] = hash_prefix(proof, m['controls'][arm]['certificate']['bytes'])\n"+anchor)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for arm in ('candidate','kissat'): shutil.copy2(old/'pins'/arm,c/'pins'/arm)
shutil.copy2(root/'tools/bench/certificate_prefix.py',c/'pins/certificate_prefix.py')
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6669','6673'))
m.update(stop_conflict=1000000,controls=controls,cached_controls=str(old.relative_to(root)),
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Only two adjacent decisions swap through100K; bound1M to locate sustained divergence behind eventual1.823x conflicts. No early-swap causality assumed.',
    validation='Require every100K decision row as exact own prefix, matching rolling common digest and exact partial-certificate byte prefix. Full stream0 retained. Prefix hash reader rejects short proof, invalid counts and missing files; six fixed unit tests. Compare later decisions without erasing the known two-row swap.',
    test_consideration='No solver/observer implementation change. Six new strict certificate-prefix tests plus existing190 tools tests; pin helper immutably with runner. New source repair still requires actual regression/proof/model gates.')
m['hashes'] = {rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as out: json.dump(m,out,indent=2); out.write('\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

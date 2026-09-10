#!/usr/bin/env python3
"""New candidate-only causal replay; use frozen native controls, not new timings."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6721-rank46-earlier-allscan'
c=root/'benchmarks/runs/20260909-6735-reduce-watermark-prefix'; assert not c.exists()
authority=root/'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json'
b=json.loads(authority.read_text())
for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest,rel
binary=root/'build/pin-6730-reduce-watermark-trace/tactix'
assert sha(binary)==b['binaries'][str(binary.relative_to(root))]
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest,rel
deps={str(p.relative_to(root)):sha(p) for p in [authority,old/'manifest.json',old/'summary-6723.json',
    root/'benchmarks/runs/20260909-6734-reduce-watermark-proof-control/summary.json',
    root/'benchmarks/runs/20260909-6726-rank46-collect-boundary/summary-6729.json',
    root/'benchmarks/runs/20260909-6673-rank46-long-prefix/summary-6675.json']}
for a in ('candidate','kissat'):
    p=old/f'results/{a}.json'; deps[str(p.relative_to(root))]=sha(p)
    for name,digest in json.loads(p.read_text())['trace_sha256'].items():
        p=old/'traces'/name; assert sha(p)==digest; deps[str(p.relative_to(root))]=digest
runner=(old/'run.py').read_text().replace('tactix-6721-','tactix-6735-')
changes={"arm = ('candidate', 'kissat')[int(sys.argv[1])]":"assert int(sys.argv[1]) == 0\narm = 'candidate'",
    "str(m['controls'][arm]['prefix']['count'])":"'1'",
    "'61030'":"'62676'", "'73636'":"'62682'",
    "    sys.path.insert(0, str(c/'pins'))\n    from certificate_prefix import hash_prefix\n    r['prior_certificate_prefix'] = hash_prefix(proof, m['controls'][arm]['certificate']['bytes'])\n":""}
for before,after in changes.items(): assert before in runner,before; runner=runner.replace(before,after)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
shutil.copy2(binary,c/'pins/candidate')
(c/'run.py').write_text(runner)
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6721','6735').replace('--array=0-1','--array=0-0')
(c/'array.sbatch').write_text(batch)
m.update(candidate_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),
    dependencies=deps,cached_controls=str(old.relative_to(root)),search_window=[62676,62682],
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Test historical Reduce watermark repair: promoted clause cursor must reset at62677; compare full1M decisions against frozen native and predecessor. No native run or timing refresh.',
    validation='Require clean requested1M stop, complete decision accounting, unchanged decisions before62677, selected-clause watch states matching native in62676..62682; measure later first divergence without assuming full closure. Partial proof hash retained but no final certificate claim.',
    test_consideration=b['test_consideration'])
m.pop('controls',None);m.pop('standalone_controls',None)
m['resource_contract']['array']='0-0'
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as out: json.dump(m,out,indent=2);out.write('\n')
(c/'pins/candidate').chmod(0o555)
print(c)

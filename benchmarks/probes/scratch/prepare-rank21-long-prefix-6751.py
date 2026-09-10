#!/usr/bin/env python3
"""Extend the missing Rank21 trace, without changing any timing authority."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
old=root/'benchmarks/runs/20260908-6139-rank21-current-prefix'
c=root/'benchmarks/runs/20260909-6751-rank21-long-prefix';assert not c.exists()
authority=root/'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json'
b=json.loads(authority.read_text())
for rel,digest in b['files'].items():assert sha(root/b['source']/rel)==digest,rel
binary=root/'build/pin-6730-reduce-watermark-trace/tactix'
assert sha(binary)==b['binaries'][str(binary.relative_to(root))]
m=json.loads((old/'manifest.json').read_text());s=json.loads((old/'summary.json').read_text())
for rel,digest in m['hashes'].items():assert sha(old/rel)==digest,rel
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
for campaign in ('20260909-6737-reduce-watermark-BC-gate','20260909-6743-reduce-watermark-ranks1-50'):
    for folder in ('provisional','results'):
        for p in (root/'benchmarks/runs'/campaign/folder).glob('*.json'):
            state=classify(json.loads(p.read_text()),folder=='results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
view=root/'benchmarks/reports/2026-09-09-reduce-watermark-full50-6750/summary.json'
v=json.loads(view.read_text());row=v['rows'][20]
assert row['rank']==21 and row['candidate_conflicts']==112110 and row['candidate_decisions']==140595299
deps={str(p.relative_to(root)):sha(p) for p in [authority,old/'manifest.json',old/'summary.json',view,
    root/'benchmarks/runs/20260909-6734-reduce-watermark-proof-control/summary.json',
    root/'benchmarks/runs/20260909-6735-reduce-watermark-prefix/summary-6739.json']}
for a in ('candidate','kissat'):
    p=old/f'results/{a}.json';deps[str(p.relative_to(root))]=sha(p)
    for name,digest in json.loads(p.read_text())['trace_sha256'].items():
        p=old/'traces'/name;assert sha(p)==digest;deps[str(p.relative_to(root))]=digest
runner=(old/'run.py').read_text().replace('tactix-6139-','tactix-6751-')
anchor="str(m['common_decisions'] if arm == 'candidate' else 1)";assert runner.count(anchor)==1
runner=runner.replace(anchor,"str(m['common_decisions'])")
anchor="    r['trace_sha256'] =";assert runner.count(anchor)==1
runner=runner.replace(anchor,"    if arm == 'kissat':\n        sys.path.insert(0,str(c/'pins'))\n        from certificate_prefix import hash_prefix\n        r['prior_certificate_prefix'] = hash_prefix(proof,m['native_control']['certificate']['bytes'])\n"+anchor)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'):(c/folder).mkdir(parents=True)
shutil.copy2(binary,c/'pins/candidate');shutil.copy2(old/'pins/kissat',c/'pins/kissat')
shutil.copy2(root/'tools/bench/certificate_prefix.py',c/'pins/certificate_prefix.py')
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6139','6751'))
m.update(stop_conflict=50000,common_decisions=s['prefixes']['kissat']['count'],
    candidate_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),
    native_control=dict(prefix=s['prefixes']['kissat'],certificate=json.loads((old/'results/kissat.json').read_text())['partial_certificate']),
    cached_controls=str(old.relative_to(root)),dependencies=deps,
    protected_campaign_hashes=json.loads((root/'benchmarks/reports/2026-09-09-reduce-watermark-audit-6747.json').read_text())['protected_campaign_hashes'],
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Rank21 has nearly3x native decisions/propagations and2.19x peak memory. Existing native trace stops10K; extend to50K to locate a current harmful work divergence without reverting correct root guidance or touching unapproved FlatLists.',
    hypothesis='An earlier general physical/scheduling discrepancy can explain excess subsequent search work and offer a runtime/memory gain. Existing42-binary Sweep difference through10K was decision-neutral; do not assume it is harmful.',
    validation='Both50K stops, complete streams and scratch cleanup. Native must replay every old10K decision and partial-proof byte prefix. Current must account its entire stream; report any change before old10K rather than assuming old source neutrality. Compare first differing decisions and preceding portable boundaries, not just counters.',
    test_consideration='Configuration-only extended trace on immutable6730 and native6090. Reuse tested decision-prefix/stream/certificate readers and218 benchmark tests. A selected source repair requires new focused regression consideration.',
    limitation='Missing native diagnostic only, not new reference timing. No algorithm/observer edit or final SAT/model certificate claim from a stopped prefix. All prior runs and pins preserved.')
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as out:json.dump(m,out,indent=2);out.write('\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)

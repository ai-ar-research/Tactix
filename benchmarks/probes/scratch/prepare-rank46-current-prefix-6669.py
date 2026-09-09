#!/usr/bin/env python3
"""First bounded current/native Rank46 localization; never refresh timing refs."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha, reference
from checked_comparison import classify
old = root / 'benchmarks/runs/20260909-6201-rank49-current-prefix'
c = root / 'benchmarks/runs/20260909-6669-rank46-current-prefix'; assert not c.exists()
authority = root / 'benchmarks/runs/20260909-6654-congruence-empty-gates-build/manifest.json'
b = json.loads(authority.read_text())
for rel,digest in b['files'].items(): assert sha(root/b['source']/rel) == digest,rel
binary = root / 'build/pin-6654-congruence-empty-gates-trace/tactix'
assert sha(binary) == b['binaries'][str(binary.relative_to(root))]
om = json.loads((old/'manifest.json').read_text())
for rel,digest in om['hashes'].items(): assert sha(old/rel) == digest,rel
ref = reference(); row = ref['records'][45]; assert row['rank'] == 46 and row['expected'] == 'UNSAT'
full = root / 'benchmarks/runs/20260909-6666-congruence-empty-ranks1-50'
oracle = full/'results/045.json'
if not oracle.exists(): oracle = full/'provisional/045.json'
r = json.loads(oracle.read_text()); status = classify(r, oracle.parent.name == 'results')
assert status['status'] in ('pending_validation','checked_UNSAT')
assert r['instance'] == row['instance'] and r['rank'] == 46
summary = root/'benchmarks/reports/2026-09-09-congruence-empty-full50-6668/summary.json'
s = json.loads(summary.read_text()); rr = s['rows'][45]
assert rr['rank'] == 46 and rr['candidate_conflicts'] == 5619873 and rr['kissat_conflicts'] == 3082376
deps = {str(p.relative_to(root)):sha(p) for p in (authority, old/'manifest.json', summary, oracle,
        root/'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json',
        root/'benchmarks/runs/20260909-6657-congruence-empty-proof-control/summary.json')}
for p in (old/'pins/kissat', old/'run.py', old/'array.sbatch'): deps[str(p.relative_to(root))] = sha(p)
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
shutil.copy2(binary, c/'pins/candidate'); shutil.copy2(old/'pins/kissat', c/'pins/kissat')
(c/'run.py').write_text((old/'run.py').read_text().replace('tactix-6201-','tactix-6669-'))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6201','6669'))
m = dict(rank=46,input='benchmarks/cache/'+row['instance'],input_sha256_compressed=row['input_sha256'],
    stop_conflict=100000,stream_from=0,common_decisions=1,resource_contract=om['resource_contract'],
    candidate_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),
    native_authority=om['native_authority'],dependencies=deps,
    protected_campaign_hashes=json.loads((root/'benchmarks/reports/2026-09-09-congruence-empty-gates-audit-6646.json').read_text())['protected_campaign_hashes'],
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Locate first decision/boundary divergence on UNSAT Rank46, a147.865s versus80.043s fixed-native loss with1.823x conflicts. Rank31 rejected broad preclean and Rank35 legitimate binary-deletion provenance remain open, not reverted.',
    validation='Both requested100K stops, complete decision streams and bound partial proof hashes, clean scratch. First localization only; any detailed observer must replay its own control before causal interpretation. No new native timing reference or full-run neutrality claim.',
    test_consideration='No solver or observer code change. Existing decision-prefix/stream comparator tests,190 benchmark tools,111/111/110 source suites and64 fixed proof/model controls apply. Consider focused regression after locating source cause.',
    limitation='All prior runs and pins retained. This bounded native diagnostic does not rerun or overwrite the fixed Kissat timing vector; no source promotion, fuzz access or FlatLists work.')
m['hashes'] = {rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as out: json.dump(m,out,indent=2); out.write('\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

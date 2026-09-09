#!/usr/bin/env python3
"""Price the semantic effect at Rank49's first Probe with the new trace pin."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6651-rank49-congruence-stages'
c = root / 'benchmarks/runs/20260909-6658-rank49-congruence-empty-prefix'
assert not c.exists()
authority = root / 'benchmarks/runs/20260909-6654-congruence-empty-gates-build/manifest.json'
b = json.loads(authority.read_text())
for rel, digest in b['files'].items(): assert sha(root / b['source'] / rel) == digest, rel
binary = root / 'build/pin-6654-congruence-empty-gates-trace/tactix'
assert sha(binary) == b['binaries'][str(binary.relative_to(root))]
s = json.loads((old / 'summary-6653.json').read_text())
for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest, rel
proof = root / 'benchmarks/runs/20260909-6657-congruence-empty-proof-control/summary.json'
assert json.loads(proof.read_text())['final_proofs_checked'] == 64
m = json.loads((old / 'manifest.json').read_text())
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir(parents=True)
shutil.copy2(binary, c / 'pins/candidate')
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-6651-', 'tactix-6658-'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6651-congruence-stages', '6658-empty-prefix'))
for p in (authority, proof, old / 'summary-6653.json', old / 'results/candidate.json'):
    m['dependencies'][str(p.relative_to(root))] = sha(p)
m.update(cached_controls=str(old.relative_to(root)), candidate_build=str(authority.relative_to(root)),
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Test general no-gates return-path repair on Rank49, without native rerun. Require no drift before second Congruence2988; inspect second dense cleanup, first Probe formula and clock, then decision agreement against frozen native.',
    validation='Changed post-repair work/proof expected, not neutrality. Require bounded100K stop and cleanup; compare all earlier decisions and boundaries and compute first native mismatch. Require a new complete runtime/proof gate before any gain claim.',
    test_consideration=b['test_consideration'])
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as out: json.dump(m, out, indent=2); out.write('\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)

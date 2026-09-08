#!/usr/bin/env python3
"""Find the next Rank32 boundary after the repaired19375 Sweep episode."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-5479-rank32-bound-prefix'
authority = runs / '20260908-5522-sweep-small'
closure = runs / '20260908-5529-sweep-small-rank32-trace/repair-localization.json'
c = runs / '20260908-5537-rank32-small-prefix'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
a = json.loads((authority / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest
binary = root / 'build/pin-5523-sweep-small-trace/tactix'
assert sha(binary) == a['binaries'][str(binary.relative_to(root))]
v = json.loads(closure.read_text())
assert v['clean_stops'] == 2 and v['decision_comparisons']['native']['first_difference'] is None
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'):
    (c / name).mkdir()
shutil.copy2(binary, c / 'pins/candidate')
shutil.copy2(old / 'pins/kissat', c / 'pins/kissat')
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5479-', 'tactix-5537-'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)),
    candidate_authority=str(authority.relative_to(root)), candidate_source=a['source'],
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    hypothesis='Real small-environment solving closes all18593182 Rank32 decisions through19376. Extend the whole stream to100000 and localize the next material boundary; do not assume divergence is a defect.',
    causal_authority=dict(path=str(closure.relative_to(root)), sha256=sha(closure)),
    validation='Two clean stops, complete streams and pin/trace hashes; exact native parent replay and candidate prefix agreement with5529 through19376. No timing or full verdict claim.',
    test_consideration='Diagnostic-only extension reuses tested trace binary, runner and comparator; source/hash/input assertions cover changed bindings. No new solver code or unit test needed.',
    source_patch_sha256=sha(authority / 'cumulative.patch'))
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

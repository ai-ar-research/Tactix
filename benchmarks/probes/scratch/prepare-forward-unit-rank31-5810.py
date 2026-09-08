#!/usr/bin/env python3
"""Pin a full Rank31 repaired trace against existing parent/native controls."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-5755-reuse-target-rank31-replay'
authority = runs / '20260908-5798-forward-unit'
c = runs / '20260908-5810-forward-unit-rank31-prefix'
def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''): h.update(block)
    return h.hexdigest()
a = json.loads((authority / 'manifest.json').read_text())
m = json.loads((old / 'manifest.json').read_text())
for rel,digest in a['files'].items(): assert sha(root / a['source'] / rel) == digest
for rel,digest in m['hashes'].items(): assert sha(old / rel) == digest
binary = root / 'build/pin-5800-forward-unit-trace/tactix'
assert sha(binary) == a['binaries'][str(binary.relative_to(root))]
bc = runs / '20260908-5806-forward-unit-BC-gate'
outcomes = [json.loads(p.read_text()) for p in (bc / 'provisional').glob('*.json')]
assert len(outcomes) >= 64 and all(r['expected_match'] for r in outcomes)
for p in (bc / 'results').glob('*.json'):
    r = json.loads(p.read_text())
    assert r['valid'] and not r['cleanup_pending'], p
c.mkdir(exist_ok=False)
for folder in ('pins','logs','results','traces'): (c / folder).mkdir()
shutil.copy2(binary, c / 'pins/candidate')
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5755-', 'tactix-5810-'))
batch = (old / 'array.sbatch').read_text().replace(str(old),str(c))
batch = '\n'.join('#SBATCH --job-name=5810-forward-unit-rank31' if line.startswith('#SBATCH --job-name=') else line
                  for line in batch.split('\n'))
(c / 'array.sbatch').write_text(batch)
native = root / m['cached_controls']['kissat']['record']
assert sha(native) == m['cached_controls']['kissat']['sha256']
parent = old / 'results/candidate.json'
for p in (parent, native):
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), parent_campaign=str(old.relative_to(root)),
    candidate_authority=str(authority.relative_to(root)), candidate_source=a['source'],
    source_patch_sha256=sha(authority / 'cumulative.patch'),
    hypothesis='Canonical unit continuation recovers the omitted Rank31 forward tail; require unchanged decisions before its first actual effect and inspect later portable Strengthen/BVE boundaries against cached native,not identity after intended change.',
    validation='Clean300K diagnostic stop and scratch cleanup; verify complete trace hashes;compare all available decision rows and portable boundaries to both parent and cached native. Not a final proof or acceptance timing.',
    contract='Single candidate task0;12CPU/48000MiB,25min allocation,1200s solver,proof-on diagnostic; no node constraint/throttle. No Kissat rerun.',
    cached_controls=dict(candidate=dict(record=str(parent.relative_to(root)),sha256=sha(parent)),
                         kissat=m['cached_controls']['kissat']),
    test_consideration=a['test_consideration'],
    bc_dependency=dict(campaign=str(bc.relative_to(root)), provisional_outcomes=len(outcomes),
        limitation='Remaining BC solves/checks continue independently;no gate closure or promotion. Investigative trace only.'))
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in (c / 'pins/candidate',c / 'run.py',c / 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m,indent=2) + '\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)

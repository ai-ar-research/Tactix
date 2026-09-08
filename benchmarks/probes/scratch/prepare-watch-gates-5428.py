#!/usr/bin/env python3
"""Pin replicated exact-predecessor timing and the fixed cumulative B28+C5 gate."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260907-5426-watch-truncate'
assert (authority / 'manifest.json').exists()
builds = {t: root / p / 'tactix' for t,p in [('B','build/pin-5426-watch-truncate-release'), ('C','build/pin-5427-watch-truncate-bc')]}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(builds['B']) == sha(builds['C'])
def init(c):
    c.mkdir(exist_ok=False)
    for d in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / d).mkdir()
def finish(c,m):
    m.pop('slurm_job_id', None)
    m['created_at'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    m['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)
old = runs / '20260907-5421-rephase-root-rank35-runtime'
c = runs / '20260907-5428-watch-truncate-rank35-runtime'
init(c)
m = json.loads((old / 'manifest.json').read_text())
arms = {}
for name, binary, patch in [('candidate', builds['B'], authority / 'cumulative.patch'),
                            ('previous', old / 'pins/candidate', old / 'pins/candidate.patch')]:
    shutil.copy2(binary,c / 'pins' / name)
    shutil.copy2(patch,c / 'pins' / (name + '.patch'))
    arms[name] = dict(m['arms']['candidate'], path='pins/' + name, sha256=sha(binary),
                     source_patch='pins/' + name + '.patch', source_patch_sha256=sha(patch))
m['arms'] = arms
for name in ('checkmodel','drat-trim','modelcheck.py','runtime_task.py'): shutil.copy2(old / 'pins' / name,c / 'pins' / name)
with (old / 'matrix.tsv').open(newline='') as f:
    reader = csv.DictReader(f, delimiter='\t'); fields = reader.fieldnames; row = next(reader)
with (c / 'matrix.tsv').open('w',newline='') as f:
    w = csv.DictWriter(f,fields,delimiter='\t'); w.writeheader()
    for wave in range(3):
        for i,arm in enumerate(arms): w.writerow(dict(row,task=2*wave+i,arm=arm))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old),str(c)).replace('--array=0-8','--array=0-5'))
m.update(parent_campaign=str(old.relative_to(root)), candidate_authority=str(authority.relative_to(root)),
    candidate_validation='392 exact old/new state cases; 78/78 tests in both independent identical Release builds; UNFUZZED.',
    hypothesis='Removing redundant packed watch metadata traffic reduces Rank-35 unit cost at unchanged exact solver work and proof output.',
    comparison='Three independent matched candidate/previous replica blocks, full width without forced placement.',
    wave_tasks={str(w):list(range(2*w,2*w+2)) for w in range(3)},
    decision='Require identical non-timing solver statistics and proof hashes against the exact preceding Rephase candidate. Use median all-three paired runtime ratios and full range; absolute headline only if every arm range <=2%. Expected pending verdicts guide work provisionally; all six proofs and cleanup required for final closure.')
m['resource_contract']['array'] = '0-5'
finish(c,m)
old = runs / '20260907-5422-rephase-root-BC-gate'
c = runs / '20260907-5429-watch-truncate-BC-gate'
init(c)
m = json.loads((old / 'manifest.json').read_text())
for tier in ('B','C'):
    for name in ('current','candidate'):
        arm = name + '_' + tier
        binary = builds[tier] if name == 'candidate' else old / 'pins' / arm
        shutil.copy2(binary,c / 'pins' / arm)
        m['arms'][arm]['sha256'] = sha(binary)
        if name == 'candidate': m['arms'][arm].update(source_build=str(binary.parent.relative_to(root)), source_patch_sha256=sha(authority / 'cumulative.patch'))
for name in ('checkmodel','drat-trim','modelcheck.py','runtime_task.py'): shutil.copy2(old / 'pins' / name,c / 'pins' / name)
shutil.copy2(authority / 'cumulative.patch',c / 'pins/candidate.patch')
shutil.copy2(old / 'matrix.tsv',c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old),str(c)).replace('5422-rephase-root-BC','5429-watch-metadata-BC'))
m.update(predecessor=str(old.relative_to(root)),candidate_authority=str(authority.relative_to(root)),
    purpose='Unchanged checked B28+C5 cumulative gate for equivalent watch-metadata candidate against production.',
    hypothesis='Preserve all verdicts and candidate work/proof identity from 5422 while eliminating redundant watch metadata stores.',
    build_identity='Independent immutable Release 5426/5427; identical hashes; 78/78 tests each. Trace/conflict-stop/journal/shadow disabled.',
    continuation='Checks run asynchronously; expected results guide continued investigation, never final gate closure or promotion.')
finish(c,m)

#!/usr/bin/env python3
"""Pin Rank32 causal replay, paired timing waves, and unchanged B28+C5."""
from pathlib import Path
from datetime import datetime, timezone
import csv
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260908-5510-sweep-retained'
assert (authority / 'manifest.json').exists()
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def init(campaign, diagnostic=False):
    campaign.mkdir(exist_ok=False)
    for name in ('pins', 'logs', 'results', 'traces') if diagnostic else ('pins', 'logs', 'results', 'provisional', 'claims'):
        (campaign / name).mkdir()
def finish(campaign, manifest, diagnostic=False):
    manifest.pop('slurm_job_id', None)
    manifest['created_at'] = datetime.now(timezone.utc).isoformat()
    manifest['candidate_authority'] = str(authority.relative_to(root))
    names = [*(campaign / 'pins').iterdir(), campaign / 'array.sbatch',
             campaign / ('run.py' if diagnostic else 'matrix.tsv')]
    manifest['hashes' if diagnostic else 'pin_sha256'] = {
        str(path.relative_to(campaign)): sha(path) for path in names}
    (campaign / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for path in (campaign / 'pins').iterdir():
        path.chmod(path.stat().st_mode & ~0o222)
    print(campaign)

old = runs / '20260908-5490-rank32-sweep'
c = runs / '20260908-5515-sweep-retained-rank32-trace'
init(c, True)
shutil.copy2(root / 'build/pin-5511-sweep-retained-trace/tactix', c / 'pins/candidate')
shutil.copy2(old / 'pins/kissat', c / 'pins/kissat')
runner = (old / 'run.py').read_text().replace('tactix-5490-', 'tactix-5515-')
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
    candidate_source='.claude/worktrees/runtime-sweep-retained-5510',
    hypothesis='Entry-only retained eligibility removes the known extra seed836; determine the earliest remaining decision and Sweep boundary difference against native after the general repair.',
    validation='Two clean stops, trace/pin hashes, exact native full parent replay. Compare candidate to native and to old candidate without requiring unchanged candidate trajectory. Earlier differences must be localized, not discarded.',
    test_consideration='85/85 twice, trace84/84; actual old pure-seed counterexample; entry/cap/ledger/N1 regression fixtures.',
    source_patch_sha256=sha(authority / 'cumulative.patch'))
finish(c, m, True)

old = runs / '20260908-5476-sweep-cursor-BC-gate'
c = runs / '20260908-5516-sweep-retained-BC-gate'
init(c)
m = json.loads((old / 'manifest.json').read_text())
for tier, build in [('B', 'build/pin-5510-sweep-retained-release'), ('C', 'build/pin-5512-sweep-retained-bc')]:
    for role in ('current', 'candidate'):
        arm = role + '_' + tier
        binary = root / build / 'tactix' if role == 'candidate' else old / 'pins' / arm
        shutil.copy2(binary, c / 'pins' / arm)
        m['arms'][arm]['sha256'] = sha(binary)
        if role == 'candidate':
            m['arms'][arm].update(source_build=build, source_patch_sha256=sha(authority / 'cumulative.patch'))
assert sha(c / 'pins/candidate_B') == sha(c / 'pins/candidate_C')
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('5476-sweep-cursor-BC', '5516-sweep-retained-BC'))
m.update(predecessor=str(old.relative_to(root)),
    purpose='Unchanged cumulative B28+C5 gate for retained Sweep entry admission against production.',
    hypothesis='Preserve all expected verdicts and cumulative regression behavior after the general retained-seed scheduling repair.',
    build_identity='Independent Release5510/5512, identical SHA256,85/85 each; no event trace/conflict stop/journal/shadow.',
    status='UNFUZZED isolated candidate; pending checks are provisional, not promotion.')
finish(c, m)

old = runs / '20260908-5475-sweep-cursor-rank35-runtime'
c = runs / '20260908-5517-sweep-retained-rank32-runtime'
init(c)
m = json.loads((old / 'manifest.json').read_text())
for arm, build, source_authority in [
    ('candidate', 'build/pin-5510-sweep-retained-release', authority),
    ('previous', 'build/pin-5473-sweep-cursor-release', runs / '20260908-5473-sweep-cursor')]:
    binary = root / build / 'tactix'
    patch = source_authority / 'cumulative.patch'
    shutil.copy2(binary, c / 'pins' / arm)
    shutil.copy2(patch, c / 'pins' / (arm + '.patch'))
    m['arms'][arm].update(sha256=sha(binary), source_build=build,
        source_patch_sha256=sha(patch))
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
with (runs / '20260908-5485-sweep-cursor-ranks1-40/matrix.tsv').open() as stream:
    selected = [row for row in csv.DictReader(stream, delimiter='\t') if row['rank'] == '32']
assert len(selected) == 2 and all(row['expected'] == 'UNSAT' for row in selected)
instance = selected[0]['instance']
assert selected[1]['instance'] == instance
with (c / 'matrix.tsv').open('w', newline='') as stream:
    writer = csv.writer(stream, delimiter='\t')
    writer.writerow(('task', 'rank', 'arm', 'tier', 'instance', 'expected'))
    for task in range(6):
        writer.writerow((task, 32, 'candidate' if task % 2 == 0 else 'previous', 'D', instance, 'UNSAT'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)),
    input_sha256={instance: sha(root / 'benchmarks/cache' / instance)},
    hypothesis='Restoring retained-entry eligibility reduces divergent Sweep work and improves Rank32 proof-on runtime against the exact cumulative predecessor5473.',
    candidate_validation='Release85/85 twice, trace84/84; pure retained old-binary counterexample and focused entry/ledger/N1 tests; UNFUZZED.',
    decision='Three independent paired waves; submit0-1, then2-3, then4-5 after the preceding pair finishes solver work (checkers continue). Require correct verdicts and median improvement, inspect every pair/range and work counters. This semantic repair need not preserve work/proof bytes across arms; replicas within each arm should agree. Absolute headline requires each arm spread<=2%. B28+C5 remains mandatory; full Rank1-40 determines goal.')
finish(c, m)

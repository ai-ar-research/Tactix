#!/usr/bin/env python3
"""Prepare B28+C5 and candidate-only causal replays with cached native controls."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260908-5741-reuse-target'
a = json.loads((authority / 'manifest.json').read_text())
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
for rel,digest in a['binaries'].items(): assert sha(root / rel) == digest
for rel,digest in a['files'].items(): assert sha(root / a['source'] / rel) == digest
assert a['binaries']['build/pin-5741-reuse-target-release/tactix'] == a['binaries']['build/pin-5743-reuse-target-bc/tactix']

def prepare(old, name, diagnostic):
    m = json.loads((old / 'manifest.json').read_text())
    for rel,digest in m.get('hashes', m.get('pin_sha256', {})).items(): assert sha(old / rel) == digest
    c = runs / name
    c.mkdir(exist_ok=False)
    for folder in (('pins','logs','results','traces') if diagnostic else ('pins','logs','results','provisional','claims')):
        (c / folder).mkdir()
    for p in (old / 'pins').iterdir():
        if p.is_file() and (not diagnostic or p.name == 'candidate'):
            shutil.copy2(p, c / 'pins' / p.name)
            (c / 'pins' / p.name).chmod(p.stat().st_mode | 0o200)
    item = 'run.py' if diagnostic else 'matrix.tsv'
    (c / item).write_text((old / item).read_text().replace('tactix-5735-', 'tactix-5755-').replace('tactix-5726-', 'tactix-5756-'))
    batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
    if diagnostic:
        assert '#SBATCH --array=0-1' in batch
        batch = batch.replace('#SBATCH --array=0-1', '#SBATCH --array=0-0')
    (c / 'array.sbatch').write_text(batch)
    m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        predecessor=str(old.relative_to(root)), candidate_authority=str(authority.relative_to(root)),
        candidate_source=a['source'], test_consideration=a['test_consideration'])
    return c,m

def finish(c,m,diagnostic):
    key = 'hashes' if diagnostic else 'pin_sha256'
    paths = [*(c / 'pins').iterdir(), c / 'array.sbatch', c / ('run.py' if diagnostic else 'matrix.tsv')]
    m[key] = {str(p.relative_to(c)):sha(p) for p in paths}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)

old = runs / '20260908-5727-root-reuse-BC-gate'
c,m = prepare(old, '20260908-5754-reuse-target-BC-gate', False)
for tier,n,mode in [('B',5741,'release'),('C',5743,'bc')]:
    build = root / f'build/pin-{n}-reuse-target-{mode}'
    arm = 'candidate_' + tier
    shutil.copy2(build / 'tactix', c / 'pins' / arm)
    m['arms'][arm].update(sha256=sha(c / 'pins' / arm), source_build=str(build.relative_to(root)),
        source_patch_sha256=sha(authority / 'cumulative.patch'))
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
m.update(purpose='B28+C5 cumulative regression gate for public reuse-target repair5741.',
    hypothesis='Public targets change only explicit/canonical callers; preserve expected verdicts and cumulative DefaultPolicy performance.',
    build_identity='92/92 independent Release/BC,91/91 trace,5088 checked micro-proofs; matching Release hashes.',
    status='Unfuzzed investigation material; solve outcomes permit provisional continuation before checks.')
finish(c,m,False)

for number,old_name,new_name in [
    (5755,'20260908-5735-root-reuse-rank31-prefix','20260908-5755-reuse-target-rank31-replay'),
    (5756,'20260908-5726-root-reuse-rank33-prefix','20260908-5756-reuse-target-rank33-replay')]:
    old = runs / old_name
    c,m = prepare(old, new_name, True)
    shutil.copy2(root / 'build/pin-5742-reuse-target-trace/tactix', c / 'pins/candidate')
    controls = {}
    for arm in ('candidate','kissat'):
        p = old / 'results' / (arm + '.json')
        r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        for name,digest in r['trace_sha256'].items(): assert sha(old / 'traces' / name) == digest
        controls[arm] = dict(record=str(p.relative_to(root)),sha256=sha(p))
    m.update(parent_campaign=str(old.relative_to(root)), cached_controls=controls,
        source_patch_sha256=sha(authority / 'cumulative.patch'),
        hypothesis='Public reuse-target dispatch preserves DefaultPolicy: require exact entire stopped candidate trace and partial-proof replay of5718, then reuse unchanged native controls.',
        validation='Only candidate task0 runs. Require clean requested stop, exact parent candidate trace/proof hashes; cached native remains bound to prior pin/input/observer contract.',
        resource_contract=dict(partition='cpu-dense-hwulab-q', cpus_per_task=12,memory_mib=48000,array='0-0',throttle=None,node_constraint=None),
        limitation='Stopped diagnostic,not final checked verdict or acceptance timing; no new Kissat run.')
    finish(c,m,True)

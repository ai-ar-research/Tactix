#!/usr/bin/env python3
"""Preserve the parent causal, B28+C5 and three-wave timing contracts."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260908-5592-factor-root-support'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
a = json.loads((authority / 'manifest.json').read_text())
for name, digest in a['binaries'].items(): assert sha(root / name) == digest
for name, digest in a['patches'].items(): assert sha(authority / name) == digest
def initialize(old, name, diagnostic=False):
    m = json.loads((old / 'manifest.json').read_text())
    for rel, digest in m.get('hashes', m.get('pin_sha256', {})).items(): assert sha(old / rel) == digest
    c = runs / name
    c.mkdir(exist_ok=False)
    for folder in (('pins', 'logs', 'results', 'traces') if diagnostic else ('pins', 'logs', 'results', 'provisional', 'claims')):
        (c / folder).mkdir()
    for p in (old / 'pins').iterdir():
        if p.is_file():
            shutil.copy2(p, c / 'pins' / p.name)
            (c / 'pins' / p.name).chmod(p.stat().st_mode | 0o200)
    name = 'run.py' if diagnostic else 'matrix.tsv'
    body = (old / name).read_text()
    if diagnostic: body = body.replace('tactix-5537-', 'tactix-5597-')
    (c / name).write_text(body)
    (c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
    m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        candidate_authority=str(authority.relative_to(root)), predecessor=str(old.relative_to(root)),
        test_consideration='87/87 in both independent Release builds;trace86/86.96 actual old/new fixtures,16 targeted changes,64 native connection cases. Existing runner/oracle/matrix/resource contracts unchanged.')
    m.pop('slurm_job_id', None)
    return c, m
def finish(c, m, diagnostic=False):
    names = [*(p for p in (c / 'pins').iterdir() if p.is_file()), c / 'array.sbatch', c / ('run.py' if diagnostic else 'matrix.tsv')]
    m['hashes' if diagnostic else 'pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in names}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)

old = runs / '20260908-5537-rank32-small-prefix'
c, m = initialize(old, '20260908-5597-factor-support-rank32-prefix', True)
shutil.copy2(root / 'build/pin-5593-factor-root-support-trace/tactix', c / 'pins/candidate')
m.update(parent_campaign=str(old.relative_to(root)), parent_summary_sha256=sha(old / 'summary.json'),
    candidate_source=a['source'], source_patch_sha256=sha(authority / 'cumulative.patch'),
    hypothesis='Repair root-fixed smaller-endpoint support before Factor peeling. Determine whether the19375 Factor count/grid discrepancy and20095 decision split close, then localize any remaining full100K prefix difference.',
    validation='Two clean bounded stops and hashed complete traces. Native whole parent decisions/prefix/boundaries/proof must replay. Candidate may change: compare full native trajectory, preserve and report any earlier divergence. No acceptance timing.')
finish(c, m, True)

old = runs / '20260908-5530-sweep-small-BC-gate'
c, m = initialize(old, '20260908-5598-factor-support-BC-gate')
for tier, build in [('B', 'build/pin-5592-factor-root-support-release'), ('C', 'build/pin-5594-factor-root-support-bc')]:
    arm = 'candidate_' + tier
    shutil.copy2(root / build / 'tactix', c / 'pins' / arm)
    m['arms'][arm].update(sha256=sha(c / 'pins' / arm), source_build=build,
        source_patch_sha256=sha(authority / 'cumulative.patch'))
assert sha(c / 'pins/candidate_B') == sha(c / 'pins/candidate_C')
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
m.update(purpose='Unchanged cumulative B28+C5 regression gate for general Factor support-count repair.',
    hypothesis='Preserve expected verdicts and cumulative regression behavior after correcting candidate support from root-fixed endpoints.',
    build_identity='Independent Release5592/5594,87/87 each and matching SHA; observers disabled.',
    status='UNFUZZED isolated candidate. Pending checks permit investigation, not promotion.')
finish(c, m)

old = runs / '20260908-5531-sweep-small-rank32-runtime'
c, m = initialize(old, '20260908-5599-factor-support-rank32-runtime')
for arm, build, auth in [('candidate', 'build/pin-5592-factor-root-support-release', authority),
                        ('previous', 'build/pin-5522-sweep-small-release', runs / '20260908-5522-sweep-small')]:
    shutil.copy2(root / build / 'tactix', c / 'pins' / arm)
    shutil.copy2(auth / 'cumulative.patch', c / 'pins' / (arm + '.patch'))
    m['arms'][arm].update(sha256=sha(c / 'pins' / arm), source_build=build,
        source_patch_sha256=sha(auth / 'cumulative.patch'))
m.update(parent_campaign=str(old.relative_to(root)),
    hypothesis='Correcting root-fixed endpoint support closes a Factor implementation divergence and reduces net Rank32 work/runtime against parent5522.',
    candidate_validation='Release87/87 twice,trace86/86;96 actual old/new fixtures,64 native connection cases. UNFUZZED.',
    decision='After all B28 expected solver snapshots, submit waves0-1,2-3,4-5, each after predecessor solver completion without waiting for certificates. Require within-arm full non-timing statistics/proof-byte identity, all pair ratios and spreads. Absolute timing headline requires each spread<=2%. Cross-arm work/proofs may change. B28+C5 and full Rank1-40 remain separate gates.',
    status='UNFUZZED isolated candidate; no promotion.')
finish(c, m)

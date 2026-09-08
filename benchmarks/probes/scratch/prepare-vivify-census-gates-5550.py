#!/usr/bin/env python3
"""Preserve B28+C5 and predeclare three exact-work Rank39 timing pairs."""
from pathlib import Path
import csv
import datetime
import hashlib
import json
import shutil
root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260908-5546-vivify-census'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
a = json.loads((authority / 'manifest.json').read_text())
for rel, digest in a['binaries'].items(): assert sha(root / rel) == digest
assert len(set(a['binaries'].values())) == 1
for rel, digest in a['patches'].items(): assert sha(authority / rel) == digest
def initialize(old, name):
    m = json.loads((old / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): assert sha(old / rel) == digest
    c = runs / name
    if c.exists():
        # Resume only the verified, unpublished copy scaffold left by the
        # initial attempt to copy Python's runtime-created __pycache__ folder.
        assert not (c / 'manifest.json').exists() and not (c / 'array.sbatch').exists()
        assert not (c / 'matrix.tsv').exists()
        assert set(p.name for p in c.iterdir()) == {'pins', 'logs', 'results', 'provisional', 'claims'}
        for d in ('logs', 'results', 'provisional', 'claims'): assert not list((c / d).iterdir())
        for p in (c / 'pins').iterdir(): assert p.is_file() and sha(p) == sha(old / 'pins' / p.name)
    else:
        c.mkdir()
        for d in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / d).mkdir()
    for p in (old / 'pins').iterdir():
        if p.is_file() and not (c / 'pins' / p.name).exists():
            shutil.copy2(p, c / 'pins' / p.name)
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode | 0o200)
    shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
    (c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
    m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
             candidate_authority=str(authority.relative_to(root)),
             predecessor=str(old.relative_to(root)))
    m.pop('slurm_job_id', None)
    return c, m
def finish(c, m):
    m['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in
                     [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)
old = runs / '20260908-5530-sweep-small-BC-gate'
c, m = initialize(old, '20260908-5550-vivify-census-BC-gate')
for tier, build in [('B', 'build/pin-5546-vivify-census-release'), ('C', 'build/pin-5547-vivify-census-bc')]:
    arm = 'candidate_' + tier
    shutil.copy2(root / build / 'tactix', c / 'pins' / arm)
    m['arms'][arm].update(sha256=sha(c / 'pins' / arm), source_build=build,
                         source_patch_sha256=sha(authority / 'cumulative.patch'))
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
m.update(purpose='Unchanged cumulative B28+C5 gate for the unused Vivify census optimization.',
    hypothesis='Preserve every expected verdict and cumulative regression behavior with no scheduling or proof change.',
    build_identity='Independent5546/5547 Release builds87/87 with identical SHA; no diagnostic observer.',
    test_consideration='192 actual old/new state/proof cases and both full suites pass. Unchanged matrix,runner,oracles,resources; measurement-only bindings need no further solver unit test.')
finish(c, m)
old = runs / '20260908-5531-sweep-small-rank32-runtime'
c, m = initialize(old, '20260908-5551-vivify-census-rank39-runtime')
for arm, build, auth in [('candidate', 'build/pin-5546-vivify-census-release', authority),
                         ('previous', 'build/pin-5522-sweep-small-release', runs / '20260908-5522-sweep-small')]:
    shutil.copy2(root / build / 'tactix', c / 'pins' / arm)
    shutil.copy2(auth / 'cumulative.patch', c / 'pins' / (arm + '.patch'))
    m['arms'][arm].update(sha256=sha(c / 'pins' / arm), source_build=build,
                         source_patch_sha256=sha(auth / 'cumulative.patch'))
with (runs / '20260908-5535-sweep-small-ranks1-40/matrix.tsv').open() as stream:
    rows = [r for r in csv.DictReader(stream, delimiter='\t') if r['rank'] == '39']
assert len(rows) == 2 and all(r['expected'] == 'UNSAT' for r in rows)
instance = rows[0]['instance']
assert rows[1]['instance'] == instance
with (c / 'matrix.tsv').open('w', newline='') as stream:
    writer = csv.writer(stream, delimiter='\t')
    writer.writerow(('task', 'rank', 'arm', 'tier', 'instance', 'expected'))
    for task in range(6):
        writer.writerow((task, 39, 'candidate' if task % 2 == 0 else 'previous', 'D', instance, 'UNSAT'))
m.update(parent_campaign=str(old.relative_to(root)),
    input_sha256={instance:sha(root / 'benchmarks/cache' / instance)},
    hypothesis='Skipping an unused whole-arena census in redundant-only Vivify schedules lowers Rank39 execution cost while preserving all work and proof bytes.',
    candidate_validation='Release87/87 twice and192 actual5522/5546 fixed-state/proof cases; UNFUZZED.',
    decision='After B28 expected solver snapshots, submit three independent paired waves0-1,2-3,4-5; next wave follows solver completion without waiting for proof checks. Require identical complete non-timing statistics and proof SHA across arms and replicas. Report all paired ratios and arm spreads, with absolute headline only if each spread<=2%; no tuning or repeated passing campaign.',
    test_consideration='No timing-runner change; reuse complete-statistics/proof identity validator. Candidate tests explicitly cover the schedule branches changed by the optimization.')
finish(c, m)

#!/usr/bin/env python3
"""Prepare separate BC and Rank32 diagnostic campaigns from frozen5830."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260908-5830-forward-admission'
a = json.loads((authority / 'manifest.json').read_text())
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in a['binaries'].items():
    assert sha(root / rel) == digest
old = runs / '20260908-5806-forward-unit-BC-gate'
c = runs / '20260908-5836-forward-admission-BC-gate'
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(old / rel) == digest, rel
assert not (c / 'manifest.json').exists() and not (c / 'array.sbatch').exists()
c.mkdir(exist_ok=True)
for folder in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / folder).mkdir(exist_ok=True)
    if folder != 'pins':
        assert not list((c / folder).iterdir())
for p in (c / 'pins').iterdir():
    assert p.is_file() and sha(p) == m['pin_sha256']['pins/' + p.name]
for p in (old / 'pins').iterdir():
    if p.is_file() and not p.name.startswith('candidate') and not (c / 'pins' / p.name).exists():
        shutil.copy2(p, c / 'pins' / p.name)
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
batch = '\n'.join('#SBATCH --job-name=5836-forward-admission-BC' if line.startswith('#SBATCH --job-name=') else line for line in batch.split('\n'))
(c / 'array.sbatch').write_text(batch)
for tier, number, name in [('B', 5830, 'release'), ('C', 5831, 'bc')]:
    build = root / f'build/pin-{number}-forward-admission-{name}'
    arm = 'candidate_' + tier
    shutil.copy2(build / 'tactix', c / 'pins' / arm)
    m['arms'][arm].update(sha256=sha(c / 'pins' / arm), source_build=str(build.relative_to(root)),
                          source_patch_sha256=sha(authority / 'cumulative.patch'))
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), predecessor=str(old.relative_to(root)),
         candidate_authority=str(authority.relative_to(root)), candidate_source=a['source'],
         purpose='B28+C5 cumulative regression gate for isolated C2 admission producer5830.',
         hypothesis='Native clause-addition-only C2 marks preserve expected verdicts and improve cumulative work;report B and C separately,not only combined ratio.',
         test_consideration=a['test_consideration'], build_identity=a['test_consideration'])
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

old = runs / '20260908-5771-reuse-target-rank32-extended'
c = runs / '20260908-5837-forward-admission-rank32-prefix'
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'):
    (c / folder).mkdir()
shutil.copy2(root / 'build/pin-5832-forward-admission-trace/tactix', c / 'pins/candidate')
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5771-', 'tactix-5837-'))
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('#SBATCH --array=0-1', '#SBATCH --array=0-0')
batch = '\n'.join('#SBATCH --job-name=5837-admission-rank32' if line.startswith('#SBATCH --job-name=') else line for line in batch.split('\n'))
(c / 'array.sbatch').write_text(batch)
early = runs / '20260908-5597-factor-support-rank32-prefix/results/kissat.json'
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), parent_campaign=str(old.relative_to(root)),
         candidate_authority=str(authority.relative_to(root)), candidate_source=a['source'],
         source_patch_sha256=sha(authority / 'cumulative.patch'), stream_from=0,
         hypothesis='Remove root-only C2 admission producer;locate earliest changed work and compare with native. Earlier divergences must be reported,not hidden by the179463 witness.',
         validation='Clean300K stop and cleanup,complete new stream from0. Compare cached native5597 first100K rows and5771 suffix;common-prefix hash separately checks first40102204 decisions. Intended changes need not replay parent.',
         contract='One12CPU/48000MiB candidate task,25min allocation,1200s solver;no node constraint,throttle,or native rerun. Diagnostic only.',
         resource_contract=dict(m['resource_contract'], array='0-0'),
         cached_controls={arm: dict(record=str((old / ('results/' + arm + '.json')).relative_to(root)),
                                   sha256=sha(old / ('results/' + arm + '.json'))) for arm in ('candidate', 'kissat')},
         cached_early_native=dict(record=str(early.relative_to(root)), sha256=sha(early)),
         test_consideration=a['test_consideration'])
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'pins/candidate', c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)

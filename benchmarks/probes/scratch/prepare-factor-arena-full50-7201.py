#!/usr/bin/env python3
"""Transfer the independently measured arena to all50, without composition."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha, reference
from checked_comparison import classify
c = root/'benchmarks/runs/20260911-7201-factor-arena-ranks1-50'
assert not c.exists()
evidence = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected, p
    evidence[str(p.relative_to(root))] = digest
subprocess.run([sys.executable, str(Path(__file__).with_name('check-submit-factor-arena-runtime-7186.py'))], check=True)
assessment = root/'benchmarks/reports/2026-09-11-factor-arena-assessment-7192.json'
a = json.loads(assessment.read_text()); bind(assessment)
assert a['runtime_expected'] == 18 and a['profile_expected'] == 6
assert all(r['absolute_headline_stable'] for r in a['spreads'].values())
assert a['complete_factor']['max'] < 1
for rel, digest in a['hashes'].items(): bind(root/rel, digest)
bc = root/'benchmarks/runs/20260910-7183-factor-arena-BC'
for task in range(66):
    p = bc/f'results/{task:03d}.json'
    assert classify(json.loads(p.read_text()), True)['checked']; bind(p)
authority = root/'benchmarks/runs/20260910-7180-factor-arena-build'
b = json.loads((authority/'manifest.json').read_text()); bind(authority/'manifest.json')
for rel, digest in b['files'].items(): bind(root/b['source']/rel, digest)
for rel, digest in b['binaries'].items(): bind(root/rel, digest)
old = root/'benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50'
m = json.loads((old/'manifest.json').read_text()); bind(old/'manifest.json')
for rel, digest in m['pin_sha256'].items(): bind(old/rel, digest)
ref = reference(); assert len(ref['records']) == 50
for task in range(50):
    p = old/f'results/{task:03d}.json'; final = p.exists()
    if not final: p = old/f'provisional/{task:03d}.json'
    assert classify(json.loads(p.read_text()), final)['status'] in ('checked_SAT','checked_UNSAT','pending_validation'); bind(p)
for folder in ('pins','logs','results','provisional','claims'): (c/folder).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):
    shutil.copy2(old/'pins'/name, c/'pins'/name)
binary = root/'build/pin-7180-factor-arena-release/tactix'
shutil.copy2(binary, c/'pins/tactix')
shutil.copy2(authority/'cumulative.patch', c/'pins/candidate-source.patch')
shutil.copy2(old/'matrix.tsv', c/'matrix.tsv')
with (c/'array.sbatch').open('x') as out:
    out.write((old/'array.sbatch').read_text().replace(str(old), str(c)))
arm = dict(m['arms']['tactix'], sha256=sha(binary), source_build=str(binary.parent.relative_to(root)),
           source_patch_sha256=sha(authority/'cumulative.patch'))
bind(Path(__file__))
m.update(arms={'tactix':arm}, evidence_sha256=evidence,
    candidate_authority=dict(path=str((authority/'manifest.json').relative_to(root)),sha256=sha(authority/'manifest.json')),
    parent_comparison=str(old.relative_to(root)),
    hypothesis='Isolated7180 Factor arena on all50 after exact three-wave runtime/profile evidence. No Congruence fusion or telemetry ablation. Preserve every rank, solve loss and RSS tradeoff.',
    decision='Retain the candidate for full-set investigation: complete Factor median0.872448474, Rank31/35 whole-runtime0.993302108/0.986103414, Rank38neutral1.000755619 but+6220KiB. Not production acceptance or source fuzz waiver.',
    test_consideration=b['test_consideration']+' All66 BC checks complete; all18 targeted runtimes and6 profiles exact. Copied full50 runner/matrix unchanged; maintained fixed-runtime and strict fingerprint fixtures cover this transfer.',
    validation='All50 exact retained7144 work and certificate fingerprints before scoring. Pending independent checks allow development, not acceptance. Inspect memory on every rank; fixed native denominator unchanged.')
m['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out: json.dump(m,out,indent=2)
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

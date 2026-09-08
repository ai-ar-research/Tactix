#!/usr/bin/env python3
"""Bind the general Factor support repair, focused validation and three clean pins."""
from pathlib import Path
import difflib
import hashlib
import json
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5592-factor-root-support'
source = root / '.claude/worktrees/runtime-factor-root-support-5592'
parent = root / '.claude/worktrees/runtime-sweep-small-5522'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
builds = dict(release='build/pin-5592-factor-root-support-release',
              trace='build/pin-5593-factor-root-support-trace', bc='build/pin-5594-factor-root-support-bc')
for arm, path in builds.items():
    expected = 86 if arm == 'trace' else 87
    assert f'100% tests passed out of {expected}' in (c / 'logs' / f'ctest-{arm}.log').read_text()
assert sha(root / builds['release'] / 'tactix') == sha(root / builds['bc'] / 'tactix')
reference = json.loads((c / 'reference-validation.json').read_text())
assert reference['cases'] == 96 and reference['native_cases'] == 64 and len(reference['changes']) == 16
for path, digest in reference['hashes'].items(): assert sha(root / path) == digest
files = ['src/rules/factor/factor.cpp', 'tests/test_factor_root_support.cpp']
def patch(before, names):
    return ''.join(''.join(difflib.unified_diff(
        (before / name).read_text().splitlines(True) if (before / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (before / name).exists() else '/dev/null', tofile='b/' + name)) for name in names)
for name, body in [('repair.patch', patch(parent, files)),
                   ('cumulative.patch', patch(root, [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))]:
    assert not (c / name).exists()
    (c / name).write_text(body)
evidence = root / 'benchmarks/runs/20260908-5588-rank32-factor-root-neighbor/literal-localization.json'
m = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Translate native active smaller-endpoint binary support into the stable-identity kernel before Factor candidate peeling.',
    evidence=str(evidence.relative_to(root)), evidence_sha256=sha(evidence),
    contract='Count each binary pair once from its smaller imported endpoint only when unassigned and unretired. This changes candidate support, not physical degrees, retained binaries, proof emission, matching budget, public seed enumeration or any guard. No benchmark-conditioned constants or new primitive.',
    references=reference['references'], tests='Release87/87 in each independent build, trace86/86.96 actual old/new cases across import order, polarity, root fixation, zero/one/two prune rounds, canonical/explicit seed.64 actual native connection cases match; exactly16 old/new support scores change.',
    status='Isolated UNFUZZED successor to5522. No runtime result or promotion. Existing Claude5539 covers5522, not this repair. B28+C5 and full Rank1-40 remain required.',
    files={name:sha(source / name) for name in files},
    binaries={path + '/tactix':sha(root / path / 'tactix') for path in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    logs={p.name:sha(p) for p in (c / 'logs').iterdir()},
    reference_validation_sha256=sha(c / 'reference-validation.json'))
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(json.dumps(m['binaries'], indent=2))

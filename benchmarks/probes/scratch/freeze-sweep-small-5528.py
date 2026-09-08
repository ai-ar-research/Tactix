#!/usr/bin/env python3
"""Freeze actual small-environment lifecycle repair and independent tested pins."""
from pathlib import Path
import difflib
import hashlib
import json

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5522-sweep-small'
source = root / '.claude/worktrees/runtime-sweep-small-5522'
parent = root / '.claude/worktrees/runtime-sweep-retained-5510'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
builds = {'release':'build/pin-5522-sweep-small-release',
          'bc':'build/pin-5524-sweep-small-bc', 'trace':'build/pin-5523-sweep-small-trace'}
for arm, directory in builds.items():
    expected = 85 if arm == 'trace' else 86
    assert f'100% tests passed out of {expected}' in (c / 'logs' / f'ctest-verified-{arm}.log').read_text()
assert sha(root / builds['release'] / 'tactix') == sha(root / builds['bc'] / 'tactix')
reference = json.loads((c / 'reference-validation.json').read_text())
assert reference['cases'] == 12
for path, digest in reference['hashes'].items():
    assert sha(root / path) == digest
assert 'old counterexample: all one-clause environments skipped' in (c / 'logs/old-verified-counterexample.log').read_text()
files = ['src/rules/sweep/sweep.cpp', 'tests/test_sweep_small_env.cpp']
def patch(before, names):
    return ''.join(''.join(difflib.unified_diff(
        (before / name).read_text().splitlines(True) if (before / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (before / name).exists() else '/dev/null',
        tofile='b/' + name)) for name in names)
for name, body in [('repair.patch', patch(parent, files)),
                   ('cumulative.patch', patch(root, [str(path.relative_to(root))
                    for path in sorted((root / 'src').rglob('*')) if path.is_file()]))]:
    assert not (c / name).exists()
    (c / name).write_text(body)
manifest = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Remove empty/single-clause Sweep solve shortcuts; execute and charge real Kitten work.',
    evidence='benchmarks/runs/20260908-5515-sweep-retained-rank32-trace/effort-localization.json',
    evidence_sha256=sha(root / 'benchmarks/runs/20260908-5515-sweep-retained-rank32-trace/effort-localization.json'),
    contract='Every constructed live-seed environment follows initial import/randomize/solve/model/refinement and real effort accounting. Builder false still means no usable clauses or fixed/retired seed; Sweep distinguishes a clause-free live seed by its nonempty variable list. Fixed/retired seeds still decline. No synthetic tick charge, bound/option change, new primitive, proof vocabulary or public guard restriction.',
    references='Both references solve one-clause environments. Kissat also solves singleton/empty environments; CaDiCaL skips a singleton. Follow Kissat tie-break. Twelve direct actual native/SubSolver cases match initial/post-flip ticks, flip counts, generator and model hash.',
    tests='Release86/86 twice and trace85/85. New one-clause widths2/3/17/270, bounded solves, empty/singleton/public-invalid cases, fixed/retired decline, repeated sessions, no false equivalences/units or formula mutation, and strict outer/inclusive inner budget transition leaving the third seed retained. Actual5510 objects skip the same four one-clause cases. Full existing DRAT/LRAT, model and API suites pass.',
    failed_expectations='Initial test exposed an earlier builder-empty shortcut, now included in the repair. Budget fixture initially forgot strict outer equality, then omitted dense connection for its third unwatched large occurrence. Those fixture expectations/setup were corrected without changing budget or admission code. Initial failed logs retained; no soundness/model/proof failure observed.',
    status='Isolated UNFUZZED successor to5510. Predecessor5510 median runtime1.067134x5473 on Rank32; this successor has no runtime result yet. No production promotion. Existing Claude fuzz5492 covers5473 only.',
    files={name:sha(source / name) for name in files},
    binaries={directory + '/tactix':sha(root / directory / 'tactix') for directory in builds.values()},
    patches={path.name:sha(path) for path in c.glob('*.patch')},
    logs={path.name:sha(path) for path in (c / 'logs').iterdir()},
    reference_validation_sha256=sha(c / 'reference-validation.json'))
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest['binaries'], indent=2))

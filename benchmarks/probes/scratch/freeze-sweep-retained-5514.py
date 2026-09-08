#!/usr/bin/env python3
"""Freeze the bounded scheduling repair and final tested source/pins."""
from pathlib import Path
import difflib
import hashlib
import json

root = Path(__file__).resolve().parents[3]
campaign = root / 'benchmarks/runs/20260908-5510-sweep-retained'
source = root / '.claude/worktrees/runtime-sweep-retained-5510'
parent = root / '.claude/worktrees/runtime-sweep-cursor-5473'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
builds = {'release': 'build/pin-5510-sweep-retained-release',
          'bc': 'build/pin-5512-sweep-retained-bc',
          'trace': 'build/pin-5511-sweep-retained-trace'}
for arm, directory in builds.items():
    expected = 84 if arm == 'trace' else 85
    log = (campaign / 'logs' / f'ctest-final-{arm}.log').read_text()
    assert f'100% tests passed out of {expected}' in log
assert sha(root / builds['release'] / 'tactix') == sha(root / builds['bc'] / 'tactix')
assert 'old counterexample: retained pure seed attempted; public API legal' in (
    campaign / 'logs/old-final-counterexample.log').read_text()
files = ['src/rules/sweep/sweep.cpp', 'tests/test_sweep.cpp', 'tests/test_sweep_retained.cpp']
def patch(before, names):
    return ''.join(''.join(difflib.unified_diff(
        (before / name).read_text().splitlines(True) if (before / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (before / name).exists() else '/dev/null',
        tofile='b/' + name)) for name in names)
for name, body in [('repair.patch', patch(parent, files)),
                   ('cumulative.patch', patch(root, [str(p.relative_to(root))
                    for p in sorted((root / 'src').rglob('*')) if p.is_file()]))]:
    assert not (campaign / name).exists()
    (campaign / name).write_text(body)
manifest = dict(
    source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Revalidate retained Sweep seeds once at call entry with the existing fresh-admission predicate.',
    evidence='benchmarks/runs/20260908-5505-rank32-sweep-eligibility/eligibility-validation.json',
    evidence_sha256=sha(root / 'benchmarks/runs/20260908-5505-rank32-sweep-eligibility/eligibility-validation.json'),
    references=['kissat-imply-fast-5414/src/sweep.c::reschedule_previously_remaining',
                'cadical-extdiag/src/sweep.cpp::Internal::reschedule_previously_remaining'],
    contract='Filter retained front/tail in place before environments using current external mapping, activity, root status and occurrence caps. Preserve ordering, slot identities, live counts, completion ownership and same-call representative rescheduling. Capture retained before filtering, so all-filtered does not start a fresh pass in the same call. No public API, proof vocabulary, option or budget change.',
    limitation='Incomplete-variable flags and native fresh-admission/completion parity remain unaudited beyond this entry predicate. Existing sparse raw-watch and dense occurrence conventions are unchanged. No runtime win or full semantic trajectory closure is claimed.',
    tests='New actual-kernel fixtures cover pure seeds/public N1, all-filtered no fresh restart, both occurrence caps and grown caps, front/tail holes/order/slots, duplicates/invalid identities, fixed/retired seeds, sparse/dense large occurrences and same-call representative crossing its entry cap. Existing cycle fixture updated only after proving all surviving seeds isolated/retired. Old actual 5473 objects reproduce an attempted pure retained seed.',
    failed_expectations='Initial full suites fail the old resumed>=1 assertion after the fixture leaves only an isolated representative. Intermediate new-test expectations incorrectly assumed same-call bridges disappeared and sparse large watches were one word; focused observation corrected those tests without further solver changes. No soundness/checker/model failure observed.',
    status='Isolated UNFUZZED candidate on5473. Existing Claude campaign5492 covers5473 only. Not promoted; B28+C5 and Rank1-40 runtime validation required.',
    files={name: sha(source / name) for name in files},
    binaries={directory + '/tactix': sha(root / directory / 'tactix') for directory in builds.values()},
    patches={p.name: sha(p) for p in campaign.glob('*.patch')},
    logs={p.name: sha(p) for p in (campaign / 'logs').iterdir()})
assert not (campaign / 'manifest.json').exists()
(campaign / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest['binaries'], indent=2))

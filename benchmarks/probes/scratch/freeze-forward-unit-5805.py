#!/usr/bin/env python3
"""Bind isolated continuation source, independent builds and fixed evidence."""
import difflib
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
campaign = root / 'benchmarks/runs/20260908-5798-forward-unit'
source = root / '.claude/worktrees/runtime-forward-unit-5798'
parent = root / '.claude/worktrees/runtime-reuse-target-5741'
builds = {arm: root / f'build/pin-{number}-forward-unit-{arm}'
          for arm, number in [('release',5798),('bc',5799),('trace',5800)]}
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
assert not (campaign / 'manifest.json').exists()
assert sha(builds['release'] / 'tactix') == sha(builds['bc'] / 'tactix')
for arm in builds:
    count = 92 if arm == 'trace' else 93
    assert f'100% tests passed out of {count}' in (campaign / f'logs/ctest-final-{arm}.log').read_text()
proofs = json.loads((campaign / 'fixed-proofs/summary.json').read_text())
native = json.loads((campaign / 'native-controls-2/summary.json').read_text())
assert proofs['checked_micro_proofs'] == 16 and proofs['binary_sha256'] == sha(builds['release'] / 'tests/test_forward_unit')
assert native['cases'] == 6 and native['exact_rows_equal']
for rel, digest in native['hashes'].items(): assert sha(root / rel) == digest, rel
files = ['src/rules/strengthen/strengthen.cpp','src/rules/strengthen/strengthen.hpp',
         'tests/test_strengthen.cpp','tests/test_forward_unit.cpp']
assert (source / 'tools').is_symlink() and (source / 'tools').resolve() == (parent / 'tools').resolve()
changed = [str(p.relative_to(source)) for folder in ('src','include','tests')
           for p in (source / folder).rglob('*') if p.is_file() and
           (not (parent / p.relative_to(source)).exists() or sha(p) != sha(parent / p.relative_to(source)))]
assert sorted(changed) == sorted(files), changed
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff(
        (base / name).read_text().splitlines(True) if (base / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (base / name).exists() else '/dev/null', tofile='b/' + name)) for name in names)
(campaign / 'repair.patch').write_text(patch(parent, files))
(campaign / 'cumulative.patch').write_text(patch(root, [str(p.relative_to(root))
    for p in sorted((root / 'src').rglob('*')) if p.is_file()]))
for name in files:
    target = campaign / 'source' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes((source / name).read_bytes())
manifest = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Continue canonical C2 after roots using its partial connected watch image; retain sparse debt, proof-safe roots, strict quota and single-action public surface.',
    test_consideration='93/93 independent Release and BC;92/92 trace;17 state/model regressions;8 DRAT+8 LRAT independent checks;six actual native/Tactix root/trail/step/completion rows equal. Existing interruption and N1 tests retained.',
    files={name:sha(source / name) for name in files},
    binaries={str((b / 'tactix').relative_to(root)):sha(b / 'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in campaign.glob('*.patch')},
    logs={p.name:sha(p) for p in (campaign / 'logs').iterdir()},
    validation={str(p.relative_to(root)):sha(p) for folder in ('fixed-proofs','native-controls-2')
        for p in (campaign / folder).iterdir() if p.suffix in ('.json','.log','.cnf','.proof')},
    setup_correction='First native adapter compile requested optional forward statistics absent in this pin; changed adapter to available aggregate counters,which contain only this pass in these fixtures. First compile log retained. No solver failure.',
    limitations='UNFUZZED investigation only, not promoted; B28+C5, Rank31 real-prefix closure, full runtime and current-source fuzz still pending. Additional connected-conflict/interruption branch coverage remains useful; fixed controls are not exhaustive.')
(campaign / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Frozen5798:93/93 twice,92/92 trace,16 checked micro-proofs,six exact native controls')

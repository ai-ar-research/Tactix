#!/usr/bin/env python3
"""Freeze the tested unused-census optimization and actual differential oracle."""
from pathlib import Path
import difflib
import hashlib
import json
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5546-vivify-census'
source = root / '.claude/worktrees/runtime-vivify-census-5546'
parent = root / '.claude/worktrees/runtime-sweep-small-5522'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
builds = {'release': 'build/pin-5546-vivify-census-release',
          'bc': 'build/pin-5547-vivify-census-bc'}
for arm in builds:
    assert '100% tests passed out of 87' in (c / 'logs' / f'ctest-{arm}.log').read_text()
assert sha(root / builds['release'] / 'tactix') == sha(root / builds['bc'] / 'tactix')
v = json.loads((c / 'state-validation.json').read_text())
assert v['cases'] == 192
for path, digest in v['hashes'].items(): assert sha(root / path) == digest
files = ['src/rules/vivify/vivify.cpp', 'tests/test_vivify_census.cpp']
def patch(before, names):
    return ''.join(''.join(difflib.unified_diff(
        (before / name).read_text().splitlines(True) if (before / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (before / name).exists() else '/dev/null',
        tofile='b/' + name)) for name in names)
for name, body in [('repair.patch', patch(parent, files)),
                   ('cumulative.patch', patch(root, [str(p.relative_to(root))
                     for p in sorted((root / 'src').rglob('*')) if p.is_file()]))]:
    assert not (c / name).exists()
    (c / name).write_text(body)
m = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Skip the global large-clause population census when a Vivify schedule has no irredundant candidate.',
    contract='Track actual pass-time candidate redundancy, not a policy scope restriction. Preserve both population counts whenever any irredundant candidate exists. All schedule/order/priority/flush/proof/model/API/budget behavior is unchanged; no new constant or kernel primitive.',
    evidence='benchmarks/runs/20260907-5425-rephase-root-profile/summary.json',
    evidence_sha256=sha(root / 'benchmarks/runs/20260907-5425-rephase-root-profile/summary.json'),
    references='Kissat vivify.c sort_vivification_candidates dispatches tier sorting without the irredundant population ratio; CaDiCaL vivify_round short-circuits the ratio for non-IRREDUNDANT tiers. Tactix previously scanned the whole arena even though all consumers select only irredundant ordering. Preserve mixed/public scopes through actual candidate membership.',
    tests='Two independent Release builds87/87, identical binary SHA.192 actual5522/5546 cases have byte-identical schedule/state/proof digests: six scopes,four populations,reuse/flush flags,DRAT/LRAT,priority/garbage/root-false,query purity and budget/unbounded resumption. Existing proof/model/N1 suites pass.',
    status='Isolated unmeasured/unpromoted successor to5522. Current Claude fuzz5539 covers parent5522, not this source. No runtime gain claimed.',
    files={name:sha(source / name) for name in files},
    binaries={directory + '/tactix':sha(root / directory / 'tactix') for directory in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    logs={p.name:sha(p) for p in (c / 'logs').iterdir()},
    state_validation_sha256=sha(c / 'state-validation.json'))
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(json.dumps(m['binaries'], indent=2))

#!/usr/bin/env python3
"""Freeze the tested complete root-reuse transition before SLURM submission."""
import difflib
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5718-root-conflict-reuse'
source=root/'.claude/worktrees/runtime-root-conflict-reuse-5718'
parent=root/'.claude/worktrees/runtime-assignment-level-5672'
builds={arm:root/f'build/pin-{n}-root-conflict-reuse-{arm}' for arm,n in [('release',5718),('trace',5719),('bc',5720)]}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert not (c/'manifest.json').exists()
assert sha(builds['release']/'tactix')==sha(builds['bc']/'tactix')
for arm in builds:
    expected=90 if arm=='trace' else 91
    assert f'100% tests passed out of {expected}' in (c/'logs'/f'ctest-r1-{arm}.log').read_text()
validation=c/'fixed-validation/summary.json'
v=json.loads(validation.read_text())
assert v['native_cases']==24 and v['all_native_states_equal']
assert v['predecessor_cases']==288 and len(v['micro_proofs'])==288
for rel,digest in v['hashes'].items():assert sha(root/rel)==digest
files=['src/rules/analyze/analyze.cpp','tests/test_root_conflict_reuse.cpp']
changed=[str(p.relative_to(source)) for folder in ('src','tests') for p in (source/folder).rglob('*')
         if p.is_file() and (not (parent/p.relative_to(source)).exists() or sha(p)!=sha(parent/p.relative_to(source)))]
assert sorted(changed)==sorted(files),changed
def patch(base,names):
    return ''.join(''.join(difflib.unified_diff(
        (base/name).read_text().splitlines(True) if (base/name).exists() else [],
        (source/name).read_text().splitlines(True),
        fromfile='a/'+name if (base/name).exists() else '/dev/null',tofile='b/'+name)) for name in names)
(c/'repair.patch').write_text(patch(parent,files))
(c/'cumulative.patch').write_text(patch(root,[str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))
m=dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Reuse one-top-literal conflicts even at true jump0,with chronological target,named DRAT/LRAT unit,root reason,deactivation and no learned/analyzed-score transaction. No tuned constants or per-instance behavior.',
    test_consideration='91/91 each independent Release,90/90 trace;288 root/binary/large/chrono/proof/H/API/disabled-reuse cases;actual predecessor controls and24 exact native transitions;288 independently checked DRAT/LRAT micro-proofs. Initial registry-vs-policy target expectation repaired without solver changes;original test/logs retained.',
    files={name:sha(source/name) for name in files},
    binaries={str((b/'tactix').relative_to(root)):sha(b/'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()},
    fixed_validation={str(p.relative_to(root)):sha(p) for p in (c/'fixed-validation').iterdir() if p.suffix in ('.json','.log')},
    initial_fixture={str(p.relative_to(root)):sha(p) for p in [c/'initial-fixture.cpp',c/'initial-fixture-expectation.json']},
    reference='Kissat analyze.c one_literal_on_conflict_level,inlineassign.h zero-level publication andlearn.c chronological target;CaDiCaL analyze.cpp forced conflict reuse andunit chain. Actual24-case native fixture calls kissat_analyze,not a copied algorithm.',
    motivation='5698 proves equal selected admissions but one extra increment generation. Partial5710 captures the same root-driving binary conflict182092;Tactix learns/bumps an inactive variable while native reuses.5710 timed out,so its full1M observer replay is not accepted. Fixed actual-native transition/proof tests independently establish the repair.',
    status='Isolated UNFUZZED successor to5672;5715 pending fuzz covers parent only. Require causal prefix,B28+C5,three timing waves andfull Rank1-40 before acceptance;no promotion.')
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('Frozen:91/91 twice,90/90 trace,24 native transitions and288 checked proofs')

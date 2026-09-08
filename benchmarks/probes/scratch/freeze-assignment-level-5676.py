#!/usr/bin/env python3
"""Bind the tested true-level optimization before any solver submission."""
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5672-assignment-level'
source = root / '.claude/worktrees/runtime-assignment-level-5672'
parent = root / '.claude/worktrees/runtime-propagation-cursor-repair-5647'
builds = {arm:root/f'build/pin-{n}-assignment-level-{arm}' for arm,n in [('release',5672),('trace',5673),('bc',5674)]}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert not (c / 'manifest.json').exists()
assert sha(builds['release']/'tactix') == sha(builds['bc']/'tactix')
old = (c/'predecessor-fixture/run.log').read_text()
assert '3288 assignment-level fixtures; state digest af23481afdb9aa83' in old
for arm,b in builds.items():
    expected = 89 if arm == 'trace' else 90
    assert f'100% tests passed out of {expected}' in (c/'logs'/f'ctest-{arm}.log').read_text()
    r = subprocess.run([b/'tests/test_assignment_level_bound'],check=True,capture_output=True,text=True)
    assert r.stdout == old
    (c/'logs'/f'fixture-{arm}.log').write_text(r.stdout)
files = ['src/kernel/kernel.hpp','tests/test_assignment_level_bound.cpp']
def patch(base,names):
    return ''.join(''.join(difflib.unified_diff(
        (base/name).read_text().splitlines(True) if (base/name).exists() else [],
        (source/name).read_text().splitlines(True),
        fromfile='a/'+name if (base/name).exists() else '/dev/null',tofile='b/'+name)) for name in names)
(c/'repair.patch').write_text(patch(parent,files))
(c/'cumulative.patch').write_text(patch(root,[str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))
for arm,b in [('previous',root/'build/pin-propagation-cursor-repair-5647-release'),('candidate',builds['release'])]:
    r = subprocess.run(['objdump','-d','-C','--disassemble=tactix::kernel_propagate(tactix::Kernel&, unsigned int)',b/'tactix'],check=True,capture_output=True,text=True)
    (c/'logs'/(arm+'-propagate.asm')).write_text(r.stdout)
m = dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Stop the exact antecedent-level maximum at its current-level upper bound;root maximum is zero. No scheduling,guards,proof/state/tick changes.',
    test_consideration='90/90 each independent Release,89/89 trace;3288 fixed root/high-current/lower-true-level and maximum-position cases. Actual predecessor-header/object fixture and all candidate builds share digest af23481afdb9aa83. Existing micro-proof,model,API,chrono and propagation suites cover unchanged semantics.',
    files={name:sha(source/name) for name in files},
    binaries={str((b/'tactix').relative_to(root)):sha(b/'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()},
    predecessor_fixture={str(p.relative_to(root)):sha(p) for p in (c/'predecessor-fixture').iterdir() if p.suffix in ('.json','.log')},
    source_baseline_sha256=sha(parent/'src/kernel/kernel.hpp'),
    reference='Kissat inlineassign.h and CaDiCaL propagate.cpp compute the full antecedent maximum. The upper-bound early exit preserves that exact result;kernel_check_invariants already enforces every assigned level<=current level.',
    motivation='Checked old5522 Rank39 annotation5671:maximum-level scan is5.86% of main-propagation sampled periods,approximately2.28% of all samples. Old profile prioritizes this hypothesis,not a current gain claim.',
    status='Isolated UNFUZZED successor to5649;5657 pending fuzz covers parent only. Require exact replay,paired cost,B28+C5 and full Rank1-40 before acceptance;no promotion.')
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('PASS:90/90 twice,89/89 trace;3288 actual old/new states identical')

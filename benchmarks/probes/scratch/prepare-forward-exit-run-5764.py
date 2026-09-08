#!/usr/bin/env python3
"""Freeze the diagnostic-only observer and prepare its exact-parent replay."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
runs=root / 'benchmarks/runs'
authority=runs / '20260908-5760-forward-exit-observer'
source=root / '.claude/worktrees/runtime-forward-exit-observer-5760'
parent=root / '.claude/worktrees/runtime-reuse-target-5741'
build=root / 'build/pin-5760-forward-exit-trace'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''):h.update(block)
    return h.hexdigest()
assert not (authority / 'manifest.json').exists()
assert '100% tests passed out of 91' in (authority / 'logs/ctest-r3.log').read_text()
control=json.loads((authority / 'fixed-control.json').read_text())
for rel,digest in control['hashes'].items():assert sha(root / rel)==digest
name='src/rules/strengthen/strengthen.cpp'
changed=[str(p.relative_to(source)) for p in (source / 'src').rglob('*')
    if p.is_file() and sha(p)!=sha(parent / p.relative_to(source))]
assert changed==[name],changed
(authority / 'observer.patch').write_text(''.join(difflib.unified_diff(
    (parent / name).read_text().splitlines(True),(source / name).read_text().splitlines(True),fromfile='a/'+name,tofile='b/'+name)))
am=dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),
    files={name:sha(source / name)},binaries={str((build / 'tactix').relative_to(root)):sha(build / 'tactix')},
    patches={'observer.patch':sha(authority / 'observer.patch')},
    logs={p.name:sha(p) for p in (authority / 'logs').iterdir()},
    fixed_control=dict(path=str((authority / 'fixed-control.json').relative_to(root)),sha256=sha(authority / 'fixed-control.json')),
    test_consideration='Actual-code two Tactix and two native unit/tail controls;all91 trace tests pass. New observer only emits under explicit target conflict and env;full parent trace/proof replay required.',
    fixture_setup='Initial native compile referenced an optional counter;use live tail garbage state. Initial native dense entry incorrectly saved/removed binary watches;correct to Eliminate null save-list. Original compile/control logs retained;no solver modification to obtain the witness.',
    status='Diagnostic-only;not a performance repair or accepted runtime pin.')
(authority / 'manifest.json').write_text(json.dumps(am,indent=2)+'\n')
old=runs / '20260908-5755-reuse-target-rank31-replay'
c=runs / '20260908-5764-rank31-forward-exit'
m=json.loads((old / 'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(old / rel)==digest
assert json.loads((old / 'summary.json').read_text())['candidate_parent_trace_and_proof_equal']
c.mkdir(exist_ok=False)
for folder in ('pins','logs','results','traces'):(c / folder).mkdir()
shutil.copy2(build / 'tactix',c / 'pins/candidate')
runner=(old / 'run.py').read_text().replace('tactix-5755-','tactix-5764-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor)==1
runner=runner.replace(anchor,"env['TACTIX_FORWARD_ROUND_TRACE'] = str(c / 'traces/candidate.forward-round.tsv')\nenv['SAT_FORWARD_ROUND_CONFLICT'] = '179823'\n"+anchor)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old),str(c)))
m.update(candidate_authority=str(authority.relative_to(root)),candidate_source=am['source'],
    parent_campaign=str(old.relative_to(root)),source_patch_sha256=sha(authority / 'observer.patch'),
    hypothesis='Determine whether the Rank31 Strengthen179823 exit is a unit-induced early stop,quota cut or complete pass. Actual-code tiny controls prove the general early-stop gap,not this instance cause.',
    validation='Clean300K stop;all ordinary trace/proof hashes equal5755;new selected round rows identify candidate count,spent allowance,unit clause and unreached tail.',
    test_consideration=am['test_consideration'])
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c / 'run.py',c / 'array.sbatch',c / 'pins/candidate']}
(c / 'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)

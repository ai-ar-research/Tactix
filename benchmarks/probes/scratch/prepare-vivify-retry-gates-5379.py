#!/usr/bin/env python3
"""Checked four-arm runtime screen and fixed B28+C5 gate for retry repair."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260907-5371-congruence-prefix-runtime'
runtime = runs / '20260907-5379-vivify-retry-runtime'
gate = runs / '20260907-5380-vivify-retry-BC-gate'
builds = {t: root / b / 'tactix' for t, b in [('B', 'build/pin-5379-vivify-retry-release'), ('C', 'build/pin-5380-vivify-retry-bc')]}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(builds['B']) == sha(builds['C'])
def init(c):
    c.mkdir(exist_ok=False)
    for d in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / d).mkdir()
def finish(c, m):
    m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)
init(runtime)
patch = ''
for before in sorted((root / 'src').rglob('*')):
    if not before.is_file(): continue
    rel = before.relative_to(root)
    after = root / '.claude/worktrees/runtime-vivify-retry-5378' / rel
    patch += ''.join(difflib.unified_diff(before.read_text().splitlines(True), after.read_text().splitlines(True), fromfile='a/' + str(rel), tofile='b/' + str(rel)))
(runtime / 'pins/candidate.patch').write_text(patch)
shutil.copy2(old / 'pins/candidate.patch', runtime / 'pins/previous.patch')
m = json.loads((old / 'manifest.json').read_text()); m.pop('slurm_job_id', None)
for name, binary in [('current', old / 'pins/current'), ('previous', old / 'pins/candidate'), ('candidate', builds['B']), ('kissat', old / 'pins/kissat')]:
    shutil.copy2(binary, runtime / 'pins' / name)
    m['arms'][name]['sha256'] = sha(binary)
    if name in ('previous', 'candidate'): m['arms'][name]['source_patch_sha256'] = sha(runtime / 'pins' / (name + '.patch'))
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'): shutil.copy2(old / 'pins' / name, runtime / 'pins' / name)
shutil.copy2(old / 'matrix.tsv', runtime / 'matrix.tsv')
(runtime / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(runtime)).replace('5371-congruence-prefix', '5379-vivify-retry'))
m.update(parent_campaign=str(old.relative_to(root)), hypothesis='Native learned-large Vivify retry advice closes the BVE frontier; measure full-solve effects against exact preceding candidate and production.',
         candidate_validation='5378 fixed native-positive/old-negative/new-positive, 76 serial trace CTests, all 2107 BVE attempts and 786753 decisions match through 345034.',
         decision='Require 12 independently checked clean verdicts; one diagnostic wave only. No promotion or global claim; UNFUZZED.')
finish(runtime, m)
init(gate)
oldgate = runs / '20260907-5372-congruence-prefix-BC-gate'
m = json.loads((oldgate / 'manifest.json').read_text()); m.pop('slurm_job_id', None)
for tier in ('B', 'C'):
    for name, binary in [('current', runtime / 'pins/current'), ('candidate', builds[tier])]:
        arm = name + '_' + tier
        shutil.copy2(binary, gate / 'pins' / arm)
        m['arms'][arm]['sha256'] = sha(binary)
        if name == 'candidate':
            m['arms'][arm].update(source_build=str(binary.parent.relative_to(root)), source_patch_sha256=sha(runtime / 'pins/candidate.patch'))
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py', 'candidate.patch'): shutil.copy2(runtime / 'pins' / name, gate / 'pins' / name)
shutil.copy2(oldgate / 'matrix.tsv', gate / 'matrix.tsv')
(gate / 'array.sbatch').write_text((oldgate / 'array.sbatch').read_text().replace(str(oldgate), str(gate)).replace('5372-congruence-prefix', '5380-vivify-retry'))
m.update(predecessor=str(oldgate.relative_to(root)), purpose='Complete checked B28+C5 gate for isolated cumulative Vivify retry repair against production.',
         hypothesis='Path-specific native retry advice preserves checked solves; quantify regressions.',
         build_identity='Two fresh independent Release builds from immutable 5378 source, equal hashes; no event tracing/conflict stops.')
finish(gate, m)

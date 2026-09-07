#!/usr/bin/env python3
"""Freeze symmetric read-only retry-mark producer observations."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5376-bve-history'
new = root / 'benchmarks/runs/20260907-5377-retry-producers'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
for arm, src in [('tactix', 'build/pin-5377-retry-producers/tactix'), ('kissat', 'tools/solvers/variants/kissat-retry-producers-5377/build/kissat')]:
    shutil.copy2(root / src, new / 'pins' / arm)
for p in old.glob('*.patch'):
    shutil.copy2(p, new / p.name)
for arm, before, after, files in [
    ('tactix', '.claude/worktrees/runtime-congruence-prefix-5369', '.claude/worktrees/runtime-retry-producers-5377', ['src/diagnostics/event_trace.cpp']),
    ('kissat', 'tools/solvers/variants/kissat-queue-bumps-5361', 'tools/solvers/variants/kissat-retry-producers-5377', ['src/inline.h', 'src/eventtrace.h', 'src/eventtrace.c'])]:
    patch = b''
    for file in files:
        r = subprocess.run(['diff', '-u', '--label', 'a/' + file, '--label', 'b/' + file, str(root / before / file), str(root / after / file)], capture_output=True)
        assert r.returncode == 1
        patch += r.stdout
    (new / (arm + '-retry-producers.patch')).write_bytes(patch)
source = (old / 'run.py').read_text()
anchor = 'env.update({prefix + "_DECISION_STREAM_TRACE"'
observer = '''env[prefix + "_RETRY_PRODUCER_TRACE"] = str(C/"traces"/(arm+".retry.tsv"))
env["SAT_RETRY_PRODUCER_FROM"] = "240040"
env["SAT_RETRY_PRODUCER_TO"] = "343785"
env["SAT_RETRY_PRODUCER_EXTERNAL"] = "10700"
'''
assert source.count(anchor) == 1
source = source.replace("'SAT_VIVIFY_'", "'SAT_VIVIFY_', 'SAT_RETRY_'")
(new / 'run.py').write_text(source.replace(anchor, observer + anchor))
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
m = json.loads((old / 'manifest.json').read_text())
m.update(purpose='Identify selected retry-mark producer between last shared attempt and missing initial schedule member.',
         hypothesis='A mark producer differs after BVE at 240040; identify its caller before selecting any general repair.',
         acceptance='Both clean zero schedule stops. Decisions, selected BVE history and initial schedules match 5376 byte for byte. Compare selected retry producer records.',
         parent_campaign=str(old.relative_to(root)),
         source_note='Fresh source-only copies plus read-only scalar producer journals. No semantic edit. Native initial observer compilation referenced two nonexistent mode fields; removed before execution.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new / 'pins').iterdir(), *new.glob('*.patch'), new / 'run.py', new / 'array.sbatch']}
(new / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)

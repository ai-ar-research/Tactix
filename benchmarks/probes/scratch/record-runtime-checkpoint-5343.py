#!/usr/bin/env python3
"""Record terminal normal diagnostics without editing their frozen inputs."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
RUNS = ROOT/'benchmarks/runs'

def save(path, content):
    if path.exists():
        assert path.read_text() == content, path
    else:
        path.write_text(content)

for campaign, script in (
    ('20260907-5314-pose-r2-ranks1-40','aggregate.py'),
    ('20260907-5342-deact-trace','compare.py')):
    c = RUNS/campaign
    result = subprocess.run(['python3',c/script],check=True,text=True,capture_output=True)
    save(c/'summary.json',result.stdout)

c = RUNS/'20260907-5314-pose-r2-ranks1-40'
result = subprocess.run(['python3',ROOT/'tools/bench/runtime_summary.py',c],check=True,text=True,capture_output=True)
save(c/'runtime-summary.tsv',result.stdout)

c = RUNS/'20260907-5338-duplicate-fixture'
c.mkdir(exist_ok=True)
src = ROOT/'benchmarks/probes/scratch'
ksrc = ROOT/'tools/solvers/variants/kissat-root-all-5321'
tsrc = ROOT/'.claude/worktrees/runtime-sequential-trace-5325'
tbuild = ROOT/'build/pin-5325-sequential-trace'
objects = sorted((tbuild/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
commands = [
    ['gcc','-O2','-DNDEBUG','-I',str(ksrc/'src'),'-I',str(ksrc/'build'),str(src/'sweep-duplicate-reference-5338.c'),str(ksrc/'build/libkissat.a'),'-lm','-o',str(src/'sweep-duplicate-reference-5338')],
    ['c++','-std=c++17','-O2','-DNDEBUG','-DTACTIX_ENABLE_EVENT_TRACE','-I'+str(tsrc/'src'),'-I'+str(tsrc/'include'),str(src/'sweep-duplicate-tactix-5338.cpp'),*map(str,objects),'-o',str(src/'sweep-duplicate-tactix-5338')],
]
outputs = []
for executable, args in (('sweep-duplicate-reference-5338',[]),('sweep-duplicate-tactix-5338',[]),('sweep-duplicate-tactix-5338',['sequential'])):
    result = subprocess.run([src/executable,*args],check=True,text=True,capture_output=True)
    outputs.append(dict(command=[str((src/executable).relative_to(ROOT)),*args],exit_code=result.returncode,stdout=result.stdout))
inputs = [src/'sweep-duplicate-reference-5338.c',src/'sweep-duplicate-tactix-5338.cpp',ksrc/'src/sweep.c',ksrc/'build/libkissat.a',*objects]
save(c/'manifest.json',json.dumps(dict(
    purpose='Fixed entailed-equivalence mechanism replay; native Kissat Sweep implementation, not a mirror. No benchmark execution or performance claim.',
    tactix_source='Exact immutable 5325 sequential-trace source and objects',
    kissat_source='Exact immutable 5321 source; fixture includes its sweep.c directly and links libkissat.a',
    compile_commands=commands,
    input_sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}),indent=2)+'\n')
save(c/'summary.json',json.dumps(dict(outputs=outputs,conclusion='Native reference and sequential Tactix each retain two copies; bulk retains one. Do not deduplicate to erase the Rank-21 delta. This fixed fixture does not establish full-instance transaction equality or timing causality.'),indent=2)+'\n')

for campaign, job in (('20260907-5339-warmup-heap','1668682'),('20260907-5340-warmup-implications','1668684')):
    c = RUNS/campaign
    summary = dict(job=job,stopped_clean=2,rows={},trace_sha256={})
    for task,arm in enumerate(('tactix','kissat')):
        log = (c/'logs'/f'{job}_{task}.log').read_text()
        assert f'{arm} exit {0 if arm == "tactix" else -15}' in log and 'scratch_deleted True' in log
        raw = (c/'traces'/f'{arm}.tsv').read_bytes()
        assert b'trace_truncated' not in raw and b'trace_limit' not in raw
        summary['rows'][arm] = sum(not l.startswith(b'#') for l in raw.splitlines())
        summary['trace_sha256'][arm] = hashlib.sha256(raw).hexdigest()
    if '5339' in campaign:
        summary['limitation'] = 'Kissat heap_pop observer also records non-SCORES heaps; do not compare every pop as a decision-heap operation. Superseded by explicit inheap deactivation stream 5342.'
    else:
        summary['limitation'] = 'Kissat warmup.c omitted implication observer calls. Incomplete observer coverage, not a solver failure; corrected by 5341, whose decisions match this run.'
    save(c/'summary.json',json.dumps(summary,indent=2)+'\n')
print('Recorded 5314, 5338, 5339, 5340, 5342 terminal summaries.')

#!/usr/bin/env python3
"""Four-conflict producer windows on unchanged pins; one solver per task."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),
    '--deadline','2026-09-10T06:00:00-04:00'],check=True)
old = root/'benchmarks/runs/20260909-6798-rank21-queue-bumps'
c = root/'benchmarks/runs/20260910-6804-rank21-analysis-admission'
assert not c.exists()
accepted = old/'summary-6800.json'
s = json.loads(accepted.read_text())
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
grouped = old/'groups-6803.json'
g = json.loads(grouped.read_text())
for rel,digest in g['hashes'].items(): assert sha(root/rel)==digest,rel
assert [d['clock'][0] for d in g['differences']] == [23987,23988,24045,24060,25095,25096]
m = json.loads((old/'manifest.json').read_text())
deps = dict(m['dependencies']); deps.update(s['hashes']); deps.update(g['hashes'])
for p in (accepted,grouped,Path(__file__),
    root/'.claude/worktrees/runtime-reduce-watermark-6730/src/diagnostics/event_trace.cpp',
    root/'tools/solvers/variants/kissat-macro-heap-6090/src/eventtrace.c'):
    deps[str(p.relative_to(root))] = sha(p)
windows = dict(early=[23986,23989],late=[25094,25097])
for name,window in windows.items():
    sub = c/name
    for folder in ('pins','logs','results','traces'): (sub/folder).mkdir(parents=True)
    for pin in ('candidate','kissat','certificate_prefix.py'): shutil.copy2(old/'pins'/pin,sub/'pins'/pin)
    runner = (old/'run.py').read_text().replace('root = c.parents[2]','root = c.parents[3]').replace('tactix-6798-', 'tactix-6804-'+name+'-')
    anchor = '\ndef interrupted(signum, frame):'
    assert runner.count(anchor)==1
    addition = """
env.update({prefix + '_EVENT_TRACE': str(c/'traces'/(arm+'.search.tsv')),
    prefix + '_EVENT_TRACE_STOP_CONFLICT': str((1 << 64) - 2),
    prefix + '_EVENT_TRACE_CONTROL_ONLY': '1',
    prefix + '_EVENT_TRACE_CONFLICT_FROM': str(m['search_window'][0]),
    prefix + '_EVENT_TRACE_CONFLICT_TO': str(m['search_window'][1]),
    prefix + '_EVENT_TRACE_SKIP_WATCH': '1',
    prefix + '_EVENT_TRACE_SKIP_IMPLY': '1',
    prefix + '_EVENT_TRACE_SKIP_HEAP': '1',
    prefix + '_EVENT_TRACE_SKIP_RANDEC_DRAWS': '1',
    prefix + '_EVENT_TRACE_LIMIT': str(m['trace_row_cap'])})
"""
    runner = runner.replace(anchor,addition+anchor)
    compile(runner,str(sub/'run.py'),'exec')
    (sub/'run.py').write_text(runner)
    sm = dict(m)
    sm.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),dependencies=deps,
        cached_controls={arm:str(old.relative_to(root)) for arm in ('candidate','kissat')},
        search_window=window,trace_row_cap=250000,
        purpose='Distinguish conflict-resolution membership, reason-side gate, and inherited reasons at the early substitution and late five-variable omission.',
        hypothesis='Existing CONTROL_ONLY admits analysis marks/resolutions/reasons and analyzed membership while excluding broad Sweep records. Four-conflict windows bound full post-analysis snapshots. The original50K prefix remains the only reachable stop.',
        validation='Both clean exit0; full6798 decision/prefix/state/bump traces and partial proof exact; old native10K prefix exact. Only source-backed macro additions and native analysis/end window additions allowed. New search rows uncapped, required analysis and learned events in all four conflicts; inspect raw comparisons before causality.',
        test_consideration='No solver or hook edit. Six new bump-group tests plus existing search-window/macro/boundary/state/queue tests. Complete6798 replay is the integration oracle;6757/6774 failures remain frozen.',
        limitation='Diagnostic only; no fresh reference scoring, policy changes, source promotion, fuzz access, second reachable stop or longer solver budget.')
    sm['hashes'] = {rel:sha(sub/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py')}
    with (sub/'manifest.json').open('x') as stream: json.dump(sm,stream,indent=2);stream.write('\n')
    for p in (sub/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
(c/'logs').mkdir()
array = (old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6798','6804').replace('--array=0-1','--array=0-3')
tail = 'exec python3 '+str(c)+'/run.py "$SLURM_ARRAY_TASK_ID"'
assert array.count(tail)==1
array = array.replace(tail, 'case "$SLURM_ARRAY_TASK_ID" in\n  0|1) window=early; arm="$SLURM_ARRAY_TASK_ID" ;;\n  2|3) window=late; arm="$((SLURM_ARRAY_TASK_ID - 2))" ;;\nesac\nexec python3 '+str(c)+'/$window/run.py "$arm"')
(c/'array.sbatch').write_text(array)
manifest = dict(windows=windows,dependencies=deps,protected_campaign_hashes=m['protected_campaign_hashes'],
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    resources=dict(partition='cpu-dense-hwulab-q',logical_cpus=12,memory_mib=48000,solver_seconds=1200,allocation_minutes=25,tasks=4,throttle=None,node_constraint=None),
    hashes={str(p.relative_to(c)):sha(p) for p in [c/'array.sbatch']+
        [q for name in windows for q in (c/name/'manifest.json',c/name/'run.py')]+[
        c/name/'pins'/pin for name in windows for pin in ('candidate','kissat','certificate_prefix.py')]})
with (c/'manifest.json').open('x') as stream:json.dump(manifest,stream,indent=2);stream.write('\n')
print(c)

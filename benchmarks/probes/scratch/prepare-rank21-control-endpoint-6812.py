#!/usr/bin/env python3
"""Prepare a corrected distinct campaign only if original strict replay failed."""
import datetime as dt
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from bounded_trace_config import control_window,validate_trace_keys
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),
    '--deadline','2026-09-10T06:00:00-04:00'],check=True)
old=root/'benchmarks/runs/20260910-6804-rank21-analysis-admission'
c=root/'benchmarks/runs/20260910-6812-rank21-control-endpoint'
assert not c.exists()
# Never replace a live observation or rerun an accepted pair merely for speed.
launch=json.loads((old/'launch.json').read_text())
command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],
    '-o','JobID,State%40,ExitCode,Submit','-P']
accounting=subprocess.check_output(command,text=True)
rows=[r.split('|') for r in accounting.splitlines()]
assert len(rows)==4 and all(r[1] in ('COMPLETED','FAILED','TIMEOUT') and r[3]>=launch['accounting_since'] for r in rows),accounting
outcomes=[json.loads(p.read_text()) for p in sorted(old.glob('*/results/*.json'))]
assert len(outcomes)==4 and all(r['scratch_deleted'] and r['exit_code'] in (0,124) for r in outcomes)
assert any(r['exit_code']==124 for r in outcomes), 'No classified timeout; assess strict replay before requesting any rerun'
classification=old/'configuration-6810.json';s=json.loads(classification.read_text())
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
m=json.loads((old/'manifest.json').read_text())
failed_windows={window:scope for window,scope in m['windows'].items()
    if any(json.loads(p.read_text())['exit_code']==124 for p in (old/window/'results').glob('*.json'))}
assert failed_windows
assert all(not (old/window/'summary-6806.json').exists() for window in failed_windows)
m['windows']=failed_windows
deps=dict(m['dependencies']);deps.update(s['hashes'])
for p in (classification,Path(__file__),root/'tools/bench/bounded_trace_config.py',root/'tools/bench/test_bounded_trace_config.py'):
    deps[str(p.relative_to(root))]=sha(p)
sources=dict(TACTIX=root/'.claude/worktrees/runtime-reduce-watermark-6730/src/diagnostics/event_trace.cpp',
    KISSAT=root/'tools/solvers/variants/kissat-macro-heap-6090/src/eventtrace.c')
for window,scope in m['windows'].items():
    sub=c/window;parent=old/window;sm=json.loads((parent/'manifest.json').read_text())
    runner=(parent/'run.py').read_text()
    wrong="prefix + '_EVENT_TRACE_STOP_CONFLICT'"
    assert runner.count(wrong)==1
    runner=runner.replace(wrong,"prefix + '_EVENT_TRACE_STOP_AFTER_CONFLICT'").replace('tactix-6804-','tactix-6812-')
    preflight={}
    for prefix,arm in (('TACTIX','candidate'),('KISSAT','kissat')):
        source=sources[prefix].read_text()
        expected=control_window(prefix,str(sub/'traces'/(arm+'.search.tsv')),tuple(scope),sm['stop_conflict'],sm['trace_row_cap'],source)
        actual_keys={prefix+suffix for suffix in re.findall(r"prefix \+ '(_EVENT_TRACE[^']*)'",runner)}
        assert actual_keys==set(expected),(actual_keys,expected)
        preflight[arm]=dict(environment=expected,source_sha256=sha(sources[prefix]),
            recognized_keys=validate_trace_keys(expected,prefix,source))
    compile(runner,str(sub/'run.py'),'exec')
    for folder in ('pins','logs','results','traces'):(sub/folder).mkdir(parents=True)
    for name in ('candidate','kissat','certificate_prefix.py'):shutil.copy2(parent/'pins'/name,sub/'pins'/name)
    (sub/'run.py').write_text(runner)
    sm.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),dependencies=deps,
        configuration_correction=str(classification.relative_to(root)),trace_preflight=preflight,
        purpose='Correct the source-proven unknown environment key in6804, preserving the original6798 replay contract and all solver settings.',
        test_consideration='Seven bounded_trace_config tests plus actual pinned-source key preflight; generated runner event-key set exactly equals the tested builder output. Prior232 tests remain. No solver/hook edit.',
        limitation='Distinct successor after a terminal classified timeout only; never replace a live job or accepted pair. No source promotion, reference timing refresh, solver budget increase, fuzz access or second reachable stop.')
    sm['hashes']={rel:sha(sub/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py')}
    with (sub/'manifest.json').open('x') as stream:json.dump(sm,stream,indent=2);stream.write('\n')
    for p in (sub/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
(c/'logs').mkdir()
array=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6804-rank21','6812-rank21')
array=array.replace('--array=0-3','--array=0-'+str(2*len(failed_windows)-1))
begin=array.index('case "$SLURM_ARRAY_TASK_ID" in\n')
end=array.index('esac\n',begin)+len('esac\n')
cases='case "$SLURM_ARRAY_TASK_ID" in\n'
for i,window in enumerate(failed_windows):
    cases+='  '+str(2*i)+'|'+str(2*i+1)+') window='+window+'; arm="$((SLURM_ARRAY_TASK_ID - '+str(2*i)+'))" ;;\n'
array=array[:begin]+cases+'esac\n'+array[end:]
(c/'array.sbatch').write_text(array)
m['resources']=dict(m['resources'],tasks=2*len(failed_windows))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),dependencies=deps,
    original_accounting=dict(command=command,stdout=accounting),
    configuration_correction=str(classification.relative_to(root)))
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c/'array.sbatch']+
    [q for window in m['windows'] for q in (c/window/'manifest.json',c/window/'run.py')]+
    [c/window/'pins'/pin for window in m['windows'] for pin in ('candidate','kissat','certificate_prefix.py')]}
with (c/'manifest.json').open('x') as stream:json.dump(m,stream,indent=2);stream.write('\n')
print(c)

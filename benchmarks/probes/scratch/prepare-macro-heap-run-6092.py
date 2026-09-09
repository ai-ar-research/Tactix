#!/usr/bin/env python3
"""Pin tested whole-macro observers; retain every parent replay obligation."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
parent=root/'benchmarks/runs/20260908-6076-root-lock-rank37-choice'
control=root/'benchmarks/runs/20260908-6090-macro-heap-controls'
c=root/'benchmarks/runs/20260908-6092-rank37-macro-heaps'
assert not c.exists()
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()
assert '100% tests passed' in (control/'logs/tactix-ctest.log').read_text()
test = subprocess.run([str(control/'test-writer')], capture_output=True, text=True, check=True)
test_log = control/'logs/writer-test.log'; assert not test_log.exists()
test_log.write_text(test.stdout + test.stderr)
prior=root/'benchmarks/reports/2026-09-08-rank37-phase-heaps.json'
s=json.loads(prior.read_text())
assert s['full_parent_replay_except_selected_heap']
assert not any(s['stages']['prephase']['change_counts'].values())
assert s['stages']['postphase']['change_counts']['position']==298
for rel,d in s['hashes'].items():assert sha(root/rel)==d,rel
m=json.loads((parent/'manifest.json').read_text())
for rel,d in m['hashes'].items():assert sha(parent/rel)==d,rel
for name in ('pins','logs','traces','results'):(c/name).mkdir(parents=True)
binaries={'candidate':root/'build/pin-6090-macro-heap-trace/tactix',
          'kissat':root/'tools/solvers/variants/kissat-macro-heap-6090/build/kissat'}
for arm,p in binaries.items():
    shutil.copy2(p,c/'pins'/arm);(c/'pins'/arm).chmod(0o555)
runner=(parent/'run.py').read_text().replace('tactix-6076-','tactix-6092-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor)==1
runner=runner.replace(anchor,"env.update(SAT_MACRO_HEAP_PATH=str(c/'traces'/(arm+'.macro-heaps.tsv')), SAT_MACRO_HEAP_CONFLICT='272979')\n"+anchor)
compile(runner,str(c/'run.py'),'exec');(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)).replace('6076-rank37-choice','6092-rank37-macro-heaps'))
sources={'candidate':root/'.claude/worktrees/runtime-macro-heap-6090',
         'kissat':root/'tools/solvers/variants/kissat-macro-heap-6090'}
changed={'candidate':['src/diagnostics/event_trace.cpp','src/diagnostics/macro-heap-6090.h','tests/test_event_trace.cpp'],
         'kissat':['src/eventtrace.c','src/macro-heap-6090.h']}
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
         parent_campaign=str(parent.relative_to(root)),parent_summary_sha256=sha(parent/'choice-validation.json'),
         bracket_authority=dict(path=str(prior.relative_to(root)),sha256=sha(prior)),
         observer_sources={a:str(p.relative_to(root)) for a,p in sources.items()},
         observer_source_hashes={str((sources[a]/rel).relative_to(root)):sha(sources[a]/rel) for a in sources for rel in changed[a]},
         hypothesis='Complete incoming heap identical;298 positions differ after conflict/backtrack/two inprocessing rounds/first next decision. Capture each macro heap at272979 to locate first differing transition before any solver repair.',
         validation='Every ordinary1M stream,prior heap,choice,all auxiliary traces and partial proof bytes must replay6076 despite changed observer binaries. No global EventTrace enabled. Normalize only known rule-boundary names;retain unmatched sites.',
         native_rerun_reason='Missing complete macro heap transitions;not a fixed timing refresh.',
         test_consideration='Shared writer C fixture tests disabled/malformed/overflow/mismatched selectors,empty/repeated frames,exact rows and no mutation. Actual Tactix macro fixture covers integration without global trace;complete trace suite required. Full real-instance replay validates both adapters.',
         control_logs={str(p.relative_to(root)):sha(p) for p in (control/'logs').glob('*.log')})
m['writer_test_sources']={str(p.relative_to(root)):sha(p) for p in
    (Path(__file__).with_name('macro-heap-6090.h'),Path(__file__).with_name('test-macro-heap-6090.c'))}
m['hashes']={name:sha(c/name) for name in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(c)

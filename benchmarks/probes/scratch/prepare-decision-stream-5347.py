#!/usr/bin/env python3
"""Standalone ordered decisions with the existing cumulative-prefix endpoint."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5343-round-retirement'
new = root/'benchmarks/runs/20260907-5347-decision-stream'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):
    (new/name).mkdir()
for arm,binary in (
    ('tactix',root/'build/pin-5347-decision-stream/tactix'),
    ('kissat',root/'tools/solvers/variants/kissat-decision-stream-5347/build/kissat')):
    shutil.copy2(binary,new/'pins'/arm)
for p in old.glob('*.patch'):
    shutil.copy2(p,new/p.name)
source = (old/'run.py').read_text()
start = source.index('env[prefix + "_DEACT_TRACE"]')
end = source.index('\ntry:',start)
source = source[:start]+'''env.update({prefix + "_DECISION_STREAM_TRACE": str(C/"traces"/(arm+".decisions.tsv")),
            prefix + "_DECISION_PREFIX_TRACE": str(C/"traces"/(arm+".prefix.tsv")),
            "SAT_DECISION_PREFIX_CONFLICT": "200000",
            "SAT_DECISION_PREFIX_COMMON": "1",
            "SAT_DECISION_PREFIX_STOP": "1"})'''+source[end:]
source = source.replace("'SAT_EVENT_', 'SAT_CORE_'","'SAT_EVENT_', 'SAT_CORE_', 'SAT_DECISION_'")
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
for arm,before,after,rel in (
    ('tactix','.claude/worktrees/runtime-round-retirement-5343','.claude/worktrees/runtime-decision-stream-5347','src/diagnostics/event_trace.cpp'),
    ('kissat','tools/solvers/variants/kissat-deact-trace-5342','tools/solvers/variants/kissat-decision-stream-5347','src/eventtrace.c')):
    result = subprocess.run(['diff','-u','--label','a/'+rel,'--label','b/'+rel,root/before/rel,root/after/rel],text=True,capture_output=True)
    assert result.returncode == 1
    (new/(arm+'-decision-stream.patch')).write_text(result.stdout)
m = json.loads((old/'manifest.json').read_text())
m.pop('job',None)
m.update(purpose='UNFUZZED standalone decision frontier; avoid global trace payload and snapshot overhead in 5346.',
    hypothesis='Record the first post-repair ordered decision mismatch with constant-size scalar rows and no global EventTrace output.',
    acceptance='Both exit zero at cumulative-prefix checkpoint 200001 with selected target 200000; verify prefix count equals full stream length, unchanged 5343 warm-up decisions, and clean scratch. Not a proof-verdict or timing gate.',
    source_note='Exact 5343 Tactix and 5342 Kissat sources plus standalone scalar logger; fresh builds. Kissat rebuilt with default -O3 -DNDEBUG, not QUIET.',
    parent_campaign=str(old.relative_to(root)))
m['hashes'] = {str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)

#!/usr/bin/env python3
"""Preserve the submitted typo and source-backed correction; no live trace claim."""
import datetime as dt
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from bounded_trace_config import control_window,validate_trace_keys
c=root/'benchmarks/runs/20260910-6804-rank21-analysis-admission'
target=c/'configuration-6810.json'
assert not target.exists()
m=json.loads((c/'manifest.json').read_text())
hashes={}
def bind(p,expected=None):
    digest=sha(p);assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
for rel,digest in m['hashes'].items():bind(c/rel,digest)
for rel,digest in m['dependencies'].items():bind(root/rel,digest)
for name in ('manifest.json','launch.json'):bind(c/name)
sources=dict(TACTIX=root/'.claude/worktrees/runtime-reduce-watermark-6730/src/diagnostics/event_trace.cpp',
    KISSAT=root/'tools/solvers/variants/kissat-macro-heap-6090/src/eventtrace.c')
checks={}
for prefix,p in sources.items():
    bind(p); source=p.read_text()
    corrected=control_window(prefix,'trace.tsv',(23986,23989),50000,250000,source)
    bad=dict(corrected);bad[prefix+'_EVENT_TRACE_STOP_CONFLICT']=bad.pop(prefix+'_EVENT_TRACE_STOP_AFTER_CONFLICT')
    try:validate_trace_keys(bad,prefix,source)
    except ValueError as error:checks[prefix]=dict(rejected_typo=str(error),corrected=corrected)
    else:raise AssertionError('Misspelled trace setting was accepted')
for window in m['windows']:
    runner=(c/window/'run.py').read_text()
    assert "prefix + '_EVENT_TRACE_STOP_CONFLICT'" in runner
    assert "prefix + '_EVENT_TRACE_STOP_AFTER_CONFLICT'" not in runner
for p in (Path(__file__),root/'tools/bench/bounded_trace_config.py',root/'tools/bench/test_bounded_trace_config.py'):bind(p)
out=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,checks=checks,
    classification='Submitted6804 uses EVENT_TRACE_STOP_CONFLICT, but both pinned sources read EVENT_TRACE_STOP_AFTER_CONFLICT. The unknown setting leaves the event endpoint disabled at UINT64_MAX, so intended finite-mode snapshot/heap suppression is inactive. Prefix50K remains the sole actual stop.',
    correction='Supersedes6804 manifest and0004 report statements that the unreachable finite endpoint was active. Preserve all original immutable artifacts. Build a distinct successor with source-validated keys and the same solver budget, pins, windows and original6798 replay controls.',
    limitation='Static configuration defect, not a solver soundness finding. Existing job1678836 remains independently owned; no terminal result, timeout, observer replay or new causal interpretation is asserted here. Do not rerun or mutate the submitted campaign.',
    test_consideration='Seven new tests cover both solver names, exact finite endpoint, misspelled key rejection, actual getenv call recognition, missing source keys, invalid window/path/cap and exclusion of conflicting modes. Actual pinned sources pass the corrected-key preflight.')
with target.open('x') as stream:json.dump(out,stream,indent=2);stream.write('\n')
print(out['classification'])

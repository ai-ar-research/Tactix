#!/usr/bin/env python3
"""Bracket same-conflict inprocessing with full incoming/outgoing heap states."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
parent=root/'benchmarks/runs/20260908-6076-root-lock-rank37-choice'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(8<<20),b''):h.update(block)
    return h.hexdigest()
history=root/'benchmarks/runs/20260908-6081-rank37-position-history/summary.json'
v=json.loads(history.read_text())
assert v['parent_full_replay'] and v['first_position_difference'] is None
for number,stage,conflict,decision in ((6084,'prephase',272978,76952961),(6085,'postphase',272979,76952962)):
    c=root/f'benchmarks/runs/20260908-{number}-rank37-{stage}-heap';assert not c.exists()
    m=json.loads((parent/'manifest.json').read_text())
    for rel,digest in m['hashes'].items():assert sha(parent/rel)==digest,rel
    for name in ('pins','logs','traces','results'):(c/name).mkdir(parents=True)
    for arm in ('candidate','kissat'):
        shutil.copy2(parent/'pins'/arm,c/'pins'/arm);(c/'pins'/arm).chmod(0o555)
    runner=(parent/'run.py').read_text().replace('tactix-6076-',f'tactix-{number}-')
    old="SAT_EVENT_TRACE_HEAP_SET_CONFLICT='272979', SAT_EVENT_TRACE_HEAP_SET_DECISION='76953348'"
    assert runner.count(old)==1
    runner=runner.replace(old,f"SAT_EVENT_TRACE_HEAP_SET_CONFLICT='{conflict}', SAT_EVENT_TRACE_HEAP_SET_DECISION='{decision}'")
    compile(runner,str(c/'run.py'),'exec');(c/'run.py').write_text(runner)
    (c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)).replace('6076-rank37-choice',f'{number}-rank37-{stage}'))
    m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),parent_campaign=str(parent.relative_to(root)),
        parent_summary_sha256=sha(parent/'choice-validation.json'),history_summary=dict(path=str(history.relative_to(root)),sha256=sha(history)),
        selected_heap=dict(stage=stage,conflict=conflict,decision=decision),
        hypothesis='Whole-heap bracket of two inprocessing rounds at272979/76952961,previously omitted by a conflict-only cutoff. Distinguish an incoming topology difference from one introduced in this phase or subsequent decisions;selected-variable equality is insufficient.',
        validation='Both ordinary1M streams,prefixes,choice-state rows,other traces and partial proofs replay6076;only heap.tsv is intentionally moved to the declared selector. Compare complete membership,score,assignment,active andposition metadata.',
        native_rerun_reason='Missing complete heap snapshots immediately before/after the same-conflict inprocessing phase;not a timing refresh.',
        test_consideration='No new observer or solver code. Reuse tested standalone snapshot selector and6080 heap parser controls;assert only selected heap changes while every other cached artifact replays.')
    m['hashes']={name:sha(c/name) for name in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
    (c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(c)

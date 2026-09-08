#!/usr/bin/env python3
"""Bind the last common macro score before the established unequal heap."""
import hashlib
import json
from pathlib import Path
import runpy
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5694-rank33-score-history'
s=json.loads((c/'summary.json').read_text())
assert s['parent_full_replay'] and s['unequal_common_score_count']==0
support=runpy.run_path(str(root/'benchmarks/probes/scratch/compare-rank33-score-history-5695.py'))
histories={a:support['rows']((c/'traces'/(a+'.scores.tsv')).read_text(),'tactix' if a=='candidate' else a)
    for a in ('candidate','kissat')}
expected=dict(label='restart_end',conflicts=180773,decisions=49323622,external=120438)
selected={}
for arm,history in histories.items():
    before=[r for r in history if r['conflicts']<=182093]
    selected[arm]=before[-1]
    assert all(before[-1][key]==value for key,value in expected.items())
assert selected['candidate']['score']==selected['kissat']['score']
heap=root/'benchmarks/runs/20260908-5689-rank33-common-heap/summary.json'
h=json.loads(heap.read_text())
assert h['parent_full_replay'] and h['selected_score_relation']=={'candidate':1,'kissat':-1}
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
result=dict(history_summary_sha256=sha(c/'summary.json'),heap_summary_sha256=sha(heap),
    last_common_macro_score=selected,first_choice_split=182093,
    update_window_conflicts=[180773,182093],
    conclusion='Equal120438 score through the last restart_end at180773; unequal at the pre-split heap182093. There are no later score snapshots at or below182093 in either arm. Bound selected bump/analysis inspection to this interval;the responsible update remains unproved.')
p=c/'score-bracket.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))

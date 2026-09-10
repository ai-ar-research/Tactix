#!/usr/bin/env python3
"""Capture the Reduce boundary where the replacement cursor first separates."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank46-search-window-6676.py')
code=source.read_text()
changes={
    '20260909-6676-rank46-search-window':'20260909-6726-rank46-collect-boundary',
    "'tactix-6676-'":"'tactix-6726-'","replace('6673','6676')":"replace('6673','6726')",
    '108330':'62676','108341':'62681',
    'Rank46 decisions largely agree through100K apart from two-row swap; later split108339/403138 picks13046 versus12721,focused. Inspect conflict-analysis/implication state before this split, without treating earlier swap or BVE99580 as the cause.':
        '6723 first516 exact reason observations match; after Reduce62677, next62680/245587/level28 has identical clause pose/values but searched6 current versus2 native. Capture Reduce entry formula including garbage, selected clause entry/exit, reduction candidates and native verbose sparse-start offset. Test collector watermark/prefix classification before changing cursor semantics.',
    'Observer configuration only;196 benchmark tests, including six prefix-byte controls and seven search-window/replay fixtures. Add fixed regression if a source defect is isolated.':
        'Configuration-only reuse of full search, formula, Reduce and clause-pose-series observers with existing framing/replay tests.218 maintained benchmark tests. Any source repair requires an actual old/new focused regression.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
anchor="compile(runner,str(c/'run.py'),'exec')"; assert code.count(anchor)==1
code=code.replace(anchor,"""s2path=root/'benchmarks/runs/20260909-6721-rank46-earlier-allscan/summary-6723.json'
s2=json.loads(s2path.read_text()); assert s2['observer_replay_validated']
for rel,digest in s2['hashes'].items(): assert sha(root/rel)==digest,rel
m['dependencies'][str(s2path.relative_to(root))]=sha(s2path)
m['reason_key']=s2['reason_key']
extra=\"\\nenv['SAT_CLAUSE_POSE_SERIES_PATH'] = str(c/'traces'/(arm+'.clause-pose-series.tsv'))\\nenv['SAT_CLAUSE_POSE_SERIES_LITS'] = m['reason_key']\\nenv['SAT_CLAUSE_POSE_SERIES_FROM'] = '62677'\\nenv['SAT_CLAUSE_POSE_SERIES_TO'] = '62677'\\nenv['SAT_CLAUSE_POSE_SERIES_NAMES'] = 'reduce,sparsecollect'\\nenv['SAT_EVENT_TRACE_FORMULA_SET_PATH'] = str(c/'traces'/(arm+'.formula.tsv'))\\nenv['SAT_EVENT_TRACE_FORMULA_SET_CONFLICT'] = '62677'\\nenv['SAT_EVENT_TRACE_FORMULA_SET_MATCH'] = 'reduce_begin'\\nenv['SAT_EVENT_TRACE_FORMULA_SET_POSE'] = '1'\\nenv['SAT_EVENT_TRACE_FORMULA_SET_META'] = '1'\\nenv['SAT_EVENT_TRACE_FORMULA_SET_VALUES'] = '1'\\nenv['SAT_EVENT_TRACE_FORMULA_SET_INCLUDE_GARBAGE'] = '1'\\nenv[prefix+'_REDUCE_TRACE'] = str(c/'traces'/(arm+'.reduce.tsv'))\\nenv[prefix+'_REDUCE_TRACE_CONFLICT_FROM'] = '62677'\\nenv[prefix+'_REDUCE_TRACE_CONFLICT_TO'] = '62677'\\n\"
split='\\ndef interrupted(signum, frame):'; assert runner.count(split)==1
runner=runner.replace(split,extra+split)
split=\"    r['command'] = command\"; assert runner.count(split)==1
runner=runner.replace(split,\"    if arm == 'kissat': command.insert(-2, '-v')\\n\"+split)
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

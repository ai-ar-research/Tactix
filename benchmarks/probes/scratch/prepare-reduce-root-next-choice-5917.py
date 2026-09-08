#!/usr/bin/env python3
"""Observe the next current-candidate Rank32 choice using frozen300K controls."""
from pathlib import Path
import re
source=Path(__file__).with_name('prepare-rank32-reduce-5869.py')
code=source.read_text()
changes={
 '20260908-5857-forward-caller-rank32-prefix':'20260908-5907-reduce-root-rank32-prefix',
 '20260908-5869-rank32-reduce-frontier':'20260908-5917-reduce-root-next-choice',
 's[\'matching_decisions\'] == 61154590':'s[\'matching_decisions\'] == 68091135',
 "s['suffix_comparison']['left'][0] == 225987":"s['suffix_comparison']['left'][0] == 271085",
 'tactix-5857-':'tactix-5907-', 'tactix-5869-':'tactix-5917-',
 '5857-caller-rank32':'5907-reduce-root-rank32','5869-reduce-frontier':'5917-next-choice',
 'At common225987 state distinguish Reduce candidate/victim/learned-state mismatch from subsequent Reorder. Full Reduce table,not a sampled clause;no production change.':
   '5900 closes225987;first later split271085/67954350 differs-29407 versusnative-29066. Compare full last-common heap and29066 assignment reasons to distinguish score/pose from availability,without assuming same cause.',
 'Compare reduction scope,rank,selection,glue,usage,reasons and full clause sets.':
   'Compare full physical heap after common decision67954349 andall29066 assignment reasons at271085.',
 'Cached native5771 has decision/boundary traces but lacks Reduce candidates/victims at225987.':
   'Cached native5771 lacks complete heap and29066 assignment reasons at newly localized271085.',
 'Read-only observer configuration using unchanged tested5849 and cached native pins. Existing reduce trace protocol tests retained; require exact ordinary trace/proof replay before interpretation. No schedule-dump observer or early-exit selector is enabled.':
   'Read-only standalone choice/heap observers on tested5900 andcached native pins. Require exact ordinary300K trace/proof replay;no broad event trace, schedule dump or early-exit selector.',
}
code=re.sub('|'.join(re.escape(k) for k in sorted(changes,key=len,reverse=True)),lambda m:changes[m.group()],code)
old="""env[prefix + '_REDUCE_TRACE'] = str(c / 'traces' / (arm + '.reduce.tsv'))
env[prefix + '_REDUCE_TRACE_CONFLICT_FROM'] = '225987'
env[prefix + '_REDUCE_TRACE_CONFLICT_TO'] = '225987'
"""
new="""env.update(SAT_DECISION_HEAP_SNAPSHOT='1', SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION='1',
           SAT_EVENT_TRACE_HEAP_SET_PATH=str(c / 'traces' / (arm + '.heap.tsv')),
           SAT_EVENT_TRACE_HEAP_SET_CONFLICT='271085', SAT_EVENT_TRACE_HEAP_SET_DECISION='67954349',
           SAT_CHOICE_STATE_CONFLICT='271085', SAT_CHOICE_STATE_VARIABLE='29066')
env[prefix + '_CHOICE_STATE_TRACE'] = str(c / 'traces' / (arm + '.choice.tsv'))
"""
assert code.count(old)==1
code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

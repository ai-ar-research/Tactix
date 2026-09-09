#!/usr/bin/env python3
"""Prepare B28+C5 and a Tactix-only causal prefix on immutable6049/6054."""
from pathlib import Path
import re
source=Path(__file__).with_name('prepare-forward-admission-jobs-5836.py')
code=source.read_text()
changes={
 '20260908-5830-forward-admission':'20260908-6049-reduce-root-lock',
 '20260908-5806-forward-unit-BC-gate':'20260908-5968-bve-active-pop-BC-gate',
 '20260908-5836-forward-admission-BC-gate':'20260908-6059-root-lock-BC-gate',
 '5836-forward-admission-BC':'6059-root-lock-BC',
 "('B', 5830, 'release'), ('C', 5831, 'bc')":"('B', 6049, 'release'), ('C', 6050, 'bc')",
 'pin-{number}-forward-admission':'pin-{number}-reduce-root-lock',
 'isolated C2 admission producer5830':'isolated root-reason Reduce repair6049 on5962',
 'Native clause-addition-only C2 marks preserve expected verdicts and improve cumulative work':'Native root-reason eligibility restores candidate population and downstream clause retention; require no B/C regression',
 '20260908-5837-forward-admission-rank32-prefix':'20260908-6060-root-lock-rank32-prefix',
 'pin-5832-forward-admission-trace':'pin-6054-reduce-root-lock-trace',
 'tactix-5837-':'tactix-6060-',
 '5837-admission-rank32':'6060-root-lock-rank32',
 'Remove root-only C2 admission producer;locate earliest changed work and compare with native. Earlier divergences must be reported,not hidden by the179463 witness.':'Restore native root-reason eligibility. Compare every Reduce population/rank/victim row with cached6042 through270978, then all decisions with cached native through300K. Report earlier or downstream differences; no assumed runtime gain.',
}
for old in changes:assert old in code,old
code=re.sub('|'.join(re.escape(k) for k in sorted(changes,key=len,reverse=True)),lambda m:changes[m.group()],code)
anchor="(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5771-', 'tactix-6060-'))"
replacement='''runner=(old/'run.py').read_text().replace('tactix-5771-','tactix-6060-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor)==1
runner=runner.replace(anchor,"env[prefix + '_REDUCE_TRACE'] = str(c / 'traces' / (arm + '.reduce.tsv'))\\nenv[prefix + '_REDUCE_TRACE_CONFLICT_FROM'] = '0'\\nenv[prefix + '_REDUCE_TRACE_CONFLICT_TO'] = '270978'\\n"+anchor)
compile(runner,str(c/'run.py'),'exec');(c/'run.py').write_text(runner)'''
assert code.count(anchor)==1;code=code.replace(anchor,replacement)
anchor="m['hashes'] = {str(p.relative_to(c)): sha(p)"
assert code.count(anchor)==1
code=code.replace(anchor,"""retained=runs/'20260908-5969-bve-active-pop-rank32-prefix/results/candidate.json'
native_history=runs/'20260908-6042-retention-history'
m['retained_parent']=dict(record=str(retained.relative_to(root)),sha256=sha(retained))
m['native_reduce_history']=dict(path=str(native_history.relative_to(root)),summary_sha256=sha(native_history/'summary.json'))
diagnostic=root/'.claude/worktrees/runtime-reduce-root-lock-trace-6054'
m['diagnostic_source']=str(diagnostic.relative_to(root))
m['diagnostic_source_hashes']={name:sha(diagnostic/name) for name in ['src/rules/reduce/reduce.cpp','src/diagnostics/event_trace.cpp','tests/test_reduce_root_trace.cpp']}
assert '100% tests passed out of 98' in (runs/'20260908-6054-root-lock-trace-controls/logs/ctest.log').read_text()
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

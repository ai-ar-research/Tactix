#!/usr/bin/env python3
"""Classify global-observer additions; do not relabel7362 as byte-exact."""
import gzip
from pathlib import Path

def macro_projection(parent, observed):
    # These are the only observer-owned macro producers activated by opening
    # EventTrace. Every original row, including all state columns, must agree.
    added={'trace_begin','retier','checkpoint'}
    def validate(rows):
        assert rows and all(len(r)==15 and r[0]=='tactix' for r in rows)
        assert [int(r[1]) for r in rows]==list(range(len(rows)))
        assert all(all(int(r[i])>=0 for i in (3,4,5,6,8,9,10,11,12,13,14)) for r in rows)
    validate(parent);validate(observed)
    assert not any(r[2] in added for r in parent)
    original=[r for r in observed if r[2] not in added]
    extras=[r for r in observed if r[2] in added]
    assert [r[:1]+r[2:] for r in original]==[r[:1]+r[2:] for r in parent]
    assert observed[0][2]=='trace_begin' and sum(r[2]=='trace_begin' for r in observed)==1
    return dict(original_rows=len(parent),observed_rows=len(observed),added_rows=extras,
        exact_original_state_columns=True,raw_archive_exact=False)

row=['tactix','0','startup_begin','0','0','3','0','focused','2','1','0','0','0','0','4']
begin=row.copy();begin[2]='trace_begin'
shift=row.copy();shift[1]='1'
assert macro_projection([row],[begin,shift])['original_rows']==1
for column,value in ((2,'unknown-added'),(5,'4'),(8,'3'),(14,'5'),(1,'2')):
    bad=shift.copy();bad[column]=value
    try:macro_projection([row],[begin,bad])
    except AssertionError:pass
    else:raise AssertionError('changed original macro row accepted')
try:macro_projection([row],[begin])
except AssertionError:pass
else:raise AssertionError('missing original macro row accepted')

base=Path(__file__).with_name('analyze-root-lifecycle-7363.py')
code=base.read_text().replace('root-lifecycle-analysis-7363.json','root-lifecycle-analysis-7367.json')
before='hashes={};analyses=[]'
assert before in code;code=code.replace(before,'hashes={};analyses=[];macro_classifications=[]')
before="authority=prior['trace_artifacts'][kind];bind(old/authority['path'],authority['sha256']);bind(path,authority['sha256'])"
after="""authority=prior['trace_artifacts'][kind];bind(old/authority['path'],authority['sha256'])
            if kind!='macro':bind(path,authority['sha256'])
            else:
                streams=[]
                for p in (old/authority['path'],path):
                    with gzip.open(p,'rt') as stream:streams.append([line.rstrip().split('\\t') for line in stream if not line.startswith('#')])
                macro_classifications.append(macro_projection(*streams))"""
assert before in code;code=code.replace(before,after)
before="for p in (Path(__file__),helper,qpath,c/'manifest.json',c/'launch.json'):bind(p)"
after="for p in (Path(__file__),base,helper,qpath,c/'manifest.json',c/'launch.json',root/'.claude/worktrees/runtime-debug-contracts-7339/src/diagnostics/event_trace.cpp'):bind(p)"
assert before in code;code=code.replace(before,after)
code=code.replace("status='CHECKED_FULL_REPLAY_LATE_LUCKY_ROOT_LIFECYCLE_MISS'", "status='CLASSIFIED_OBSERVER_ADDITIONS_AND_LATE_LUCKY_LIFECYCLE_MISS',original_six_archive_byte_gate='FAILED_MACRO_ADDITIONS_NOT_WAIVED',macro_classifications=macro_classifications")
code=code.replace("print('deactivation schema: 3 negative controls PASS')", "print('macro classification: 6 negative controls; deactivation schema:3 negative controls PASS')")
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__',base=base,macro_projection=macro_projection))

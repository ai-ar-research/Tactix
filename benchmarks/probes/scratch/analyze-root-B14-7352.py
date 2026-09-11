#!/usr/bin/env python3
"""Account explicitly for warmup choices, which do not advance CDCL decisions."""
from pathlib import Path
def count_choice(row,normal,warm):
    assert len(row)==5 and row[0]>=0 and row[2]>=0 and row[3]!=0 and row[4] in (0,1)
    if row[4]: warm+=1
    else: normal+=1
    assert row[1]==normal,('decision cursor',row,normal)
    return normal,warm
normal=warm=0
for row in [(0,1,1,2,0),(1,1,1,3,1),(1,1,2,4,1),(1,2,1,-2,0)]:
    normal,warm=count_choice(row,normal,warm)
assert (normal,warm)==(2,2)
for row in [(1,3,1,2,1),(1,2,1,2,0),(1,3,1,0,0),(1,3,1,2,2)]:
    try: count_choice(row,normal,warm)
    except AssertionError: pass
    else: raise AssertionError('invalid decision/warmup cursor accepted')
print('trace accounting: interleaved normal/warmup and four invalid controls PASS',flush=True)
base=Path(__file__).with_name('analyze-root-B14-7351.py'); code=base.read_text()
for before,after in {
    'root-B14-analysis-7351.json':'root-B14-analysis-7352.json',
    'counts=[0,0]; digests=':'normal_counts=[0,0]; warm_counts=[0,0]\ncounts=[0,0]; digests=',
    "assert row[1]==counts[arm] and row[0]>=0 and row[2]>=0 and row[3]!=0 and row[4] in (0,1),row":
        'normal_counts[arm],warm_counts[arm]=count_choice(row,normal_counts[arm],warm_counts[arm])',
    "assert len(expected)==1 and expected[0] and counts[arm]==int(expected[0][1]),(counts,expected)":
        "assert len(expected)==1 and expected[0] and normal_counts[arm]==int(expected[0][1]),(normal_counts,expected)\n    warm=[re.fullmatch(r'c warmup      (\\d+) warmups, (\\d+) decisions, (\\d+) propagations',s) for s in r['solver_statistics'] if s.startswith('c warmup ')]\n    assert len(warm)==1 and warm[0] and warm_counts[arm]==int(warm[0][2]),(warm_counts,warm)",
    "for path in [Path(__file__),Path(__file__).with_name('root-storage-statistics-7350.py'),":
        "for path in [Path(__file__),Path(__file__).with_name('analyze-root-B14-7351.py'),Path(__file__).with_name('root-storage-statistics-7350.py'),",
    'decisions=dict(counts=counts,':'decisions=dict(counts=counts,normal_counts=normal_counts,warmup_counts=warm_counts,',
}.items():
    assert before in code,before; code=code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__',count_choice=count_choice))

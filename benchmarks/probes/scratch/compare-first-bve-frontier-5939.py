#!/usr/bin/env python3
"""Validate full replay and first-forward input before comparing BVE choices."""
from collections import Counter
import hashlib
import io
import importlib.util
import itertools
import json
from pathlib import Path
import sys
from types import SimpleNamespace

root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5937-first-bve-frontier'
source=Path(__file__).with_name('compare-second-forward-formulas-5934.py')
code=source.read_text()
for old,new in {
    "for number,stage in ((5931,'begin'),(5932,'end')):":"for number,stage in ((5937,'end'),):",
    "f'benchmarks/runs/20260908-{number}-second-forward-{stage}-formula'":"'benchmarks/runs/20260908-5937-first-bve-frontier'",
    "boundary=select(list(csv.DictReader(stream,fieldnames=header,delimiter='\\t')),stage)":
        "boundary=select(list(csv.DictReader(stream,fieldnames=header,delimiter='\\t')),stage,0)",
    "=={arm+'.formula.tsv'}":"=={arm+'.formula.tsv',arm+'.binaries.tsv',arm+'.bve-candidates.tsv'}",
    "print(json.dumps({k:v for k,v in out.items() if k!='evidence'},indent=2),flush=True)":
        "print(json.dumps({k:v for k,v in out.items() if k in ('stage','large_clause_multisets_equal','physical_clause_order_equal','candidate_only_copies','kissat_only_copies','pose_differences','metadata_differences','root_value_differences')},indent=2),flush=True)",
}.items():
    assert old in code,old
    code=code.replace(old,new)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def binaries(path):
    groups=[]
    with path.open() as stream:
        assert next(stream).strip() in ('# tactix-binary-set-snapshot\t1','# kissat-binary-set-snapshot\t1')
        assert next(stream).strip()=='# snapshot\ta\tb'
        counts=None
        for line in stream:
            row=line.rstrip('\n').split('\t')
            if row[0]=='# begin':
                assert counts is None and len(row)==7 and int(row[1])==len(groups) and row[4]=='271079'
                expected=int(row[5]); seq=row[1]; counts=Counter()
            elif row[0]=='# end':
                assert row==['# end',seq] and sum(counts.values())==expected
                groups.append(counts); counts=None
            else:
                assert counts is not None and len(row)==3 and row[0]==seq
                a,b=map(int,row[1:]); assert a and b and a<=b
                counts[(a,b)]+=1
        assert counts is None and len(groups)==2,len(groups)
    return groups
def candidates(path):
    rows=[]
    with path.open() as stream:
        assert next(stream).strip()=='# bve-candidate-observer\t1'
        for line in stream:
            fields=line.rstrip('\n').split('\t')
            assert fields[0] in ('tactix','kissat')
            row=dict(f.split('=',1) for f in fields[1:])
            assert len(row)==len(fields)-1 and row['conflicts']=='271079'
            assert int(row['external'])>0 and row['applied'] in ('0','1') and int(row['value']) in (-1,0,1)
            rows.append(row)
    assert rows
    return rows
def extra_fixtures():
    fake=lambda text:SimpleNamespace(open=lambda:io.StringIO(text))
    good='# tactix-binary-set-snapshot\t1\n# snapshot\ta\tb\n'
    for i in range(2):
        good+=f'# begin\t{i}\ttactix\tslot_boundary\t271079\t2\tname=Strengthen|stage=end\n{i}\t-2\t3\n{i}\t-2\t3\n# end\t{i}\n'
    assert all(group==Counter({(-2,3):2}) for group in binaries(fake(good)))
    for old,new in [('\t271079\t','\t0\t'),('\t2\tname=','\t3\tname='),
                    ('0\t-2\t3','0\t4\t3'),('# end\t1','# end\t3')]:
        try: binaries(fake(good.replace(old,new)))
        except AssertionError: pass
        else: raise AssertionError('bad binary snapshot accepted')
    row='# bve-candidate-observer\t1\ntactix\texternal=9\tstage=begin\tconflicts=271079\tticks=5\tlimit=6\tapplied=0\tvalue=0\n'
    assert candidates(fake(row))[0]['external']=='9'
    for old,new in [('external=9','external=0'),('applied=0','applied=2'),
                    ('value=0','value=2'),('conflicts=271079','conflicts=0'),
                    ('ticks=5','value=0')]:
        try: candidates(fake(row.replace(old,new)))
        except AssertionError: pass
        else: raise AssertionError('bad candidate row accepted')
    print('PASS: binary multiplicity/two snapshots,candidate schema andnine negative fixtures',flush=True)
extra_fixtures()
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
if '--self-test' in sys.argv: sys.exit(0)
arms={a:candidates(c/'traces'/f'{a}.bve-candidates.tsv') for a in ('candidate','kissat')}
fields=('external','stage','conflicts','applied','value')
first=None; matched=0
for i,(a,b) in enumerate(itertools.zip_longest(arms['candidate'],arms['kissat'])):
    if a is None or b is None or any(a[k]!=b[k] for k in fields):
        first=dict(index=i,candidate=a,kissat=b); break
    matched+=1
binary={a:binaries(c/'traces'/f'{a}.binaries.tsv') for a in ('candidate','kissat')}
target=c/'bve-frontier.json'
assert not target.exists()
out=dict(candidate_rows={a:len(rs) for a,rs in arms.items()},matching_portable_rows=matched,
    compared_fields=fields,first_portable_candidate_difference=first,
    binary_snapshots=[dict(ordinal=i,candidate_copies=sum(a.values()),kissat_copies=sum(b.values()),
        equal=a==b,candidate_only=sum((a-b).values()),kissat_only=sum((b-a).values()))
        for i,(a,b) in enumerate(zip(binary['candidate'],binary['kissat']))],
    hashes={str(p.relative_to(root)):sha(p) for p in [c/'summary.json',*(c/'results').glob('*.json')]},
    limitation='Complete candidate sequences compared onidentity,stage,conflict,outcome,value. Raw effort,flags androot/trail counters retained for classification,not normalized away orassumed equivalent. Incoming large/binary state must be consulted before attributing pivot drift.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))

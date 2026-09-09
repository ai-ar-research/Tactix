#!/usr/bin/env python3
"""Interpret new producer rows only after exact full native parent replay."""
import json
from pathlib import Path
import re
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
def parse(text):
    assert text and text.endswith('\n')
    rows=[]
    for line in text.splitlines():
        m=re.fullmatch(r'kissat\tconflicts=(\d+)\tdecisions=(\d+)\tcount=(\d+)\tliteral=(-?[1-9]\d*)\tsource=([a-zA-Z_]\w*)',line,re.ASCII)
        assert m,line
        conflict,decision,count,lit=map(int,m.groups()[:4])
        assert conflict<=823562 and 2508<count<=2510 and decision>0 and abs(lit)<1<<31
        rows.append(dict(conflicts=conflict,decisions=decision,count=count,literal=lit,source=m[5]))
    assert [r['count'] for r in rows]==[2509,2510]
    assert rows[0]['conflicts']<=rows[1]['conflicts']
    return rows
def fixtures():
    good='kissat\tconflicts=700000\tdecisions=1\tcount=2509\tliteral=1\tsource=learn\n'+'kissat\tconflicts=700000\tdecisions=1\tcount=2510\tliteral=-2\tsource=learn\n'
    assert len(parse(good))==2
    bad=['',good[:-1],good.splitlines(True)[0],good.replace('2510','2509'),good.replace('literal=1','literal=0'),good.replace('700000','823563'),good.replace('source=learn','source=bad-name'),good.replace('count=2509','other=2509')]
    for text in bad:
        try: parse(text)
        except (AssertionError,ValueError): pass
        else: raise AssertionError('bad producer stream accepted')
    print('PASS: eight negative producer schema/count/filter fixtures')
def main():
    fixtures()
    if '--self-test' in sys.argv:return
    c=root/'benchmarks/runs/20260909-6298-rank50-subsumption-producer'
    assert not (c/'summary.json').exists()
    m=json.loads((c/'manifest.json').read_text()); hashes={}
    def bind(p,expected=None):
        digest=sha(p)
        if expected is not None: assert digest==expected,p
        hashes[str(p.relative_to(root))]=digest
    for rel,digest in m['hashes'].items():bind(c/rel,digest)
    p=root/m['parent_summary']['path'];bind(p,m['parent_summary']['sha256'])
    parent=p.parent; ps=json.loads(p.read_text())
    r=json.loads((c/'results/kissat.json').read_text())
    old=json.loads((parent/'results/kissat.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==sha(c/'pins/kissat')
    assert r['partial_certificate']==old['partial_certificate']
    ordinary={k:v for k,v in r['trace_sha256'].items() if not k.endswith('.subsumption.tsv')}
    assert ordinary==old['trace_sha256']
    for name,digest in old['trace_sha256'].items():
        p=parent/'traces'/name;bind(p,digest);assert ps['hashes'][str(p.relative_to(root))]==digest
    for name,digest in r['trace_sha256'].items():bind(c/'traces'/name,digest)
    for p in (c/'manifest.json',c/'launch.json',c/'results/kissat.json',parent/'results/kissat.json'):bind(p)
    rows=parse((c/'traces/kissat.subsumption.tsv').read_text())
    result=dict(full_parent_trace_and_proof_replay=True,producers=rows,partial_certificate=r['partial_certificate'],hashes=hashes,
        limitation='Native producer identity only; no Tactix tracker implemented or timing claim. Counterexample motivates a separately approved kernel-material tracker; public/proof semantics unchanged.')
    (c/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(rows,indent=2))
if __name__=='__main__':main()

import json,glob,re,sys,collections
R='/home/hwu/SAT-solving/benchmarks/runs/'
def load(d,arm=None):
    out={}
    for f in sorted(glob.glob(R+d+'/results/*.json')):
        try: r=json.load(open(f))
        except Exception: continue
        if arm and r.get('arm')!=arm: continue
        out[r['instance']]=r
    return out
def stat(r):
    st=r.get('solver_statistics') or []
    if not isinstance(st,list): st=[]
    d={}
    for l in st:
        if l.startswith('c localsearch'):
            m=re.search(r'(\d+) hints offered',l); d['off']=int(m.group(1))
            m=re.search(r'(\d+) applied',l); d['app']=int(m.group(1)) if m else None
        for name in ['affine','parity','onehot','balance','directcsp','graphlabel']:
            if re.match(r'c %s\s'%name,l):
                m=re.search(r'(\d+) candidates',l)
                if m and int(m.group(1))>0: d.setdefault('prod',[]).append(name)
            if re.match(r'c directcsp\s+([1-9]\d*) calls',l): d.setdefault('prod',[]).append('directcsp')
            m=re.match(r'c graph-schedule useful_hints=([1-9]\d*)',l)
            if m: d.setdefault('prod',[]).append('graphlabel')
            m=re.match(r'c localsearch.*?(\d+) target installs',l)
            if m: d['inst']=int(m.group(1))
    return d
def oc(r):
    if not r: return '—'
    v=r.get('verdict'); s=r.get('solve') or {}
    t=s.get('wall_seconds') if isinstance(s,dict) else None
    if v in ('SAT','UNSAT'):
        ok=r.get('valid')
        tag='' if ok else ('?' if ok is None else '✗')
        return f"{v} {t:.0f}{tag}" if t is not None else v
    return 'TO' if (isinstance(s,dict) and s.get('timed_out')) else str(v)
b=load('20260928-am546-no-mab-tactix-hybrid-tierd-cpuq')
a=load('20261010-tierd-apigap-final')
sk=load('20260928-am547-satsuma-hybrid-tierd-cpuq','satsuma-kissat')
k24=load('20260928-am540-four-winner-tierd-cpuq','kissat-sc2024')
# Rows offering >= 1 hint in either run.
rows=[k for k in set(b)|set(a) if (stat(b.get(k,{})).get('off') or 0)>0 or (stat(a.get(k,{})).get('off') or 0)>0]
def solved(r): return r and r.get('verdict') in ('SAT','UNSAT')
def wall(r):
    s=(r or {}).get('solve') or {}
    return s.get('wall_seconds') if solved(r) else None
out=[]
for k in rows:
    sb,sa=stat(b.get(k,{})),stat(a.get(k,{}))
    exp=(b.get(k) or a.get(k)).get('expected')
    wb,wa=wall(b.get(k)),wall(a.get(k))
    if wb and not wa: cls='lost'
    elif wa and not wb: cls='gained'
    elif wa and wb:
        cls='slower' if wa>1.5*wb and wa-wb>10 else ('faster' if wb>1.5*wa and wb-wa>10 else 'same')
    else: cls='both-unsolved'
    out.append((cls,k,exp,sb,sa))
order={'lost':0,'gained':1,'slower':2,'faster':3,'both-unsolved':4,'same':5}
out.sort(key=lambda x:(order[x[0]],x[1].split('-',1)[1]))
cnt=collections.Counter(); cntapp=collections.Counter()
print('| Class | Row | Exp | Producer | Applied before / after | Before (d170b4a5) | After (final) | Satsuma+Kissat | Kissat-SC2024 |')
print('|---|---|---|---|---|---|---|---|---|')
for cls,k,exp,sb,sa in out:
    name=k.split('/')[-1].split('-',1)[1].replace('.cnf.xz','').replace('.sanitized','')
    prod=','.join(sorted(set(sa.get('prod',[])+sb.get('prod',[])))) or '?'
    ab=sb.get('app'); 
    cnt[cls]+=1
    if ab: cntapp[cls]+=1
    print(f"| {cls} | `{name}` | {exp} | {prod} | {ab if ab is not None else '—'} / {sa.get('app') if sa.get('app') is not None else '—'} | {oc(b.get(k))} | {oc(a.get(k))} | {oc(sk.get(k))} | {oc(k24.get(k))} |")
print(dict(cnt)); print('applied>0 before:',dict(cntapp))
# aggregate: rows with applied>0 before
def par2(r): w=wall(r); return w if w else 10000
tot=collections.Counter()
for cls,k,exp,sb,sa in out:
    if (sb.get('app') or 0)>0:
        tot['n']+=1; tot['b_s']+=bool(solved(b.get(k))); tot['a_s']+=bool(solved(a.get(k)))
        tot['b_par2']+=par2(b.get(k)); tot['a_par2']+=par2(a.get(k))
        tot['sk_s']+=bool(solved(sk.get(k))); tot['k24_s']+=bool(solved(k24.get(k)))
print(dict(tot))

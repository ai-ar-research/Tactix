import json,glob,os,math
d="/home/hwu/SAT-solving-special/benchmarks/runs/20261015-124533-chvatal-screen"
m=json.load(open(f"{d}/manifest.json"));names=[s["name"] for s in m["solvers"]]
tab={}
for f in glob.glob(f"{d}/results/*.json"):
    i=int(os.path.basename(f)[:5]);t=m["tasks"][i];r=json.load(open(f))
    tab.setdefault(t["rel"],{})[names[t["solver"]]]=r
rows=[]
for rel,v in tab.items():
    if len(v)<2 or not (v["tactix-default"]["solved"] and v["satsuma-kissat"]["solved"]): continue
    a,b=v["tactix-default"],v["satsuma-kissat"]
    ta=a["time"] if a["solved"] else 3600; tb=b["time"] if b["solved"] else 3600
    rows.append((max(ta,tb),ta,tb,rel))
rows.sort()
# medium band: faster baseline >= 10 s; drop near-duplicates (both times within 3%)
band=[r for r in rows if min(r[1],r[2])>=10]
sel=[];
for r in band:
    if any(abs(r[1]-s[1])<=0.03*s[1] and abs(r[2]-s[2])<=0.03*s[2] for s in sel): continue
    sel.append(r)
print(len(rows),len(band),len(sel))
# pick 30 spread on log scale of max time
if len(sel)>30:
    lo,hi=math.log(sel[0][0]),math.log(sel[-1][0])
    picked=[]
    for k in range(30):
        target=lo+(hi-lo)*k/29
        best=min((r for r in sel if r not in picked),key=lambda r:abs(math.log(r[0])-target))
        picked.append(best)
    sel=sorted(picked)
for r in sel: print(f"{r[1]:8.1f} {r[2]:8.1f} {os.path.basename(r[3])}")
open("/home/hwu/SAT-solving-special/corpus/chvatal/chvatal30.txt","w").write("".join(r[3]+"\n" for r in sel))

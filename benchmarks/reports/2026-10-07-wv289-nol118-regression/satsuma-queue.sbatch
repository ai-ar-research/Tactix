#!/bin/bash
#SBATCH --job-name=satsuma-queue
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=16384M
#SBATCH --time=00:30:00
#SBATCH --array=0-4
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/runs/20261007-wv289-nol118-satsuma-queue/logs/%A_%a.log
# Owner residual (Satsuma fix, exact SC2026 run_base.sh flags) for activation-queue comparison; no solver search.
set -u
W=/home/hwu/SAT-solving/benchmarks/runs/20261007-wv289-nol118-satsuma-queue; C=/home/hwu/SAT-solving/benchmarks/cache/tierD24-26
pats=(noL-11-8.sanitized noL-11-2.sanitized noL-11-6.sanitized noL-11-10.sanitized oski15a01b06s_opt)
p=${pats[$SLURM_ARRAY_TASK_ID]}
T=$(mktemp -d /tmp/satq-$SLURM_JOB_ID-XXXXXX); trap '/bin/rm -rf "$T"' EXIT
xz -dc $C/*-$p.cnf.xz > $T/in.cnf
s=$(date +%s.%N)
timeout 600 $W/pins/satsuma fix $T/in.cnf --silent --full-skip-limit 100000000 --add-reduced-as-unit --bsr --proof-file $T/proof --out-file $T/res.cnf > $T/log 2>&1; rc=$?
e=$(date +%s.%N)
python3 - $T/in.cnf $T/res.cnf > $W/out/$p.json <<'PY'
import sys,json
def read(f):
    cls=[]
    for l in open(f):
        if l[0] in 'cp' or not l.strip(): continue
        cls.append([int(x) for x in l.split()[:-1]])
    return cls
o=read(sys.argv[1]); r=read(sys.argv[2])
def first(cls):
    seen=set(); q=[]
    for c in cls:
        for x in c:
            v=abs(x)
            if v not in seen: seen.add(v); q.append(v)
    return q
units=[c[0] for c in r if len(c)==1]
fixed=set(abs(u) for u in units)
qr=[v for v in first(r) if v not in fixed]
qo=[v for v in first(o) if v not in fixed]
diff=next((i for i,(a,b) in enumerate(zip(qr,qo)) if a!=b),None)
print(json.dumps(dict(orig_clauses=len(o),res_clauses=len(r),units=len(units),res_active_queue=len(qr),orig_minus_fixed=len(qo),equal=qr==qo,first_diff=diff,res_at=qr[diff:diff+10] if diff is not None else [],orig_at=qo[diff:diff+10] if diff is not None else [])))
PY
echo "$p rc=$rc $(echo "$e - $s"|bc)s" >> $W/out/$p.json

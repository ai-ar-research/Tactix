from pathlib import Path
import subprocess
import collections
import re

root=Path(__file__).resolve().parents[4]
case=root/'benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/handoffs/D1-dsr-probe-cce-unit-rejection'
checker=root/'benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/dsr-trim'
lines=case.joinpath('min-rel.dsr.txt').read_text().splitlines(keepends=True)
assert lines[0]=='-14 0\n' and all(x.startswith('d ') for x in lines[1:11])
work=root/'benchmarks/probes/scratch/am458-d1-order'
forward=[]
backward=[]
failures=collections.Counter()
for mask in range(1<<10):
    early=[lines[1+i] for i in range(10) if mask&(1<<i)]
    late=[lines[1+i] for i in range(10) if not mask&(1<<i)]
    proof=work/'candidate.dsr'
    proof.write_text(''.join(early+[lines[0]]+late+lines[11:]))
    output=work/'candidate.lsr'
    r=subprocess.run([str(checker),'-f','-V',str(case/'min.cnf'),str(proof),str(output)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    match=re.search(r'Error: \[line (\d+)\] ([^\n]+)',r.stdout)
    if r.returncode and match:
        failures[f'{match.group(1)}: {match.group(2)}']+=1
    if r.returncode==0:
        forward.append(mask)
        q=subprocess.run([str(checker),str(case/'min.cnf'),str(proof),str(output)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        if q.returncode==0:
            backward.append(mask)
            if len(backward)==1:
                (work/'first-both.dsr').write_bytes(proof.read_bytes())
print('forward',len(forward),[hex(x) for x in forward[:30]])
print('both',len(backward),[hex(x) for x in backward[:30]])
print('failures',dict(failures))

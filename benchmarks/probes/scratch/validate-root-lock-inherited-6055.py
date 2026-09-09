#!/usr/bin/env python3
"""Recheck inherited proof paths on6049 and add its root-lock witnesses."""
from pathlib import Path
import hashlib
import json
import subprocess
root=Path(__file__).resolve().parents[3]
here=Path(__file__).resolve().parent
source=here/'validate-bve-active-pop-inherited-5966.py'
code=source.read_text()
for old,new in [('20260908-5962-bve-active-pop','20260908-6049-reduce-root-lock'),
                ('pin-5962-bve-active-pop','pin-6049-reduce-root-lock'),
                ('20260908-5966-bve-active-pop-edges','20260908-6055-root-lock-edges'),
                ('5965-bve-active-pop','6052-reduce-root-lock'),
                ('5962-bve-active-pop','6049-reduce-root-lock'),
                ('runtime-bve-active-pop-5962','runtime-reduce-root-lock-6049'),
                ('against5962','against6049'),
                ('final-proof-validation.json','root-and-inherited-proof-validation.json')]:
    code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
c=root/'benchmarks/runs/20260908-6049-reduce-root-lock'
out=c/'bve-proofs';out.mkdir(exist_ok=False)
exe=root/'build/pin-6049-reduce-root-lock-release/tests/test_bve_active_pop'
r=subprocess.run([exe,out/'bve'],capture_output=True,text=True,check=True)
(out/'fixtures.log').write_text(r.stdout+r.stderr)
records=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for cnf in sorted(out.glob('*.cnf')):
    proof=cnf.with_suffix('.proof');lrat=cnf.stem.endswith('-lrat')
    checker=root/'tools/checkers/bin'/('lrat-check' if lrat else 'drat-trim')
    command=list(map(str,[checker,cnf,proof]));r=subprocess.run(command,capture_output=True,text=True)
    (out/(cnf.stem+'.log')).write_text(r.stdout+r.stderr)
    assert r.returncode==0 and ('c VERIFIED' if lrat else 's VERIFIED') in r.stdout
    records.append(dict(command=command,cnf_sha256=sha(cnf),proof_sha256=sha(proof),checker_sha256=sha(checker)))
assert len(records)==8
prior=c/'root-and-inherited-proof-validation.json'; assert json.loads(prior.read_text())['checked_proofs']==54
(c/'final-proof-validation.json').write_text(json.dumps(dict(checked_proofs=62,bve_records=records,
    hashes={str(p.relative_to(root)):sha(p) for p in [prior,c/'controls/summary.json',exe,out/'fixtures.log']},
    scope='8 new root-lock proofs +46 inherited root/continuation/caller/edge/journal proofs +8 BVE inactive-pop proofs, all executed on6049. Current-source fuzz remains unverified.'),indent=2)+'\n')
print('PASS:62 independent current-source DRAT/LRAT proofs')

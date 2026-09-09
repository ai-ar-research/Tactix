#!/usr/bin/env python3
"""Seal successful6055 oracles without rerunning or renaming frozen evidence."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6049-reduce-root-lock'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
prior=c/'final-proof-validation.json';p=json.loads(prior.read_text()); assert p['checked_proofs']==46
for rel,digest in p['hashes'].items():assert sha(root/rel)==digest,rel
own=c/'controls/summary.json';p=json.loads(own.read_text());assert p['checked_proofs']==8
for rel,digest in p['hashes'].items():assert sha(root/rel)==digest,rel
out=c/'bve-proofs';out.mkdir(exist_ok=False)
exe=root/'build/pin-6049-reduce-root-lock-release/tests/test_bve_active_pop'
r=subprocess.run([exe,out/'bve'],capture_output=True,text=True,check=True)
(out/'fixtures.log').write_text(r.stdout+r.stderr)
records=[]
for cnf in sorted(out.glob('*.cnf')):
    proof=cnf.with_suffix('.proof');lrat=cnf.stem.endswith('-lrat')
    checker=root/'tools/checkers/bin'/('lrat-check' if lrat else 'drat-trim')
    command=list(map(str,[checker,cnf,proof]));r=subprocess.run(command,capture_output=True,text=True)
    (out/(cnf.stem+'.log')).write_text(r.stdout+r.stderr)
    assert r.returncode==0 and ('c VERIFIED' if lrat else 's VERIFIED') in r.stdout
    records.append(dict(command=command,cnf_sha256=sha(cnf),proof_sha256=sha(proof),checker_sha256=sha(checker)))
assert len(records)==8
(c/'complete-proof-validation.json').write_text(json.dumps(dict(checked_proofs=62,bve_records=records,
    hashes={str(p.relative_to(root)):sha(p) for p in [prior,own,exe,out/'fixtures.log']},
    tooling_incident='6055 completed46 inherited checks then read a nonexistent inherited-proof-validation.json. Its actual final-proof-validation.json remains frozen with46 checks. This successor validates that authority, adds8 root-lock and8 BVE proofs without rerunning it.',
    scope='All62 proofs executed on6049. Current-source fuzz remains unverified.'),indent=2)+'\n')
print('PASS:62 current-source checked proofs;6055 aggregation-only failure closed')

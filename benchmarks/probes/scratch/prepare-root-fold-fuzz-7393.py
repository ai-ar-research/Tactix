#!/usr/bin/env python3
"""Direct-reachability supplement to current-source campaign7391; no solver edit."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'tools/bench'))
from fixed_runtime import sha
C=ROOT/'benchmarks/runs/20260911-7393-root-fold-fuzz'
PARENT=ROOT/'benchmarks/runs/20260911-7391-direct-root-fuzz'
DEBUG=ROOT/'build/pin-7392-direct-root-fuzz-debug'
def read(p):return json.loads(p.read_text())
def main():
    assert sys.argv[1:] in (['--prepare'],['--submit'])
    if sys.argv[1:]==['--prepare']:
        assert not C.exists()
        parent=read(PARENT/'manifest.json'); build=read(PARENT/'outcomes/debug-build.json')
        assert build['status']=='PASS' and read(PARENT/'outcomes/selftest.json')['status']=='PASS'
        for rel,digest in parent['hashes'].items():assert sha(ROOT/rel)==digest,rel
        deps={}
        for p in (PARENT/'manifest.json',PARENT/'outcomes/debug-build.json',PARENT/'outcomes/selftest.json',
                  DEBUG/'tests/test_root_fold',Path(__file__)):
            deps[str(p.relative_to(ROOT))]=sha(p)
        for name in ('pins','logs','failures'):(C/name).mkdir(parents=True)
        (C/'pins/tools/checkers/bin').mkdir(parents=True)
        pairs=[(DEBUG/'tests/test_root_fold','root-fixture'),(PARENT/'pins/kissat','kissat'),
            (Path(__file__).with_name('root-fold-fuzz-7393.py'),'run.py'),
            (Path(__file__).with_name('root-fold-fuzz-solver-7393.py'),'solver.py')]
        pairs += [(ROOT/'tools/fuzz'/name,name) for name in ('fuzz.py','generators.py','modelcheck.py','shrink.py')]
        pairs += [(ROOT/'tools/checkers/bin'/name,'tools/checkers/bin/'+name) for name in ('drat-trim','lrat-check')]
        for src,name in pairs:shutil.copy2(src,C/'pins'/name);deps[str(src.relative_to(ROOT))]=sha(src)
        script=f'''#!/usr/bin/env bash
#SBATCH --job-name=7393-root-fold-fuzz
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=100
#SBATCH --mem=400000M
#SBATCH --time=05:00:00
#SBATCH --no-requeue
#SBATCH --output={C}/logs/%j.log
set -euo pipefail
exec python3 {C}/pins/run.py {C}
'''
        (C/'array.sbatch').write_text(script)
        m=dict(schema='random-root-fold-supplement-7393-v1',dependencies=deps,cases=10000,seed=739140000,
            risk='Force actual physical root folding on every generated input with isolated root units, randomized external-ID permutation/signs/root counts and live clauses. Half planted SAT, half a six-variable pigeonhole UNSAT core; alternating DRAT/native-LRAT.',
            validation='Debug invariants and compiled proof shadow through unchanged test_root_fold; exact original-CNF model/proof checks, required reference verdict and archived-root counter >=2 on every case. 2500 cases in each SAT/UNSAT x DRAT/LRAT stratum. Any failure or missing verdict/reach stops acceptance and preserves a bundle.',
            limitation='Structured lifecycle fuzz supplements broad7391; it is not a general-formula or every-API-schedule gate. This adapter is not a new policy or production change.',
            resources=dict(partition='cpu-dense-hwulab-q',logical_cpus=100,memory_mib=400000,physical_workers=50),
            test_consideration='200 deterministic generator invariant/SAT witness/UNSAT core checks and four negative reachability controls; existing five current Debug lifecycle fixtures and harness negative model/proof selftests PASS.')
        m['pin_sha256']={str(p.relative_to(C)):sha(p) for p in [*(C/'pins').rglob('*'),C/'array.sbatch'] if p.is_file()}
        with (C/'manifest.json').open('x') as out:json.dump(m,out,indent=2)
        for p in (C/'pins').rglob('*'):
            if p.is_file():p.chmod(p.stat().st_mode & ~0o222)
        print(C)
    else:
        assert not (C/'launch.json').exists();m=read(C/'manifest.json')
        for rel,digest in m['dependencies'].items():assert sha(ROOT/rel)==digest,rel
        for rel,digest in m['pin_sha256'].items():assert sha(C/rel)==digest,rel
        command=['sbatch','--parsable',str(C/'array.sbatch')]
        since=dt.datetime.now().replace(microsecond=0).isoformat()
        job=subprocess.check_output(command,text=True).strip().split(';')[0];assert job.isdigit()
        with (C/'launch.json').open('x') as out:json.dump(dict(job_id=job,accounting_since=since,command=command,manifest_sha256=sha(C/'manifest.json')),out,indent=2)
        print(job,since)
if __name__=='__main__':main()

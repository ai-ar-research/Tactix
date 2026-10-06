#!/usr/bin/env python3
import gzip, hashlib, json, os, subprocess, tempfile
from pathlib import Path
root=Path('/home/hwu/SAT-solving')
run=Path(__file__).resolve().parent
manifest=json.loads((run/'manifest.json').read_text())
index=int(os.environ['SLURM_ARRAY_TASK_ID']); batch=(1,4)[index]
out=run/f'result-{os.environ["SLURM_ARRAY_JOB_ID"]}-{index}'
out.mkdir()
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
solver=root/manifest['solver']; input_xz=root/manifest['input']; checker=root/manifest['checker']
assert digest(solver)==manifest['solver_sha256']
assert digest(input_xz)==manifest['input_xz_sha256']
assert digest(checker)==manifest['checker_sha256']
with tempfile.TemporaryDirectory(prefix='directcsp-native-',dir=os.environ.get('SLURM_TMPDIR','/tmp')) as tmp:
 cnf=Path(tmp)/'input.cnf'
 with cnf.open('wb') as stream: subprocess.run(['xz','-dc',str(input_xz)],stdout=stream,check=True)
 assert digest(cnf)==manifest['input_cnf_sha256']
 command=[str(solver),'--dsr','--controller-policy=5','--controller-native-producers=direct-csp','--controller-families=DirectCspSupport,Propagate,SemanticVariableAdvice',f'--controller-direct-csp-profile={run}/batch-{batch}.profile',f'--controller-trace={out}/controller.trace',f'--feedback={out}/feedback.jsonl.gz',f'--structural-provenance={out}/provenance.jsonl',str(cnf),str(out/'proof.dsr')]
 (out/'command.json').write_text(json.dumps(command,indent=2)+'\n')
 with (out/'stdout.log').open('wb') as stdout,(out/'stderr.log').open('wb') as stderr:
  rc=subprocess.run(['/usr/bin/time','-f','wall_seconds=%e\npeak_rss_kb=%M','-o',str(out/'resource.txt'),'timeout','--signal=TERM','--kill-after=5s','30s',*command],stdout=stdout,stderr=stderr).returncode
 result={'batch':batch,'exit_code':rc,'model_check':'not_applicable_no_SAT','proof_check':'not_attempted_no_UNSAT' if rc!=20 else 'UNSAT_requires_checker_pipeline','input_cnf_sha256':digest(cnf)}
 if rc==10:
  with (out/'checkmodel.log').open('wb') as stream: checked=subprocess.run([str(checker),str(cnf),str(out/'stdout.log')],stdout=stream,stderr=subprocess.STDOUT)
  result['model_check']='accepted' if checked.returncode==0 else 'FAILED'
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')

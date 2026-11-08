"""Small smoke tests for ER definitions followed by ordinary solve."""
from pathlib import Path
import subprocess,sys
from tempdir_cleanup import RetryingTemporaryDirectory
binary,root=map(lambda p:Path(p).resolve(),sys.argv[1:])
fixture=root/'examples/cnf/php/4-3-pairwise.cnf'
for options in [[],['--prioritize']]:
 run=subprocess.run([binary,fixture,*options],capture_output=True,text=True,timeout=5)
 assert run.returncode==20,(run.stdout,run.stderr)
 assert 'definitions=' in run.stdout and 'queries=' in run.stdout and 's UNSATISFIABLE' in run.stdout
with RetryingTemporaryDirectory(dir=Path.cwd(),prefix='php-smoke-') as temporary:
 path=Path(temporary)/'bad.cnf'
 for text in ['p cnf 1 1\n1 0\n','p dnf 3 1\n1 2 3 0\n']:
  path.write_text(text)
  run=subprocess.run([binary,path],capture_output=True,text=True,timeout=5)
  assert run.returncode==1 and 's UNSATISFIABLE' not in run.stdout

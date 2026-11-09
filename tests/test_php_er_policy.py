"""Small smoke tests for ER definitions followed by ordinary solve."""
from pathlib import Path
import subprocess, sys
from tempdir_cleanup import RetryingTemporaryDirectory
binary = Path(sys.argv[1]).resolve()
with RetryingTemporaryDirectory(dir=Path.cwd(), prefix='php-smoke-') as temporary:
 path = Path(temporary) / 'input.cnf'
 # Three pigeons and two holes; every pair in a column is exclusive.
 path.write_text('p cnf 6 9\n1 2 0\n3 4 0\n5 6 0\n-1 -3 0\n-1 -5 0\n-3 -5 0\n-2 -4 0\n-2 -6 0\n-4 -6 0\n')
 for options in [[], ['--prioritize']]:
  run = subprocess.run([binary, path, *options], capture_output=True, text=True, timeout=5)
  assert run.returncode == 20, (run.stdout, run.stderr)
  assert 'definitions=' in run.stdout and 'queries=' in run.stdout and 's UNSATISFIABLE' in run.stdout
 for text in ['p cnf 1 1\n1 0\n', 'p dnf 3 1\n1 2 3 0\n']:
  path.write_text(text)
  run = subprocess.run([binary, path], capture_output=True, text=True, timeout=5)
  assert run.returncode == 1 and 's UNSATISFIABLE' not in run.stdout

"""Tiny controls for the ER-to-default qualification client."""
from pathlib import Path
import re, subprocess, sys
from tempdir_cleanup import RetryingTemporaryDirectory
binary=Path(sys.argv[1]).resolve()
fixtures=[('p cnf 3 1\n1 3 0\n',10),
          ('p cnf 3 3\n1 0\n2 3 0\n-2 -3 0\n',10),
          ('p cnf 1 2\n1 0\n-1 0\n',20)]
with RetryingTemporaryDirectory(dir=Path.cwd(),prefix='er-client-') as temporary:
 for i,(text,verdict) in enumerate(fixtures):
  p=Path(temporary)/f'{i}.cnf';p.write_text(text)
  r=subprocess.run([binary,'--er-seed=1',p],text=True,capture_output=True,timeout=60)
  assert r.returncode==verdict,(i,r.stdout,r.stderr)
  assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
  m=re.search(r'^c default-er (.+)$',r.stdout,re.M);assert m,r.stdout
  counts={k:int(v) for k,v in (item.split('=') for item in m[1].split())}
  assert counts['requests']==counts['definitions']+counts['declined'],counts
  if i==0:assert counts['definitions'] and counts['declined'],counts
  if i==2:assert counts['requests']==0,counts

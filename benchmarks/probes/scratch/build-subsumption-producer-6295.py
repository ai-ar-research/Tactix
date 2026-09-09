#!/usr/bin/env python3
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6293-subsumption-producer-build'
p = json.loads((c/'parents.json').read_text())
source, parent = root/p['source'], root/p['parent']
assert not (c/'manifest.json').exists()
def run(name, args, cwd=None):
    with (c/'logs'/(name+'.log')).open('x') as out: subprocess.run(args,cwd=cwd,stdout=out,stderr=subprocess.STDOUT,check=True)
    print(name,'PASS',flush=True)
run('configure',['./configure'],source); run('build',['make','-j4'],source)
run('fixture-build',['gcc','-O2','-I'+str(source/'src'),'-I'+str(source/'build'),str(root/'benchmarks/probes/scratch/test-subsumption-producer-6295.c'),str(source/'build/libkissat.a'),'-lm','-o',str(c/'fixture')])
run('fixture',[str(c/'fixture')])
files = {str(f.relative_to(source)):sha(f) for f in (source/'src').rglob('*') if f.is_file()}
changed = {rel for rel,digest in files.items() if p['files'].get(rel) != digest}
assert changed == {'src/eventtrace.h','src/eventtrace.c','src/inline.h','src/makefile'}, changed
assert (source/'src/makefile').read_text() == (parent/'src/makefile').read_text().replace(str(parent),str(source))
patch = ''.join(''.join(difflib.unified_diff((parent/rel).read_text().splitlines(True),(source/rel).read_text().splitlines(True),fromfile='a/'+rel,tofile='b/'+rel,n=0)) for rel in sorted(changed-{'src/makefile'}))
(c/'native.patch').write_text(patch)
(c/'manifest.json').write_text(json.dumps(dict(source=p['source'],files=files,binary_sha256=sha(source/'build/kissat'),parent_sha256=sha(c/'parents.json'),patch_sha256=sha(c/'native.patch'),fixture=(c/'logs/fixture.log').read_text(),limitation='Pure diagnostic; must reproduce6286 native full traces and partial proof before causal interpretation.'),indent=2)+'\n')
print(sha(source/'build/kissat'))

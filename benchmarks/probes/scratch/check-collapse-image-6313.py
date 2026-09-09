#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6313-collapse-image-fixtures'
assert not c.exists();c.mkdir();records={}
for kind in ('release','bc','trace'):
    exe=root/f'build/pin-6305-substitute-collapse-{kind}/tests/test_substitute_collapse'
    command=['env','TACTIX_SUBSTITUTE_IMAGE_PATH=1',str(exe)]
    p=subprocess.run(command,cwd=c,text=True,capture_output=True)
    assert p.returncode==0,(kind,p.stdout,p.stderr)
    records[kind]=dict(command=command,binary_sha256=sha(exe),stdout=p.stdout,stderr=p.stderr)
(c/'summary.json').write_text(json.dumps(records,indent=2)+'\n');print('PASS: all3 pins x72 forced-image collapse fixtures')

"""Independent checks of native deletion certificates and SAT witnesses."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

fixture,root=map(Path,sys.argv[1:])
spec=importlib.util.spec_from_file_location('trace',root/'tools/fuzz/ipasir_trace.py')
trace=importlib.util.module_from_spec(spec);spec.loader.exec_module(trace)
for name in ('veripb','cake-pb-cnf','drat-trim','cake_lpr','dsr-trim','dsr-compress','srcheck'):
    if not (root/'tools/checkers/bin'/name).is_file():sys.exit(77)
digest=hashlib.sha256(b'tactix-permanent-input-i32le-v1\n'+struct.pack('<ii',1,0)).hexdigest()
with tempfile.TemporaryDirectory(prefix='ipasir-native-min-') as tmp:
    prefix=Path(tmp)/'q'
    result=subprocess.run([fixture,prefix],capture_output=True,text=True,timeout=45)
    assert result.returncode==0,(result.stdout,result.stderr)
    for i,status in enumerate((20,20,10,20,10)*2,1):
        stem=Path(str(prefix)+f'-{i}')
        meta=json.loads(Path(str(stem)+'.json').read_text());assert meta['result']==status
        assert meta['permanent_sha256']==digest
        for suffix,digest_member in meta['sha256'].items():
            assert hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest()==digest_member
        clauses=trace.read_cnf(Path(str(stem)+'.cnf'))
        if status==20:
            trace.check_certificate(stem,30)
            assert not trace.satisfiable(clauses)
        else:
            values={int(x) for x in Path(str(stem)+'.model').read_text().split() if x not in ('v','0')}
            assert trace.satisfies(clauses,values)
    print('Native minimization: six UNSAT certificates, four SAT witnesses and exact permanent-input digests verified after closure')

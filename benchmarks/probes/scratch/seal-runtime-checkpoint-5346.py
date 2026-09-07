#!/usr/bin/env python3
"""Preserve owned terminal summaries and small witnesses; never rewrite pins."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
RUNS = ROOT/'benchmarks/runs'

def save(path,content):
    if path.exists():
        assert path.read_text() == content,path
    else:
        path.write_text(content)

for name,script in [('5343-round-retirement','compare.py'),('5344-round-runtime','aggregate.py'),('5347-decision-stream','compare.py')]:
    c = RUNS/('20260907-'+name)
    result = subprocess.run(['python3',c/script],check=True,text=True,capture_output=True)
    save(c/'summary.json',result.stdout)

c = RUNS/'20260907-5346-round-frontier'
failed = dict(job='1668707',acceptance_met=False,scratch_clean=2,arms={},
    conclusion='Both broad observers hit the 600-second cap before the requested endpoint. Not a solver failure or timing gate. Replaced by complete lightweight stream 5347.')
for task,arm in enumerate(('tactix','kissat')):
    log = (c/'logs'/f'1668707_{task}.log').read_text()
    assert f'{arm} exit 124' in log and 'scratch_deleted True' in log
    raw = (c/'traces'/(arm+'.tsv')).read_bytes()
    rows = [line.split(b'\t') for line in raw.splitlines() if not line.startswith(b'#')]
    failed['arms'][arm] = dict(exit_code=124,rows=len(rows),
        last_recorded_conflict=int(rows[-1][3]),
        truncation_marker=b'truncated' in raw,
        trace_sha256=hashlib.sha256(raw).hexdigest())
save(c/'summary.json',json.dumps(failed,indent=2)+'\n')

validation = {}
for name in ('pin-5343-round-retirement','pin-5344-round-only','pin-5344-round-release','release'):
    build = ROOT/'build'/name
    log = build/'Testing/Temporary/LastTest.log'
    raw = log.read_bytes()
    assert b'Test Failed.' not in raw
    validation[name] = dict(passed=raw.count(b'Test Passed.'),log_sha256=hashlib.sha256(raw).hexdigest(),
        binary_sha256=hashlib.sha256((build/'tactix').read_bytes()).hexdigest())
save(RUNS/'20260907-5344-round-runtime/validation.json',json.dumps(validation,indent=2)+'\n')

for name in ('5341-warmup-reference','5343-round-retirement'):
    c = RUNS/('20260907-'+name)
    for arm in ('tactix','kissat'):
        projection = ['event\tlevel\tpayload']
        for line in (c/'traces'/(arm+'.tsv')).read_text().splitlines():
            if line.startswith('#'):
                continue
            r = line.split('\t')
            if r[3] == '75451' and r[7] == 'stable' and r[2] in ('decision','imply'):
                projection.append('\t'.join((r[2],r[6],r[-1])))
        save(c/(arm+'-warmup-semantic.tsv'),'\n'.join(projection)+'\n')
print('Terminal summaries, validation identities and selected semantic witnesses sealed.')

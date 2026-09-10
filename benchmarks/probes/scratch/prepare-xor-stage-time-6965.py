#!/usr/bin/env python3
"""One paired stage diagnostic using existing timers, isolated from timing gate."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
parent = root/'benchmarks/runs/20260910-6960-xor-presence-runtime'
c = root/'benchmarks/runs/20260910-6965-xor-stage-time'
assert not c.exists()
m = json.loads((parent/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items(): assert sha(parent/rel) == digest,rel
for group in ('dependencies','protected_campaign_hashes'):
    for rel,digest in m[group].items(): assert sha(root/rel) == digest,rel
with (parent/'matrix.tsv').open() as inp:
    rows = [r for r in csv.DictReader(inp,delimiter='\t') if int(r['task']) < 6 and r['rank'] == '47']
assert len(rows) == 2 and {r['arm'] for r in rows} == {'candidate','previous'}
for i,row in enumerate(rows): row['task'] = str(i)
for name in ('pins','logs','results','provisional','claims'): (c/name).mkdir(parents=True)
for p in (parent/'pins').iterdir():
    if p.name != 'runtime_task.py': shutil.copy2(p,c/'pins'/p.name)
runner = (parent/'pins/runtime_task.py').read_text()
anchor = '        affinity = sorted(os.sched_getaffinity(0))'
assert runner.count(anchor) == 1
runner = runner.replace(anchor,'        os.environ["TACTIX_CONG_TIME"] = "1"\n'+anchor)
anchor = '"solver_stderr_tail": tail(stderr),'
assert runner.count(anchor) == 1
runner = runner.replace(anchor,'"solver_stderr_tail": tail(stderr, 256),')
(c/'pins/runtime_task.py').write_text(runner)
with (c/'matrix.tsv').open('x',newline='') as out:
    writer = csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n')
    writer.writeheader();writer.writerows(rows)
batch = (parent/'array.sbatch').read_text().replace(str(parent),str(c))
assert batch.count('--array=0-17') == 1
batch = batch.replace('--array=0-17','--array=0-1').replace('6960-xor-presence-runtime','6965-xor-stage-time')
(c/'array.sbatch').write_text(batch)
manifest = dict(schema='tactix-existing-congruence-stage-pair-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=m['arms'],authorities=m['authorities'],input_sha256={rows[0]['instance']:m['input_sha256'][rows[0]['instance']]},
    collect_runtime_context=True,resource_contract=dict(m['resource_contract'],array='0-1'),
    protected_campaign_hashes=m['protected_campaign_hashes'],
    work_oracles={arm:{'47':m['work_oracles'][arm]['47']} for arm in m['arms']},
    dependencies={str(p.relative_to(root)):sha(p) for p in (Path(__file__),parent/'manifest.json',root/'tools/bench/congruence_timing.py',root/'tools/bench/test_congruence_timing.py')},
    diagnostic_environment={'TACTIX_CONG_TIME':'1'},expected_timer_rows=5,
    hypothesis='Existing ternary-presence reuse reduces XOR extraction time; observe index/ITE/closure/other stages for transfer costs. Same-work replay mandatory.',
    validation='Both expected UNSAT, exact complete6730 non-timing statistics and certificate bytes/hash, five cumulative rows per arm, equal cumulative items, independent proof checks and cleanup. Missing/malformed rows invalidate diagnostic.',
    test_consideration='Existing8 cumulative-timer tests cover exact integer fields, cardinality, malformed/decreasing rows, overlaps and unrelated stderr. Same validated6535 environment injection after sanitation and256-line tail; no solver source change. Exact work/certificate gate protects instrumentation neutrality.',
    limitation='Single instrumented wall-time pair, not repeated uninstrumented performance acceptance. Stages overlap; do not sum all fields or infer native unit prices. Existing campaigns and fixed reference unchanged; proofs asynchronous, no promotion.')
manifest['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out: json.dump(manifest,out,indent=2);out.write('\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

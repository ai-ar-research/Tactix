#!/usr/bin/env python3
"""A bounded ownership attribution pair independent of queued full checking."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
template=root/'benchmarks/runs/20260908-5650-propagation-cursor-rank33-prefix'
full=root/'benchmarks/runs/20260908-5664-propagation-cursor-ranks1-40'
c=root/'benchmarks/runs/20260908-5702-rank39-memory-prefix'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8<<20),b''):h.update(block)
    return h.hexdigest()
m=json.loads((template/'manifest.json').read_text())
assert sha(template/'pins/candidate')==m['hashes']['pins/candidate']
with (full/'matrix.tsv').open() as stream:
    selected=[r for r in csv.DictReader(stream,delimiter='\t') if r['rank']=='39' and r['arm']=='tactix']
assert len(selected)==1
score=json.loads((full/'solver-checkpoint.json').read_text())['per_rank']['39']['tactix']
runner=(template/'run.py').read_text()
runner=runner.replace("('candidate', 'kissat')", "('candidate', 'control')")
runner=runner.replace("prefix = 'KISSAT' if arm == 'kissat' else 'TACTIX'", "prefix = 'TACTIX'")
runner=runner.replace('tactix-5650-','tactix-5702-')
start=runner.index("selector = 'congruence_end'")
end=runner.index("assert not any(key.endswith('_EVENT_TRACE') for key in env)",start)
runner=runner[:start]+"""if arm == 'candidate':
    env['TACTIX_SLOT_MEMORY_TRACE'] = str(c / 'traces' / 'candidate.memory.tsv')
"""+runner[end:]
compile(runner,str(c/'run.py'),'exec')
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
for arm in ('candidate','control'):shutil.copy2(template/'pins/candidate',c/'pins'/arm)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((template/'array.sbatch').read_text().replace(str(template),str(c)))
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    input='benchmarks/cache/'+selected[0]['instance'],stop_conflict=100000,stream_from=0,common_decisions=1,
    parent_campaign=str(full.relative_to(root)),parent_checkpoint_sha256=sha(full/'solver-checkpoint.json'),
    full_release_record=score,arms=['candidate','control'],
    hypothesis='Queued full ownership5687 cannot yet provide evidence. Bound early Rank39 memory owners through100K conflicts; paired identical trace binaries with and without SLOT_MEMORY_TRACE establish unchanged logical work/proof prefix while full solve/check remains independently queued.',
    validation='Two clean stopped diagnostics,identical candidate/control decision,boundary,macro,prefix streams and partial-proof bytes. Strict memory schema and trace/pin/input hashes. No complete verdict or full peak/runtime acceptance claim.',
    test_consideration='No source or checker changes. Reuse parent5649 trace suite88/88 and tested slot observer; parser5688 has11 fixtures. Exact paired replay validates this bounded configuration. No solver unit-test change needed.',
    limitation='Early phase ownership only; absence of a peak before100K is not absence later. No certificate verdict/check is claimed for these deliberately stopped prefixes. Full5687 remains queued,not replaced.')
m['input_sha256_compressed']=sha(root/m['input'])
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
command=['sbatch','--parsable',str((c/'array.sbatch').relative_to(root))]
job=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip();assert job.isdecimal()
(c/'submission.json').write_text(json.dumps(dict(job=job,command=command,manifest_sha256=sha(c/'manifest.json')),indent=2)+'\n')
print('Submitted bounded Rank39 ownership/control',job)

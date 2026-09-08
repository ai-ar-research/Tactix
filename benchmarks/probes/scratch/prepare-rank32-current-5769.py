#!/usr/bin/env python3
"""Probe the new largest loss with one current candidate and cached native100K."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
runs=root / 'benchmarks/runs'
template=runs / '20260908-5755-reuse-target-rank31-replay'
native=runs / '20260908-5597-factor-support-rank32-prefix'
full=runs / '20260908-5758-reuse-target-ranks1-40'
c=runs / '20260908-5769-reuse-target-rank32-prefix'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''):h.update(block)
    return h.hexdigest()
loss=json.loads((full / 'next-losses.json').read_text())['losses'][0]
assert loss['rank']==32 and not loss['same_parent_certificate']
m=json.loads((template / 'manifest.json').read_text())
n=json.loads((native / 'manifest.json').read_text())
nr=json.loads((native / 'results/kissat.json').read_text())
assert nr['exit_code']==0 and nr['scratch_deleted']
for rel,digest in nr['trace_sha256'].items():assert sha(native / 'traces' / rel)==digest
for folder,manifest in ((template,m),(native,n)):
    for rel,digest in manifest['hashes'].items():assert sha(folder / rel)==digest
c.mkdir(exist_ok=False)
for folder in ('pins','logs','traces','results'):(c / folder).mkdir()
shutil.copy2(template / 'pins/candidate',c / 'pins/candidate')
(c / 'run.py').write_text((template / 'run.py').read_text().replace('tactix-5755-','tactix-5769-'))
(c / 'array.sbatch').write_text((template / 'array.sbatch').read_text().replace(str(template),str(c)))
for key in ('input','input_sha256_compressed','stop_conflict','stream_from','common_decisions'):m[key]=n[key]
m.update(parent_campaign=str(template.relative_to(root)),cached_native_campaign=str(native.relative_to(root)),
    cached_controls=dict(kissat=dict(record=str((native / 'results/kissat.json').relative_to(root)),sha256=sha(native / 'results/kissat.json'))),
    selection=dict(path=str((full / 'next-losses.json').relative_to(root)),sha256=sha(full / 'next-losses.json')),
    hypothesis='Current Rank32 is the largest fixed-reference loss:633.485/274.456s,versus316.715s previous5672 with changed work/proof. Localize first common-prefix divergence against existing validated native100K before attributing it to a repair.',
    validation='One clean100K current stop;complete stream and immutable hashes;compare every portable row against cached native100K. No native rerun,no complete-verdict or timing claim.',
    test_consideration='Configuration-only use of unchanged tested trace5742 and stopped runner. Existing complete-row/control hash checks;candidate-only replay5755/5756 proves target-dispatch neutrality on two prior witnesses.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c / 'run.py',c / 'array.sbatch',c / 'pins/candidate']}
(c / 'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(c,sha(c / 'manifest.json'))

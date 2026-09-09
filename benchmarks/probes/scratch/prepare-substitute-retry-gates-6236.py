#!/usr/bin/env python3
"""Independent BC and Rank32 successor campaigns; no existing runner edits."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
authority = root/'benchmarks/runs/20260909-6231-substitute-retry-build'
b = json.loads((authority/'manifest.json').read_text())
for rel,digest in b['binaries'].items(): assert sha(root/rel) == digest
assert (root/'benchmarks/runs/20260909-6234-substitute-retry-references/summary.json').exists()
old = root/'benchmarks/runs/20260909-6171-learned-sort-BC-gate'
c = root/'benchmarks/runs/20260909-6236-substitute-retry-BC-gate'
assert not c.exists()
m = json.loads((old/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items(): assert sha(old/rel) == digest
for name in ('pins','logs','results','provisional','claims'): (c/name).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file() and not p.name.startswith('candidate'): shutil.copy2(p,c/'pins'/p.name)
shutil.copy2(old/'matrix.tsv',c/'matrix.tsv')
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
for tier,kind in (('B','release'),('C','bc')):
    binary = root/f'build/pin-6231-substitute-retry-{kind}/tactix'
    arm = 'candidate_'+tier; shutil.copy2(binary,c/'pins'/arm)
    m['arms'][arm].update(sha256=sha(binary),source_build=str(binary.parent.relative_to(root)),source_patch_sha256=sha(authority/'cumulative.patch'))
shutil.copy2(authority/'cumulative.patch',c/'pins/candidate.patch')
m.update(predecessor=str(old.relative_to(root)),candidate_authority=str(authority.relative_to(root)),candidate_source=b['source'],
    purpose='B28+C5 gate for canonical Substitute removal publication; all verdicts independently checked.',
    hypothesis='Reference-backed retry repair may change search work. No exact parent work or speedup assumed.',test_consideration=b['test_consideration'])
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

old = root/'benchmarks/runs/20260909-6183-rank32-current-prefix'
c = root/'benchmarks/runs/20260909-6237-substitute-retry-rank32-prefix'
assert not c.exists()
for name in ('pins','logs','results','traces'): (c/name).mkdir(parents=True)
m = json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel) == digest
binary = root/'build/pin-6231-substitute-retry-trace/tactix'
shutil.copy2(binary,c/'pins/candidate')
runner=(old/'run.py').read_text().replace('tactix-6183-','tactix-6237-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor)==1
runner=runner.replace(anchor,"env['TACTIX_RETRY_PRODUCER_TRACE'] = str(c / 'traces/candidate.retry-producer.tsv')\nenv['SAT_RETRY_PRODUCER_FROM'] = '0'\nenv['SAT_RETRY_PRODUCER_TO'] = '386968'\nenv['SAT_RETRY_PRODUCER_EXTERNAL'] = '337674'\n"+anchor)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('--array=0-1','--array=0').replace('6183-rank32','6237-rank32'))
m.update(candidate_authority=dict(path=str((authority/'manifest.json').relative_to(root)),sha256=sha(authority/'manifest.json')),
    purpose='Candidate-only causal replay of canonical Substitute retry repair against cached native6183; no reference timing rerun.',
    hypothesis='Restored retry producer closes Rank32 missing schedule members; first check inherited300K hash, then full suffix and portable boundaries.',
    validation='Require requested1M stop/cleanup; compare69828932-row hash10d4b4ca139060f5 before inheriting300K closure. If hash differs, cached early closure is invalid for this source and next diagnostic must localize earlier. No parent proof identity assumed after semantic repair.',
    test_consideration=b['test_consideration'],scope='Trace pin corresponds exactly to new6231 source, not semantic6107.',
    cached_native=dict(path=str((old/'results/kissat.json').relative_to(root)),sha256=sha(old/'results/kissat.json')))
m['resource_contract']['array']='0'
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(c/'pins/candidate').chmod((c/'pins/candidate').stat().st_mode & ~0o222)
print(c)

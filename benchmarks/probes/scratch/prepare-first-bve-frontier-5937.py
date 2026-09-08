#!/usr/bin/env python3
"""Bracket the earlier formula divergence on the retained independent5900 pin."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
parent=root/'benchmarks/runs/20260908-5932-second-forward-end-formula'
current=root/'benchmarks/runs/20260908-5907-reduce-root-rank32-prefix'
c=root/'benchmarks/runs/20260908-5937-first-bve-frontier'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((parent/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(parent/rel)==digest
cm=json.loads((current/'manifest.json').read_text())
for rel,digest in cm['hashes'].items(): assert sha(current/rel)==digest
c.mkdir(exist_ok=False)
for folder in ('pins','logs','results','traces'): (c/folder).mkdir()
shutil.copy2(current/'pins/candidate',c/'pins/candidate')
shutil.copy2(parent/'pins/kissat',c/'pins/kissat')
runner=(parent/'run.py').read_text().replace('tactix-5932-','tactix-5937-')
assert runner.count("SAT_EVENT_TRACE_FORMULA_SET_ORDINAL='1'")==1
runner=runner.replace("SAT_EVENT_TRACE_FORMULA_SET_ORDINAL='1'","SAT_EVENT_TRACE_FORMULA_SET_ORDINAL='0'")
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor)==1
extra="""env[prefix + '_BVE_CANDIDATE_TRACE'] = str(c / 'traces' / (arm + '.bve-candidates.tsv'))
env['SAT_BVE_CANDIDATE_CONFLICT'] = '271079'
env.update(SAT_EVENT_TRACE_BINARY_SET_PATH=str(c / 'traces' / (arm + '.binaries.tsv')),
           SAT_EVENT_TRACE_BINARY_SET_MATCH='name=Strengthen|stage=end',
           SAT_EVENT_TRACE_BINARY_SET_CONFLICT='271079')
"""
(c/'run.py').write_text(runner.replace(anchor,extra+anchor))
batch=(parent/'array.sbatch').read_text().replace(str(parent),str(c)).replace('5932-forward-end','5937-first-bve-frontier')
(c/'array.sbatch').write_text(batch)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),parent_campaign=str(current.relative_to(root)),
    candidate_authority=cm['candidate_authority'],candidate_source=cm['candidate_source'],source_patch_sha256=cm['source_patch_sha256'],
    formula_stage='end',formula_ordinal=0,
    hypothesis='Second-forward entry already has172 different clauses per arm. Compare full first-forward exit large/binary multisets before firstBVE and every BVE candidate at271079 to separate inherited formula drift from pivot/order/application drift.',
    validation='Every ordinary300K trace hash andpartial proof must replay cached5907 candidate and5771 native. First-forward exit formula snapshot ordinal0 required;binary snapshots cover each forward exit;candidate rows cover bothBVE rounds without early-stop selectors.',
    native_rerun_reason='Cached native lacks first-forward exit formula/binaries andcomplete BVE candidate sequence at271079. Causal data only;fixed runtime reference unchanged.',
    test_consideration='Read-only configuration;unchanged full-tested5900/native pins. Existing formula/ordinal parser fixtures retained. Validate field schema,per-arm ordinary replay andboundary counts;no solver implementation orunit change.',
    limitation='Independent5900 is development base.5920 regresses full1.176794x/C1.017340x andis not retained as successor. Its diagnostic window equals5900 exactly;all old artifacts immutable.')
m['cached_controls']=dict(m['cached_controls'])
record=current/'results/candidate.json'
m['cached_controls']['candidate']=dict(record=str(record.relative_to(root)),sha256=sha(record))
m['hashes']={str(p.relative_to(c)):sha(p) for p in (c/'run.py',c/'array.sbatch',c/'pins/candidate',c/'pins/kissat')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode&~0o222)
print(c)

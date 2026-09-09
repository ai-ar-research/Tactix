#!/usr/bin/env python3
"""Existing-pin all-source census with exact current-parent replay required."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6321-collapse-rank32-prefix'
native=root/'benchmarks/runs/20260909-6303-rank32-later-prefix'
s=json.loads((old/'summary.json').read_text());assert s['matching_decisions_from_start']==108987868
near=s['last_boundaries_before_first_decision_difference']
for arm,expected in (('candidate',95937),('kissat',95936)):
    rows=[r for r in near[arm] if r['name']=='TransitiveReduce' and r['stage']=='end']
    assert len(rows)==1 and rows[0]['conflicts']==1368343 and rows[0]['binary']==expected
m=json.loads((old/'manifest.json').read_text())
for binding in (m['candidate_authority'],m['native_authority']):assert sha(root/binding['path'])==binding['sha256']
a=json.loads((root/m['candidate_authority']['path']).read_text())
for rel,digest in a['files'].items():assert sha(root/a['source']/rel)==digest
c=root/'benchmarks/runs/20260909-6340-rank32-transitive-census';assert not c.exists()
for name in ('pins','traces','results','logs'):(c/name).mkdir(parents=True)
for arm,parent in (('candidate',old),('kissat',native)):
    r=json.loads((parent/'results'/f'{arm}.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and sha(parent/'pins'/arm)==r['binary_sha256']
    shutil.copy2(parent/'pins'/arm,c/'pins'/arm)
runner=(old/'run.py').read_text().replace('tactix-6321-','tactix-6340-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)";assert runner.count(anchor)==1
extra="""env[prefix + '_TRANSITIVE_TRACE'] = str(c / 'traces' / (arm + '.transitive.tsv'))
# Unmatched selector only initializes the existing logger; ALL selects every
# actual source/candidate. It does not enter the solver formula or policy.
env[prefix + '_TRANSITIVE_TRACE_PAIRS'] = '2147483646,2147483647'
env[prefix + '_TRANSITIVE_TRACE_ALL'] = '1'
"""
runner=runner.replace(anchor,extra+anchor);compile(runner,str(c/'run.py'),'exec')
(c/'run.py').write_text(runner)
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6321-collapse-prefix','6340-transitive-census').replace('--array=0\n','--array=0-1\n')
assert '--array=0-1' in batch;(c/'array.sbatch').write_text(batch)
m['resource_contract']['array']='0-1'
m.update(parent_runs={'candidate':str(old.relative_to(root)),'kissat':str(native.relative_to(root))},
    parent_summary={'path':str((old/'summary.json').relative_to(root)),'sha256':sha(old/'summary.json')},
    diagnostic_conflict=1368343,
    scope='Unchanged6305/6090 trace pins; new run configuration only, no production or old campaign mutation.',
    hypothesis='The first current post-1M count discrepancy in TransitiveReduce reflects source/candidate order, traversal outcome, or effort cut-off; identify exact differing candidate before a repair.',
    validation='Require exact every recorded ordinary current-parent trace and per-arm partialproof through3M; the only added trace is transitive.tsv. Then strictly parse source/candidate framing and compare phase1368343. No full-formula equivalence or timing claim.',
    test_consideration='Existing actual observer implementations and immutable binaries reused. Add malformed/framing/parser controls before reading the new census;39benchmark13SLURM tests already pass. All-source logging avoids guessing the missing binary.',
    limitation='Partial certificates are hash-bound/deleted, not final-proof acceptance. Native diagnostic invocation does not refresh fixed timing reference. All-phase trace may add overhead;1200solver cap unchanged.')
m['hashes']={name:sha(c/name) for name in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)

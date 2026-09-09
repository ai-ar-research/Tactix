#!/usr/bin/env python3
"""Observe the first Rank45 differing transformation with exact replay control."""
import csv
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from transitive_trace import binary_drops
old=root/'benchmarks/runs/20260909-6402-rank45-stamp-prefix'
c=root/'benchmarks/runs/20260909-6410-rank45-transitive-census'; assert not c.exists()
s=json.loads((old/'summary.json').read_text())
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest
assert s['pairs']['previous_kissat']['complete_equal']
assert s['pairs']['candidate_previous']['matching_rows']==160836
for arm in ('candidate','previous','kissat'):
    lines=(old/'traces'/f'{arm}.boundaries.tsv').read_text().splitlines()
    rows=list(csv.DictReader(lines[2:],fieldnames=lines[1][2:].split('\t'),delimiter='\t'))
    drops=binary_drops(rows,'transitive' if arm=='kissat' else 'TransitiveReduce',76464)
    assert [r['drop'] for r in drops]==([32,0] if arm=='candidate' else [32,5])
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest
for name in ('pins','logs','results','traces'): (c/name).mkdir(parents=True)
for p in (old/'pins').iterdir(): shutil.copy2(p,c/'pins'/p.name)
runner=(old/'run.py').read_text().replace('tactix-6402-','tactix-6410-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor)==1
extra="""env[prefix + '_TRANSITIVE_TRACE'] = str(c / 'traces' / (arm + '.transitive.tsv'))
env[prefix + '_TRANSITIVE_TRACE_PAIRS'] = '2147483646,2147483647'
env[prefix + '_TRANSITIVE_TRACE_ALL'] = '1'
"""
runner=runner.replace(anchor,extra+anchor); compile(runner,str(c/'run.py'),'exec')
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6402-rank45-stamp-prefix','6410-rank45-transitive'))
m.update(parent_summary=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')),
    parent_runs={arm:str(old.relative_to(root)) for arm in ('candidate','previous','kissat')},diagnostic_conflict=76464,
    purpose='At second TransitiveReduce76464,6357 drops0 binaries versus5 in6305/native. Identify exact missing candidates/order/effort using existing all-source instrumentation.',
    validation='Before interpreting ALL-source trace, require exact every ordinary6402 trace and each own stopped-proof bytes/hash through100K. Account for BOTH76464 invocations:32/0 versus32/5, not just the final call.',
    test_consideration='Same pinned logger/protocol, no observer source edit. Existing transitive framing/deletion/multiple-invocation tests plus65benchmark tests cover parsing. Explicit multiset and full-replay assertions bind this three-arm experiment.',
    limitation='First transformation-count difference and observed candidate omissions are localization, not full-graph equivalence or causal repair. Partial proofs are not final acceptance; fixed native timing vector unchanged.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(c)

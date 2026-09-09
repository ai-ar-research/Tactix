#!/usr/bin/env python3
"""Select the census-proved one-sided binary for existing admission/walk logs."""
import json
from pathlib import Path
import shutil
import sys

root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6340-rank32-transitive-census'
summary=json.loads((old/'summary.json').read_text())
assert summary['parent_replay'] and not summary['candidate_only_reductions']
extra=summary['kissat_only_reductions']
assert len(extra)==1 and extra[0]['copies']==1
pair=extra[0]['pair'];assert pair==[-30097,936]
for rel,digest in summary['hashes'].items():assert sha(root/rel)==digest,rel
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(old/rel)==digest,rel
c=root/'benchmarks/runs/20260909-6345-rank32-selected-transitive';assert not c.exists()
for name in ('pins','traces','results','logs'):(c/name).mkdir(parents=True)
for arm in ('candidate','kissat'):shutil.copy2(old/'pins'/arm,c/'pins'/arm)
runner=(old/'run.py').read_text().replace('tactix-6340-','tactix-6345-')
begin=runner.index('# Unmatched selector')
end=runner.index("assert not any(key.endswith('_EVENT_TRACE')",begin)
runner=runner[:begin]+"# Selected-pair diagnostics only; no all-source census.\nenv[prefix + '_TRANSITIVE_TRACE_PAIRS'] = '-30097,936'\n"+runner[end:]
compile(runner,str(c/'run.py'),'exec');(c/'run.py').write_text(runner)
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6340-transitive-census','6345-transitive-pair')
(c/'array.sbatch').write_text(batch)
m.update(parent_runs={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    parent_summary=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')),
    diagnostic_pair=pair,
    hypothesis='For binary936 OR -30097 at1368343, distinguish ownership/admission, source effort cutoff, and differing graph traversal using existing selected-pair logs.',
    validation='All non-transitive current6340 ordinary trace hashes and partial proof must replay exactly per arm through3M. Transitive trace intentionally changes from census to selected endpoints/admission/walk. Verify complete schema/row framing, then localize actual pair events; no timing or final certificate acceptance.',
    test_consideration='Existing unchanged observer pins reused;51 benchmark tests and13SLURM tests pass, including12 census/boundary tests. Add selected-event parser tests before interpreting the new format. No solver behavior change.',
    scope='Config-only successor6345; original6340/6332/cpu-q/fixed references unchanged.',
    limitation='No promotion or causal fix from localization alone; preserve partial-proof cleanup and continue final checkers asynchronously.')
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)

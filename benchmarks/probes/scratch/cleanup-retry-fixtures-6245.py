#!/usr/bin/env python3
"""Remove only reproduced fixture setup artifacts, retaining hashes and logs."""
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
good=json.loads((root/'benchmarks/runs/20260909-6244-substitute-retry-fixed-proofs/summary.json').read_text())
control=json.loads((root/'benchmarks/runs/20260909-6242-input-id-control/summary.json').read_text())
assert good['checked']==12 and control['parent_candidate_bad_proofs_identical']
target=root/'benchmarks/reports/2026-09-09-retry-fixture-cleanup-6245.json'
assert not target.exists()
names=[
 'benchmarks/runs/20260909-6241-substitute-retry-fixed-proofs/1-1-0.cnf',
 'benchmarks/runs/20260909-6241-substitute-retry-fixed-proofs/1-1-0.proof',
 'benchmarks/runs/20260909-6242-input-id-control/input.cnf',
 'benchmarks/runs/20260909-6242-input-id-control/bad.proof',
 'benchmarks/runs/20260909-6243-substitute-retry-fixed-proofs/2-0-0.cnf',
 'benchmarks/runs/20260909-6243-substitute-retry-fixed-proofs/2-0-0.proof']
records={}
for rel in names:
    p=root/rel; assert p.is_file() and p.stat().st_size<100000
    records[rel]=dict(bytes=p.stat().st_size,sha256=sha(p))
target.write_text(json.dumps(dict(removed=records,reason='Known fixture-only setup failures, reproduced on parent and corrected by6244 twelve checked proofs. Raw checker outputs and exact fixture sources retained; these are not benchmark campaign artifacts.',recoverability='Reproducible from pinned fixture/source and commands; deleted files not moved to trash.'),indent=2)+'\n')
for rel in names: (root/rel).unlink()
assert all(not (root/rel).exists() for rel in names)
print('Removed',len(names),'reproducible fixture artifacts;',sum(r['bytes'] for r in records.values()),'bytes;hashes/logs retained')

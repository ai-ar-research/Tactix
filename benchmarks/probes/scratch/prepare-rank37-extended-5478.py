#!/usr/bin/env python3
"""Continue the verified Rank-37 common prefix without changing solver code."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5463-rank37-bound-prefix'
c = root / 'benchmarks/runs/20260908-5478-rank37-bound-extended'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
prior = json.loads((old / 'summary.json').read_text())
assert prior['clean_stops'] == 2 and prior['complete_prefix_identical']
assert prior['stop_conflict'] == 100000 and prior['matching_rows'] == 35346388
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text((old / 'run.py').read_text().replace("'tactix-5463-'", "'tactix-5478-'"))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)),
    prior_summary_sha256=sha(old / 'summary.json'),
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    stop_conflict=1000000, stream_from=100000,
    hypothesis='Bound repair matches every decision through 100000 conflicts, but full-run Rank-37 work remains unresolved. Continue from that proved common prefix to localize any later mechanism before selecting a repair; no inference from elapsed job time.',
    validation='Same immutable trace pins/input as validated 5463. Require two clean one-million-conflict stops and complete suffix streams from conflict 100000, plus whole-prefix digests and trace hashes. Earlier 35346388-row common prefix is separately authoritative.',
    limitation='Diagnostic-only partial proof, no full verdict/proof check or acceptance timing. Stream matching_rows counts only the recorded suffix, not all decisions since startup.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

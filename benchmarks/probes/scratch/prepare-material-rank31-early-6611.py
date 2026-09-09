#!/usr/bin/env python3
"""Collect missing current0..300K rows after a verified earlier-prefix change."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6605-material-rank31-prefix'
c = root / 'benchmarks/runs/20260909-6611-material-rank31-early'; assert not c.exists()
summary = old / 'summary-6607.json'; s = json.loads(summary.read_text())
assert s['status'] == 'earlier_prefix_changed'
for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest, rel
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest, rel
for folder in ('pins', 'logs', 'traces', 'results'): (c / folder).mkdir(parents=True)
shutil.copy2(old / 'pins/candidate', c / 'pins/candidate')
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-6605-', 'tactix-6611-'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6605-material-rank31-prefix', '6611-material-rank31-early'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), stop_conflict=300000, stream_from=0,
    cached_native=m['cached_common'],
    diagnosis=dict(path=str(summary.relative_to(root)), sha256=sha(summary)),
    hypothesis='Current6579 changes the inherited300K hash in6607. Collect all earlier decisions against cached5735 to locate the first actual split; coarse BVE end first differs at179823, not proof of cause.',
    validation='Require clean300K stop, exact pin/input, full trace hashes/row accounting and compare every0..300K row against cached native5735. No common-prefix equality assumed. Keep179823 BVE/count difference distinct from earliest changed decision.',
    test_consideration='Same source/binary/runner, only missing earlier stream selection. Existing full-stream accounting, modulo-reset and chunk-framing tests; retain new suffix tests without applying suffix assumptions to this complete stream.',
    limitation='Missing earlier diagnostic data, not a repeated acceptance measurement. No new native run, proof acceptance or source edit. Partial certificate hashed/deleted.')
m['dependencies'][str(summary.relative_to(root))] = sha(summary)
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)

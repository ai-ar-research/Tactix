#!/usr/bin/env python3
"""Continue Rank33 beyond its completely validated100K-conflict common prefix."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5558-rank33-small-prefix'
c = root / 'benchmarks/runs/20260908-5567-rank33-small-extended'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576), b''): h.update(block)
    return h.hexdigest()
prior = json.loads((old / 'summary.json').read_text())
assert prior['clean_stops'] == 2 and prior['complete_prefix_identical']
assert prior['stop_conflict'] == 100000 and prior['matching_rows'] == 31494584
m = json.loads((old / 'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old / rel) == digest
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5558-', 'tactix-5567-'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)), prior_summary_sha256=sha(old / 'summary.json'),
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    stop_conflict=1000000, stream_from=100000, common_decisions=31494584,
    hypothesis='All31494584 decisions through100K agree but full Rank33 has24.46% extra conflicts and22.88% extra propagations. Locate the later first split before selecting a general fidelity repair.',
    validation='Two clean1M-conflict stops and hashed complete suffix streams from100K. The common31494584-literal prefix hash and final literal must match the full validated5558 prefix per arm. Validate all prior portable boundary rows through100K. No timing or complete verdict claim.',
    limitation='matching_rows counts the recorded suffix, which includes conflict100K. The prior whole-prefix hash is an additional replay check, not a replacement for the earlier full-row comparison.',
    test_consideration='Configuration-only reuse of immutable trace pins and tested runner. Existing full-stream comparator plus explicit common-prefix and boundary replay assertions validate the extension; no solver unit-test change.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

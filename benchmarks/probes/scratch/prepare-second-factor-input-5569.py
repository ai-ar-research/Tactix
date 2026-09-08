#!/usr/bin/env python3
"""Select the second Factor pass at19375, retaining clause pose and flags."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5565-rank32-factor-input'
c = root / 'benchmarks/runs/20260908-5569-rank32-second-factor-input'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''): h.update(block)
    return h.hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for name,digest in m['hashes'].items(): assert sha(old / name) == digest
runner = (old / 'run.py').read_text().replace('tactix-5565-', 'tactix-5569-')
anchor = "            'SAT_EVENT_TRACE_FORMULA_SET_CONFLICT': '19375'"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, anchor + ",\n            'SAT_EVENT_TRACE_FORMULA_SET_ORDINAL': '1',\n            'SAT_EVENT_TRACE_FORMULA_SET_POSE': '1',\n            'SAT_EVENT_TRACE_FORMULA_SET_META': '1'")
compile(runner,str(c / 'run.py'),'exec')
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'): (c/name).mkdir()
for arm in ('candidate','kissat'): shutil.copy2(old/'pins'/arm,c/'pins'/arm)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    first_input_campaign=str(old.relative_to(root)),
    hypothesis='Two Factor passes share19375. The first incoming large multisets agree at586000irred/2420red. Capture ordinal1 (553534irred/2013red) at the pass preceding the13-clause split, including actual literal pose and metadata; do not infer this input from the first-pass snapshot.',
    validation='Full parent5537 replay and proof SHA, both binary snapshots equal5565, exactly553534irred+2013red live large clauses, pose multiset equals sorted literals, explicit metadata framing. Immutable source; no complete verdict or acceptance timing.',
    test_consideration='No solver change. Explicit nonzero snapshot ordinal plus repeated-boundary/parser fixtures and full parent replay prevent conflating the two same-conflict passes.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

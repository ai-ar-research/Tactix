#!/usr/bin/env python3
"""Observe the current Congruence return path without changing solver code."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6648-rank49-binary-life'
c = root / 'benchmarks/runs/20260909-6651-rank49-congruence-stages'
assert not c.exists()
s = json.loads((old / 'summary-6650.json').read_text())
assert s['full_own_replay']
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
m = json.loads((old / 'manifest.json').read_text())
runner = (old / 'run.py').read_text().replace('tactix-6648-', 'tactix-6651-')
runner = runner.replace("arm = ('candidate', 'kissat')[int(sys.argv[1])]", "assert int(sys.argv[1]) == 0\narm = 'candidate'")
anchor = '\ndef interrupted(signum, frame):'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "\nenv['TACTIX_CONGRUENCE_MEMORY_TRACE'] = str(c/'traces'/'candidate.congruence-memory.tsv')\nenv['TACTIX_CONGRUENCE_BINARY_TRACE'] = str(c/'traces'/'candidate.congruence-binaries.tsv')\nenv['TACTIX_CONGRUENCE_BINARY_TRACE_CONFLICT'] = '2988'\nenv['TACTIX_SLOT_MEMORY_TRACE'] = str(c/'traces'/'candidate.slot-memory.tsv')\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
for folder in ('pins', 'logs', 'results', 'traces'):
    (c / folder).mkdir(parents=True)
shutil.copy2(old / 'pins/candidate', c / 'pins/candidate')
for name in ('summary-6650.json', 'results/candidate.json'):
    p = old / name
    m['dependencies'][str(p.relative_to(root))] = sha(p)
r = json.loads((old / 'results/candidate.json').read_text())
for name, digest in r['trace_sha256'].items():
    m['dependencies'][str((old / 'traces' / name).relative_to(root))] = digest
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6648-binary-life', '6651-congruence-stages').replace('--array=0-1', '--array=0'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), cached_controls=str(old.relative_to(root)),
    purpose='Determine whether current skips the second Congruence dense entry (including the gates.empty early return) or enters it with different values. Native6648 already records second-entry319 to second313 at2988; no native rerun needed.',
    validation='Require every6648 candidate trace and partial certificate exactly; parse bounded stage records and selected binary populations. Memory fields are observer metadata, not a memory performance comparison.',
    test_consideration='Existing observer configuration only;190 benchmark tests apply. New interpretation needs structural checks. No semantic source change, fuzz launch, reference refresh or promotion.')
m['hashes'] = {name: sha(c / name) for name in ('pins/candidate', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)

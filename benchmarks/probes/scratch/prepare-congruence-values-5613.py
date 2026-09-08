#!/usr/bin/env python3
"""Inspect root values and retained garbage for the five prior Congruence clauses."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5611-current-congruence-output'
c = root / 'benchmarks/runs/20260908-5613-congruence-clause-values'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for name, digest in m['hashes'].items(): assert sha(old / name) == digest
runner = (old / 'run.py').read_text().replace('tactix-5611-', 'tactix-5613-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env.update({'SAT_EVENT_TRACE_FORMULA_SET_EXTERNALS': '236195,236197',
            'SAT_EVENT_TRACE_FORMULA_SET_VALUES': '1',
            'SAT_EVENT_TRACE_FORMULA_SET_INCLUDE_GARBAGE': '1',
            'SAT_EVENT_TRACE_FORMULA_SET_POSE': '1'})
""" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)), parent_manifest_sha256=sha(old / 'manifest.json'),
    hypothesis='Both native lineages garbage root-satisfied XOR/ITE-index clauses; Tactix only omits them from its read-only index. Determine whether the five retained ternaries are root-satisfied, or instead require another representative/subsumption explanation.',
    validation='Require complete per-arm5611 decisions/prefix/boundaries/binaries/partial-proof identity; only selected formula format changes. Preserve garbage flags, literal values and physical pose. No performance or deletion-cause claim from clause absence alone.',
    test_consideration='Configuration-only existing observers; new strict value/pose/garbage parser fixtures and full parent replay checks are required in5614. No solver unit-test change.')
m.pop('parent_summary_sha256', None)
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job, command=command,
    manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print('Submitted root-value witness:', job)

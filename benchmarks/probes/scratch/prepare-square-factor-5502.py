#!/usr/bin/env python3
"""Locate the first unequal startup Factor extension, not its later phase use."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5482-square-phase'
c = root / 'benchmarks/runs/20260908-5502-square-factor-fresh'
authority = root / 'benchmarks/runs/20260908-5498-square-definition-selectors/definition-snapshots.json'
assert json.loads(authority.read_text())['parent_replay']
runner = (old / 'run.py').read_text().replace("'tactix-5482-'", "'tactix-5502-'")
runner = runner.replace("prefix + '_EVENT_TRACE_PHASE_ONLY'", "'SAT_EVENT_TRACE_FACTOR_FRESH_ONLY'")
runner = runner.replace("'.phase.tsv'", "'.factor.tsv'")
runner = runner.replace("'SAT_EVENT_TRACE_PHASE_VAR': '43773'", "'SAT_EVENT_TRACE_FACTOR_FRESH_CONFLICT': '0'")
assert '_EVENT_TRACE_PHASE_ONLY' not in runner
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'):
    (c / name).mkdir()
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         prior_summary_sha256=sha(old / 'summary.json'),
         definition_authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority)),
         hypothesis='Auxiliary43773 has different startup Factor definitions in validated5498. Trace all startup fresh extensions to locate the first unequal selected seed/member grid before attributing the later saved-phase history to search.',
         validation='Require complete per-arm parent decision/prefix/boundary replay and clean stopped results, pin/trace hashes, untruncated startup factor_fresh events and contiguous extension ordinals. Compare seed/members/factor/remainder/gain; no timing or final proof claim.',
         test_consideration='Configuration-only reuse of existing factor_fresh observers and frozen pins. Syntax and complete parent replay guard neutrality; comparison fixtures cover event framing and extension order. No solver code change.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

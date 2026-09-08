#!/usr/bin/env python3
"""Inspect actual directed occurrences behind the second-entry score discrepancy."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5580-rank32-factor-entry'
parent = root / 'benchmarks/runs/20260908-5529-sweep-small-rank32-trace'
c = root / 'benchmarks/runs/20260908-5585-rank32-factor-literal'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
prior = json.loads((old / 'heap-localization.json').read_text())
assert prior['parent_replay']
assert prior['unequal_scores'] == [dict(lit=41096, candidate=78, kissat=77)]
m = json.loads((parent / 'manifest.json').read_text())
for name, digest in m['hashes'].items(): assert sha(parent / name) == digest
runner = (old / 'run.py').read_text().replace('tactix-5580-', 'tactix-5585-')
start = runner.index("env.update({'SAT_EVENT_TRACE_FACTOR_HEAP_SET_PATH'")
end = runner.index('\ndef interrupted', start)
runner = runner[:start] + '''env.update({prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.events.tsv')),
            prefix + '_EVENT_TRACE_FACTOR_LITERAL': '41096',
            'SAT_EVENT_TRACE_FACTOR_LITERAL_ONLY': '1',
            'SAT_EVENT_TRACE_FACTOR_LITERAL_CONFLICT': '19375'})
''' + runner[end:]
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)), selected_literal=41096,
    hypothesis='The second Factor entry score differs by one despite equal canonical binary and large-clause multisets. Compare actual directed binary occurrences and connected large candidates to discriminate binary degree from candidate peeling.',
    selection_authority=dict(path=str((old / 'heap-localization.json').relative_to(root)), sha256=sha(old / 'heap-localization.json')),
    validation='Require whole per-arm5529 decision/prefix and partial-proof identity; account explicitly for additional global-observer boundary events. Compare all selected literal state/occurrence groups, preserving multiplicity. Diagnostic only, no acceptance timing.',
    test_consideration='No solver/observer change. Reuse immutable existing selected-literal observers; add parser framing/multiplicity fixtures when analyzing results. Stop19376 uses the independently validated short parent, not a new timing budget.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

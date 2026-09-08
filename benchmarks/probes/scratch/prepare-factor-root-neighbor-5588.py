#!/usr/bin/env python3
"""Test whether the missing candidate is supported only by an inactive binary endpoint."""
import datetime
import json
from pathlib import Path
import runpy
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5585-rank32-factor-literal'
c = root / 'benchmarks/runs/20260908-5588-rank32-factor-root-neighbor'
sha = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))['sha']
prior = json.loads((old / 'peel-localization.json').read_text())
assert prior['input_identity'] and prior['target_connected']
assert prior['target_binary_partners']['-491572'] == [-174483]
m = json.loads((old / 'manifest.json').read_text())
for name, digest in m['hashes'].items(): assert sha(old / name) == digest
runner = (old / 'run.py').read_text().replace('tactix-5585-', 'tactix-5588-')
assert runner.count("'_EVENT_TRACE_FACTOR_LITERAL': '41096'") == 1
runner = runner.replace("'_EVENT_TRACE_FACTOR_LITERAL': '41096'", "'_EVENT_TRACE_FACTOR_LITERAL': '-174483'")
anchor = "'SAT_EVENT_TRACE_FACTOR_LITERAL_ONLY': '1',"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, anchor + "\n            prefix + '_EVENT_TRACE_CONFLICT_FROM': '19375',\n            prefix + '_EVENT_TRACE_CONFLICT_TO': '19375',")
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), selected_literal=-174483,
    selection_authority=dict(path=str((old / 'peel-localization.json').relative_to(root)), sha256=sha(old / 'peel-localization.json')),
    hypothesis='The extra five-literal Factor candidate has one large and one binary occurrence at -491572; its sole binary neighbor is -174483. Native bincount ignores pairs whose smaller imported endpoint is inactive. Inspect that endpoint active/value/physical occurrences at both entries before any repair.',
    validation='Full per-arm5529 decisions/prefix/proof identity and original boundary replay; selected state and physical directed occurrences at19375. Narrow global detail window explicitly matches parent and avoids5585 extra analysis rows.',
    test_consideration='No solver/observer edit. Reuse16 occurrence/projection parser fixtures with explicit selected-literal parameter; require actual state evidence before implementation tests or behavioral changes.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

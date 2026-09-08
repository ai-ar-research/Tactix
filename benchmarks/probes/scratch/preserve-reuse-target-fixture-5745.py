#!/usr/bin/env python3
"""Preserve the initial target fixture before adding failure localization."""
from pathlib import Path
import hashlib
import json
import shutil
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5741-reuse-target'
p = root / '.claude/worktrees/runtime-reuse-target-5741/tests/test_conflict_reuse_targets.cpp'
t = c / 'initial-target-fixture.cpp'
assert not t.exists()
shutil.copy2(p, t)
(c / 'initial-target-fixture.json').write_text(json.dumps(dict(
    sha256=hashlib.sha256(t.read_bytes()).hexdigest(),
    observation='New fixture target/value assertion fails; all other tests pass. Diagnosis pending; no assertion expectation or solver changed.'), indent=2) + '\n')

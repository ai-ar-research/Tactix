#!/usr/bin/env python3
"""Isolate approved BVE mapping/index stage with the corrected unit fixture."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260910-7087-bve-views-build/manifest.json'
m = json.loads(authority.read_text()); parent = root / m['source']
for rel, digest in m['files'].items(): assert sha(parent / rel) == digest, rel
gate = root / 'benchmarks/runs/20260910-7103-bve-views-BC-gate/neutrality-7104.json'
g = json.loads(gate.read_text())
assert g['solver_records'] == 66 and g['all_work_and_certificates_equal']
source = root / '.claude/worktrees/runtime-bve-import-7110'
campaign = root / 'benchmarks/runs/20260910-7110-bve-import-build'
assert not source.exists() and not campaign.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
fixture = root / '.claude/worktrees/runtime-bve-fixture-repair-7100/tests/test_bve_antecedent_view.cpp'
validation = json.loads((root / 'benchmarks/runs/20260910-7100-bve-fixture-repair/summary.json').read_text())
assert sha(fixture) == validation['hashes'][str(fixture.relative_to(root))]
shutil.copy2(fixture, source / 'tests/test_bve_antecedent_view.cpp')
(campaign / 'logs').mkdir(parents=True)
with (campaign / 'parent.json').open('x') as stream:
    json.dump(dict(authority=str(authority.relative_to(root)), sha256=sha(authority),
        gate=str(gate.relative_to(root)), gate_sha256=sha(gate),
        approval='Approved BVE data-path prototype after73a78fd8: mapping/preparation fusion and consumer-local indexes; direct Codex debugging per user and plan §8.2.',
        scope='Map physical BVE clauses directly into SubSolver import, retain old add_clause control; reuse first-occurrence map, defer XOR-only indexes. No new kernel primitive, strategy, proof vocabulary or public SAT API.',
        invariants='Keep physical root-false inputs, order, ids, dedup/tautology handling, reset/generator, ticks, core lineage and proofs. Views live only for the synchronous import; SubSolver owns its copied clauses.'), stream, indent=2)
print(source)

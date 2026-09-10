#!/usr/bin/env python3
"""Isolate Kissat's satisfied-large collection advice contract on current6730."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json'
m = json.loads(authority.read_text())
parent = root / m['source']
for rel, digest in m['files'].items():
    assert sha(parent / rel) == digest, rel
evidence = root / 'benchmarks/runs/20260910-7024-rank35-retry-producer/replay-summary-7026.json'
s = json.loads(evidence.read_text())
assert s['exact_whole_parent_schedule_trace_and_partial_proof_replay']
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
source = root / '.claude/worktrees/runtime-collect-retry-7030'
c = root / 'benchmarks/runs/20260910-7030-collect-retry-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c / 'logs').mkdir(parents=True)
with (c / 'parent.json').open('x') as out:
    json.dump(dict(path=str(authority.relative_to(root)), sha256=sha(authority),
        evidence=dict(path=str(evidence.relative_to(root)), sha256=sha(evidence)),
        scope='Suppress removal advice only for satisfied large clauses retired by sparse root collection. Preserve binary, Vivify, root-false rewrite, watermark and semantic deletion contracts. Existing semantic_removal primitive; no layout, guard, legal-instance, proof or reconstruction change.',
        test_consideration='Fixed parent-negative retry/BCE/score-notification matrix, both redundancy classes, roots in every clause position, preexisting marks, binary and Vivify controls, repeated collection. Full independent suites and certificate/B/C/causal/runtime gates follow. Unpromoted/unfuzzed.'), out, indent=2)
    out.write('\n')
print(source)

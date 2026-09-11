#!/usr/bin/env python3
"""Exact mechanical compression of the frozen root-census unified patch."""
import gzip
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

target = root / 'benchmarks/reports/2026-09-11-root-census-landing-7325.json'
assert not target.exists(), 'successful receipt frozen'
audit_path = root / 'benchmarks/reports/2026-09-11-rank25-root-census-audit-7324.json'
audit = json.loads(audit_path.read_text())
rel = 'benchmarks/runs/20260911-7322-root-census-build/incremental.patch'
source = root / rel
assert sha(source) == audit['hashes'][rel]
destination = source.with_suffix('.patch.gz')
assert not destination.exists()
original = source.read_bytes()
with destination.open('xb') as out:
    out.write(gzip.compress(original, mtime=0))
assert gzip.decompress(destination.read_bytes()) == original
with target.open('x') as out:
    json.dump(dict(audit=str(audit_path.relative_to(root)), audit_sha256=sha(audit_path),
        helper=str(Path(__file__).relative_to(root)), helper_sha256=sha(Path(__file__)),
        canonical=rel, canonical_sha256=sha(source),
        packed=str(destination.relative_to(root)), packed_sha256=sha(destination),
        removed_bytes=0,
        reason='Standard unified-diff blank context lines fail Git whitespace checks. Raw patch remains unchanged locally and hash-bound. Track exact compressed copy instead; no normalization or gate weakening.'), out, indent=2)
print(target)

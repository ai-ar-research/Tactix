#!/usr/bin/env python3
"""Preserve raw objdump output by exact mechanical compression."""
import gzip
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
target = root/'benchmarks/reports/2026-09-11-root-gdb-landing-7329.json'
assert not target.exists(), 'successful receipt frozen'
audit_path = root/'benchmarks/reports/2026-09-11-external-root-state-audit-7328.json'
audit = json.loads(audit_path.read_text())
rel = 'benchmarks/runs/20260911-7326-root-gdb-fixture/logs/assembly.log'
source = root/rel
assert sha(source) == audit['hashes'][rel]
destination = source.with_suffix('.log.gz')
assert not destination.exists()
original = source.read_bytes()
with destination.open('xb') as out:
    out.write(gzip.compress(original, mtime=0))
assert gzip.decompress(destination.read_bytes()) == original
with target.open('x') as out:
    json.dump(dict(audit=str(audit_path.relative_to(root)), audit_sha256=sha(audit_path),
        helper=str(Path(__file__).relative_to(root)), helper_sha256=sha(Path(__file__)),
        canonical=rel, canonical_sha256=sha(source),
        packed=str(destination.relative_to(root)), packed_sha256=sha(destination), removed_bytes=0,
        reason='Raw objdump includes emitted trailing spaces; retain canonical bytes unchanged locally and hash-bound, track exact compressed copy instead. No whitespace normalization or gate weakening.'), out, indent=2)
print(target)

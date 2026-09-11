#!/usr/bin/env python3
"""Preserve frozen unified patches verbatim without Git context-space errors."""
import gzip
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-11-memory-only-landing-7292.json'
assert not target.exists(), 'successful packaging receipt frozen'
audit_path = root / 'benchmarks/reports/2026-09-11-memory-only-audit-7290.json'
audit = json.loads(audit_path.read_text())
records = []
for number in (7285, 7287, 7289, 7291):
    for arm in ('candidate', 'previous'):
        rel = f'benchmarks/runs/20260911-{number}-memory-only-build/{arm}-observer.patch'
        source = root / rel
        assert sha(source) == audit['hashes'][rel]
        destination = source.with_suffix('.patch.gz')
        assert not destination.exists()
        original = source.read_bytes()
        with destination.open('xb') as stream:
            stream.write(gzip.compress(original, mtime=0))
        assert gzip.decompress(destination.read_bytes()) == original
        records.append(dict(canonical=rel, canonical_sha256=sha(source),
                            packed=str(destination.relative_to(root)), packed_sha256=sha(destination)))
with target.open('x') as out:
    json.dump(dict(audit=str(audit_path.relative_to(root)), audit_sha256=sha(audit_path),
        helper=str(Path(__file__).relative_to(root)), helper_sha256=sha(Path(__file__)),
        records=records, removed_bytes=0,
        reason='Git whitespace gate rejects standard unified-diff blank context lines stored as patch content. Canonical successful/failed patch evidence remains unchanged locally and hash-bound. Track byte-exact compressed copies instead; no whitespace normalization or gate weakening.'), out, indent=2)
print(target)

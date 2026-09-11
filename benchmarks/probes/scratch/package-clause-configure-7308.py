#!/usr/bin/env python3
"""Preserve three frozen raw configure logs byte-exactly across Git's gate."""
import gzip
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-11-clause-arena-landing-7308.json'
assert not target.exists(), 'successful packaging frozen'
audit = root / 'benchmarks/reports/2026-09-11-clause-arena-audit-7307.json'
a = json.loads(audit.read_text())
records = []
for kind in ('release', 'bc', 'trace'):
    original = root / f'benchmarks/runs/20260911-7301-clause-realloc-build/logs/{kind}-configure.log'
    rel = str(original.relative_to(root))
    assert sha(original) == a['hashes'][rel]
    payload = original.read_bytes()
    assert payload.endswith(b'\n\n'), 'only known raw EOF whitespace is excepted'
    packed = original.with_suffix(original.suffix + '.gz')
    assert not packed.exists()
    with packed.open('xb') as out:
        out.write(gzip.compress(payload, mtime=0))
    assert gzip.decompress(packed.read_bytes()) == payload
    records.append(dict(canonical=rel, canonical_sha256=sha(original),
        packed=str(packed.relative_to(root)), packed_sha256=sha(packed)))
with target.open('x') as out:
    json.dump(dict(audit=str(audit.relative_to(root)), audit_sha256=sha(audit), records=records,
        helper_sha256=sha(Path(__file__)),
        reason='git diff --cached --check rejects the untouched failed CMake logs for a blank EOF line. Retain canonical bytes and frozen7307 hashes locally; track exact gzip copies instead. Do not normalize raw logs, edit the frozen audit, or weaken the whitespace gate. No files deleted from disk; this is a packaging exception only.',
        progress_sha256=sha(root / 'PROGRESS.md')), out, indent=2)
print(target)

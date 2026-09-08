#!/usr/bin/env python3
"""Keep immutable patch/log/CSV bytes while checking all editable staged files."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-rank32-forward-landing-validation.json'
assert not target.exists()
audit = root / 'benchmarks/reports/2026-09-08-rank32-forward-checkpoint-validation.json'
bound = json.loads(audit.read_text())['hashes']
raw = subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, capture_output=True, text=True)
assert raw.returncode in (0, 2) and not raw.stderr
diagnostics = Counter()
for line in raw.stdout.splitlines():
    match = re.match(r'^(.*):(\d+): (.*)$', line)
    if not match: continue
    rel, number, error = match.groups()
    p = root / rel
    assert rel in bound and hashlib.sha256(p.read_bytes()).hexdigest() == bound[rel]
    assert error == 'trailing whitespace.', line
    data = p.read_bytes().splitlines(keepends=True)[int(number)-1]
    if p.suffix == '.patch':
        assert data.rstrip(b'\r\n') == b' ', (rel, number, data)
        reason = 'Required unified-diff blank context marker; preserve frozen patch bytes.'
    elif p.suffix == '.out':
        assert data.rstrip(b'\r\n') == b'c ', (rel, number, data)
        reason = 'Native solver blank comment output; preserve frozen log bytes.'
    else:
        assert rel == 'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s/matrix.tsv'
        assert data.endswith(b'\r\n') and data[:-2] == data[:-2].rstrip()
        reason = 'CSV writer CRLF framing in submitted immutable TSV; csv.DictReader supports it.'
    diagnostics[(rel, reason)] += 1
paths = subprocess.run(['git', 'diff', '--cached', '--name-only', '-z'], cwd=root,
                       capture_output=True, check=True).stdout.decode().split('\0')
exempt = {rel for rel, _ in diagnostics}
editable = [p for p in paths if p and p not in exempt]
strict = subprocess.run(['git', 'diff', '--cached', '--check', '--', *editable],
                        cwd=root, capture_output=True, text=True, check=True)
assert not strict.stdout and not strict.stderr
out = dict(audit_sha256=hashlib.sha256(audit.read_bytes()).hexdigest(),
           validator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
           editable_staged_whitespace_check='PASS', editable_paths_checked=len(editable),
           preserved_artifact_formatting=[dict(path=p, count=n, reason=reason)
                                          for (p, reason), n in diagnostics.items()],
           mutation='No submitted script, matrix, patch or log changed to satisfy formatting. No deletions or push.')
target.write_text(json.dumps(out, indent=2) + '\n')
print('PASS editable staged paths:', len(editable), '; validated immutable formatting cases:', sum(diagnostics.values()))

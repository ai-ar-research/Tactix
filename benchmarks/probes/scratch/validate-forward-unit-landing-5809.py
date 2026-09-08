#!/usr/bin/env python3
"""Validate staged formatting without altering immutable unified patches."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-forward-unit-landing-validation.json'
assert not target.exists()
audit = root / 'benchmarks/reports/2026-09-08-forward-unit-validation.json'
bound = json.loads(audit.read_text())['hashes']
campaign = root / 'benchmarks/runs/20260908-5806-forward-unit-BC-gate'
manifest = json.loads((campaign / 'manifest.json').read_text())
bound.update({str((campaign / rel).relative_to(root)):digest for rel,digest in manifest['pin_sha256'].items()})
raw = subprocess.run(['git','diff','--cached','--check'], cwd=root, capture_output=True, text=True)
assert raw.returncode in (0,2) and not raw.stderr
diagnostics = Counter()
for line in raw.stdout.splitlines():
    match = re.match(r'^(.*):(\d+): (.*)$', line)
    if not match: continue
    rel, number, error = match.groups()
    p = root / rel
    assert error == 'trailing whitespace.', line
    assert hashlib.sha256(p.read_bytes()).hexdigest() == bound[rel]
    data = p.read_bytes().splitlines()[int(number)-1]
    if p.suffix == '.patch':
        assert data == b' ', (rel,number)
    else:
        assert re.fullmatch(r'benchmarks/runs/20260908-5798-forward-unit/fixed-proofs/forward-[01]-[0-3]-drat.log', rel)
        assert re.fullmatch(rb'c [0-9]+ of [0-9]+ clauses in core {28}', data), (rel,number,data)
    diagnostics[rel] += 1
paths = subprocess.run(['git','diff','--cached','--name-only','-z'], cwd=root,
    capture_output=True,check=True).stdout.decode().split('\0')
editable = [p for p in paths if p and p not in diagnostics]
subprocess.run(['git','diff','--cached','--check','--',*editable], cwd=root, check=True)
report = root / 'report-to-andrew/20260908-1232-forward-unit-continuation'
assert 'Overfull' not in (report / 'report.log').read_text()
assert 'Output written on report.pdf (2 pages,' in (report / 'report.log').read_text()
target.write_text(json.dumps(dict(
    audit_sha256=hashlib.sha256(audit.read_bytes()).hexdigest(),
    editable_staged_paths=len(editable), whitespace_check='PASS outside exact hash-bound unified-diff blank context and checker progress lines',
    immutable_artifact_formatting_lines=dict(diagnostics),
    andrew_report={str((report / name).relative_to(root)):hashlib.sha256((report / name).read_bytes()).hexdigest()
                   for name in ('report.tex','report.pdf')},
    cleanup='No manual deletion; no frozen artifact formatting altered. No push or Claude-network retry.'), indent=2) + '\n')
print('PASS:',len(editable),'editable staged paths;',sum(diagnostics.values()),'preserved artifact formatting lines;two-page TeX/PDF')

#!/usr/bin/env python3
"""Check editable formatting while preserving exact frozen evidence bytes."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-forward-admission-landing-validation.json'
assert not target.exists()
audit = root / 'benchmarks/reports/2026-09-08-forward-admission-final-validation.json'
audit_data = json.loads(audit.read_text())
bound = audit_data['hashes']
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
raw = subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root,
                     capture_output=True, text=True)
assert raw.returncode in (0, 2) and not raw.stderr
diagnostics = Counter()
fixtures = {
    'benchmarks/runs/20260908-5813-forward-unit-edges/forward-fixture.hpp':
        '.claude/worktrees/runtime-forward-unit-5798/tests/test_forward_unit.cpp',
    'benchmarks/runs/20260908-5834-forward-admission-edges/forward-fixture.hpp':
        '.claude/worktrees/runtime-forward-admission-5830/tests/test_forward_unit.cpp',
}
for line in raw.stdout.splitlines():
    match = re.match(r'^(.*):(\d+): (.*)$', line)
    if not match:
        continue
    rel, number, error = match.groups()
    p = root / rel
    assert sha(p) == bound[rel], rel
    data = p.read_bytes().splitlines()[int(number) - 1]
    if error == 'new blank line at EOF.':
        source = root / fixtures[rel]
        assert p.read_bytes() == source.read_bytes().split(b'static void independent_tail(')[0]
        assert data == b'' and int(number) == len(p.read_bytes().splitlines())
    else:
        assert error == 'trailing whitespace.', line
        if p.suffix == '.patch':
            assert data == b' ', (rel, number)
        else:
            assert re.fullmatch(
                r'benchmarks/runs/20260908-(?:5813-forward-unit-edges|5834-forward-admission-edges)/edge-[01]-[0-2]-drat-check.log'
                r'|benchmarks/runs/20260908-5830-forward-admission/fixed-proofs/forward-[01]-[0-3]-drat.log', rel), rel
            assert re.fullmatch(rb'c [0-9]+ of [0-9]+ clauses in core {28}', data)
    diagnostics[rel] += 1
paths = subprocess.run(['git', 'diff', '--cached', '--name-only', '-z'], cwd=root,
                       capture_output=True, check=True).stdout.decode().split('\0')
editable = [p for p in paths if p and p not in diagnostics]
subprocess.run(['git', 'diff', '--cached', '--check', '--', *editable], cwd=root, check=True)
report = root / 'report-to-andrew/20260908-1342-forward-admission-runtime'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
report_hashes = {str((report / name).relative_to(root)): sha(report / name)
                 for name in ('report.tex', 'report.pdf')}
for rel, digest in report_hashes.items():
    assert digest == audit_data['andrew_report'][rel]
target.write_text(json.dumps(dict(
    audit_sha256=sha(audit), editable_staged_paths=len(editable),
    whitespace_check='PASS outside exact hash-bound frozen artifact formatting',
    immutable_artifact_formatting_lines=dict(diagnostics), andrew_report=report_hashes,
    test_consideration='No solver change. Exact-byte assertions reject unrecognized formatting; benchmark and documentation tests are bound by the final audit.',
    cleanup='No manual deletion or frozen evidence edits. No push or Claude-network retry.'), indent=2) + '\n')
print('PASS:', len(editable), 'editable paths;', sum(diagnostics.values()), 'preserved artifact formatting lines')

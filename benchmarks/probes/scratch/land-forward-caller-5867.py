#!/usr/bin/env python3
"""Stage explicit audited paths and validate frozen-artifact formatting."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root / 'benchmarks/reports/2026-09-08-forward-caller-final-validation.json'
target = root / 'benchmarks/reports/2026-09-08-forward-caller-landing-validation.json'
assert not target.exists()
data = json.loads(audit.read_text())
bound = data['hashes']
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in bound.items(): assert sha(root / rel) == digest, rel
pending = subprocess.run(['git', 'diff', '--cached', '--name-only'], cwd=root,
                         capture_output=True, text=True, check=True).stdout
assert not pending, 'Preserve pre-existing staged work; review overlap before landing'
paths = sorted(set(bound) | {str(audit.relative_to(root)), str(Path(__file__).resolve().relative_to(root))})
subprocess.run(['git', 'add', '-f', '--', *paths], cwd=root, check=True)
raw = subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, capture_output=True, text=True)
assert raw.returncode in (0, 2) and not raw.stderr
diagnostics = Counter()
for line in raw.stdout.splitlines():
    match = re.match(r'^(.*):(\d+): (.*)$', line)
    if not match: continue
    rel, number, error = match.groups()
    p = root / rel
    assert sha(p) == bound[rel]
    content = p.read_bytes().splitlines()[int(number) - 1]
    if error == 'new blank line at EOF.':
        assert rel in ['benchmarks/runs/20260908-5853-forward-caller-edges/forward-fixture.hpp',
                       'benchmarks/runs/20260908-5859-forward-caller-edges-final/forward-fixture.hpp']
        source = root / '.claude/worktrees/runtime-forward-caller-5849/tests/test_forward_unit.cpp'
        assert p.read_bytes() == source.read_bytes().split(b'static void independent_tail(')[0]
        assert content == b'' and int(number) == len(p.read_bytes().splitlines())
    else:
        assert error == 'trailing whitespace.', line
        if p.suffix == '.patch': assert content == b' '
        else:
            assert p.suffix == '.log' and rel.startswith('benchmarks/runs/20260908-58')
            assert re.fullmatch(rb'c [0-9]+ of [0-9]+ clauses in core {28}', content), (rel, number, content)
    diagnostics[rel] += 1
editable = [p for p in paths if p not in diagnostics]
subprocess.run(['git', 'diff', '--cached', '--check', '--', *editable], cwd=root, check=True)
for rel, digest in data['andrew_report'].items(): assert sha(root / rel) == digest
target.write_text(json.dumps(dict(audit_sha256=sha(audit), staged_paths=len(paths),
    editable_staged_paths=len(editable), immutable_formatting_lines=dict(diagnostics),
    whitespace_check='PASS outside exact hash-bound patch context,fixture prefix EOF and checker progress bytes',
    tests='Four final solver suites,40 independently checked proofs,active-journal closure,22 benchmark tests,6 documentation checks;see bound final audit.',
    cleanup='No manual deletion or evidence rewriting. Existing runtime scripts/pins unchanged. No push or Claude-network retry.'), indent=2) + '\n')
subprocess.run(['git', 'add', '-f', '--', str(target.relative_to(root))], cwd=root, check=True)
print('PASS:', len(paths), 'explicit bound paths;', sum(diagnostics.values()), 'preserved immutable formatting lines')

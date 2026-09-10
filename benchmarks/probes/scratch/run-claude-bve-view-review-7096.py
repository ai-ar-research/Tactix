#!/usr/bin/env python3
"""Approval-gated bounded external review; prints terminal output only."""
from pathlib import Path
import subprocess
import sys
if sys.argv[1:] != ['--user-approved-external-review']:
    raise SystemExit('Requires explicit user approval for this bounded source/log transfer.')
root = Path(__file__).resolve().parents[3]
base = root / 'benchmarks/probes/scratch'
out = base / 'claude-bve-view-review-7096-response.json'
err = base / 'claude-bve-view-review-7096-stderr.log'
assert not out.exists() and not err.exists()
with (base / 'claude-bve-view-review-7096.txt').open() as inp, out.open('x') as stdout, err.open('x') as stderr:
    p = subprocess.Popen(['claude', '-p', '--permission-mode', 'acceptEdits',
        '--allowedTools', 'Bash,Read,Edit,Write,Glob,Grep',
        '--tools', 'Bash,Read,Edit,Write,Glob,Grep', '--permission-prompts', 'none',
        '--strict-mcp-config', '--output-format', 'json'],
        cwd=root, stdin=inp, stdout=stdout, stderr=stderr)
    print('Bounded Claude fixture owner launched: ' + str(p.pid), flush=True)
    code = p.wait()
print('Claude fixture owner exit:', code, flush=True)
print(out.read_text(), flush=True)
if code: print(err.read_text(), flush=True)
raise SystemExit(code)

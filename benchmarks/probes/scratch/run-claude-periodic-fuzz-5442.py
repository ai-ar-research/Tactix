#!/usr/bin/env python3
"""Run the authoritative fuzz owner; expose only its completed response."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
prompt = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5442.txt'
out = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5442-response.json'
err = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5442-stderr.log'
assert not out.exists() and not err.exists(), 'Do not relaunch a live/completed fuzz owner'
command = ['claude', '-p', '--permission-mode', 'acceptEdits',
           '--allowedTools', 'Bash,Read,Edit,Write,Glob,Grep',
           '--tools', 'Bash,Read,Edit,Write,Glob,Grep',
           '--permission-prompts', 'none', '--strict-mcp-config',
           '--output-format', 'json']
with prompt.open() as inp, out.open('w') as stdout, err.open('w') as stderr:
    process = subprocess.Popen(command, cwd=root, stdin=inp, stdout=stdout, stderr=stderr)
    print('Claude fuzz owner launched, process ' + str(process.pid) + '; terminal response only.', flush=True)
    code = process.wait()
print('Claude owner exit:', code, flush=True)
print(out.read_text(), flush=True)
if code:
    print(err.read_text(), flush=True)
raise SystemExit(code)

#!/usr/bin/env python3
"""Resume the same submitted fuzz owner; never duplicate its existing campaign."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
out = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5610-resume-5624-response.json'
err = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5610-resume-5624-stderr.log'
assert not out.exists() and not err.exists()
prompt = '''Resume ownership of EXISTING periodic fuzz5610 in this SAME conversation.
Your previous CLI invocation exited with a submission-only result, not a terminal
PASS/FAILURE/BLOCKED. Parent has not inspected your jobs/artifacts and has not
started any duplicate campaign. Resume the existing prep/witness jobs and their
watchers, then submit only your already-planned remaining legs. Do not restart
work because a foreground observation times out. Do not rely on callbacks to
re-invoke you after ending the CLI response; remain owner until a terminal gate.
Use tool-owned SLURM waiters in the background and bounded observations as needed.
An ordinary live queue/check wait is not a blocker. Original scope, source5592,
100logicalCPU/400000MiB/50worker generated-leg contract, immutable harness,
independent oracles, direct root-fixed support/actual rewrite reachability,
cleanup and terminal report requirements remain unchanged.
No source/production/root-doc/report edits or commits. Parent normal candidate
5619 is paused under separate bounded unit-test review5623; it is NOT included
in your campaign. Leave it alone. Your immutable5592 is unchanged.
Return only terminal PASS/FAILURE/BLOCKED with the originally required evidence.
'''
command = ['claude', '--resume', 'a234d14c-c00c-412d-bd5b-17c3d7786c23', '-p',
    '--permission-mode', 'acceptEdits', '--allowedTools', 'Bash,Read,Edit,Write,Glob,Grep',
    '--tools', 'Bash,Read,Edit,Write,Glob,Grep', '--permission-prompts', 'none',
    '--strict-mcp-config', '--output-format', 'json']
with out.open('w') as stdout, err.open('w') as stderr:
    process = subprocess.Popen(command, cwd=root, stdin=subprocess.PIPE,
        stdout=stdout, stderr=stderr, text=True)
    print('Resumed existing5610 owner, process ' + str(process.pid), flush=True)
    process.communicate(prompt)
print('Claude owner exit:', process.returncode, flush=True)
print(out.read_text(), flush=True)
if process.returncode: print(err.read_text(), flush=True)
raise SystemExit(process.returncode)

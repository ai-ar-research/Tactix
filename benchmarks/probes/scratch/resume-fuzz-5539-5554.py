#!/usr/bin/env python3
"""Resume the same submitted5539 owner, never create a duplicate campaign."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
out = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5539-resume-5554-response.json'
err = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5539-resume-5554-stderr.log'
assert not out.exists() and not err.exists()
prompt = '''Continue ownership of the EXISTING5539 fuzz campaign in this same conversation.
Your last response was submission-only, not PASS/FAILURE/BLOCKED. Parent has not
inspected your jobs/artifacts and has not launched a duplicate campaign. Resume
the existing watchers/jobs, submit only already-planned remaining legs when ready,
and monitor/validate/clean until a terminal campaign outcome. A queue wait is not
a blocker. Do not return another submission-only update or rely on external
re-invocation after ending. Use your tool-owned watchers in the background.
Do not restart a job because observation times out. Original5539 scope, resource
contracts, ownership and failure handling remain unchanged. Parent continues
normal runtime development. Source5522 remains immutable.
Additional dependent normal jobs to stop ONLY on a genuine reported5539
correctness failure:1672385(final5531 pair),1672400(current5522 Rank39 profiles),
1672418(new5546 successor B28+C5). Prepared successor timing5551 may be submitted:
read only its exact wave submission records to resolve IDs if cancellation is needed.
Do not audit or modify parent source/report/docs; parent owns those. Return a
terminal PASS/FAILURE/BLOCKED report with the originally required evidence.
'''
command = ['claude', '--resume', 'c1bdbe36-d155-4edf-83fb-d553578505eb', '-p',
    '--permission-mode', 'acceptEdits', '--allowedTools', 'Bash,Read,Edit,Write,Glob,Grep',
    '--tools', 'Bash,Read,Edit,Write,Glob,Grep', '--permission-prompts', 'none',
    '--strict-mcp-config', '--output-format', 'json']
with out.open('w') as stdout, err.open('w') as stderr:
    process = subprocess.Popen(command, cwd=root, stdin=subprocess.PIPE,
                               stdout=stdout, stderr=stderr, text=True)
    print('Resumed existing5539 owner, process ' + str(process.pid), flush=True)
    process.communicate(prompt)
print('Claude owner exit:', process.returncode, flush=True)
print(out.read_text(), flush=True)
if process.returncode: print(err.read_text(), flush=True)
raise SystemExit(process.returncode)

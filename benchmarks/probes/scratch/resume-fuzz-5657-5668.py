#!/usr/bin/env python3
"""Resume the existing owner after submission-only return, without duplication."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
out = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5657-resume-5668-response.json'
err = root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5657-resume-5668-stderr.log'
assert not out.exists() and not err.exists()
prompt = '''Resume ownership of EXISTING periodic fuzz5657 in this SAME conversation.
Your CLI returned submission-only, not terminal PASS/FAILURE/BLOCKED. Parent
has not inspected your jobs, logs, reports or artifacts and has not duplicated
anything. Resume your existing prep1672921,reach1672922,legs1672923..1672929 and
watchers. Inspect authoritative state yourself; never restart because an
observation times out. Pending/running jobs are ordinary waits, not blockers.
Do not rely on callbacks to restart a completed CLI. Remain owner using bounded
observations/tool-owned waiters until terminal oracles and cleanup are reported.
Original candidate5649,scope,immutable source/harness,100logicalCPU/400000MiB/
50workers per generated leg,independent proofs/models,direct actual deletion and
propagation reachability,all cleanup and terminal report requirements remain.
No production,submitted source,root docs/report or commit edits. No new campaign
or duplicate passing leg. Parent normal dependent full5664/job1672864 and timing
5652/job1672918 are now submitted in addition to5651/job1672791. Stop affected
owned dependent jobs on a genuine correctness failure and report the original
causal reproducer for bounded repair. Do not alter the runner/checker contract.
Return terminal PASS/FAILURE/BLOCKED with originally required exact provenance,
cases/oracles,resource/concurrency,reachability and cleanup. Queue saturation
alone must not make you terminate ownership with a submission-only message.
'''
command = ['claude', '--resume', '6fc02fd8-be67-4f60-b8f7-801229eb8dc8', '-p',
    '--permission-mode', 'acceptEdits', '--allowedTools', 'Bash,Read,Edit,Write,Glob,Grep',
    '--tools', 'Bash,Read,Edit,Write,Glob,Grep', '--permission-prompts', 'none',
    '--strict-mcp-config', '--output-format', 'json']
with out.open('w') as stdout, err.open('w') as stderr:
    process = subprocess.Popen(command, cwd=root, stdin=subprocess.PIPE,
        stdout=stdout, stderr=stderr, text=True)
    print('Resumed existing5657 owner, process ' + str(process.pid), flush=True)
    process.communicate(prompt)
print('Claude owner exit:', process.returncode, flush=True)
print(out.read_text(), flush=True)
if process.returncode: print(err.read_text(), flush=True)
raise SystemExit(process.returncode)

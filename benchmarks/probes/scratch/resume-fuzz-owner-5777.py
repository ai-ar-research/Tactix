#!/usr/bin/env python3
"""Continue the existing owner; never create replacement fuzz jobs."""
from pathlib import Path
import argparse
import subprocess
root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--attempt', type=int, choices=(2, 3), default=2)
attempt = parser.parse_args().attempt
out = root / f'benchmarks/probes/scratch/claude-periodic-fuzz-5765-resume{attempt}-response.json'
err = root / f'benchmarks/probes/scratch/claude-periodic-fuzz-5765-resume{attempt}-stderr.log'
assert not out.exists() and not err.exists()
prompt = '''Continue the SAME existing5765 campaign and session. The prior CLI38524 exited with an interim watcher message, not PASS/FAILURE. The parent attempt96672 then failed before any API inference with ENOTFOUND; it created no campaign. Background notification cannot keep a terminated -p CLI alive. Do not create another campaign, build, solver job, or duplicate watcher. Revalidate your existing handles, including watcher bhye6v7e6. Keep this CLI alive with foreground Bash calls to the existing tool-owned wait.py/watcher and read its outcome; do not use run_in_background and do not return a waiting-only final response. Bounded foreground tool observations can repeat on the SAME live handles until actual terminal evidence. Parent continues development independently and does not inspect your fuzz artifacts. Retain all previous scope, oracle, cleanup and genuine-failure stop obligations (dependent normal jobs1673255,1673256,1673257,1673329,1673370,1673386, plus new campaign5779 Rank32 forward observer); do not touch unrelated/user1668242 jobs. Return only an actual terminal PASS/FAILURE or a demonstrated external blocker, with source identity, generated coverage separate from fixed coverage, reachability and oracle evidence, limitations and cleanup. A timeout of observation alone is not terminal and must not trigger replacement jobs.'''
command = ['claude', '--resume', '3c676397-a271-4992-bc38-a4d2786e8724', '-p',
           '--permission-mode', 'acceptEdits', '--allowedTools', 'Bash,Read,Edit,Write,Glob,Grep',
           '--tools', 'Bash,Read,Edit,Write,Glob,Grep', '--permission-prompts', 'none',
           '--strict-mcp-config', '--output-format', 'json']
with out.open('w') as stdout, err.open('w') as stderr:
    process = subprocess.Popen(command, cwd=root, stdin=subprocess.PIPE, stdout=stdout,
                               stderr=stderr, text=True)
    print('Resuming existing5765 owner in foreground CLI; no duplicate jobs authorized.', flush=True)
    process.communicate(prompt)
print('Claude owner exit:', process.returncode, flush=True)
print(out.read_text(), flush=True)
if process.returncode:
    print(err.read_text(), flush=True)
raise SystemExit(process.returncode)

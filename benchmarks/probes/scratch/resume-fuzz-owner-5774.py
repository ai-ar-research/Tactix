#!/usr/bin/env python3
"""Resume the existing5765 owner after its nonterminal CLI response."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
out=root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5765-resume1-response.json'
err=root / 'benchmarks/probes/scratch/claude-periodic-fuzz-5765-resume1-stderr.log'
assert not out.exists() and not err.exists()
prompt='''Resume ownership of the EXISTING5765 fuzz checkpoint on5741. Your previous CLI response ended with an interim waiting message about demo3_scripted_default and queued/running legs,not a terminal verdict. Do not create a duplicate campaign,build,job or watcher because observation ended. Revalidate and resume your existing handles and keep owning monitoring,oracles,cleanup and reporting until actual terminal PASS/FAILURE/BLOCKED. Do not return another submission/wait-only final response. Use your permitted watcher/wait mechanism and poll the same handles;parent observes only your CLI response and must not inspect your artifacts. All prior bounded scope and constraints remain. New dependent normal diagnostic5771/job1673386 uses5741;include it among exact dependent tasks to stop if you find a genuine correctness failure. Production,submitted sources/pins,unrelated jobs and user1668242 remain untouched. Return complete terminal report with current-source identity,generated and fixed coverage distinguished,reachability,oracle results,limitations and cleanup. A waiting statement is not a terminal report.'''
command=['claude','--resume','3c676397-a271-4992-bc38-a4d2786e8724','-p',
    '--permission-mode','acceptEdits','--allowedTools','Bash,Read,Edit,Write,Glob,Grep',
    '--tools','Bash,Read,Edit,Write,Glob,Grep','--permission-prompts','none',
    '--strict-mcp-config','--output-format','json']
with out.open('w') as stdout,err.open('w') as stderr:
    process=subprocess.Popen(command,cwd=root,stdin=subprocess.PIPE,stdout=stdout,stderr=stderr,text=True)
    print('Resumed existing5765 owner session; no new campaign authorized.',flush=True)
    process.communicate(prompt)
print('Claude resumed owner exit:',process.returncode,flush=True)
print(out.read_text(),flush=True)
if process.returncode:print(err.read_text(),flush=True)
raise SystemExit(process.returncode)

#!/usr/bin/env python3
"""Expose only the bounded fixture owner's terminal result."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
base=root/'benchmarks/probes/scratch'
out=base/'claude-eliminate-fixture-5450-response.json'
err=base/'claude-eliminate-fixture-5450-stderr.log'
assert not out.exists() and not err.exists()
with (base/'claude-eliminate-fixture-5450.txt').open() as inp,out.open('w') as stdout,err.open('w') as stderr:
    p=subprocess.Popen(['claude','-p','--permission-mode','acceptEdits',
        '--allowedTools','Bash,Read,Edit,Write,Glob,Grep','--tools','Bash,Read,Edit,Write,Glob,Grep',
        '--permission-prompts','none','--strict-mcp-config','--output-format','json'],
        cwd=root,stdin=inp,stdout=stdout,stderr=stderr)
    print('Bounded Claude fixture owner launched: '+str(p.pid),flush=True)
    code=p.wait()
print('Claude fixture owner exit:',code,flush=True)
print(out.read_text(),flush=True)
if code: print(err.read_text(),flush=True)
raise SystemExit(code)

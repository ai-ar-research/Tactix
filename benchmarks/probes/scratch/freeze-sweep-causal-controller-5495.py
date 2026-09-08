#!/usr/bin/env python3
"""Keep accounting outages explicit while auditing live controller and artifacts."""
from pathlib import Path
source = Path(__file__).with_name('freeze-sweep-causal-5493.py').read_text()
source = source.replace('2026-09-08-sweep-causal-main-ctest.log',
                        '2026-09-08-sweep-causal-final-main-ctest.log')
anchor = "exec(compile(source, __file__, 'exec'))"
assert source.count(anchor) == 1
source = source.replace(anchor, '''start = source.index('def run(args):')
end = source.index('\\nqueue=', start)
source = source[:start] + """accounting_probe = subprocess.run(
    ['sacct','-j','1670198','-X','--format=JobID,State,ExitCode','--parsable2'],
    cwd=root,capture_output=True,text=True)
def run(args):
    if args[0] == 'sacct' and accounting_probe.returncode:
        # The controller may have purged old terminal jobs. Preserve UNKNOWN,
        # not an inferred success, while leaving every artifact oracle strict.
        controller = subprocess.run(['scontrol','show','job',args[2]],
            cwd=root,capture_output=True,text=True)
        return json.dumps(dict(accounting_available=False,
            accounting_error=accounting_probe.stderr.strip(),
            controller_exit_code=controller.returncode,
            controller_stdout=controller.stdout,
            controller_stderr=controller.stderr,
            limitation='Missing controller jobs have unknown current scheduler state; no terminal-state claim is inferred. Immutable result and certificate checks remain separate.'))
    return subprocess.run(args,cwd=root,check=True,capture_output=True,text=True).stdout
""" + source[end:]
source = source.replace("result=dict(observed_at=", "result=dict(accounting_query=dict(exit_code=accounting_probe.returncode,stdout=accounting_probe.stdout,stderr=accounting_probe.stderr),observed_at=")
''' + anchor)
exec(compile(source, __file__, 'exec'))

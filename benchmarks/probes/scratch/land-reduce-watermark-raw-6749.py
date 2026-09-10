#!/usr/bin/env python3
"""Land the audited checkpoint without rewriting checker-emitted raw spaces."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
audit=root/'benchmarks/reports/2026-09-09-reduce-watermark-audit-6747.json'
receipt=root/'benchmarks/reports/2026-09-09-reduce-watermark-landing-6749.json'
assert not receipt.exists()
a=json.loads(audit.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items():assert sha(root/rel)==digest,rel
owned=set(a['owned_hashes'])|{str(audit.relative_to(root)),str(Path(__file__).relative_to(root)),str(receipt.relative_to(root))}
staged=set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())
assert staged and staged<=owned
raw=['benchmarks/runs/20260909-6734-reduce-watermark-proof-control/logs/check-root-'+suffix+'-drat.log'
     for suffix in ('0-0-1','0-1-1','1-0-1','1-1-1')]
assert set(raw)<=set(a['owned_hashes'])
commands=[['git','diff','--cached','--check','--','.',*[':(exclude)'+r for r in raw]],
          ['git','-c','core.whitespace=-blank-at-eol','diff','--cached','--check','--',*raw]]
for command in commands:subprocess.run(command,cwd=root,check=True)
with receipt.open('x') as out:
    json.dump(dict(audit=dict(path=str(audit.relative_to(root)),sha256=sha(audit)),
        successor=dict(path=str(Path(__file__).relative_to(root)),sha256=sha(Path(__file__))),
        raw_log_hashes={r:sha(root/r) for r in raw},checks=commands,
        reason='Initial6748 stopped at git whitespace check before commit: four independently verified drat-trim raw logs contain trailing spaces in their core-count line. Preserve all frozen bytes. All non-exempt files retain the full whitespace gate; only these four hash-bound logs permit blank-at-eol. No solver/test/checker result is relaxed.',
        push='Not attempted; existing denial remains.'),out,indent=2)
    out.write('\n')
subprocess.run(['git','add','-f','--',str(Path(__file__).relative_to(root)),str(receipt.relative_to(root))],cwd=root,check=True)
staged=set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())
assert staged<=owned
for command in commands:subprocess.run(command,cwd=root,check=True)
subprocess.run(['git','diff','--cached','--stat'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Preserve historical Reduce boundary across clause promotion'],cwd=root,check=True)
assert not subprocess.check_output(['git','status','--porcelain','--',*sorted(owned)],cwd=root)
print('Owned checkpoint clean; production unchanged; no push.')

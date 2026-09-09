#!/usr/bin/env python3
"""Land replay-validated retention evidence and its tested observers locally."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
target=root/'benchmarks/reports/2026-09-08-retention-origin-landing.json'; assert not target.exists()
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(8<<20),b''): h.update(block)
    return h.hexdigest()
report=root/'report-to-andrew/20260908-2004-retention-origin'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
audit=root/'benchmarks/reports/2026-09-08-retention-origin-validation.json'
for rel,digest in json.loads(audit.read_text())['hashes'].items(): assert sha(root/rel)==digest,rel
gate=root/'benchmarks/reports/2026-09-08-retained5962-checked-gates.json'
g=json.loads(gate.read_text())
for rel,digest in g['hashes'].items(): assert sha(root/rel)==digest,rel
assert g['campaigns']['5968']['checked_clean']==66 and g['campaigns']['5970']['checked_clean']==40
assert g['fixed_reference_checked']==40 and not g['runtime_target_met']
controls=root/'benchmarks/runs/20260908-6030-learned-stage-controls'
v=json.loads((controls/'summary.json').read_text())
for rel,digest in v['hashes'].items(): assert sha(root/rel)==digest,rel
assert v['full_trace_suite']=='100/100 PASS'
assert '100% tests passed out of 100' in (controls/'logs/tactix-ctest.log').read_text()
assert v['native_controls']['valid']['exit_code']==0
assert all(x['exit_code']==2 for name,x in v['native_controls'].items() if name!='valid')
owned=set()
def retain(p):
    assert p.is_file() and p.stat().st_size<5<<20,p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md','benchmarks/reports/2026-09-08-retention-origin.md',
            'benchmarks/reports/2026-09-08-retention-origin-validation.json',
            'benchmarks/reports/2026-09-08-retained5962-checked-gates.json'): retain(root/rel)
for number in range(6030,6049):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(number)+'.*'): retain(p)
for number in (6030,6035,6038,6042):
    matches=list((root/'benchmarks/runs').glob(f'20260908-{number}-*')); assert len(matches)==1
    c=matches[0]
    if number!=6030:
        m=json.loads((c/'manifest.json').read_text())
        for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest,rel
        for rel,digest in m['native_source_files'].items(): assert sha(root/m['native_source']/rel)==digest,rel
        for rel,digest in m['diagnostic_source_files'].items(): assert sha(root/m['candidate_source']/rel)==digest,rel
        assert json.loads((c/'summary.json').read_text())['full_parent_replay']
        for arm in ('candidate','kissat'):
            r=json.loads((c/'results'/f'{arm}.json').read_text())
            assert r['exit_code']==0 and r['scratch_deleted']
    for p in c.rglob('*'):
        parts=p.relative_to(c).parts
        if not p.is_file() or p.stat().st_size>=5<<20 or any(n in parts for n in ('pins','__pycache__')): continue
        if 'traces' in parts and not p.name.endswith('.clause-pose-series.tsv'): continue
        if 'logs' in parts and number!=6030: continue
        if p.suffix in ('.json','.py','.sbatch','.tsv','.cpp','.c','.h','.txt','.patch','.log'): retain(p)
checks={}
for name in ('validate-learned-stage-6033.py','validate-retention-history-6044.py'):
    r=subprocess.run(['python3','benchmarks/probes/scratch/'+name,'--self-test'],cwd=root,capture_output=True,text=True)
    assert r.returncode==0,(name,r.stdout,r.stderr)
    checks[name]=r.stdout+r.stderr
terminal=subprocess.run(['sacct','-n','-X','-j','1676555,1676601,1676657','-o','JobID,State,ExitCode','-P'],capture_output=True,text=True,check=True).stdout
assert terminal.count('COMPLETED|0:0')==6,terminal
target.write_text(json.dumps(dict(hashes={rel:sha(root/rel) for rel in sorted(owned)},checks=checks,terminal=terminal,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    outcome='Full history proves first changed Reduce victims at51/conf240062: one differently reason-protected clause changes pool5030/5031 and victims3395/3396. Survivor chain explains causal Vivify clause deletion at54. All raw/shrunk/final canonical learning rows agree.100/100 observer tests pass;5962 remains1.116453x.',
    next_action='New5962 successor: actual root-propagated reason/unit-proof fixture and native comparison; general proof-safe eligibility repair preserving pending/non-root locks, then causal replay and B28+C5/full40.',
    cleanup='No manual deletion. All six new diagnostic task records assert scratch cleanup. Preserve authority pins, sources, manifests and large hash-bound traces. No fuzz-owner artifact access.',
    limitations='Goal not met; no solver implementation change, runtime gain, source promotion or push. Current-source fuzz unverified; transfer retry unapproved. Andrew report local, not sent.'),indent=2)+'\n')
retain(target)
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Trace runtime drift to Reduce reason protection and seal checked gates'],cwd=root,check=True)
print('PASS:',len(owned),'explicit paths landed locally; no push')

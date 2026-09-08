#!/usr/bin/env python3
"""Land the complete causal increment and exact next diagnostic, locally."""
import hashlib
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[3]
target=root/'benchmarks/reports/2026-09-08-first-bve-attempts-landing.json'
assert not target.exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
report=root/'report-to-andrew/20260908-1659-first-bve-attempts'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
validated=root/'benchmarks/runs/20260908-5949-bve-heap-lifecycle/heap-validation.json'
v=json.loads(validated.read_text())
assert v['full_ordinary_trace_and_partial_proof_replay'] and v['complete_bve_candidate_replay']
for arm in ('candidate','kissat'):
    r=json.loads((root/f'benchmarks/runs/20260908-5945-bve-heap-frontier/results/{arm}.json').read_text())
    assert r['scratch_deleted'] and 'exit_code' not in r
checks={}
commands=[(name,['python3',f'tools/docs/{name}.py']) for name in
          ('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary','check-proof-rule-sync','check-inventory')]
commands += [('attempt-parser',['python3','benchmarks/probes/scratch/analyze-first-bve-attempts-5943.py','--self-test']),
             ('heap-parser',['python3','benchmarks/probes/scratch/compare-bve-heap-lifecycle-5951.py','--self-test'])]
for name,cmd in commands:
    r=subprocess.run(cmd,cwd=root,capture_output=True,text=True)
    assert r.returncode==0,(name,r.stdout,r.stderr)
    checks[name]=r.stdout+r.stderr
owned=set()
def retain(p):
    assert p.is_file() and p.stat().st_size<5<<20,p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md','docs/scheduling-parity-ledger.md',
            'benchmarks/reports/2026-09-08-first-bve-attempts.md',
            'benchmarks/reports/2026-09-08-first-bve-attempts-validation.json'):
    retain(root/rel)
for number in range(5943,5955):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(number)+'.*'):retain(p)
for name in ('summary.json','bve-frontier.json','attempt-classification.json'):
    retain(root/'benchmarks/runs/20260908-5937-first-bve-frontier'/name)
for number in (5944,5945,5948,5949,5952):
    c=next((root/'benchmarks/runs').glob(f'20260908-{number}-*'))
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size>=5<<20:continue
        if 'logs' in p.relative_to(c).parts:continue
        if 'traces' in p.relative_to(c).parts and p.name not in ('candidate.bve-heap.tsv','kissat.bve-heap.tsv'):continue
        if p.suffix in ('.json','.py','.sbatch','.tsv','.c','.patch','.txt'):retain(p)
target.write_text(json.dumps(dict(hashes={rel:sha(root/rel) for rel in sorted(owned)},checks=checks,
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex','report.pdf')},
    outcome='Retain5900 at1.112038x. New evidence moves Rank32 BVE diagnosis to same-pop topology;no solver repair orpromotion.5952 small controls do not reproduce duplicate retirement.',
    cleanup='No manual deletion; cancelled5945 and completed5949 records confirm their owned scratch cleanup. Old campaigns,pins,large raw traces and uncertain material retained. No fuzz-owner artifact access.',
    next_action='Full heap immediately before122629 in firstBVE at271079;trace differing child subtrees to earlier update/push. Separately minimize inactive-pop eligibility witness.',
    limitation='Local checkpoint only. Push remains unapproved and is not retried. Report saved locally,not sent. Goal remains active and unmet.'),indent=2)+'\n')
retain(target)
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
print('PASS:',len(owned),'explicit owned paths staged; large raw traces retained locally')

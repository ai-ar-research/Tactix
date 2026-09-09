#!/usr/bin/env python3
"""Land scope, frozen evidence and isolated repair; never stage live traces."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
target=root/'benchmarks/reports/2026-09-08-rank50-publication-landing.json'
assert not target.exists()
assert not subprocess.run(['git','diff','--cached','--name-only'],cwd=root,capture_output=True,text=True,check=True).stdout
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8<<20),b''):h.update(block)
    return h.hexdigest()
owned=set()
def retain(p):
    assert p.is_file() and p.stat().st_size<5<<20,p
    owned.add(str(p.relative_to(root)))
audit=root/'benchmarks/reports/2026-09-08-rank50-publication-validation.json'
a=json.loads(audit.read_text())
for rel,digest in a['hashes'].items():assert sha(root/rel)==digest,rel
for name in ('DEVELOPMENT_PLAN.md','PROGRESS.md','docs/implementation-notes.md',
             'docs/scheduling-parity-ledger.md','tools/bench/fixed_runtime.py','tools/bench/test_fixed_runtime.py',
             'benchmarks/reports/2026-09-08-rank50-publication.md',
             'benchmarks/reports/2026-09-08-rank50-start.json',
             'benchmarks/reports/2026-09-08-rank50-publication-validation.json'):retain(root/name)
for n in range(6100,6120):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.*'):retain(p)
for name in ('rank1-50-selection-v1','rank1-50-kissat-12cpu-v1'):
    for p in (root/'benchmarks/baselines'/name).iterdir():retain(p)
c=root/'benchmarks/runs/20260908-6100-rank37-substitute-deactivation'
terminal=subprocess.run(['sacct','-n','-X','-j','1676781','-o','JobID,State,ExitCode','-P'],capture_output=True,text=True,check=True).stdout
assert terminal.count('COMPLETED|0:0')==2,terminal
s=json.loads((c/'summary.json').read_text());assert s['full_parent_replay']
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
for p in c.iterdir():
    if p.is_file():retain(p)
for folder in ('results','replay-controls','publication-fixture'):
    for p in (c/folder).iterdir():
        if p.suffix in ('.json','.log','.tsv','.txt'):retain(p)
# No traces from any campaign are added. The completed6100 traces are already
# bound by final results/summary; four extracted heaps suffice for fixed replay.
c=root/'benchmarks/runs/20260908-6107-substitute-publication'
for p in c.iterdir():
    if p.is_file() and p.suffix in ('.json','.patch'):retain(p)
for folder in ('source','logs'):
    for p in (c/folder).rglob('*'):
        if p.is_file():retain(p)
for number in (6108,6114,6115):
    matches=list((root/'benchmarks/runs').glob(f'20260908-{number}-*'));assert len(matches)==1
    c=matches[0]
    for name in ('manifest.json','launch.json','array.sbatch','matrix.tsv'):retain(c/name)
    for p in (c/'pins').iterdir():
        if p.suffix in ('.py','.patch'):retain(p)
    # Only records present in the completed audit are staged, not arbitrary
    # live output discovered later. These records were atomically published.
    for rel,digest in a['hashes'].items():
        p=root/rel
        if p.parent in (c/'results',c/'provisional'):retain(p)
report=root/'report-to-andrew/20260908-2232-rank50-publication'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(hashes={rel:sha(root/rel) for rel in sorted(owned)},
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex','report.pdf')},
    terminal=terminal,queue_at_audit=a['queue'],
    outcome='Userapproved full50goal. Current6049 starting1.216724524x. Tested6107generalSubstitutepublicationrepair hasBC6114/full506115running;no runtimegain orpromotion.',
    cleanup='No manual deletion. Audited normal results assert certificate/derivative cleanup. Allactive scratch/pins preserved. No live traces staged;terminal6100fulltraces remainlocalhashbound.',
    next_action='Inspect6114/6115,trace6107Rank37 against cached6100native,validate trace/journal builds,then largestremainingfull50gap. Continue proofs asynchronously;no denied fuzztransfer orpush retry.',
    validation='99/99Release twice,oldnegative andeightactualpolicycases;actualheap replayexactbotharms;24benchmarktests,sixselectionrejects,parsercontrols,sixdocchecks. Newfixtureadmissioncorrectedwithoutsolverchange.'),indent=2)+'\n')
retain(target)
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Expand runtime target to Rank 1-50 and repair Substitute publication order'],cwd=root,check=True)
assert not subprocess.run(['git','status','--porcelain','--',*sorted(owned)],cwd=root,capture_output=True,text=True,check=True).stdout
print('PASS:',len(owned),'explicit paths landed;no live traces,no deletion,no push')

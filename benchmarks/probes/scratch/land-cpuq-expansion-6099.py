#!/usr/bin/env python3
"""Land benchmark proposals and final diagnostic evidence; preserve live runs."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
target=root/'benchmarks/reports/2026-09-08-cpuq-expansion-landing.json'
assert not target.exists()
assert not subprocess.run(['git','diff','--cached','--name-only'],cwd=root,capture_output=True,text=True,check=True).stdout
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()
owned=set()
def retain(p):
    assert p.is_file() and p.stat().st_size<5<<20,p
    owned.add(str(p.relative_to(root)))
audit=root/'benchmarks/reports/2026-09-08-cpuq-expansion-validation.json'
for rel,digest in json.loads(audit.read_text())['hashes'].items():assert sha(root/rel)==digest,rel
selection=root/'benchmarks/reports/2026-09-08-cpuq-expansion'
s=json.loads((selection/'summary.json').read_text())
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
for p in selection.iterdir():retain(p)
for name in ('PROGRESS.md','benchmarks/reports/2026-09-08-cpuq-expansion.md',
             'benchmarks/reports/2026-09-08-cpuq-expansion-validation.json',
             'benchmarks/reports/2026-09-08-rank37-macro-heaps-completion.json'):retain(root/name)
for n in range(6096,6100):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.*'):retain(p)
cpuq=root/s['campaign']
for r in s['proposed_additions']:
    for key in ('tactix_task','kissat_task'):
        for sub in ('results','provisional'):retain(cpuq/sub/f"{r[key]:03d}.json")
c=root/'benchmarks/runs/20260908-6092-rank37-macro-heaps'
final=json.loads((c/'summary.json').read_text());assert final['full_parent_replay']
for p in (c/'results').glob('*.json'):
    r=json.loads(p.read_text());assert r['exit_code']==0 and r['scratch_deleted'];retain(p)
    for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest,name
retain(c/'summary.json')
for p in (c/'traces').iterdir():
    if p.stat().st_size<5<<20:retain(p)
# Only remove the accidental in-progress series from Git's index. Final bytes
# remain on disk and are hash-bound by the complete replay/completion seal.
large=c/'traces/kissat.macro-heaps.tsv'
rel=str(large.relative_to(root));digest=sha(large)
tracked=subprocess.run(['git','ls-files','--',rel],cwd=root,capture_output=True,text=True,check=True).stdout.strip()
assert tracked==rel and large.stat().st_size>=5<<20
report=root/'report-to-andrew/20260908-2150-cpuq-expansion'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
terminal=subprocess.run(['sacct','-n','-X','-j','1676779','-o','JobID,State,ExitCode','-P'],capture_output=True,text=True,check=True).stdout
assert terminal.count('COMPLETED|0:0')==2,terminal
queue=subprocess.run(['squeue','-h','-j','1673402,1676727','-o','%i|%P|%T|%C|%m|%R'],capture_output=True,text=True,check=True).stdout
target.write_text(json.dumps(dict(hashes={r:sha(root/r) for r in sorted(owned)},
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    terminal=terminal,queue=queue,
    untrack_without_deletion=dict(path=rel,final_bytes=large.stat().st_size,final_sha256=digest,
        reason='6095 inadvertently tracked this series while it was growing. Final evidence remains local/hash-bound;historical partial bytes remain recoverable in Git.'),
    outcome='Proposed50/60 preserveoriginal40;recommend7UNSAT+3SAT additions from checked older5741 cpu-q pairs. User width/mix pending,no baseline orrunner change.6092 completedfullreplay;firstheapdiff acrossSubstitute. Goal remainsunmet,no runtimegain orpromotion.',
    next_action='Confirm user width/mix and current-source verification contract;continue general Substitute order investigation. Keep originalRank1-40 runtime denominator and B28+C5 gates intact.',
    cleanup='No solver data or source deleted. Completed6092 andnormalpublishedrecords assert scratch/certificate cleanup;retain liveproofchecks. Large series onlyuntracked,notremoved. No fuzz-owner access ordenied transfer/push retry.',
    validation='Selection endpoint/classification fixtures,existing22benchmarktests,98/98observertrace suite,whole-parent replay andsixdocguardsPASS. Source5741 screeningnotcurrent6049 timing;cpu-q valuesnotmixedintofixedreference.'),indent=2)+'\n')
retain(target)
subprocess.run(['git','rm','--cached','--',rel],cwd=root,check=True)
assert large.is_file() and sha(large)==digest
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Select mostly UNSAT cpu-q expansion and seal Substitute heap bracket'],cwd=root,check=True)
print('PASS:',len(owned),'explicit paths landed;large series preserved locally;no push')

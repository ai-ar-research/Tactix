#!/usr/bin/env python3
"""Land validated heap brackets and the tested next observer, without promotion."""
import hashlib
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[3]
target=root/'benchmarks/reports/2026-09-08-rank37-heap-ancestry-landing.json'
assert not target.exists()
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()
def campaign(n):
    paths=list((root/'benchmarks/runs').glob(f'20260908-{n}-*'))
    assert len(paths)==1;return paths[0]
audit=root/'benchmarks/reports/2026-09-08-rank37-heap-ancestry-validation.json'
a=json.loads(audit.read_text())
for rel,digest in a['hashes'].items():assert sha(root/rel)==digest,rel
bc=next(c for c in a['campaigns'] if c['number']==6059)
assert bc['counts']=={'checked_valid':66} and not bc['unscored']
bracket=root/'benchmarks/reports/2026-09-08-rank37-phase-heaps.json'
b=json.loads(bracket.read_text());assert b['full_parent_replay_except_selected_heap']
assert not any(b['stages']['prephase']['change_counts'].values())
assert b['stages']['postphase']['change_counts']==dict(candidate_only=0,kissat_only=0,score=0,assigned=0,active=0,position=298)
report=root/'report-to-andrew/20260908-2129-rank37-heap-ancestry'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
owned=set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5<<20,p
    owned.add(str(p.relative_to(root)))
for p in (root/'PROGRESS.md',audit,bracket,root/'benchmarks/reports/2026-09-08-rank37-heap-ancestry.md'):retain(p)
for n in range(6080,6096):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.*'):retain(p)
for n in (6059,6061,6076,6081,6084,6085,6090,6092):
    c=campaign(n)
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size>=5<<20:continue
        parts=p.relative_to(c).parts
        if any(x in parts for x in ('claims','__pycache__')):continue
        if 'pins' in parts and p.suffix!='.py':continue
        if 'logs' in parts:
            if n==6090 and p.name in ('tactix-ctest.log','writer-test.log'):retain(p)
            continue
        if p.suffix in ('.json','.py','.sbatch','.tsv'):retain(p)
m=json.loads((campaign(6092)/'manifest.json').read_text())
for rel,digest in m['observer_source_hashes'].items():
    assert sha(root/rel)==digest,rel
    retain(root/rel)
for rel,digest in m['hashes'].items():assert sha(campaign(6092)/rel)==digest,rel
terminal=subprocess.run(['sacct','-n','-X','-j','1676769,1676773,1676775,1676776','-o','JobID,State,ExitCode','-P'],capture_output=True,text=True,check=True).stdout
assert terminal.count('COMPLETED|0:0')==8,terminal
queue=subprocess.run(['squeue','-h','-j','1673402,1676727,1676779','-o','%i|%P|%T|%C|%m|%R'],capture_output=True,text=True,check=True).stdout
target.write_text(json.dumps(dict(hashes={rel:sha(root/rel) for rel in sorted(owned)},
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex','report.pdf')},
    terminal=terminal,queue=queue,
    outcome='Validated equal incoming whole heap and298 positional differences after bounded conflict/inprocessing window. New macro observer98/98 and writer/parser fixturesPASS;6092/job1676779 awaits full replay before interpretation. No semantic repair or runtime gain;6049 remains1.092957983x fixedKissat,goal unmet.',
    next_action='Run6094 once both6092 tasks finish. Preserve frozen snapshots;locate first differing shared macro heap and its preceding common state before a general solver-transition repair. Continue proof checking asynchronously.',
    cleanup='No manual deletion. All four completed diagnostic pairs assert scratch cleanup;normal published results assert certificate/derivative cleanup. Keep live checker and6092 scratch,all source/pin/trace authorities. Large traces/build logs stay local,hash-bound;no fuzz-owner artifact access.',
    test_consideration='New timeline,writer,real Tactix macro integration and indexed-parser fixtures cover changed diagnostics. Full98-test trace suite,22 benchmark tests and six doc guards pass. No solver semantics or acceptance logic changed;native adapter replay is still required.',
    limitations='Current-source fuzz unverified;no promotion. Denied transfer/push not retried. Andrew PDF local,not sent. This checkpoint is causal localization,not runtime acceptance.'),indent=2)+'\n')
retain(target)
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Bracket Rank37 heap divergence and add tested macro history observer'],cwd=root,check=True)
print('PASS:',len(owned),'explicit paths landed locally;no push')

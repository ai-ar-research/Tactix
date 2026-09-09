#!/usr/bin/env python3
"""Seal final6092 evidence; supersede accidentally retained in-progress bytes."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6092-rank37-macro-heaps'
target=root/'benchmarks/reports/2026-09-08-rank37-macro-heaps-completion.json'
assert not target.exists()
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()
s=json.loads((c/'summary.json').read_text());assert s['full_parent_replay']
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
first=s['first_difference'];assert first['boundary']['key']==[272979,76952961,'Substitute_end',0]
assert first['boundary']['counts']==dict(candidate_only=0,kissat_only=0,score=0,assigned=0,active=0,position=296)
assert not any(first['preceding']['counts'].values())
terminal=subprocess.run(['sacct','-n','-X','-j','1676779','-o','JobID,State,ExitCode','-P'],capture_output=True,text=True,check=True).stdout
assert terminal.count('COMPLETED|0:0')==2,terminal
hashes={str((c/'summary.json').relative_to(root)):sha(c/'summary.json')}
for arm in ('candidate','kissat'):
    p=c/'results'/f'{arm}.json';r=json.loads(p.read_text())
    assert r['exit_code']==0 and r['scratch_deleted']
    hashes[str(p.relative_to(root))]=sha(p)
    for name,digest in r['trace_sha256'].items():
        p=c/'traces'/name;assert sha(p)==digest
        hashes[str(p.relative_to(root))]=digest
prior=root/'benchmarks/reports/2026-09-08-rank37-heap-ancestry-landing.json'
old=json.loads(prior.read_text())
prefix=str((c/'traces').relative_to(root))+'/'
superseded=[dict(path=rel,in_progress_sha256=digest,final_sha256=sha(root/rel))
            for rel,digest in old['hashes'].items() if rel.startswith(prefix) and sha(root/rel)!=digest]
target.write_text(json.dumps(dict(hashes=hashes,terminal=terminal,first_boundary=first['boundary'],
    preceding_boundary=first['preceding'],prior_landing=dict(path=str(prior.relative_to(root)),sha256=sha(prior)),
    superseded_in_progress_trace_hashes=superseded,
    correction='6095 inadvertently staged several still-growing6092 traces. They were not validated results or immutable final artifacts. This seal owns final hashes after terminal tasks and complete parent replay;prior landing remains historical. Keep large final macro series local and hash-bound,not tracked.',
    outcome='Complete shared heap equal atfirst Substitute entry272979/76952961;296 positions differ atits exit. Other membership/scores/flags stillmatch. Next investigate Substitute deactivation/removal order and incoming equivalences before any repair.',
    limitation='Macro snapshots bracket,not exact operation provenance. Pending score-history label omissions retained;no unit-cost or runtime gain claimed.'),indent=2)+'\n')
print('PASS: final6092 full replay and first Substitute bracket sealed;',len(superseded),'in-progress hashes superseded')

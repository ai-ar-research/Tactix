#!/usr/bin/env python3
"""Require replay except the deliberately moved heap,then compare whole states."""
import hashlib
import importlib.util
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sp=importlib.util.spec_from_file_location('heaps',Path(__file__).with_name('analyze-rank37-choice-6080.py'))
mod=importlib.util.module_from_spec(sp);sp.loader.exec_module(mod);mod.fixtures()
target=root/'benchmarks/reports/2026-09-08-rank37-phase-heaps.json';assert not target.exists()
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(8<<20),b''):h.update(block)
    return h.hexdigest()
out={};hashes={}
for number,stage in ((6084,'prephase'),(6085,'postphase')):
    c=root/f'benchmarks/runs/20260908-{number}-rank37-{stage}-heap'
    m=json.loads((c/'manifest.json').read_text());parent=root/m['parent_campaign']
    assert sha(parent/'choice-validation.json')==m['parent_summary_sha256']
    for rel,digest in m['hashes'].items():assert sha(c/rel)==digest,rel
    heaps={}
    for arm in ('candidate','kissat'):
        p=c/'results'/f'{arm}.json';r=json.loads(p.read_text())
        old=json.loads((parent/'results'/f'{arm}.json').read_text())
        assert r['exit_code']==0 and r['scratch_deleted']
        assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
        assert r['partial_certificate']==old['partial_certificate']
        for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest,name
        for name,digest in old['trace_sha256'].items():
            assert sha(parent/'traces'/name)==digest,name
            if name!=arm+'.heap.tsv':assert r['trace_sha256'][name]==digest,name
        heaps[arm]=mod.heap((c/'traces'/f'{arm}.heap.tsv').read_text().splitlines())
        hashes[str(p.relative_to(root))]=sha(p)
        hp=c/'traces'/f'{arm}.heap.tsv';hashes[str(hp.relative_to(root))]=sha(hp)
    changes=mod.compare(heaps['candidate'],heaps['kissat'])
    out[stage]=dict(selector=m['selected_heap'],heap_sizes={a:len(h) for a,h in heaps.items()},changes=changes,
                   change_counts={k:len(v) for k,v in changes.items()},selected={a:{str(v):h.get(v) for v in (116360,116362)} for a,h in heaps.items()})
target.write_text(json.dumps(dict(stages=out,hashes=hashes,full_parent_replay_except_selected_heap=True,
    limitation='Prephase is after last decision before its propagation/conflict;postphase is after first common next decision. This brackets conflict/backtrack/inprocessing/decision,not inprocessing alone. Exact states narrow the next observer window but do not establish relative strength or runtime gain.'),indent=2)+'\n')
print(json.dumps({s:{k:v for k,v in r.items() if k!='changes'} for s,r in out.items()},indent=2))

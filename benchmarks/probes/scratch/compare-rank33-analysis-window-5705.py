#!/usr/bin/env python3
"""Validate all original replay surfaces before exposing the five-conflict window."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import runpy
import sys

root=Path(__file__).resolve().parents[3]
normalizer=runpy.run_path(str(root/'benchmarks/probes/scratch/validate-square-phase-5486.py'))
def event(line,arm,seq):
    assert line.endswith('\n')
    f=line.rstrip('\n').split('\t')
    assert len(f)==22 and f[0]==arm and int(f[1])==seq and f[2]
    assert 182089<=int(f[3])<=182093
    assert all(int(f[i])>=0 for i in [4,5,6,*range(8,18)])
    assert f[7] in ('stable','focused','none') and len(f[18])==16
    assert 0<=int(f[18],16)<2**64 and f[-1]
    return f
def fixtures():
    good='tactix\t0\tanalysis_begin\t182089\t20\t30\t7\tstable\t'+'\t'.join(['0']*10)+'\t0000000000000000\t0\t-\t7\n'
    assert event(good,'tactix',0)[2]=='analysis_begin'
    assert event(good.replace('tactix','kissat'),'kissat',0)[0]=='kissat'
    bad=[good.rstrip('\n'),good+'\t0\n',good.replace('182089','182088'),
        good.replace('182089','182094'),good.replace('\t0\tanalysis','\t1\tanalysis'),
        good.replace('analysis_begin',''),good.replace('\t20\t30','\t-1\t30'),
        good.replace('stable','other'),good.replace('0000000000000000','nothex'),
        good.replace('\t-\t7\n','\t-\t\n')]
    for line in bad:
        try:event(line,'tactix',0)
        except (AssertionError,ValueError):pass
        else:raise AssertionError('Malformed analysis event accepted')
    normalizer['self_test']()
    print('PASS:12 window fixtures plus8 native-boundary fixtures',flush=True)
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8<<20),b''):h.update(block)
    return h.hexdigest()
def main():
    fixtures()
    if '--self-test' in sys.argv:return
    c=root/'benchmarks/runs/20260908-5704-rank33-analysis-window'
    if any(not (c/'results'/(a+'.json')).exists() for a in ('candidate','kissat')):
        print('5704 pending');return
    m=json.loads((c/'manifest.json').read_text());parent=root/m['parent_campaign']
    assert sha(parent/'summary.json')==m['parent_summary_sha256']
    assert sha(root/m['input'])==m['input_sha256_compressed']
    for name,digest in m['hashes'].items():assert sha(c/name)==digest
    captured={};evidence={};extras=0
    for arm in ('candidate','kissat'):
        p=c/'results'/(arm+'.json');r=json.loads(p.read_text())
        old=json.loads((parent/'results'/(arm+'.json')).read_text())
        assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
        assert r['binary_sha256']==m['hashes']['pins/'+arm]
        assert r['partial_certificate']==old['partial_certificate'] and r['partial_certificate']['bytes']>0
        for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest
        omitted={arm+'.'+suffix for suffix in m['omitted_parent_dumps']}
        assert not (omitted&r['trace_sha256'].keys())
        assert r['trace_sha256'].keys()==(old['trace_sha256'].keys()-omitted)|{arm+'.analysis.tsv'}
        for name,digest in old['trace_sha256'].items():
            if name in omitted:continue
            assert sha(parent/'traces'/name)==digest
            if arm=='kissat' and name.endswith('.boundaries.tsv'):
                read=lambda path:[line.split('\t') for line in path.read_text().splitlines() if not line.startswith('#')]
                extras=normalizer['validate_native_boundaries'](read(parent/'traces'/name),read(c/'traces'/name),(182089,182093))
            else:assert r['trace_sha256'][name]==digest,(arm,name)
        path=c/'traces'/(arm+'.analysis.tsv')
        rows=[]
        for line in path.open():
            if line.startswith('#'):continue
            rows.append(event(line,'tactix' if arm=='candidate' else arm,len(rows)))
        assert rows and len(rows)<100000
        assert any(row[2]=='analysis_begin' for row in rows)
        captured[arm]=rows
        evidence[arm]=dict(record_sha256=sha(p),traces=r['trace_sha256'])
    result=dict(parent_replay=True,normalization_fixtures=8,event_fixtures=12,
        native_extra_analysis_end=extras,evidence=evidence,
        event_counts={a:dict(Counter(r[2] for r in rows)) for a,rows in captured.items()},
        transactions={a:[r for r in rows if r[2] in ('analysis_begin','analysis_end','analyzed','analyzed_post','learned','learned_raw','learned_shrunk','conflict','score_bump')]
            for a,rows in captured.items()},
        limitation='All original retained surfaces and proof bytes replay. Only predeclared formula/binary dumps omitted and source-backed native analysis/end additions normalized. Inspect exact transaction/reason semantics before repair;no runtime claim.')
    p=c/'summary.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
    print('5704 replay and window PASS;native observer-only analysis rows',extras)
    print(json.dumps(result['event_counts'],indent=2))
if __name__=='__main__':main()

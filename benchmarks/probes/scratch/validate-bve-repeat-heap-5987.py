#!/usr/bin/env python3
"""Validate every repeated-pop image and identify the unique8966-member witness."""
import hashlib
import io
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
source = Path(__file__).with_name('compare-bve-full-heap-5960.py')
code = source.read_text().replace('ordinal<2', 'ordinal>=0').replace('len(groups)==2', 'len(groups)>=2')
ns = {'__file__': str(source), '__name__': 'heap_parser'}
exec(compile(code, str(source), 'exec'), ns)
parse, sift, sha = ns['parse'], ns['sift'], ns['sha']
c = root / 'benchmarks/runs/20260908-5988-bve-repeat-heap'

def select(images, count):
    matches = [(i, rows) for i, rows in enumerate(images) if i and len(rows) == count]
    assert len(matches) == 1, 'Missing/ambiguous repeated-pop occurrence'
    return matches[0]

def final_position(rows, path, subject):
    if path['removed']['external'] == subject: return -1
    if path['last']['external'] == subject: return path['last_final_pos']
    for step in path['path']:
        if step.get('stop'): break
        child = step['right'] if step['right'] and step['selected'] == step['right']['pos'] else step['left']
        if child['external'] == subject: return step['at']
    return next(r['pos'] for r in rows if r['external'] == subject)

def fixtures():
    ns['fixtures']()
    rows = [dict(pos=i, external=i+1, score=s) for i,s in enumerate(('0x1p+2','0x1p+1','0x1p+1','0x1p+0'))]
    path = sift(rows)
    assert [final_position(rows, path, v) for v in (1,2,3,4)] == [-1,0,2,1]
    assert select([rows, rows[:3], rows[:2]], 2)[0] == 2
    for bad in ([rows, rows], [rows, rows[:2], rows[:2]]):
        try: select(bad, 2)
        except AssertionError: pass
        else: raise AssertionError('Missing/ambiguous occurrence accepted')
    row = '0\t1\t0x1p+1\t2\t3\t0\t0\t1\n'
    text = '# bve-full-heap\t1\n# begin\t0\tinitial\t1\t0\t1\n'+row+'# end\t0\n'
    for i in (1,2,3): text += f'# begin\t{i}\tbefore-pop\t1\t1\t1\n'+row+f'# end\t{i}\n'
    assert len(parse(io.StringIO(text), 1, 1)) == 4
    try: parse(io.StringIO(text.replace('# end\t2', '# end\t1')), 1, 1)
    except AssertionError: pass
    else: raise AssertionError('Repeated frame disorder accepted')
    print('PASS: repeated-pop framing, absent/ambiguous selector and all sift position roles', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    target = c / 'summary.json'
    assert not target.exists()
    m = json.loads((c / 'manifest.json').read_text())
    for rel,digest in m['hashes'].items(): assert sha(c / rel) == digest
    data, evidence = {}, {}
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json')
        r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['binary_sha256'] == sha(c / 'pins' / arm)
        bound = m['cached_controls'][arm]
        oldp = root / bound['record']; assert sha(oldp) == bound['sha256']
        old = json.loads(oldp.read_text())
        assert r['partial_certificate'] == old['partial_certificate']
        for name,digest in old['trace_sha256'].items(): assert r['trace_sha256'][name] == digest
        parent = json.loads((root / 'benchmarks/runs/20260908-5978-bve-earlier-heap/results' / (arm+'.json')).read_text())
        assert r['trace_sha256'][arm+'.bve-candidates.tsv'] == parent['trace_sha256'][arm+'.bve-candidates.tsv']
        for name,digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
        with (c / 'traces' / (arm+'.full-heap.tsv')).open() as stream: data[arm] = parse(stream,271079,462198)
        evidence[arm] = dict(result_sha256=sha(p), traces=r['trace_sha256'])
    assert len(data['candidate']) == len(data['kissat'])
    comparisons=[]
    for i,(a,b) in enumerate(zip(data['candidate'],data['kissat'])):
        am={r['external']:r for r in a}; bm={r['external']:r for r in b}; common=am.keys()&bm.keys()
        comparisons.append(dict(ordinal=i,candidate_size=len(a),kissat_size=len(b),
            candidate_only=sorted(am.keys()-bm.keys()),kissat_only=sorted(bm.keys()-am.keys()),
            differences={k:sum(am[v][k]!=bm[v][k] for v in common) for k in ns['FIELDS'] if k!='external'},
            first_differences=[dict(external=v,candidate=am[v],kissat=bm[v]) for v in sorted(common) if am[v]!=bm[v]][:30]))
    selected={arm:select(images,8966) for arm,images in data.items()}
    assert selected['candidate'][0] == selected['kissat'][0]
    paths={arm:sift(rows) for arm,(i,rows) in selected.items()}
    positions={arm:final_position(selected[arm][1],p,60705) for arm,p in paths.items()}
    assert positions == {'candidate':8930,'kissat':8954}, positions
    target.write_text(json.dumps(dict(full_replay=True,evidence=evidence,snapshots=comparisons,
        target_ordinal=selected['candidate'][0],target_subject=60705,subject_positions=positions,sift_paths=paths,
        limitation='All repeated pops captured and unique8966-member occurrence selected explicitly. Sift reconstruction validates observed placement, not its earlier cause or a runtime gain.'),indent=2)+'\n')
    print(json.dumps(dict(snapshots=[{k:v for k,v in s.items() if k!='first_differences'} for s in comparisons],
                         target_ordinal=selected['candidate'][0],subject_positions=positions),indent=2))

if __name__ == '__main__': main()

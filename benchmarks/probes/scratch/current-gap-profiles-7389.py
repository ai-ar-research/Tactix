#!/usr/bin/env python3
"""Current Rank31/43 diagnostic: immutable four-arm sampling, no score refresh."""
import csv
import datetime as dt
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import sha, reference
from checked_comparison import classify
from runtime_summary import counters

C = ROOT / 'benchmarks/runs/20260911-7389-current-gap-profiles'
CURRENT = ROOT / 'benchmarks/runs/20260911-7385-corrected-root-ranks1-50'
NATIVE = ROOT / 'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
PROTOCOL = ROOT / 'benchmarks/runs/20260908-5544-small-rank39-profile'
MATRIX = [(31,'candidate'), (31,'kissat'), (43,'candidate'), (43,'kissat')]


def read(p):
    return json.loads(p.read_text())


def eligible(r, final):
    state = classify(r, final)
    assert state['status'] in ('pending_validation', 'checked_UNSAT'), state
    return state


def replay(a, b, arm):
    for key in ('instance', 'expected', 'verdict', 'certificate_sha256', 'certificate_bytes'):
        assert a[key] == b[key], key
    assert counters(a) == counters(b) and all(v is not None for v in counters(a).values())
    if arm == 'candidate':
        assert [s for s in a['solver_statistics'] if not s.startswith('c timing')] == [s for s in b['solver_statistics'] if not s.startswith('c timing')]


def fixtures():
    r = dict(instance='x', expected='UNSAT', verdict='UNSAT', certificate_sha256='a', certificate_bytes=2,
             solver_statistics=['c conflicts 3', 'c decisions 4', 'c propagations 5', 'c denseocc 7'])
    replay(r, r, 'candidate')
    rejected = 0
    for key, value in [('instance','other'), ('expected','SAT'), ('verdict','SAT'),
                       ('certificate_sha256','b'), ('certificate_bytes',3),
                       ('solver_statistics',r['solver_statistics']+['c new work 1']),
                       ('solver_statistics',[s.replace('conflicts 3','conflicts 4') for s in r['solver_statistics']]),
                       ('solver_statistics',[s.replace('denseocc 7','denseocc 8') for s in r['solver_statistics']])]:
        try: replay(r, dict(r, **{key:value}), 'candidate')
        except AssertionError: rejected += 1
    assert rejected == 8
    print('Current profile replay: eight negative identity/work/certificate controls PASS')


def prepare():
    assert not C.exists(), 'never overwrite a prepared/submitted campaign'
    cm, nm, pm = (read(p/'manifest.json') for p in (CURRENT,NATIVE,PROTOCOL))
    deps = {}
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, str(p)
        deps[str(p.relative_to(ROOT))] = digest
    for p, m in ((CURRENT,cm),(NATIVE,nm),(PROTOCOL,pm)):
        bind(p/'manifest.json')
        for rel,digest in m['pin_sha256'].items(): bind(p/rel,digest)
    for rel,digest in cm['evidence_sha256'].items(): bind(ROOT/rel,digest)
    bind(Path(__file__))
    for name in ('fixed_runtime','checked_comparison','runtime_summary'):
        bind(ROOT / f'tools/bench/{name}.py')
    summary = ROOT / 'benchmarks/reports/2026-09-11-corrected-root-full50-7385/summary.json'
    bind(summary); full = read(summary)
    selected = sorted((r for r in full['rows'] if r['expected']=='UNSAT'),
                      key=lambda r:r['candidate_seconds']-r['kissat_seconds'], reverse=True)[:2]
    assert [r['rank'] for r in selected] == [31,43]
    ref = reference(); assert nm['arms']['kissat']['sha256'] == ref['solver']['sha256']
    bind(ROOT/'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json')
    records = {}; rows = []
    for task,(rank,arm) in enumerate(MATRIX):
        p = (CURRENT if arm=='candidate' else NATIVE) / f'results/{rank-1 if arm=="candidate" else 2*rank-1:03d}.json'
        if not p.exists():
            assert arm == 'candidate'
            p = CURRENT / f'provisional/{rank-1:03d}.json'
        r = read(p); eligible(r,p.parent.name=='results'); bind(p)
        if arm == 'kissat': assert classify(r,True)['checked'] and classify(r,True)['clean']
        assert r['rank'] == rank and r['instance'] == ref['records'][rank-1]['instance']
        bind(ROOT/'benchmarks/cache'/r['instance'],cm['input_sha256'][r['instance']])
        records[f'{rank}/{arm}'] = dict(path=str(p.relative_to(ROOT)),sha256=sha(p),final=p.parent.name=='results')
        rows.append(dict(task=task,rank=rank,arm=arm,tier='D',instance=r['instance'],expected='UNSAT'))
    protected = read(CURRENT/'launch.json')['protected_campaign_hashes']
    for rel,digest in protected.items(): bind(ROOT/rel,digest)
    for name in ('pins','logs','results','provisional','claims','profiles'): (C/name).mkdir(parents=True)
    for name in ('runtime_task.py','checkmodel','modelcheck.py','drat-trim'):
        shutil.copy2(CURRENT/'pins'/name,C/'pins'/name)
    shutil.copy2(CURRENT/'pins/tactix',C/'pins/candidate')
    shutil.copy2(NATIVE/'pins/kissat',C/'pins/kissat')
    shutil.copy2(PROTOCOL/'pins/run.py',C/'pins/run.py')
    shutil.copy2(CURRENT/'pins/candidate-source.patch',C/'pins/candidate-source.patch')
    with (C/'matrix.tsv').open('x',newline='') as out:
        writer=csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n'); writer.writeheader(); writer.writerows(rows)
    batch=(CURRENT/'array.sbatch').read_text()
    assert batch.count('--array=0-49') == 1
    batch=batch.replace(str(CURRENT),str(C)).replace('--array=0-49','--array=0-3').replace('7385-root-full50','7389-current-gap-profile').replace('/pins/runtime_task.py','/pins/run.py')
    (C/'array.sbatch').write_text(batch)
    m=dict(schema='current-gap-profiles-7389-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        arms=dict(candidate=dict(cm['arms']['tactix'],path='pins/candidate'),kissat=nm['arms']['kissat']),
        input_sha256={r['instance']:cm['input_sha256'][r['instance']] for r in rows},
        collect_runtime_context=True,resource_contract=dict(cm['resource_contract'],array='0-3'),
        profile=dict(event='cycles:u',frequency=199,call_graph='dwarf,8192'),
        dependencies=deps,protected_campaign_hashes=protected,unprofiled_records=records,
        selection=[dict(rank=r['rank'],gap_seconds=r['candidate_seconds']-r['kissat_seconds']) for r in selected],
        hypothesis='Locate complete search/inprocessing cost envelopes on the two largest current UNSAT losses. Both have excess work; no common-state unit-price inference. Use current binary callpaths to select a substantial mechanism before editing.',
        validation='Four expected UNSAT outcomes; own complete proof bytes/hash and conflicts/decisions/propagations exact; candidate every non-timing statistic exact, including storage. Valid bound profile reports and zero lost samples. Independent checks and cleanup remain asynchronous; published invalidity pauses dependent work.',
        limitation='One four-task diagnostic, not timing acceptance or fixed-reference refresh. Sampling wrapper can inflate RSS; use full7385 unprofiled memory. No promotion or fuzz waiver.',
        test_consideration='Unchanged solver, checker, runtime and profile wrappers. Eight new replay rejection controls; existing harness tests. No duplicate solver/fuzz campaign for diagnostic-only execution.',
        authority='Active convergence goal; bounded missing native diagnostics permitted by fixed-reference policy. Preserve7239.010s denominator and established4/32/33/49/46 closures. Rank21 remains shelved but scored.')
    m['pin_sha256']={str(p.relative_to(C)):sha(p) for p in [*(C/'pins').iterdir(),C/'matrix.tsv',C/'array.sbatch']}
    with (C/'manifest.json').open('x') as out: json.dump(m,out,indent=2)
    for p in (C/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(C)


def bindings(m):
    for rel,digest in m['pin_sha256'].items(): assert sha(C/rel)==digest,rel
    for group in ('dependencies','protected_campaign_hashes'):
        for rel,digest in m[group].items(): assert sha(ROOT/rel)==digest,rel


def submit():
    assert not (C/'launch.json').exists(), 'already submitted'
    m=read(C/'manifest.json'); bindings(m)
    assert [r['rank'] for r in m['selection']]==[31,43]
    # A later published failure must stop submission, even when the bound baseline is provisional.
    for rank in (31,43):
        p=CURRENT/f'results/{rank-1:03d}.json'
        if p.exists(): eligible(read(p),True)
    since=dt.datetime.now().replace(microsecond=0).isoformat()
    command=['sbatch','--parsable',str(C/'array.sbatch')]
    job=subprocess.check_output(command,text=True).strip().split(';')[0]; assert job.isdigit()
    with (C/'launch.json').open('x') as out:
        json.dump(dict(job_id=job,accounting_since=since,command=command,
                       manifest_sha256=sha(C/'manifest.json'),submit_helper_sha256=sha(Path(__file__))),out,indent=2)
    print(job,since)


def validate(seal=False):
    target=C/'solver-summary-7390.json'
    if seal: assert not target.exists(), 'successful snapshot frozen'
    m=read(C/'manifest.json'); bindings(m); hashes={}; arms={}
    def bind(p,expected=None):
        d=sha(p); assert expected is None or d==expected,str(p); hashes[str(p.relative_to(ROOT))]=d
    for task,(rank,arm) in enumerate(MATRIX):
        final=C/f'results/{task:03d}.json'; provisional=C/f'provisional/{task:03d}.json'
        p=final if final.exists() else provisional
        if not p.exists(): continue
        r=read(p); state=eligible(r,final.exists()); bind(p)
        assert (r['task'],r['rank'],r['arm'])==(task,rank,arm)
        base=m['unprofiled_records'][f'{rank}/{arm}']; oldpath=ROOT/base['path']
        bind(oldpath,base['sha256']); old=read(oldpath); eligible(old,base['final']); replay(r,old,arm)
        if final.exists(): assert r['binary_sha256']==m['arms'][arm]['sha256']
        if provisional.exists():
            bind(provisional); prov=read(provisional)
            for key in ('solve','certificate_sha256','certificate_bytes','solver_statistics'): assert r[key]==prov[key]
        profile=r['solve']['profile']; assert set(profile['reports'])=={'self','callpaths'}
        for name,report in profile['reports'].items():
            assert report['exit']==0; bind(C/f'profiles/{task:03d}-{name}.txt',report['sha256'])
        data=C/f'profiles/{task:03d}.perf.data.xz'; bind(data,profile['compressed_sha256'])
        assert data.stat().st_size==profile['compressed_bytes']>0
        body=(C/f'profiles/{task:03d}-self.txt').read_text(); assert '# Total Lost Samples: 0' in body
        samples=[]
        for line in body.splitlines():
            match=re.match(r'\s*([\d.]+)%\s+(\S+)\s+\[\.\]\s+(.+?)\s+-\s+-\s*$',line)
            if match: samples.append(dict(percent=float(match[1]),dso=match[2],symbol=match[3]))
        assert samples and any(row['dso']==arm for row in samples)
        arms[f'{rank}/{arm}']=dict(rank=rank,arm=arm,state=state,record=str(p.relative_to(ROOT)),
            work=counters(r),self_samples=samples,diagnostic_seconds=r['solve']['wall_seconds'],profile=profile)
    print('Profile solvers',len(arms),'/4; checked',sum(a['state']['checked'] for a in arms.values()))
    if len(arms)!=4: return
    for name in ('manifest.json','launch.json'): bind(C/name)
    bind(Path(__file__))
    if seal:
        with target.open('x') as out: json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
            hashes=hashes,arms=arms,limitation=m['limitation']),out,indent=2)
        print(target)
    for key,a in arms.items(): print(key,json.dumps(a['self_samples'][:8]))


if __name__=='__main__':
    assert sys.argv[1:] in ([],['--self-test'],['--prepare'],['--submit'],['--seal'])
    fixtures()
    if sys.argv[1:]==['--prepare']: prepare()
    elif sys.argv[1:]==['--submit']: submit()
    elif sys.argv[1:]!=['--self-test']: validate(sys.argv[1:]==['--seal'])

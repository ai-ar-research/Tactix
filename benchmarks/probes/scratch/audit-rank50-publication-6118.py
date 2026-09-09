#!/usr/bin/env python3
"""Fresh normal-campaign audit; preserve pending checks and frozen ancestors."""
from pathlib import Path
source=Path(__file__).with_name('audit-root-lock-6067.py')
code=source.read_text()
changes={
 '2026-09-08-root-reason-repair-validation.json':'2026-09-08-rank50-publication-validation.json',
 '2026-09-08-root-reason-repair.md':'2026-09-08-rank50-publication.md',
 '5968, 5970, 6059, 6061)':'5968, 5970, 6059, 6061, 6108, 6114, 6115)',
 'number in (6059, 6061)':'number in (6059, 6061, 6108, 6114, 6115)',
 "jobs = '1673402,1676660,1676661,1676727'":"jobs = '1673402,1676727,1676783,1676803,1676804'",
 "'-j', '1676660,1676661,1676727'":"'-j', '1676727,1676781,1676783,1676803,1676804'",
 'General6049 root-reason repair:62 checked fixed proofs,four suites,18 old/new cases,3 native cases;6054 observer98/98.6060 closes55 reductions and69828932 decisions through300K. BC66 expected;full6061 submitted.':
 'Userapproved Rank1-50target;fixedKissat7239.010s preservesold40. Starting6049=1.216724524x,50expected.6107generalpublicationrepair99/99Release twice;BC6114/full506115 submitted. No speedup orpromotion claimed.',
 "'PROGRESS.md', 'docs/calculus.md'":"'PROGRESS.md', 'DEVELOPMENT_PLAN.md', 'docs/scheduling-parity-ledger.md', 'tools/bench/fixed_runtime.py', 'tools/bench/test_fixed_runtime.py', 'docs/calculus.md'",
}
for old,new in changes.items():
    assert old in code,old;code=code.replace(old,new)
extra='''
c=run_dir(6107);m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['files'].items():assert sha(root/m['source']/rel)==digest,rel
for rel,digest in m['binaries'].items():assert sha(root/rel)==digest,rel
for rel,digest in m['logs'].items():assert sha(c/'logs'/rel)==digest,rel
for name in ('manifest.json','build-summary.json','causal-replay.json','repair.patch','cumulative.patch'):bind(c/name)
for rel,digest in json.loads((c/'causal-replay.json').read_text())['hashes'].items():assert sha(root/rel)==digest,rel
c=run_dir(6100)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(c/rel)==digest,rel
s=json.loads((c/'summary.json').read_text());assert s['full_parent_replay']
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
for p in (c/'results').glob('*.json'):
    r=json.loads(p.read_text());assert r['exit_code']==0 and r['scratch_deleted'];bind(p)
    for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest,name
for name in ('manifest.json','launch.json','summary.json'):bind(c/name)
c=run_dir(6108);m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['protected_campaign_hashes'].items():assert sha(root/rel)==digest,rel
for rel in ('benchmarks/baselines/rank1-50-selection-v1/manifest.json',
            'benchmarks/baselines/rank1-50-selection-v1/matrix.tsv',
            'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json',
            'benchmarks/reports/2026-09-08-rank50-start.json'):bind(root/rel)
s=json.loads((root/'benchmarks/reports/2026-09-08-rank50-start.json').read_text())
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
assert s['expected_solves']==50 and abs(s['ratio']-1.216724524486083)<1e-12
'''
assert code.count('checks = {}')==1
code=code.replace('checks = {}',extra+'\nchecks = {}')
anchor='for label, command in commands:'
assert code.count(anchor)==1
tests='''
commands += [('rank50-membership',['python3','benchmarks/probes/scratch/prepare-rank41-50-matched-6108.py','--test']),
 ('deactivation-parser',['python3','benchmarks/probes/scratch/compare-substitute-deactivation-6102.py','--self-test'])]
'''
code=code.replace(anchor,tests+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

#!/usr/bin/env python3
"""Fresh normal-campaign audit and heap evidence checkpoint; no fuzz access."""
from pathlib import Path
source = Path(__file__).with_name('audit-root-lock-6067.py')
code = source.read_text()
for old, new in {
    '2026-09-08-root-reason-repair-validation.json': '2026-09-08-rank37-heap-ancestry-validation.json',
    '2026-09-08-root-reason-repair.md': '2026-09-08-rank37-heap-ancestry.md',
    "jobs = '1673402,1676660,1676661,1676727'": "jobs = '1673402,1676660,1676727,1676769,1676773,1676775,1676776,1676779'",
    "'-j', '1676660,1676661,1676727'": "'-j', '1676660,1676727,1676769,1676773,1676775,1676776,1676779'",
    'General6049 root-reason repair:62 checked fixed proofs,four suites,18 old/new cases,3 native cases;6054 observer98/98.6060 closes55 reductions and69828932 decisions through300K. BC66 expected;full6061 submitted.':
        'Current6049 remains1.092957983x fixedKissat;goal unmet.6076 proves same complete heap membership/scores/assignment/active with254 differing positions.6081 matched selected-variable history is equal.6088 corrects omitted same-conflict rounds.6084/6085 bracket whole heap;no solver change or promotion.',
}.items():
    assert old in code, old
    code = code.replace(old, new)
extra = '''
for number in (6076,6081,6084,6085,6092):
    c=run_dir(number)
    m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['hashes'].items():assert sha(c/rel)==digest,rel
    for field in ('observer_source_hashes','control_logs','writer_test_sources'):
        for rel,digest in m.get(field,{}).items():
            assert sha(root/rel)==digest,rel
            bind(root/rel)
    for name in ('manifest.json','launch.json'):bind(c/name)
    for p in (c/'results').glob('*.json'):
        r=json.loads(p.read_text())
        assert r['exit_code']==0 and r['scratch_deleted']
        bind(p)
    for name in ('choice-validation.json','state-analysis.json','summary.json','same-conflict-localization.json'):
        p=c/name
        if p.exists():
            bind(p)
            for rel,digest in json.loads(p.read_text()).get('hashes',{}).items():assert sha(root/rel)==digest,rel
s=json.loads((run_dir(6076)/'state-analysis.json').read_text())
assert s['change_counts']==dict(candidate_only=0,kissat_only=0,score=0,assigned=0,active=0,position=254)
s=json.loads((run_dir(6081)/'summary.json').read_text())
assert s['parent_full_replay'] and s['common_boundary_count']==11804
assert all(v is None for v in s['first_field_differences'].values())
p=root/'benchmarks/reports/2026-09-08-rank37-phase-heaps.json'
if p.exists():
    bind(p)
    s=json.loads(p.read_text());assert s['full_parent_replay_except_selected_heap']
    for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
bind(run_dir(6061)/'solver-checkpoint.json')
s=json.loads((run_dir(6061)/'solver-checkpoint.json').read_text())
assert s['expected_solves']==40 and not s['runtime_target_met']
assert abs(s['ratio']-1.0929579830797531)<1e-12
'''
assert code.count('checks = {}') == 1
code = code.replace('checks = {}', extra + '\nchecks = {}')
anchor = 'for label, command in commands:'
tests = '''
commands += [(name,['python3','benchmarks/probes/scratch/'+name+'.py','--self-test']) for name in
             ('analyze-rank37-choice-6080','compare-rank37-position-history-6083','localize-rank37-same-conflict-6088','compare-macro-heaps-6094')]
'''
assert code.count(anchor) == 1
code = code.replace(anchor, tests + anchor)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

#!/usr/bin/env python3
"""Audit independent Factor/XOR experiments and all inherited active campaigns."""
from pathlib import Path
source = Path(__file__).with_name('audit-binary-watch-decision-6898.py')
code = source.read_text()
for before,after in {
    '2026-09-10-binary-watch-audit-6898.json':'2026-09-10-factor-xor-cost-audit-6940.json',
    '2026-09-10-subsolver-timing-audit-6891.json':'2026-09-10-watch-rebuild-timing-audit-6923.json',
    '2026-09-10-binary-watch-decision.md':'2026-09-10-factor-xor-cost.md',
    'range(6893,6900)':'range(6925,6972)',
    ".glob(f'*-{number}.py')":".glob(f'*-{number}.*')",
    "for rel, previous_states in prior['campaign_states'].items():":
        "campaigns = dict(prior['campaign_states'])\nfor name in ('6933-factor-count-BC-gate','6936-factor-count-runtime','6945-congruence-release-profiles','6957-xor-presence-BC-gate','6960-xor-presence-runtime','6965-xor-stage-time'):\n    campaigns['benchmarks/runs/20260910-'+name] = {}\nfor rel, previous_states in campaigns.items():",
    "count = 18 if c.name.endswith('-runtime') else (4 if c.name.endswith('-profiles') else 66)":
        "with (c/'matrix.tsv').open() as inp:\n        count = len(list(__import__('csv').DictReader(inp,delimiter='\\t')))",
    "assert '1679208' in accounting":"assert all(j in accounting for j in ('1679306','1679374','1679384','1679390','1679380','1679396'))",
    'report-to-andrew/20260910-0250-binary-watch-decision':
        'report-to-andrew/20260910-0450-factor-xor-cost',
}.items():
    assert before in code,before
    code = code.replace(before,after)
start = code.index("c = root/'benchmarks/runs/20260910-6893-watch-dispatch-build'")
end = code.index('accounting = {}',start)
code = code[:start]+'''object_bindings = prior['classified_object_bindings']
bind(object_bindings)
recovery = dict(classification=prior['inspection_classification'])
for stem in ('6925-factor-count','6950-xor-presence'):
    c = root/f'benchmarks/runs/20260910-{stem}-build'
    m = summary(c/'manifest.json')
    for rel,digest in m['files'].items(): assert sha(root/m['source']/rel) == digest,rel
    bind(m['binaries'])
    for rel,digest in m['patches'].items():
        assert sha(c/rel) == digest; own(c/rel)
    for rel,digest in m['logs'].items(): assert sha(c/rel) == digest,rel
    own(c/'parent.json')
    for p in (c/'logs').glob('*.json'): own(p)
    control = summary(c/('control-6928.json' if stem.startswith('6925') else 'control-6951.json'))
    for key in ('parent_objects','artifacts','logs'): bind(control[key])
    assert control['outputs']['previous'] == control['outputs']['candidate']
factor = root/'benchmarks/runs/20260910-6925-factor-count-build'
compiled = summary(factor/'compile-6926.json')
for value in compiled['objects'].values(): assert sha(root/value['path']) == value['sha256']
inspection = summary(factor/'inspection-6930.json')
assert inspection['counter_helper_bytes'] == dict(previous=431,candidate=419)
kernel = summary(factor/'kernel-code-6943.json')
assert kernel['records']['previous'] == kernel['records']['candidate']
assert not (factor/'control-6927.json').exists() and not (factor/'kernel-code-6942.json').exists()
xor = root/'benchmarks/runs/20260910-6950-xor-presence-build'
index = summary(xor/'index-control-6953.json')
assert index['outputs']['previous'] == index['outputs']['candidate']
kernel = summary(xor/'kernel-code-6964.json')
assert kernel['selected_bodies_identical']
removed = 0
for name,count,half in (('6932-factor-count-proofs',288,144),('6956-xor-presence-proofs',96,48)):
    c = root/f'benchmarks/runs/20260910-{name}'
    s = summary(c/'summary.json')
    assert s['checked'] == count and s['negative_controls'] == 2
    assert s['LRAT_cake_lpr'] == s['DRAT_drat_trim'] == half
    assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
    removed += s['removed_bytes']
    for p in (c/'logs').glob('*.json'): own(p)
assert removed == 608678
summary(root/'benchmarks/runs/20260910-6944-congruence-cost-selection/summary.json')
profile_summary = summary(root/'benchmarks/runs/20260910-6945-congruence-release-profiles/solver-summary-6947.json')
assert set(profile_summary['arms']) == {'47/candidate','47/kissat'}
annotation_dir = root/'benchmarks/runs/20260910-6948-congruence-extraction-annotation'
annotation = summary(annotation_dir/'summary.json')
assert annotation['temporary_expansions_deleted_bytes'] == 240062556
assert not list(annotation_dir.glob('annotation-*'))
location = summary(annotation_dir/'location-6949.json')
assert location['period'] == 6751443230
for name,checkpoint,cost in (('6936-factor-count-runtime',6938,6939),('6960-xor-presence-runtime',6962,6963)):
    c = root/f'benchmarks/runs/20260910-{name}'
    s = summary(c/f'solver-checkpoint-{checkpoint}.json')
    assert s['solver_outcomes'] == 18 and s['exact_per_arm_work_and_proof']
    p = summary(c/f'timer-cost-{cost}.json')
    assert p['solved'] == dict(candidate=9,previous=9,SAT=0,UNSAT=18,timeouts=0,solve_losses=0)
for name,number in (('6933-factor-count-BC-gate',6935),('6957-xor-presence-BC-gate',6968)):
    s = summary(root/f'benchmarks/runs/20260910-{name}/summary-{number}.json')
    assert s['solver_records'] == 66 and len(s['pairs']) == 33
    assert all(p['reported_work_equal'] and p['certificate_equal'] for p in s['pairs'])
summary(root/'benchmarks/runs/20260910-6957-xor-presence-BC-gate/preflight-classification-6968.json')
stage = summary(root/'benchmarks/runs/20260910-6965-xor-stage-time/summary-6967.json')
assert stage['exact_complete_work_and_proof']
ite = root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build'
compiled = summary(ite/'compile-6970.json')
for v in compiled['objects'].values(): assert sha(root/v['path']) == v['sha256']
inspection = summary(ite/'inspection-6971.json')
assert inspection['body_bytes'] == dict(previous=16014,candidate=16095)
own(ite/'parent.json'); own(ite/'compile-scope.patch')
for p in (ite/'logs').glob('*.json'): own(p)

''' + code[end:]
marker = "            own(p)\n            current[r['task']]"
assert marker in code
code = code.replace(marker,"""            if 'unprofiled_records' in m:
                binding = m['unprofiled_records'][f"{r['rank']}/{r['arm']}"]
                assert sha(root/binding['path']) == binding['sha256']
                old = json.loads((root/binding['path']).read_text())
                for key in ('certificate_sha256','certificate_bytes','verdict','instance'):
                    assert r[key] == old[key],(p,key)
                from runtime_summary import counters
                assert counters(r) == counters(old)
                if r['arm'] == 'candidate':
                    nt = lambda v: [s for s in v['solver_statistics'] if not s.startswith('c timing')]
                    assert nt(r) == nt(old),p
            own(p)
            current[r['task']]""")
marker = "command = ['squeue','-h','-u','hwu','-o','%i|%j|%T|%P|%C|%m']"
assert marker in code
code = code.replace(marker,"""new_profiles = root/'benchmarks/runs/20260910-6945-congruence-release-profiles'
for p in sorted((new_profiles/'results').glob('*.json')):
    r = json.loads(p.read_text())
    assert r['runtime_context']['perf_statistics'] == []
    counter_sources[str(p.relative_to(root))] = sha(p)
p = new_profiles/'pins/run.py'
assert "command[:2] == ['perf', 'stat']" in p.read_text()
counter_sources[str(p.relative_to(root))] = sha(p)

"""+marker)
start = code.index('    cleanup=dict(')
end = code.index("\nwith target.open('x')",start)
code = code[:start]+'''    cleanup=dict(fixed_proof_bytes=608678,profile_expansion_bytes=240062556,
        note='Only6932/6956 verified regenerable fixed CNF/proofs including negative controls and6948 own temporary profile expansion removed. Logs, failed6927 controls, raw/ compressed profiles, source, pins, old22-byte invalid LRAT witness and live checker scratch retained. No fuzz access or denied push/transfer retry.'),
    limitation='Reject6925 whole-binary tradeoff; retain6950 only for full50 measurement after repeated47/42 gains and neutral38, no composition or production promotion. Full50 remains8300.164/7239.010=1.146588277; global scheduling-CDCL/memory target unmet.384 new strict fixed proofs do not replace source fuzz or all historical LRAT checking. Both18-solve same-work runtime gates reported, adverse results retained; proofs remain asynchronous.6969 is compile-only: outer ITE pair-load premise holds but targeted tests/fullbuild/proofs/B-C required before solver runs. Rank21 shelved; native47 has different internal work despite5 closures each. No FlatLists redesign or reference refresh. Winddown05:45/cutoff06:00EDT September10. Andrew report local unsent.')
''' + code[end:]
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

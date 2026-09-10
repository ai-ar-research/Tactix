#!/usr/bin/env python3
"""Audit the completed reconnect timing gate and stronger fixed proof gate."""
from pathlib import Path

source = Path(__file__).with_name('audit-watch-rebuild-6917.py')
wrapper = source.read_text()
tail = "exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})"
assert wrapper.count(tail) == 1
scope = {'__file__':str(source)}
exec(compile(wrapper.replace(tail, 'audit_code = code'), str(source), 'exec'), scope)
code = scope['audit_code']
for before, after in {
    "target = root/'benchmarks/reports/2026-09-10-watch-rebuild-audit-6917.json'":
        "target = root/'benchmarks/reports/2026-09-10-watch-rebuild-timing-audit-6923.json'",
    "prior_path = root/'benchmarks/reports/2026-09-10-binary-watch-audit-6898.json'":
        "prior_path = root/'benchmarks/reports/2026-09-10-watch-rebuild-audit-6917.json'",
    "owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-watch-rebuild-oracle.md','tools/bench/proof_oracle.py','tools/bench/test_proof_oracle.py','tools/bench/test_proof_oracle_binaries.py'}":
        "owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-watch-rebuild-timing.md'}",
    'range(6900,6919)':'range(6919,6925)',
    "campaigns['benchmarks/runs/20260910-6907-watch-rebuild-BC-gate'] = {}":
        "campaigns.setdefault('benchmarks/runs/20260910-6907-watch-rebuild-BC-gate', {})",
    "campaigns['benchmarks/runs/20260910-6912-watch-rebuild-runtime'] = {}":
        "campaigns.setdefault('benchmarks/runs/20260910-6912-watch-rebuild-runtime', {})",
    "required = 66 if c.name == '20260910-6907-watch-rebuild-BC-gate' else len(previous_states)":
        "required = count",
    "assert '1679208' in accounting and '1679218' in accounting":
        "assert all(j in accounting for j in ('1679208','1679218','1679284','1679290','1679300'))",
    'report-to-andrew/20260910-0312-watch-rebuild-oracle':
        'report-to-andrew/20260910-0334-watch-rebuild-timing',
}.items():
    assert before in code, before
    code = code.replace(before, after)
insertion = '''strong_dir = root/'benchmarks/runs/20260910-6919-strong-inprocessing-proofs'
strong = summary(strong_dir/'summary.json')
assert strong['checked'] == 216 and strong['LRAT_cake_lpr'] == 108 and strong['DRAT_drat_trim'] == 108
assert strong['negative_controls'] == 2 and strong['removed_bytes'] == 172844
assert not list(strong_dir.rglob('*.cnf')) and not list(strong_dir.rglob('*.proof'))
for p in (strong_dir/'logs').glob('*.json'): own(p)
factor_dir = root/'benchmarks/runs/20260910-6920-factor-cost-annotation'
factor = summary(factor_dir/'summary.json')
assert factor['temporary_expansions_deleted_bytes'] == 557169956
assert not list(factor_dir.glob('annotation-*'))
summary(factor_dir/'location-6921.json')
runtime = root/'benchmarks/runs/20260910-6912-watch-rebuild-runtime'
timing = summary(runtime/'solver-checkpoint-6914.json')
assert timing['solver_outcomes'] == 18 and timing['exact_per_arm_work_and_proof']
pricing = summary(runtime/'timer-cost-6915.json')
assert pricing['solved'] == dict(candidate=9,previous=9,SAT=0,UNSAT=18,timeouts=0,solve_losses=0)
assert pricing['ranks']['32']['paired']['wall_seconds']['max'] < 1
for rank in ('39','38'): assert pricing['ranks'][rank]['paired']['wall_seconds']['min'] > 1
for r in pricing['ranks'].values():
    assert r['paired']['peak_rss_kib']['min'] == r['paired']['peak_rss_kib']['max'] == 1
'''
assert code.count('accounting = {}') == 1
code = code.replace('accounting = {}', insertion+'\naccounting = {}')
start = code.index('    cleanup=dict(')
end = code.index("\nwith target.open('x')", start)
code = code[:start]+'''    cleanup=dict(temporary_section_output_bytes=0,fixed_proof_bytes=172844,profile_expansion_bytes=557169956,
        note='Only6919 verified regenerable fixed inputs/proofs and6920 exact temporary profile expansion removed. Retain logs, raw annotation, compressed original, pins, old22-byte invalid LRAT reproducer and all live checker scratch. No fuzz access or denied push/transfer retry.'),
    limitation='Reject6902 after complete same-work three-wave gate: all32 pairs improve, all39/38 regress, RSS exact. No global competitiveness claim, repeat-to-select or adoption. Strong6919 checks216 selected Sweep/Vivify proofs; not all historical LRAT or source fuzz acceptance.6730 remains current/unpromoted/unfuzzed; production unchanged and fixed50/global scheduling-CDCL/memory target unmet. Factor6921 localizes useful-work samples and first-touch return site for compile-only investigation; no representation approval or inferred precise native unit price. Proofs asynchronous. Rank21 shelved. Winddown05:45/cutoff06:00EDT September10. Report local unsent.')
''' + code[end:]
exec(compile(code, str(source), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})

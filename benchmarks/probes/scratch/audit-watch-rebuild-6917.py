#!/usr/bin/env python3
"""Freeze reconnect experiment, strict proof-oracle correction and live gates."""
from pathlib import Path
source = Path(__file__).with_name('audit-binary-watch-decision-6898.py')
code = source.read_text()
start = code.index("c = root/'benchmarks/runs/20260910-6893-watch-dispatch-build'")
end = code.index('accounting = {}',start)
replacement = '''bind(prior['classified_object_bindings'])
c = root/'benchmarks/runs/20260910-6902-watch-rebuild-build'
build = summary(c/'manifest.json')
for rel,digest in build['files'].items(): assert sha(root/build['source']/rel) == digest,rel
bind(build['binaries'])
for rel,digest in build['patches'].items():
    assert sha(c/rel) == digest
    own(c/rel)
for rel,digest in build['logs'].items(): assert sha(c/rel) == digest
own(c/'parent.json')
control = summary(c/'control-6904.json')
assert control['outputs']['previous'] == control['outputs']['candidate']
bind(control['logs']); bind(control['parent_objects']); bind(control['artifacts'])
scope = summary(c/'scope-6911.json')
assert len(scope['records']) == 9 and all(r['status']=='verified' for r in scope['records'][1:])
for p in (c/'logs').glob('*.json'): own(p)
failed = root/'benchmarks/runs/20260910-6906-watch-rebuild-proofs'
assert not (failed/'summary.json').exists()
for p in failed.iterdir(): own(p)
negative = json.loads((failed/'negative-1.json').read_text())
assert negative['returncode'] == 0 and 'c VERIFIED' in negative['stdout']
assert (failed/'negative-1.cnf').read_text() == 'p cnf 1 1\\n1 0\\n'
assert (failed/'negative-1.proof').read_text() == '2 0 1 0\\n'
proof_dir = root/'benchmarks/runs/20260910-6910-watch-rebuild-proofs'
proof = summary(proof_dir/'summary.json')
assert proof['checked'] == 128 and len(proof['negative_controls']) == 2
assert proof['removed_bytes'] == 7366
assert not list(proof_dir.glob('*.cnf')) and not list(proof_dir.glob('*.proof'))
for p in proof_dir.glob('*.json'): own(p)
annotation = summary(root/'benchmarks/runs/20260910-6900-watch-rebuild-annotation/summary.json')
assert annotation['temporary_expansions_deleted_bytes'] == 557169956
summary(root/'benchmarks/runs/20260910-6900-watch-rebuild-annotation/location-6901.json')
assert not list((root/'benchmarks/runs/20260910-6900-watch-rebuild-annotation').glob('annotation-*'))
bc = root/'benchmarks/runs/20260910-6907-watch-rebuild-BC-gate'
gate = summary(bc/'summary-6909.json')
assert gate['solver_records'] == 66 and len(gate['pairs']) == 33
assert all(p['reported_work_equal'] and p['certificate_equal'] for p in gate['pairs'])
'''
code = code[:start]+replacement+code[end:]
for before,after in {
    "target = root/'benchmarks/reports/2026-09-10-binary-watch-audit-6898.json'":
        "target = root/'benchmarks/reports/2026-09-10-watch-rebuild-audit-6917.json'",
    "prior_path = root/'benchmarks/reports/2026-09-10-subsolver-timing-audit-6891.json'":
        "prior_path = root/'benchmarks/reports/2026-09-10-binary-watch-audit-6898.json'",
    "owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-binary-watch-decision.md'}":
        "owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-watch-rebuild-oracle.md','tools/bench/proof_oracle.py','tools/bench/test_proof_oracle.py','tools/bench/test_proof_oracle_binaries.py'}",
    'range(6893,6900)':'range(6900,6919)',
    ".glob(f'*-{number}.py')": ".glob(f'*-{number}.*')",
    "for rel, previous_states in prior['campaign_states'].items():":
        "campaigns = dict(prior['campaign_states'])\ncampaigns['benchmarks/runs/20260910-6907-watch-rebuild-BC-gate'] = {}\nif (root/'benchmarks/runs/20260910-6912-watch-rebuild-runtime/manifest.json').exists():\n    campaigns['benchmarks/runs/20260910-6912-watch-rebuild-runtime'] = {}\nfor rel, previous_states in campaigns.items():",
    'assert sha(c/pin) == digest, pin':
        "assert sha(c/pin) == digest, pin\n        if (c/pin).suffix in ('.py','.patch','.tsv','.sbatch'): own(c/pin)",
    "    if isinstance(m.get('dependencies'),dict):":
        "    own(c/'matrix.tsv'); own(c/'array.sbatch')\n    if isinstance(m.get('dependencies'),dict):",
    'assert len(current) == count, (rel,len(current),count)':
        "required = 66 if c.name == '20260910-6907-watch-rebuild-BC-gate' else len(previous_states)\n    assert required <= len(current) <= count, (rel,len(current),required,count)",
    "assert '1679208' in accounting":"assert '1679208' in accounting and '1679218' in accounting",
    'report-to-andrew/20260910-0250-binary-watch-decision':'report-to-andrew/20260910-0312-watch-rebuild-oracle',
    "classified_object_bindings=object_bindings,inspection_classification=recovery['classification'],":
        "classified_object_bindings=prior['classified_object_bindings'],inspection_classification=prior['inspection_classification'],checker_classification=scope,",
    "cleanup=dict(temporary_section_output_bytes=863596,fixed_proof_bytes=0,profile_expansion_bytes=0,\n                 note='Only6895/6896 exact temporary sections/output objects removed. Retain rewritten containers, recovered originals, source, failures, pins, compressed profiles and all live scratch. No fuzz artifact access.'),":
        "cleanup=dict(temporary_section_output_bytes=0,fixed_proof_bytes=7366,failed_preflight_completed_negative_bytes=16,profile_expansion_bytes=557169956,\n                 note='6900 removed its exact temporary profile expansion;6910 removed verified fixed inputs/proofs;6906 removed completed DRAT negative before LRAT preflight failed. Retain22-byte SAT LRAT reproducer, all source/failed compile evidence, old classified objects, pins, raw/compressed profiles and live scratch. Incidental excluded metadata search classified6911; no use or further access.'),",
    '6869 rejected after all18 exact solver results:32/39 adverse allpairs,32 RSS adverse.6893 compile-only hotbody identical; no solver run/adoption.6730 remains unpromoted/unfuzzed; production unchanged; full50/global scheduling-CDCL/memory target unmet. Proofs async. Next existing-profile large-watch rebuild localization, not representation approval. No Rank21/FlatLists/promotion/denied push or transfer retry. Report local unsent. Winddown05:45/cutoff06:00EDT September10.':
        '6902 same-layout binary-only reconnect scan omission isolated/unpromoted/unfuzzed.144 exact-image controls/840 cursor/full113-113-112 suites,128 exact DRAT/cake_lpr proofs,2 negatives; B/C all33 exact pairs. Legacy C LRAT checker false acceptance classified6911; historical standalone LRAT checks not independent acceptance;8 retained6730 witnesses rechecked. New10 maintained proof-oracle tests. Threewave runtime6912 pending, no gain/adoption.6730 current; production unchanged; full50/global scheduling-CDCL/memory unmet. Proofs async. No Rank21/FlatLists/promotion/denied transfer or push retry. Report local unsent. Winddown05:45/cutoff06:00EDT September10.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

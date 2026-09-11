#!/usr/bin/env python3
"""Prepare all three unchanged timing waves after the complete BC solver gate."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-runtime-7186.py')
code = base.read_text()
for before, after in {
    '20260910-7186-factor-arena-runtime':'20260911-7257-occurrence-runtime',
    '20260910-7183-factor-arena-BC':'20260911-7256-occurrence-BC',
    'check-submit-factor-arena-BC-7183.py':'check-submit-occurrence-BC-7256.py',
    'submit-vivify-compact-wave-7150.py':'occurrence-fingerprint-7256.py',
    '7183-factor-arena-BC':'7256-occurrence-BC',
    '7186-factor-arena-runtime':'7257-occurrence-runtime',
    "    r = json.loads(p.read_text())\n    assert classify(r, final)":
        "    if not p.exists():\n        print('BC solver outcomes pending; no output created'); sys.exit(0)\n    r = json.loads(p.read_text())\n    assert classify(r, final)",
    'tactix-factor-arena-runtime-v1':'tactix-occurrence-runtime-v1',
    'Frozen7150 exact fingerprint fixtures plus new7186 matrix/identity controls; underlying arena unit/proof/sanitizer gates bound through7183. No changes to solver or copied runtime runner.':
        '7256 strict dense-physical-counter schema and own-repeat distinction; matrix identity controls.7253 full suites117/117/116,52 cross-rule cases364 exact parent boundaries,840 exact replacements;7254 sixteen sanitizers;7255 fixed156proofs/156models with two rejecting controls. Source and copied runner unchanged.',
    'Every outcome expected, every non-timing statistic and certificate bytes/hash exact retained7144 and own repeats.':
        'Every outcome expected; certificate bytes/hash and all non-timing work exact retained7144 except two predeclared physical dense move/defrag counters. Those counters must still repeat exactly within each arm. No additional exclusion.',
    'Measure whole runtime and memory including arena holes. Complete Factor profiles are also required for structural unit-cost attribution. Normal telemetry retained. No production integration, default promotion or fuzz waiver.':
        'Measure complete Eliminate and whole runtime/RSS for single-image occurrences. Normal telemetry retained; mixed filtered traversal risk explicit. No production integration, default promotion or fuzz waiver. No checker wait for provisional continuation.',
    'bind(gate / \'manifest.json\'); bind(gate / \'launch.json\'); bind(reader); bind(Path(__file__))':
        "bind(gate / 'manifest.json'); bind(gate / 'launch.json'); bind(reader); bind(Path(__file__))\nbind(Path(__file__).with_name('check-submit-occurrence-runtime-7257.py'))",
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})

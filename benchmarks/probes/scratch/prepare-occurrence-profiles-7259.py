#!/usr/bin/env python3
"""Predeclare whole-Eliminate profiles from reviewed7253/7162 machine calls."""
from pathlib import Path
base = Path(__file__).with_name('prepare-vivify-compact-profiles-7152.py')
code = base.read_text()
for before, after in {
    '20260910-7152-vivify-compact-profiles':'20260911-7259-occurrence-profiles',
    '20260910-7150-vivify-compact-runtime':'20260911-7257-occurrence-runtime',
    '20260910-7149-vivify-compact-BC-gate':'20260911-7256-occurrence-BC',
    'submit-vivify-compact-wave-7150.py':'occurrence-fingerprint-7256.py',
    "m = json.loads((runtime / 'manifest.json').read_text())":
        "m = json.loads((runtime / 'manifest.json').read_text())\nm['protected_campaign_hashes'] = {}\nm['authorities'] = {}\nfor arm,date,stem in (('candidate','20260911','7253-occurrence-cutover'),('previous','20260910','7162-vivify-revalidation')):\n    p = root / ('benchmarks/runs/' + date + '-' + stem + '-build/manifest.json')\n    m['authorities'][arm] = dict(path=str(p.relative_to(root)),sha256=sha(p))",
    "'candidate': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}":
        "'candidate': {0x5082d3: ('initial_probe', 'xor %edx,%edx'), 0x508b83: ('probe', 'xor %edx,%edx'), 0x509722: ('eliminate', 'mov $0x1,%edx')}",
    "'previous': {0x507793: ('initial_probe', 'xor %edx,%edx'), 0x508043: ('probe', 'xor %edx,%edx'), 0x508be2: ('eliminate', 'mov $0x1,%edx')}":
        "'previous': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}",
    ".replace('7150-vivify-compact-runtime', '7152-vivify-compact')":
        ".replace('7257-occurrence-runtime', '7259-occurrence-profile')",
    'tactix-vivify-whole-lifecycle-three-wave-profiles-v1':'tactix-occurrence-whole-eliminate-three-wave-profiles-v1',
    'Assess at least15% complete-Vivify Rank35 sampled-cycle reduction on7144 versus7030 in three independent matched pairs; include scheduling, root preparation, watch rebuild and probing descendants of vivify_apply, not only the printed timer or sort symbol; no Kissat rerun.':
        'Measure whole Eliminate Rank35 sampled cycles for7253 versus retained7144 in three independent pairs. Preserve Probe, initial Probe and no-phase-frame complements, all rebuilds and rule descendants. Nested rebuild cost is explanatory, not an additive saving. No arbitrary minimum gain or Kissat rerun.',
    'All6 exact own unprofiled work/proof fingerprints and repeated own results.':
        'All6 exact own unprofiled work/proof fingerprints including physical dense counters, with exact own repeated results. Only cross-arm comparisons exclude the two predeclared7256 physical dense move/defrag counters in addition to timing.',
    'Unprofiled7150 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.':
        'Unprofiled7257 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.',
    'use the already tested7150 runtime protocol.15 callstack tests include7 new aggregation tests; require exact replay of frozen7082 current attribution before new interpretation.':
        'use unchanged7257 runtime,7256 strict counter/identity controls and exact7131 reader replay. Fresh7253/7162 calls require source-reviewed argument setup. Existing conserving callstack tests protect phase attribution; no phase is inferred from frequency or PC ordering.',
}.items():
    assert code.count(before) == 1,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

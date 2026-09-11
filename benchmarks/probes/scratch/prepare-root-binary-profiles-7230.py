#!/usr/bin/env python3
"""Predeclare complete Reduce profiles with fresh7218/7162 phase call sites."""
from pathlib import Path
base = Path(__file__).with_name('prepare-vivify-compact-profiles-7152.py')
code = base.read_text()
for before,after in {
    '20260910-7152-vivify-compact-profiles':'20260911-7230-root-binary-profiles',
    '20260910-7150-vivify-compact-runtime':'20260911-7226-root-binary-runtime',
    '20260910-7149-vivify-compact-BC-gate':'20260911-7221-root-binary-BC',
    "m = json.loads((runtime / 'manifest.json').read_text())":
        "m = json.loads((runtime / 'manifest.json').read_text())\nm['protected_campaign_hashes'] = {}\nm['authorities'] = {}\nfor arm,date,stem in (('candidate','20260911','7218-root-binary'),('previous','20260910','7162-vivify-revalidation')):\n    p = root / ('benchmarks/runs/' + date + '-' + stem + '-build/manifest.json')\n    m['authorities'][arm] = dict(path=str(p.relative_to(root)),sha256=sha(p))",
    "'candidate': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}":
        "'candidate': {0x5078b3: ('initial_probe', 'xor %edx,%edx'), 0x508163: ('probe', 'xor %edx,%edx'), 0x508d02: ('eliminate', 'mov $0x1,%edx')}",
    "'previous': {0x507793: ('initial_probe', 'xor %edx,%edx'), 0x508043: ('probe', 'xor %edx,%edx'), 0x508be2: ('eliminate', 'mov $0x1,%edx')}":
        "'previous': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}",
    ".replace('7150-vivify-compact-runtime', '7152-vivify-compact')":
        ".replace('7226-root-binary-runtime', '7230-root-binary-profile')",
    'tactix-vivify-whole-lifecycle-three-wave-profiles-v1':'tactix-root-binary-whole-lifecycle-three-wave-profiles-v1',
    'Assess at least15% complete-Vivify Rank35 sampled-cycle reduction on7144 versus7030 in three independent matched pairs; include scheduling, root preparation, watch rebuild and probing descendants of vivify_apply, not only the printed timer or sort symbol; no Kissat rerun.':
        'Measure complete Reduce Rank35 sampled-cycle change for7218 versus retained7144 in three independent matched pairs. Include sparse collection and all descendants; separately conserve nested root collection and kernel GC, preserving no-phase-frame samples because Reduce is outside Probe/Eliminate. No arbitrary minimum gain or Kissat rerun.',
    'Unprofiled7150 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.':
        'Unprofiled7226 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.',
    'use the already tested7150 runtime protocol.15 callstack tests include7 new aggregation tests; require exact replay of frozen7082 current attribution before new interpretation.':
        'use unchanged7226 runtime and frozen exact-fingerprint fixtures. Existing callstack/envelope tests and exact7131 replay protect attribution. Fresh7218/7162 machine calls must match reviewed group-argument setup before publication; Reduce outside phase ancestry is retained explicitly.',
}.items():
    assert code.count(before) == 1,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

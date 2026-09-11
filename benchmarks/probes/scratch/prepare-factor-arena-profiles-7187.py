#!/usr/bin/env python3
"""Reuse the frozen profile contract with newly reviewed arena call sites."""
from pathlib import Path

base = Path(__file__).with_name('prepare-vivify-compact-profiles-7152.py')
code = base.read_text()
changes = {
    '20260910-7152-vivify-compact-profiles': '20260910-7187-factor-arena-profiles',
    '20260910-7150-vivify-compact-runtime': '20260910-7186-factor-arena-runtime',
    '20260910-7149-vivify-compact-BC-gate': '20260910-7183-factor-arena-BC',
    "m = json.loads((runtime / 'manifest.json').read_text())":
        "m = json.loads((runtime / 'manifest.json').read_text())\nm['protected_campaign_hashes'] = {}\nm['authorities'] = {}\nfor arm, stem in (('candidate', '7180-factor-arena'), ('previous', '7162-vivify-revalidation')):\n    p = root / ('benchmarks/runs/20260910-' + stem + '-build/manifest.json')\n    m['authorities'][arm] = dict(path=str(p.relative_to(root)), sha256=sha(p))",
    "'candidate': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}":
        "'candidate': {0x507a13: ('initial_probe', 'xor %edx,%edx'), 0x5082c3: ('probe', 'xor %edx,%edx'), 0x508e62: ('eliminate', 'mov $0x1,%edx')}",
    "'previous': {0x507793: ('initial_probe', 'xor %edx,%edx'), 0x508043: ('probe', 'xor %edx,%edx'), 0x508be2: ('eliminate', 'mov $0x1,%edx')}":
        "'previous': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}",
    ".replace('7150-vivify-compact-runtime', '7152-vivify-compact')":
        ".replace('7186-factor-arena-runtime', '7187-factor-arena-profile')",
    'tactix-vivify-whole-lifecycle-three-wave-profiles-v1': 'tactix-factor-arena-whole-lifecycle-three-wave-profiles-v1',
    'Assess at least15% complete-Vivify Rank35 sampled-cycle reduction on7144 versus7030 in three independent matched pairs; include scheduling, root preparation, watch rebuild and probing descendants of vivify_apply, not only the printed timer or sort symbol; no Kissat rerun.':
        'Measure complete Factor Rank35 sampled-cycle change on7180 versus retained7144 in three independent matched pairs; include index construction, allocation/destruction, remainder lookup, grid application and sparse-watch rebuild descendants of factor_apply, not only a printed timer or entry-self symbol. No invented minimum percentage and no Kissat rerun.',
    'Unprofiled7150 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.':
        'Unprofiled7186 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.',
    'use the already tested7150 runtime protocol.15 callstack tests include7 new aggregation tests; require exact replay of frozen7082 current attribution before new interpretation.':
        'use the unchanged7186 runtime protocol and frozen7150 fingerprint fixtures. Existing callstack tests and exact7131 replay protect attribution; newly reviewed7180/7162 phase calls must match disassembly before publication.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

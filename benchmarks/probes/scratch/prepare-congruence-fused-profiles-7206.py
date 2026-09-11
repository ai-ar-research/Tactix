#!/usr/bin/env python3
"""Whole-Congruence profiles with reviewed7196/7162 machine call sites."""
from pathlib import Path
base = Path(__file__).with_name('prepare-vivify-compact-profiles-7152.py')
code = base.read_text()
changes = {
    '20260910-7152-vivify-compact-profiles': '20260911-7206-congruence-fused-profiles',
    '20260910-7150-vivify-compact-runtime': '20260911-7205-congruence-fused-runtime',
    '20260910-7149-vivify-compact-BC-gate': '20260911-7202-congruence-fused-BC',
    "m = json.loads((runtime / 'manifest.json').read_text())":
        "m = json.loads((runtime / 'manifest.json').read_text())\nm['protected_campaign_hashes'] = {}\nm['authorities'] = {}\nfor arm, date, stem in (('candidate', '20260911', '7196-congruence-fused'), ('previous', '20260910', '7162-vivify-revalidation')):\n    p = root / ('benchmarks/runs/' + date + '-' + stem + '-build/manifest.json')\n    m['authorities'][arm] = dict(path=str(p.relative_to(root)), sha256=sha(p))",
    "'candidate': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}":
        "'candidate': {0x507a53: ('initial_probe', 'xor %edx,%edx'), 0x508303: ('probe', 'xor %edx,%edx'), 0x508ea2: ('eliminate', 'mov $0x1,%edx')}",
    "'previous': {0x507793: ('initial_probe', 'xor %edx,%edx'), 0x508043: ('probe', 'xor %edx,%edx'), 0x508be2: ('eliminate', 'mov $0x1,%edx')}":
        "'previous': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}",
    ".replace('7150-vivify-compact-runtime', '7152-vivify-compact')":
        ".replace('7205-congruence-fused-runtime', '7206-congruence-fused-profile')",
    'tactix-vivify-whole-lifecycle-three-wave-profiles-v1': 'tactix-congruence-fused-whole-lifecycle-three-wave-profiles-v1',
    'Assess at least15% complete-Vivify Rank35 sampled-cycle reduction on7144 versus7030 in three independent matched pairs; include scheduling, root preparation, watch rebuild and probing descendants of vivify_apply, not only the printed timer or sort symbol; no Kissat rerun.':
        'Measure complete Congruence Rank35 sampled-cycle change on fused7196 versus retained7144 in three independent matched pairs. Include extraction/indexing, binary liveness, canonicalization, closure, forward cleanup, transitions and descendants, not only AND self or the printed timer. No arbitrary minimum gain and no Kissat rerun.',
    'Unprofiled7150 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.':
        'Unprofiled7205 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.',
    'use the already tested7150 runtime protocol.15 callstack tests include7 new aggregation tests; require exact replay of frozen7082 current attribution before new interpretation.':
        'use unchanged7205 runtime and frozen exact-fingerprint fixtures. Existing callstack tests and exact7131 replay protect attribution; reviewed7196/7162 calls must match disassembly before publication.',
}
for before,after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

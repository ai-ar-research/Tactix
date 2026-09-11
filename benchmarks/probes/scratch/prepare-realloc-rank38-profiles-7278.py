#!/usr/bin/env python3
"""One matched diagnostic pair to locate the unexpected Rank38 unit-cost gap."""
from pathlib import Path
base=Path(__file__).with_name('prepare-vivify-compact-profiles-7152.py')
code=base.read_text()
changes={
    '20260910-7152-vivify-compact-profiles':'20260911-7278-realloc-rank38-profiles',
    '20260910-7150-vivify-compact-runtime':'20260911-7273-dense-realloc-runtime',
    '20260910-7149-vivify-compact-BC-gate':'20260911-7272-dense-realloc-BC',
    'submit-vivify-compact-wave-7150.py':'occurrence-fingerprint-7256.py',
    'h.fingerprint(':'h.raw_fingerprint(',
    "m = json.loads((runtime / 'manifest.json').read_text())":
        "m = json.loads((runtime / 'manifest.json').read_text())\nm['protected_campaign_hashes']={}\nm['authorities']={}\nfor arm,stem in (('candidate','7271-dense-realloc'),('previous','7253-occurrence-cutover')):\n    p=root/f'benchmarks/runs/20260911-{stem}-build/manifest.json'\n    m['authorities'][arm]=dict(path=str(p.relative_to(root)),sha256=sha(p))",
    "((2, 'candidate'), (3, 'previous'))":"((8, 'candidate'), (9, 'previous'))",
    "r['rank'] == 35":"r['rank'] == 38",
    "['35']":"['38']",
    "oracles['35/'":"oracles['38/'",
    'dict(rank=35,':'dict(rank=38,',
    'Rank35':'Rank38',
    "'candidate': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')}":
        "'candidate': {0x508003: ('initial_probe', 'xor %edx,%edx'), 0x5088b3: ('probe', 'xor %edx,%edx'), 0x509452: ('eliminate', 'mov $0x1,%edx')}",
    "'previous': {0x507793: ('initial_probe', 'xor %edx,%edx'), 0x508043: ('probe', 'xor %edx,%edx'), 0x508be2: ('eliminate', 'mov $0x1,%edx')}":
        "'previous': {0x5082d3: ('initial_probe', 'xor %edx,%edx'), 0x508b83: ('probe', 'xor %edx,%edx'), 0x509722: ('eliminate', 'mov $0x1,%edx')}",
    'for wave in range(3):':'for wave in range(1):',
    "replace('--array=0-17', '--array=0-5')":"replace('--array=0-29', '--array=0-1')",
    "replace('7150-vivify-compact-runtime', '7152-vivify-compact')":"replace('7273-dense-realloc-runtime', '7278-realloc-rank38-profiles')",
    "array='0-5'":"array='0-1'",
    'tactix-vivify-whole-lifecycle-three-wave-profiles-v1':'tactix-realloc-rank38-diagnostic-profiles-v1',
    'Assess at least15% complete-Vivify Rank38 sampled-cycle reduction on7144 versus7030 in three independent matched pairs; include scheduling, root preparation, watch rebuild and probing descendants of vivify_apply, not only the printed timer or sort symbol; no Kissat rerun.':
        'Locate the Rank38 allocator-candidate slowdown: wave0 loses19.233s wholly outside inprocessing with exact work/certificates. Ordinary propagation generated code changes despite unchanged source (7276). Measure all phases and complete propagation ancestry/self PCs before attributing cause. Diagnostic pair, no acceptance claim or alignment tuning.',
    'Three sequential full-width2-task waves0-1,2-3,4-5; candidate/control concurrent, middle wave reversed, no throttle/node constraint. Proceed after exact expected solver outcomes while proofs continue.':
        'One fullwidth diagnostic2-task pair0-1; candidate/control concurrent, no throttle/node constraint. No additional profile wave predeclared. The separate three7273 runtime waves own reproducibility and acceptance. Do not wait for proofs to diagnose.',
    'All6 exact own unprofiled work/proof fingerprints and repeated own results.':
        'Both exact own unprofiled work/proof fingerprints INCLUDING physical dense counters.',
    'Unprofiled7150 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.':
        'Unprofiled7273 owns repeated total-runtime/RSS acceptance; sampling overhead is not solver memory.',
    'use the already tested7150 runtime protocol.15 callstack tests include7 new aggregation tests; require exact replay of frozen7082 current attribution before new interpretation.':
        'use unchanged7273 runtime and strict7256 raw fingerprint. Current7276 codegen boundaries tested. Three phase call sites reviewed against source and exact group-argument assembly; existing conserving callstack tests and exact7131 replay protect attribution.',
    'bind(replay); bind(reader); bind(Path(__file__)); bind(root / \'tools/bench/perf_callstack.py\')':
        "bind(replay); bind(reader); bind(Path(__file__)); bind(root / 'tools/bench/perf_callstack.py')\nbind(root/'benchmarks/runs/20260911-7276-realloc-propagation-code/summary.json')\nbind(Path(__file__).with_name('validate-realloc-rank38-profiles-7278.py'))",
}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

#!/usr/bin/env python3
"""Checkpoint independent arena transfer and fused AND gates, including failures."""
from pathlib import Path
base = Path(__file__).with_name('audit-runtime-telemetry-7177.py')
code = base.read_text()
extra = '''
names = ('localize-factor-arena-wave-7191', 'assess-factor-arena-7192',
    'localize-congruence-envelope-7193', 'localize-congruence-children-7194',
    'localize-congruence-and-pcs-7195', 'build-congruence-fused-7196',
    'check-congruence-fused-sanitizers-7197', 'check-congruence-fused-proofs-7198',
    'check-congruence-sanitizer-control-7199', 'check-congruence-sanitizer-control-7200',
    'prepare-factor-arena-full50-7201', 'submit-factor-arena-full50-7201',
    'validate-factor-arena-full50-7201', 'prepare-congruence-fused-BC-7202',
    'check-submit-congruence-fused-BC-7202', 'audit-inprocessing-unit-cost-7203')
for name in names: bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)
for name in ('test-congruence-fused-proof-7198', 'test-congruence-index-read-7199',
             'test-string-input-constructor-7199'):
    bind(root / f'benchmarks/probes/scratch/{name}.cpp', own=True)
summaries = [root / ('benchmarks/reports/' + name) for name in (
    '2026-09-11-factor-arena-assessment-7192.json', '2026-09-11-congruence-envelope-7193.json',
    '2026-09-11-congruence-children-7194.json')]
summaries += [root / 'benchmarks/runs/20260911-7195-congruence-and-pc-localization/summary.json']
for wave in range(3):
    summaries += [root / f'benchmarks/runs/20260910-7188-factor-arena-samples/wave{wave}/{name}'
                  for name in ('summary.json', 'factor-child-partition-7191.json')]
for p in summaries:
    bind(p, own=True)
    for rel, digest in json.loads(p.read_text())['hashes'].items(): bind(root / rel, digest)
build = root / 'benchmarks/runs/20260911-7196-congruence-fused-build'
b = json.loads((build / 'manifest.json').read_text())
for rel, digest in b['files'].items(): bind(root / b['source'] / rel, digest)
for rel, digest in b['binaries'].items(): bind(root / rel, digest)
for p in build.rglob('*'):
    if p.is_file(): bind(p, own=True)
for stem in ('7198-congruence-fused-proofs', '7200-congruence-sanitizer-control'):
    campaign = root / ('benchmarks/runs/20260911-' + stem)
    result = json.loads((campaign / 'summary.json').read_text()); assert result['status'] == 'PASS'
    for rel, digest in result['hashes'].items(): bind(root / rel, digest)
    for p in campaign.rglob('*'):
        if p.is_file() and p.suffix in ('.json', '.log'): bind(p, own=True)
for stem in ('7197-congruence-fused-sanitizers', '7199-congruence-sanitizer-control'):
    campaign = root / ('benchmarks/runs/20260911-' + stem)
    assert not (campaign / 'summary.json').exists(), 'original failures must remain failures'
    for p in campaign.rglob('*'):
        if p.is_file() and p.suffix in ('.json', '.log'): bind(p, own=True)
bind(root / 'benchmarks/reports/2026-09-11-inprocessing-unit-cost-7203.md', own=True)
assert not list(Path('/tmp').glob('tactix-congruence-index-*'))
for folder in ('20260911-7198-congruence-fused-proofs', '20260911-7200-congruence-sanitizer-control'):
    campaign = root / 'benchmarks/runs' / folder
    assert not list(campaign.glob('*.cnf')) and not list(campaign.glob('*.proof'))
'''
changes = {
    '2026-09-10-runtime-telemetry-checkpoint-7177.json': '2026-09-11-inprocessing-unit-cost-checkpoint-7203.json',
    '2026-09-10-vivify-revalidation-audit-7169.json': '2026-09-10-factor-arena-measurements-audit-7190.json',
    "('20260910-7175-runtime-telemetry-runtime', 18, 6)]":
        "('20260910-7175-runtime-telemetry-runtime', 18, 6), ('20260910-7183-factor-arena-BC', 66, None), ('20260910-7186-factor-arena-runtime', 18, 6), ('20260910-7187-factor-arena-profiles', 6, 2), ('20260911-7201-factor-arena-ranks1-50', 50, None), ('20260911-7202-congruence-fused-BC', 66, None)]",
    'report-to-andrew/20260910-2257-runtime-evaluation': 'report-to-andrew/20260911-0030-inprocessing-unit-cost',
    "commands = [[sys.executable, '-m', 'unittest'": extra + "\ncommands = [[sys.executable, '-m', 'unittest'",
    'checks = {}': """commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py'] for name in ('check-submit-factor-arena-BC-7183', 'check-submit-factor-arena-runtime-7186', 'validate-factor-arena-profiles-7187', 'validate-factor-arena-full50-7201', 'check-submit-congruence-fused-BC-7202')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py', '--self-test'] for name in ('assess-factor-arena-7192', 'localize-factor-arena-wave-7191')]
checks = {}""",
    'newly_deleted_bytes=0': 'newly_deleted_bytes=3587242928 + 264254 + 48',
    'first targeted wave launched without proof wait':
        'all18 telemetry outcomes exact and no joint-removal gain31/35. Arena7180 complete Factor median0.872448474 with small31/35 runtime gains and38RSS+6220KiB; full50 transfer7201 launched. Independent AND fusion7196 passes full suites/exact proofs and classified sanitizer fixture repair7200; BC7202 launched. Failed7197/7199 preserved',
    'Binary-scan/clock ablation also displaces downstream code; same-size search propagation moves336 bytes. Joint runtime comparison is not a pure price of counter instructions. All final proof/cleanup, memory, current-source fuzz and consolidation gates remain separate.':
        'AND7196 performance unmeasured; arena full50 transfer pending. Original7197 sanitizer fixture fails in GCC13.1 string constructor; standalone/parent reproductions and byte-equivalent reader repair7200 complete all four fixtures twice, without sanitizer suppression.7199 LRAT cleanup-format assertion failure also retained. No production or fuzz acceptance; memory/final certificates/consolidation remain open.',
    'Validate7175 wave0 with strict telemetry contract; advance waves1/2 only after exact expected own/cross-arm solver outcomes, never wait for proofs. Preserve three paired ranges/RSS and compare against larger Factor lead7171. Successful writers and submitted pins remain immutable.':
        'Read-only validate7201 full50 and7202 BC. After all66 exact BC solver outcomes, prepare three paired31/35/38 AND runtime waves and complete Congruence profiles with current-binary call-site review. Judge full runtime/RSS; do not wait for proof completion. Keep arena/AND independent, fixed native and established closures unchanged. Carry repaired fixture into consolidation. Successful writers/pins immutable.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

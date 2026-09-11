#!/usr/bin/env python3
"""Checkpoint the isolated arena runtime/profile campaign without promotion."""
from pathlib import Path

base = Path(__file__).with_name('audit-runtime-telemetry-7177.py')
code = base.read_text()
extra = '''
for name in ('prepare-factor-arena-runtime-7186', 'check-submit-factor-arena-runtime-7186',
             'prepare-factor-arena-profiles-7187', 'validate-factor-arena-profiles-7187',
             'submit-factor-arena-profile-wave-7187', 'extract-factor-arena-wave-7188',
             'census-current-inprocessing-7189', 'audit-factor-arena-measurements-7190'):
    bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)
for rel in ('benchmarks/reports/2026-09-10-current-inprocessing-census-7189.json',
            'benchmarks/reports/2026-09-10-factor-arena-first-wave-7190.md'):
    bind(root / rel, own=True)
'''
changes = {
    "target = root / 'benchmarks/reports/2026-09-10-runtime-telemetry-checkpoint-7177.json'":
        "target = root / 'benchmarks/reports/2026-09-10-factor-arena-measurements-audit-7190.json'",
    "old_path = root / 'benchmarks/reports/2026-09-10-vivify-revalidation-audit-7169.json'":
        "old_path = root / 'benchmarks/reports/2026-09-10-runtime-bookkeeping-audit-7185.json'",
    "('20260910-7175-runtime-telemetry-runtime', 18, 6)]":
        "('20260910-7175-runtime-telemetry-runtime', 18, 6), ('20260910-7183-factor-arena-BC', 66, None), ('20260910-7186-factor-arena-runtime', 18, 6), ('20260910-7187-factor-arena-profiles', 6, 2)]",
    "assert name == '20260910-7175-runtime-telemetry-runtime' and wave > 0":
        "assert name in ('20260910-7175-runtime-telemetry-runtime', '20260910-7186-factor-arena-runtime', '20260910-7187-factor-arena-profiles') and wave > 0",
    'report-to-andrew/20260910-2257-runtime-evaluation':
        'report-to-andrew/20260910-2353-factor-arena-measurements',
    "commands = [[sys.executable, '-m', 'unittest'":
        extra + "\ncommands = [[sys.executable, '-m', 'unittest'",
    'checks = {}':
        "commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py'] for name in ('check-submit-factor-arena-BC-7183', 'check-submit-factor-arena-runtime-7186', 'validate-factor-arena-profiles-7187')]\ncommands += [[sys.executable, 'benchmarks/probes/scratch/census-current-inprocessing-7189.py', '--self-test']]\nchecks = {}",
    'first targeted wave launched without proof wait':
        'all18 telemetry outcomes exact, no net removal win on31/35. Isolated7180 arena has all66 exact BC solver outcomes. Runtime7186 first wave complete/exact; wave1 proceeds without proof wait, complete Factor profiles7187 wave0 active. First pair is not a performance decision',
    'Validate7175 wave0 with strict telemetry contract; advance waves1/2 only after exact expected own/cross-arm solver outcomes, never wait for proofs. Preserve three paired ranges/RSS and compare against larger Factor lead7171. Successful writers and submitted pins remain immutable.':
        'Validate7186 runtime and7187 profiles read-only; advance each of the three predeclared waves only after exact preceding solver outcomes, never wait for proofs. Extract7188 each published profile pair once; assess full Factor cost, whole runtime and RSS across all waves.7189 census keeps larger Vivify/Sweep/Congruence and Rank31/43 search-work gaps in priority. No default/promotion/fuzz waiver. Frozen writers/pins remain immutable.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

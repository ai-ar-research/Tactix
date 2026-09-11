#!/usr/bin/env python3
"""Freeze the completed telemetry experiment and isolated arena checkpoint."""
from pathlib import Path

base = Path(__file__).with_name('audit-runtime-telemetry-7177.py')
code = base.read_text()
extra = '''
for name in ('build-factor-arena-7180', 'check-factor-arena-proofs-7181',
             'check-factor-arena-sanitizers-7182', 'prepare-factor-arena-BC-7183',
             'check-submit-factor-arena-BC-7183', 'assess-runtime-telemetry-7184',
             'audit-runtime-bookkeeping-7185'):
    bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)
for rel in ('benchmarks/reports/2026-09-10-runtime-telemetry-assessment-7184.json',
            'benchmarks/reports/2026-09-10-runtime-bookkeeping-conclusion-7185.md'):
    bind(root / rel, own=True)
arena = root / 'benchmarks/runs/20260910-7180-factor-arena-build'
am = json.loads((arena / 'manifest.json').read_text())
for rel, digest in am['files'].items(): bind(root / am['source'] / rel, digest)
for rel, digest in am['binaries'].items(): bind(root / rel, digest)
assert am['binaries']['build/pin-7180-factor-arena-release/tactix'] == am['binaries']['build/pin-7180-factor-arena-bc/tactix']
for dirname in ('20260910-7180-factor-arena-build', '20260910-7181-factor-arena-proofs',
                '20260910-7182-factor-arena-sanitizers'):
    directory = root / 'benchmarks/runs' / dirname
    for p in directory.rglob('*'):
        if p.is_file() and p.suffix in ('.json', '.log', '.patch'): bind(p, own=True)
for dirname in ('20260910-7181-factor-arena-proofs', '20260910-7182-factor-arena-sanitizers'):
    sm = json.loads((root / 'benchmarks/runs' / dirname / 'summary.json').read_text())
    assert sm['status'] == 'PASS'
    for rel, digest in sm['hashes'].items(): bind(root / rel, digest)
proofs = root / 'benchmarks/runs/20260910-7181-factor-arena-proofs'
pm = json.loads((proofs / 'summary.json').read_text())
assert pm['checked'] == 288 and pm['negative_controls'] == 2
assert pm['exact_parent_certificates'] and pm['removed_bytes'] == 570144
assert not list(proofs.rglob('*.cnf')) and not list(proofs.rglob('*.proof'))
assessment = json.loads((root / 'benchmarks/reports/2026-09-10-runtime-telemetry-assessment-7184.json').read_text())
assert assessment['outcomes'] == 18 and assessment['exact_work_and_proofs']
for rel, digest in assessment['hashes'].items(): bind(root / rel, digest)
'''
changes = {
    "target = root / 'benchmarks/reports/2026-09-10-runtime-telemetry-checkpoint-7177.json'":
        "target = root / 'benchmarks/reports/2026-09-10-runtime-bookkeeping-audit-7185.json'",
    "old_path = root / 'benchmarks/reports/2026-09-10-vivify-revalidation-audit-7169.json'":
        "old_path = root / 'benchmarks/reports/2026-09-10-telemetry-followup-audit-7179.json'",
    "('20260910-7175-runtime-telemetry-runtime', 18, 6)]":
        "('20260910-7175-runtime-telemetry-runtime', 18, 6), ('20260910-7183-factor-arena-BC', 66, None)]",
    "report-to-andrew/20260910-2257-runtime-evaluation":
        "report-to-andrew/20260910-2338-bookkeeping-conclusion",
    "commands = [[sys.executable, '-m', 'unittest'":
        extra + "\ncommands = [[sys.executable, '-m', 'unittest'",
    "checks = {}":
        "commands += [[sys.executable, 'benchmarks/probes/scratch/check-submit-factor-arena-BC-7183.py'], [sys.executable, 'benchmarks/probes/scratch/assess-runtime-telemetry-7184.py', '--self-test'], [sys.executable, 'benchmarks/probes/scratch/localize-factor-children-7178.py', '--self-test']]\nchecks = {}",
    "newly_deleted_bytes=0": "newly_deleted_bytes=570144",
    "first targeted wave launched without proof wait":
        "all three targeted waves have exact outcomes; medians1.009553/1.014068/0.998967 on31/35/38 do not support enabling ablation by default. Isolated7180 arena has full116/116/115 suites,288 proofs and twice-run four sanitizer fixtures;7183 all66 expected/exact solver outcomes",
    "Validate7175 wave0 with strict telemetry contract; advance waves1/2 only after exact expected own/cross-arm solver outcomes, never wait for proofs. Preserve three paired ranges/RSS and compare against larger Factor lead7171. Successful writers and submitted pins remain immutable.":
        "Measure isolated7180 arena against retained7144 on three paired31/35/38 runtime waves, then complete Factor profiles with newly reviewed call sites. BC solver gate is complete; no wait for remaining proofs. No arena runtime/profile submission yet. Preserve RSS, exact logical work, proof and pending-fuzz gates. Successful writers/submitted pins remain immutable.",
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

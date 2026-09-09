#!/usr/bin/env python3
"""Scoped successor audit, reusing immutable campaign validation contracts."""
from pathlib import Path

source = Path(__file__).with_name('audit-early-congruence-6542.py')
code = source.read_text()
for old, new in {
    '2026-09-09-early-congruence-audit-6542.json': '2026-09-09-factor-material-audit-6548.json',
    '2026-09-09-retention-attribution-audit-6533.json': '2026-09-09-early-congruence-audit-6542.json',
    '20260909-1240-early-congruence-timing': '20260909-1255-factor-material-contract',
    "local[str((report / 'report-overflow.log').relative_to(root))] = sha(report / 'report-overflow.log')": '',
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
begin, end = code.index('owned = {'), code.index('\nlocal, campaigns =')
code = code[:begin] + '''owned = {
    'PROGRESS.md', 'benchmarks/reports/2026-09-09-factor-cost-and-material-contract.md',
    'benchmarks/probes/scratch/annotate-factor-6544.py',
    'benchmarks/probes/scratch/summarize-factor-6545.py',
    'benchmarks/probes/scratch/test-native-material-6546.c',
    'benchmarks/probes/scratch/check-native-material-6547.py',
    'benchmarks/probes/scratch/audit-factor-material-6548.py',
    'benchmarks/probes/scratch/land-factor-material-6549.py',
}''' + code[end:]
anchor = 'checks = {}'
assert code.count(anchor) == 1
extra = '''annotation = root / 'benchmarks/runs/20260909-6544-rank32-factor-annotation'
fm = read(annotation / 'manifest.json', True)
fs = read(annotation / 'summary-6545.json', True)
for rel, digest in fm['authorities'].items(): assert sha(root / fm['parent'] / rel) == digest, rel
for rel, digest in fm['source_factor_exact_current'].items():
    for tree in ('runtime-compact-stamp-6357', 'runtime-activation-stamp-6424'):
        assert sha(root / '.claude/worktrees' / tree / rel) == digest, (tree, rel)
for rel, digest in fs['hashes'].items():
    assert sha(root / rel) == digest, rel
    local[rel] = digest
assert fm['temporary_expansions_deleted_bytes'] == 550070760
assert not any(p.is_dir() for p in annotation.iterdir())
control = root / 'benchmarks/runs/20260909-6547-native-material-control'
control_summary = read(control / 'summary.json', True)
for rel, digest in control_summary['hashes'].items(): assert sha(root / rel) == digest, rel
for name, record in control_summary['checks'].items():
    binary = control / name
    assert sha(binary) == record['binary_sha256']
    local[str(binary.relative_to(root))] = sha(binary)
    run = subprocess.run([str(binary)], cwd=root, text=True, capture_output=True, check=True)
    assert run.stdout == record['stdout'] and not run.stderr
    owned.add(str((control / (name + '-build.log')).relative_to(root)))
for rel in ('tools/solvers/variants/kissat-macro-heap-6090/src/forward.c',
            'tools/solvers/variants/kissat-macro-heap-6090/src/eliminate.c',
            '.claude/worktrees/runtime-activation-stamp-6424/src/rules/strengthen/strengthen.cpp',
            '.claude/worktrees/runtime-activation-stamp-6424/src/kernel/kernel.hpp',
            '.claude/worktrees/runtime-activation-stamp-6424/src/policies/default_policy.cpp'):
    local[rel] = sha(root / rel)
'''
code = code.replace(anchor, extra + '\n' + anchor)
begin, end = code.index('    diagnostic=dict('), code.index('    full50=dict(')
code = code[:begin] + '''    factor_material=dict(summary='benchmarks/runs/20260909-6544-rank32-factor-annotation/summary-6545.json',
                         native_control='benchmarks/runs/20260909-6547-native-material-control/summary.json',
                         solver_source_unchanged=True, new_solver_jobs=0),
''' + code[end:]
begin, end = code.index("    cleanup='"), code.index('\n)\nwith target.open')
code = code[:begin] + '''    cleanup='550070760 bytes of owned temporary profile expansion removed; compressed original and annotations retained. No new certificates/solver jobs or live scratch/fuzz access. Prior fixture cleanup remains prior-owned.',
    failure_classification='No new build, fixture, sanitizer, solver, proof or model failure. Source-only addr2line resolves symbols but not lines; instruction mapping was reviewed directly. No successful writer rerun or denied push/transfer retry.',
    next='Localize the actual native added-material producers behind Rank50 counter2508-to2510 before conflict823562 using a bounded source-tagged observer and exact cached-control stream/proof replay. No tracker implementation or inferred approval; all50/memory goal unmet.',
''' + code[end:]
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

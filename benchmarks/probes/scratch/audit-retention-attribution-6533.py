#!/usr/bin/env python3
"""Scoped successor audit: offline attribution, tests and live checker states."""
from pathlib import Path

source = Path(__file__).with_name('audit-retention-contract-6528.py')
code = source.read_text()
for old, new in {
    '2026-09-09-retention-contract-audit-6528.json': '2026-09-09-retention-attribution-audit-6533.json',
    '2026-09-09-forward-boundaries-audit-6519.json': '2026-09-09-retention-contract-audit-6528.json',
    '20260909-1155-rank32-retention': '20260909-1215-retention-attribution',
    "next='Specific revamp approval pending for three-site binary-retention fast path. Current6424 remains rollback; no implementation or promotion. All50 and memory goal unmet.'":
        "next='Three retention sites total0.619868% of sampled candidate periods: deprioritize as primary Rank32 fix. Inspect source-aligned Factor/Congruence work and costs before selecting a repair. No new implementation is authorized by a pending proposal; all50/memory goal unmet.'",
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
begin, end = code.index('owned = {\n'), code.index('\nlocal = {}')
code = code[:begin] + '''owned = {
    'PROGRESS.md', 'benchmarks/reports/2026-09-09-rank32-retention-attribution.md',
    'tools/bench/perf_annotation.py', 'tools/bench/test_perf_annotation.py',
    'benchmarks/probes/scratch/annotate-retention-6530.py',
    'benchmarks/probes/scratch/summarize-retention-6531.py',
    'benchmarks/probes/scratch/annotate-drop-6532.py',
    'benchmarks/probes/scratch/audit-retention-attribution-6533.py',
    'benchmarks/probes/scratch/land-retention-attribution-6534.py',
}''' + code[end:]
begin, end = code.index('# Refresh only'), code.index('\nchecks = {}')
code = code[:begin] + '''# Refresh prior live checker launches only; no global/fuzz query.
accounting = {}
for name, account in prior['accounting'].items():
    active = any(line.split('|')[1] in ('RUNNING', 'PENDING', 'COMPLETING')
                 for line in account['stdout'].splitlines() if '|' in line)
    if not active:
        continue
    output = subprocess.check_output(account['command'], text=True)
    assert output.strip(), name
    for line in output.splitlines():
        assert line.split('|')[1] in ('COMPLETED', 'RUNNING', 'PENDING', 'COMPLETING'), (name, line)
    accounting[name] = dict(command=account['command'], stdout=output, launch=account['launch'])
    print(name, 'accounting PASS', flush=True)
''' + code[end:]
begin, end = code.index("expected = 'PASS cases="), code.index('\ncommands = ')
code = code[:begin] + code[end:]
anchor = "report = root / 'report-to-andrew/"
assert code.count(anchor) == 1
extra = '''from perf_annotation import parse_periods
annotation = root / 'benchmarks/runs/20260909-6530-rank32-retention-annotation'
summary_path = annotation / 'summary-6531.json'
s = json.loads(summary_path.read_text())
for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest, rel
owned.add(str(summary_path.relative_to(root)))
deleted_bytes = 0
for name in ('20260909-6530-rank32-retention-annotation', '20260909-6532-rank32-drop-annotation'):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    owned.add(str((c / 'manifest.json').relative_to(root)))
    for rel, digest in m['authorities'].items(): assert sha(root / m['parent'] / rel) == digest, rel
    for rel, digest in m['source_kernel_watch_exact_current'].items():
        for tree in ('runtime-compact-stamp-6357', 'runtime-activation-stamp-6424'):
            assert sha(root / '.claude/worktrees' / tree / rel) == digest, (tree, rel)
    for label, output in m['outputs'].items():
        for suffix, key in (('.txt', 'stdout_sha256'), ('.stderr', 'stderr_sha256')):
            p = c / (label + suffix)
            assert sha(p) == output[key], p
            local[str(p.relative_to(root))] = sha(p)
    assert not any(p.is_dir() for p in c.iterdir())
    deleted_bytes += m['temporary_expansions_deleted_bytes']
drop_path = root / 'benchmarks/runs/20260909-6532-rank32-drop-annotation/candidate-drop-periods.txt'
drop = parse_periods(drop_path.read_text(), 'tactix::kernel_drop_large_watches(tactix::Kernel&)')
combined = sum(s['regions'][name]['period'] for name in (
    'rebuild_retention_and_list_bookkeeping', 'collection_pure_retention_and_metadata')) + drop.total_period
assert combined == 7599993882 and deleted_bytes == 1560523560
attribution = dict(drop_samples=drop.samples, drop_period=drop.total_period,
    three_site_period=combined, three_site_percent_program=100 * combined / s['global_event_periods']['candidate'],
    temporary_expansions_deleted_bytes=deleted_bytes)
assert '| Combined, without overlap |7599993882|0.619868%|' in (root / 'benchmarks/reports/2026-09-09-rank32-retention-attribution.md').read_text()
'''
code = code.replace(anchor, extra + '\n' + anchor)
old = 'campaigns=campaigns, accounting=accounting, checks=checks,'
assert code.count(old) == 1
code = code.replace(old, old + '\n    attribution=attribution,')
old = "cleanup='No new solver job, proof/input generation or manual deletion; no live scratch or fuzz inspection. Scoped final results checked/clean. Prior audit owns other unchanged evidence.'"
assert code.count(old) == 1
code = code.replace(old, "cleanup='Only1560523560 bytes of owned temporary profile expansions removed by annotation helpers; original compressed profiles and new annotations retained. No solver job, proof/input generation, certificate/live-scratch or fuzz inspection. Prior audit owns unchanged evidence.'")
anchor = "assert 'Overfull' not in (report / 'report.log').read_text()"
assert code.count(anchor) == 1
code = code.replace(anchor, "local[str((report / 'report-overflow.log').relative_to(root))] = sha(report / 'report-overflow.log')\n" + anchor)
code = code.replace("No unit, sanitizer, solver, proof or model failure.",
    "Initial PDF had38.5pt overflow, preserved then reflowed with two clean compiles. No unit, sanitizer, solver, proof or model failure.")
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

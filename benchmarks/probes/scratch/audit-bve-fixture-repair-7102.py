#!/usr/bin/env python3
"""Seal direct fixture repair and the user-directed debugging ownership change."""
import datetime
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-10-bve-fixture-repair-audit-7102.json'
assert not target.exists()
hashes = {}
def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or expected == digest, path
    hashes[str(path.relative_to(root))] = digest
previous = root / 'benchmarks/reports/2026-09-10-checked-control-audit-7099.json'
bind(previous)
prior = json.loads(previous.read_text())
for rel, digest in prior['hashes'].items():
    if rel not in ('PROGRESS.md', 'DEVELOPMENT_PLAN.md'):
        bind(root / rel, digest)
owned = ['DEVELOPMENT_PLAN.md', 'PROGRESS.md',
    'benchmarks/reports/2026-09-10-bve-fixture-direct-repair-7102.md']
owned += ['benchmarks/probes/scratch/' + name for name in (
    'prepare-bve-fixture-repair-7100.py', 'validate-bve-fixture-repair-7100.py',
    'test-bve-view-proofs-7101.cpp', 'check-bve-view-proofs-7101.py',
    'audit-bve-fixture-repair-7102.py', 'claude-bve-view-review-7096-response.json',
    'claude-bve-view-review-7096-stderr.log')]
for name in ('20260910-7100-bve-fixture-repair', '20260910-7101-bve-view-proofs'):
    campaign = root / 'benchmarks/runs' / name
    summary = json.loads((campaign / 'summary.json').read_text())
    for rel, digest in summary['hashes'].items(): bind(root / rel, digest)
    owned += [str(p.relative_to(root)) for p in campaign.rglob('*')
              if p.is_file() and p.suffix in ('.json', '.patch', '.log')]
    if '7100' in name:
        assert summary['status'] == 'PASS'
        assert summary['environment']['ASAN_OPTIONS'] == 'detect_leaks=1:abort_on_error=1'
        assert sum(x['expected_abort'] for x in summary['checks']) == 1
    else:
        assert summary['checked'] == 96 and summary['negative_controls'] == 98
        assert summary['exact_parent_certificates']
review = json.loads((root / 'benchmarks/probes/scratch/claude-bve-view-review-7096-response.json').read_text())
assert review['terminal_reason'] == 'aborted_streaming' and review['is_error']
assert not (root / 'benchmarks/runs/20260910-7091-bve-views-BC-gate').exists()
for name in ('20260910-7090-bve-view-sanitizers', '20260910-7095-bve-view-proofs'):
    assert not (root / 'benchmarks/runs' / name / 'summary.json').exists()
report = root / 'report-to-andrew/20260910-1405-direct-debugging'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py']
            for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
queue = subprocess.check_output(['squeue', '-u', 'hwu', '-h', '-o', '%i|%j|%T|%P'], text=True)
for rel in owned: bind(root / rel)
with target.open('x') as stream:
    json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        hashes=hashes, owned_paths=sorted(set(owned)), checks=checks, scheduler=queue,
        outcome='User-directed direct debugging policy installed. Claude review stopped, not passed. Original Debug assertion reproduced; fixed fixture and existing BVE/memory pass ASan/UBSan/LSan.96 strong positive proofs and98 negatives; exact parent/candidate bytes. No solver implementation or pin changes.',
        next_action='Resume B28+C5 using7100/7101 dependencies, then three paired31/35/38 runtime/RSS waves. No promotion, fuzz or cumulative acceptance.',
        hygiene='Original failures and small fixed reproduction artifacts retained; no material deletion or new solver jobs. Andrew report local/unsent; denied push not retried.'), stream, indent=2)
print(target)
print('PASS: frozen authority, fixture gates, documentation and benchmark/SLURM unit suites')

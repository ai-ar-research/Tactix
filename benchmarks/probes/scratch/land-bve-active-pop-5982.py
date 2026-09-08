#!/usr/bin/env python3
"""Validate and land only the owned inactive-pop/heap investigation checkpoint."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-bve-active-pop-landing.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
report = root / 'report-to-andrew/20260908-1740-bve-active-pop'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
a = root / 'benchmarks/runs/20260908-5962-bve-active-pop'
m = json.loads((a / 'manifest.json').read_text())
for rel, digest in m['files'].items():
    assert sha(root / m['source'] / rel) == sha(a / 'source' / rel) == digest
for rel, digest in m['binaries'].items(): assert sha(root / rel) == digest
for rel, digest in m['patches'].items(): assert sha(a / rel) == digest
for rel, digest in m['validation'].items(): assert sha(root / rel) == digest
v = json.loads((a / 'final-proof-validation.json').read_text())
assert v['checked_proofs'] == 54
for rel, digest in v['hashes'].items(): assert sha(root / rel) == digest
prefix = root / 'benchmarks/runs/20260908-5969-bve-active-pop-rank32-prefix'
v = json.loads((prefix / 'active-pop-validation.json').read_text())
assert v['old_inactive_count'] == 9 and not v['new_inactive_attempts']
assert v['first_parent_active_difference'] is None and v['parent_partial_proof_equal']
assert all(v['ordinary_parent_trace_equality'].values())
assert json.loads((prefix / 'summary.json').read_text())['matching_decisions'] == 68091135
checks = {}
commands = [(name, ['python3', f'tools/docs/{name}.py']) for name in
            ('check-doc-size', 'check-section-refs', 'check-option-coverage',
             'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory')]
commands += [('benchmark-unit-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py'])]
for name in ('compare-bve-full-heap-5960.py', 'compare-bve-sift-ancestry-5973.py',
             'analyze-bve-active-pop-5977.py', 'compare-bve-earlier-heap-5979.py',
             'classify-checker-timeout-5981.py'):
    commands.append((name, ['python3', 'benchmarks/probes/scratch/' + name, '--self-test']))
for name, cmd in commands:
    r = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, (name, r.stdout, r.stderr)
    checks[name] = r.stdout + r.stderr
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',
            'docs/scheduling-parity-ledger.md',
            'benchmarks/reports/2026-09-08-bve-active-pop.md',
            'benchmarks/reports/2026-09-08-bve-active-pop-replay.md',
            'benchmarks/reports/2026-09-08-bve-active-pop-validation.json',
            'benchmarks/reports/2026-09-08-checker-timeout-384.json'):
    retain(root / rel)
for number in range(5956, 5983):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'): retain(p)
for number in (5956, 5958, 5959, 5962, 5966, 5968, 5969, 5970, 5972, 5978):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches) == 1
    c = matches[0]
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size >= 5 << 20: continue
        parts = p.relative_to(c).parts
        if any(name in parts for name in ('claims', '__pycache__')): continue
        if 'traces' in parts and p.name not in ('candidate.bve-heap.tsv', 'kissat.bve-heap.tsv'): continue
        if 'logs' in parts and number not in (5956, 5959, 5962, 5966): continue
        if p.suffix in ('.json', '.py', '.sbatch', '.tsv', '.c', '.cpp', '.hpp', '.patch', '.txt', '.log', '.cnf', '.proof', '.drat', '.lrat'):
            retain(p)
out = dict(hashes={rel: sha(root / rel) for rel in sorted(owned)}, checks=checks,
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    outcome='Isolated5962 validated policy repair closes nine inactive BVE attempts, not the existing trajectory/runtime gap. BC solver screen passes; full5970 running. Neutral5958/5972 localize earlier same-pop topology;5978 captures it.',
    cleanup='No manual deletion. Audited terminal normal records confirm owned cleanup; active jobs, pins and large raw traces retained. No fuzz-owner artifact access.',
    next_action='Score5970 once complete using5976;validate5978 with5979 before interpreting earlier differing sift subtrees. Do not rerun frozen summaries. Proof checks asynchronous.',
    limitation='Local checkpoint; report saved,not sent. Push remains unapproved and is not retried. Current-source fuzz denied/pending;no promotion orgoal closure.')
target.write_text(json.dumps(out, indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Repair inactive BVE pops and trace earlier heap topology'], cwd=root, check=True)
print('PASS:', len(owned), 'explicit owned paths landed; no push', flush=True)

#!/usr/bin/env python3
"""Land only the reinsertion witness and its independent follow-up locally."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-bve-reinsert-order-landing.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()
report = root / 'report-to-andrew/20260908-1823-bve-reinsert-order'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
audit = root / 'benchmarks/reports/2026-09-08-bve-reinsert-order-validation.json'
for rel, digest in json.loads(audit.read_text())['hashes'].items():
    assert sha(root / rel) == digest, rel
checks = {}
for name in ('validate-bve-reinsert-order-5998.py', 'compare-bve-clause-entry-6002.py'):
    r = subprocess.run(['python3', 'benchmarks/probes/scratch/' + name, '--self-test'],
                       cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, (name, r.stdout, r.stderr)
    checks[name] = r.stdout + r.stderr
v = json.loads((root / 'benchmarks/runs/20260908-5996-bve-reinsert-order/heap-validation.json').read_text())
assert v['full_ordinary_trace_and_partial_proof_replay'] and v['complete_bve_candidate_replay']
assert [r['subject'] for r in v['completed_insertions']['candidate']] == ['5409', '5415']
assert [r['subject'] for r in v['completed_insertions']['kissat']] == ['5415', '5409']
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md', 'benchmarks/reports/2026-09-08-bve-reinsert-order.md',
            'benchmarks/reports/2026-09-08-bve-reinsert-order-validation.json'):
    retain(root / rel)
for number in range(5996, 6004):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'):
        retain(p)
for number in (5992, 5996, 6000):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches) == 1
    c = matches[0]
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items():
        assert sha(c / rel) == digest, rel
    for p in c.rglob('*'):
        parts = p.relative_to(c).parts
        if not p.is_file() or any(n in parts for n in ('pins', 'logs', 'traces', '__pycache__')):
            continue
        if p.suffix in ('.json', '.py', '.sbatch'):
            retain(p)
    if number in (5992, 5996):
        for arm in ('candidate', 'kissat'):
            r = json.loads((c / 'results' / (arm + '.json')).read_text())
            assert r['exit_code'] == 0 and r['scratch_deleted']
            for suffix in ('bve-heap.tsv', 'retry-producer.tsv'):
                p = c / 'traces' / (arm + '.' + suffix)
                if not p.exists():
                    assert number == 5992 and suffix == 'retry-producer.tsv'
                    continue
                assert sha(p) == r['trace_sha256'][p.name]
                retain(p)
queue = subprocess.run(['squeue', '-h', '-j', '1676209', '-o', '%i|%P|%T|%C|%m|%R'],
                       capture_output=True, text=True, check=True).stdout
terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1675996,1676156', '-o', 'JobID,State,ExitCode', '-P'],
                          capture_output=True, text=True, check=True).stdout
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)},
    checks=checks, queue=queue, terminal=terminal,
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    outcome='Validated actual5409/5415 reinsertion reversal and Tactix clause-deletion pose; no solver change or runtime improvement.5962 remains1.116453x.',
    next_action='Validate6000/job1676209 with6002, compare selected seven-literal entry pose against5937 exit; trace its earliest producer and implement general repair on5962.',
    cleanup='Both5992/5996 tasks own terminal cleanup. No manual deletion. Active pins and all large hash-bound evidence retained; no fuzz artifact access.',
    limitations='No goal closure, push, production promotion or fuzz transfer. Andrew report local, not sent.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Localize BVE heap divergence to adjacent clause-driven reinsertions'], cwd=root, check=True)
print('PASS:', len(owned), 'explicit paths landed locally; no push')

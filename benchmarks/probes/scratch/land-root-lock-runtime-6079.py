#!/usr/bin/env python3
"""Land the fixed-reference runtime result and validated Rank37 localization."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-root-lock-runtime-landing.json'
assert not target.exists()

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

def run_dir(n):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{n}-*'))
    assert len(matches) == 1
    return matches[0]

report = root / 'report-to-andrew/20260908-2055-root-lock-runtime'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
audit = root / 'benchmarks/reports/2026-09-08-root-lock-runtime-validation.json'
for rel, digest in json.loads(audit.read_text())['hashes'].items():
    assert sha(root / rel) == digest, rel
full = run_dir(6061)
s = json.loads((full / 'solver-checkpoint.json').read_text())
assert s['records'] == s['expected_solves'] == 40 and not s['runtime_target_met']
assert abs(s['ratio'] - 1.0929579830797531) < 1e-12
work = json.loads((full / 'work-cost-summary.json').read_text())
for rel, digest in work['hashes'].items():
    assert sha(root / rel) == digest, rel
assert work['verdicts'] == {'SAT': 22, 'UNSAT': 18}
c = run_dir(6070)
s = json.loads((c / 'summary.json').read_text())
assert s['matching_decisions'] == 77120545 and s['suffix_comparison']['left'] == [272979, 76953349, 388, 116360, 0]
assert s['suffix_comparison']['right'] == [272979, 76953349, 388, 116362, 0]
b = json.loads((c / 'boundary-localization.json').read_text())
for rel, digest in b['hashes'].items():
    assert sha(root / rel) == digest, rel
assert b['search']['first']['candidate']['conflicts'] == 152381
assert b['nonbinary_search']['first']['candidate']['conflicts'] == 188501
checks = subprocess.run(['python3', 'benchmarks/probes/scratch/localize-root-lock-rank37-6073.py', '--self-test'], cwd=root, capture_output=True, text=True)
assert checks.returncode == 0, checks.stderr
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md', 'benchmarks/reports/2026-09-08-root-lock-runtime.md',
            'benchmarks/reports/2026-09-08-root-lock-runtime-validation.json'):
    retain(root / rel)
for n in range(6070, 6080):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(n) + '.*'):
        retain(p)
for n in (6059, 6061, 6070, 6076):
    c = run_dir(n)
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m.get('pin_sha256', m.get('hashes', {})).items():
        assert sha(c / rel) == digest, rel
    for p in c.rglob('*'):
        parts = p.relative_to(c).parts
        if not p.is_file() or p.stat().st_size >= 5 << 20 or any(name in parts for name in ('traces', 'logs', 'claims', '__pycache__')):
            continue
        if 'pins' in parts and p.suffix != '.py':
            continue
        if p.suffix in ('.json', '.py', '.sbatch', '.tsv'):
            retain(p)
terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1676768', '-o', 'JobID,State,ExitCode', '-P'], capture_output=True, text=True, check=True).stdout
assert terminal.count('COMPLETED|0:0') == 1, terminal
queue = subprocess.run(['squeue', '-h', '-j', '1673402,1676660,1676727,1676769', '-o', '%i|%P|%T|%C|%m|%R'], capture_output=True, text=True, check=True).stdout
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)},
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    boundary_tests=checks.stdout, terminal=terminal, queue=queue,
    outcome='Current6049 full40 is1.092957983x fixedKissat,goal unmet. Rank37 firstsplit272979 after77120545 common decisions;6076/job1676769 probes missing last-common heap/assignment state. No new solver change or promotion.',
    next_action='Run6078 once both6076 tasks finish;require every ordinary1M trace and partial proof byte to replay before interpreting selected heap and assignment state. Continue normal certificate checks asynchronously.',
    cleanup='No manual deletion;completed6070 and published normal results assert owned scratch/certificate cleanup. Preserve live6076 scratch,all cited baseline/pin/source/trace authorities. No fuzz-owner artifact access.',
    limitations='Fixed-reference cross-window score,not unit-cost attribution. Current-source fuzz unverified;denied transfer and push not retried. Andrew report local,not sent.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Measure root-lock runtime gain and localize next Rank37 decision split'], cwd=root, check=True)
print('PASS:', len(owned), 'explicit paths landed locally; no push')

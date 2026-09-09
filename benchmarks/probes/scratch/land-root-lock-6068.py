#!/usr/bin/env python3
"""Land the isolated, checked root-reason repair and bounded causal closure."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-root-reason-repair-landing.json'
assert not target.exists()

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

def run_dir(number):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches) == 1
    return matches[0]

def verify_bound(path):
    record = json.loads(path.read_text())
    for rel, digest in record.get('hashes', {}).items():
        assert sha(root / rel) == digest, rel
    return record

report = root / 'report-to-andrew/20260908-2036-root-reason-repair'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
audit = root / 'benchmarks/reports/2026-09-08-root-reason-repair-validation.json'
verify_bound(audit)
c = run_dir(6049)
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['files'].items():
    assert sha(root / m['source'] / rel) == digest, rel
for rel, digest in m['binaries'].items():
    assert sha(root / rel) == digest, rel
for rel, digest in m['patches'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['logs'].items():
    assert sha(c / 'logs' / rel) == digest, rel
for name, count in (('release', 98), ('bc', 98), ('trace', 97), ('journal', 98)):
    assert f'100% tests passed out of {count}' in (c / 'logs' / f'{name}-ctest.log').read_text()
proof = verify_bound(c / 'complete-proof-validation.json')
assert proof['checked_proofs'] == 62
verify_bound(c / 'final-proof-validation.json')
v = verify_bound(c / 'controls/summary.json')
assert v['checked_proofs'] == 8 and v['outputs']['native'].count('PASS') == 3
v = verify_bound(c / 'final-controls/summary.json')
assert v['old_new_cases'] == 18 and v['root_cases'] == 16 and v['trace_suite'] == '98/98 PASS'
assert '100% tests passed out of 98' in (run_dir(6054) / 'logs/ctest.log').read_text()
bc = verify_bound(run_dir(6059) / 'solver-checkpoint.json')
assert bc['expected_solves'] == 66 and all(bc['ratios'][tier] < 1 for tier in ('B', 'C'))
c = run_dir(6060)
v = json.loads((c / 'summary.json').read_text())
assert v['suffix_comparison']['complete_equal'] and v['matching_decisions'] == 69828932
r = json.loads((c / 'reduce-validation.json').read_text())
assert r['all_reductions_match'] and r['reductions'] == 55
assert sha(c / 'summary.json') == r['decision_summary_sha256']
result = json.loads((c / 'results/candidate.json').read_text())
assert result['exit_code'] == 0 and result['scratch_deleted']

owned = set()
def retain(path):
    assert path.is_file() and path.stat().st_size < 5 << 20, path
    owned.add(str(path.relative_to(root)))

for rel in ('PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md', 'docs/proof-rules.html',
            'benchmarks/reports/2026-09-08-root-reason-repair.md',
            'benchmarks/reports/2026-09-08-root-reason-repair-validation.json'):
    retain(root / rel)
for number in range(6049, 6069):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'):
        retain(p)
for number in (6049, 6054, 6059, 6060, 6061):
    c = run_dir(number)
    for p in c.rglob('*'):
        parts = p.relative_to(c).parts
        if not p.is_file() or p.stat().st_size >= 5 << 20 or '__pycache__' in parts:
            continue
        if 'traces' in parts or 'claims' in parts:
            continue
        if 'logs' in parts and number not in (6049, 6054):
            continue
        if 'pins' in parts and p.suffix != '.py':
            continue
        if p.suffix in ('.json', '.py', '.sbatch', '.tsv', '.cpp', '.c', '.h', '.txt', '.patch', '.log', '.cnf', '.proof'):
            retain(p)

terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1676661', '-o', 'JobID,State,ExitCode', '-P'],
                          capture_output=True, text=True, check=True).stdout
assert terminal.count('COMPLETED|0:0') == 1, terminal
queue = subprocess.run(['squeue', '-h', '-j', '1673402,1676660,1676727', '-o', '%i|%P|%T|%C|%m|%R'],
                       capture_output=True, text=True, check=True).stdout
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)}, terminal=terminal, queue=queue,
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    outcome='Isolated6049 general Reduce root-reason repair is test/proof validated and6060 closes all55 recorded reductions plus69828932 decisions through300K. BC66 expected,full6061/job1676727 runs;no aggregate gain or goal completion claimed.',
    next_action='Score full6061 once40 expected solver records exist using6065,without waiting for proof checking;then prioritize the largest remaining worse-side component. Do not mutate submitted6049/6054 pins or frozen6063/6064/6066/6067 artifacts.',
    cleanup='No manual deletion. Preserve all active scratch and cited pins/sources/traces. Completed normal task and diagnostic records assert their own cleanup; fixed proof witnesses retained as test authority.',
    limitations='Unfuzzed isolated repair,not production promotion. Denied fuzz transfer and push not retried;no fuzz-owner artifact inspection. Report local,not sent.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Repair root-reason reduction locks and close Rank32 causal prefix'], cwd=root, check=True)
print('PASS:', len(owned), 'explicit paths landed locally; no push')

#!/usr/bin/env python3
"""Land tested observers and validated candidate/learned-clause localization."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-vivify-candidate-landing.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
report = root / 'report-to-andrew/20260908-1922-vivify-candidate'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
audit = root / 'benchmarks/reports/2026-09-08-vivify-candidate-validation.json'
for rel, digest in json.loads(audit.read_text())['hashes'].items(): assert sha(root / rel) == digest, rel
controls = root / 'benchmarks/runs/20260908-6016-vivify-pose-controls'
v = json.loads((controls / 'summary.json').read_text())
for rel, digest in v['hashes'].items(): assert sha(root / rel) == digest, rel
assert v['full_trace_suite'] == '99/99 PASS'
assert '100% tests passed out of 99' in (controls / 'logs/tactix-ctest.log').read_text()
assert v['native_controls']['valid']['exit_code'] == 0
assert all(case['exit_code'] == 2 for name, case in v['native_controls'].items() if name != 'valid')
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md', 'benchmarks/reports/2026-09-08-vivify-candidate-origin.md',
            'benchmarks/reports/2026-09-08-vivify-candidate-validation.json'):
    retain(root / rel)
for number in range(6016, 6030):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'): retain(p)
for number in (6016, 6020, 6024):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*')); assert len(matches) == 1
    c = matches[0]
    if number != 6016:
        m = json.loads((c / 'manifest.json').read_text())
        for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
        for rel, digest in m['native_source_files'].items(): assert sha(root / m['native_source'] / rel) == digest, rel
        for rel, digest in m['diagnostic_source_files'].items(): assert sha(root / m['candidate_source'] / rel) == digest, rel
        summary = json.loads((c / 'summary.json').read_text())
        assert summary['full_parent_replay' if number == 6020 else 'full_replay']
        for arm in ('candidate', 'kissat'):
            r = json.loads((c / 'results' / (arm + '.json')).read_text())
            assert r['exit_code'] == 0 and r['scratch_deleted']
    for p in c.rglob('*'):
        parts = p.relative_to(c).parts
        if not p.is_file() or p.stat().st_size >= 5 << 20 or any(n in parts for n in ('pins', '__pycache__')): continue
        if 'traces' in parts and not p.name.endswith('.clause-pose-series.tsv'): continue
        if 'logs' in parts and number != 6016: continue
        if p.suffix in ('.json', '.py', '.sbatch', '.tsv', '.cpp', '.c', '.h', '.txt', '.patch', '.log'): retain(p)
checks = {}
for name in ('validate-vivify-pose-6019.py', 'validate-vivify-lemma-6026.py'):
    r = subprocess.run(['python3', 'benchmarks/probes/scratch/' + name, '--self-test'], cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, (name, r.stdout, r.stderr)
    checks[name] = r.stdout + r.stderr
queue = subprocess.run(['squeue', '-h', '-j', '1673402,1675801', '-o', '%i|%P|%T|%C|%m|%R'], capture_output=True, text=True, check=True).stdout
terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1676418,1676453', '-o', 'JobID,State,ExitCode', '-P'], capture_output=True, text=True, check=True).stdout
assert terminal.count('COMPLETED|0:0') == 4, terminal
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)}, checks=checks,
    queue=queue, terminal=terminal,
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    outcome='6020 exact replay identifies native-only learned candidate as secondVivify pose mutator.6024 exact replay brackets native appearance between Reduce240062end/247206begin.99/99 trace tests and native controlsPASS.5962 runtime remains1.116453x.',
    next_action='On NEW diagnostic clones instrument standalone learned stages over240062-247206,validate replay and identify producer;then general repair/test on5962. No source or timing promotion.',
    cleanup='No manual deletion.6020/6024 terminal records confirm scratch cleanup;retain active pins, authority manifests and large hash-bound traces. No fuzz-owner artifact access.',
    limitations='Goal not complete. No push,production promotion,new runtime gain or fuzz transfer. Andrew report local,not sent.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Identify the learned clause driving Vivify and BVE order drift'], cwd=root, check=True)
print('PASS:', len(owned), 'explicit paths landed locally; no push')

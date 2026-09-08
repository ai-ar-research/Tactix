#!/usr/bin/env python3
"""Land the validated Vivify interval and tested diagnostic sources locally."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-vivify-pose-landing.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
report = root / 'report-to-andrew/20260908-1848-vivify-pose'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
audit = root / 'benchmarks/reports/2026-09-08-vivify-pose-validation.json'
for rel, digest in json.loads(audit.read_text())['hashes'].items(): assert sha(root / rel) == digest, rel
c = root / 'benchmarks/runs/20260908-6008-clause-pose-series'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
for rel, digest in m['native_source_files'].items(): assert sha(root / m['native_source'] / rel) == digest, rel
for rel, digest in m['diagnostic_source_files'].items(): assert sha(root / m['candidate_source'] / rel) == digest, rel
controls = root / 'benchmarks/runs/20260908-6004-clause-pose-series-controls'
v = json.loads((controls / 'summary.json').read_text())
for rel, digest in v['hashes'].items(): assert sha(root / rel) == digest, rel
assert v['parent_reproduction'] == dict(bad_exit=-11, fixed_exit=0)
assert '100% tests passed out of 98' in (controls / 'logs/tactix-ctest.log').read_text()
v = json.loads((c / 'summary.json').read_text())
assert v['full_replay'] and v['first_common_boundary_pose_difference']['key'] == [270978, 'vivify', 'end', 1]
checks = {}
for name in ('validate-clause-pose-series-6009.py', 'compare-bve-clause-entry-6002.py'):
    r = subprocess.run(['python3', 'benchmarks/probes/scratch/' + name, '--self-test'], cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, (name, r.stdout, r.stderr)
    checks[name] = r.stdout + r.stderr
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md', 'benchmarks/reports/2026-09-08-vivify-pose-origin.md',
            'benchmarks/reports/2026-09-08-vivify-pose-validation.json'):
    retain(root / rel)
for number in range(6004, 6015):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'): retain(p)
for number in (6000, 6004, 6008):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*')); assert len(matches) == 1
    campaign = matches[0]
    for p in campaign.rglob('*'):
        parts = p.relative_to(campaign).parts
        if not p.is_file() or p.stat().st_size >= 5 << 20 or any(n in parts for n in ('pins', '__pycache__')): continue
        if 'traces' in parts and not p.name.endswith('.clause-pose-series.tsv'): continue
        if 'logs' in parts and number != 6004: continue
        if p.suffix in ('.json', '.py', '.sbatch', '.tsv', '.cpp', '.c', '.h', '.txt', '.patch', '.log'): retain(p)
    if number != 6004:
        for arm in ('candidate', 'kissat'):
            r = json.loads((campaign / 'results' / (arm + '.json')).read_text())
            assert r['exit_code'] == 0 and r['scratch_deleted']
queue = subprocess.run(['squeue', '-h', '-j', '1673402,1675706,1675801', '-o', '%i|%P|%T|%C|%m|%R'], capture_output=True, text=True, check=True).stdout
terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1676209,1676281', '-o', 'JobID,State,ExitCode', '-P'], capture_output=True, text=True, check=True).stdout
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)}, checks=checks,
    queue=queue, terminal=terminal,
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    outcome='6008 fullreplay identifies secondVivify270978 as first differing shared selected-clause pose boundary.6000 rulesoutforward.98/98 trace suite and native controls pass after fixture ABI repair.5962 remains1.116453x.',
    next_action='On NEW diagnostic clones, trace selected pose at candidate boundaries inside secondVivify270978; isolate operation and test a general repair on5962. No heap-tie change justified.',
    cleanup='No manual deletion;6000/6008 terminal task records confirm scratch cleanup. Active pins, fixture failure evidence and large hash-bound traces retained. No fuzz-owner artifact access.',
    limitations='No goal closure, new runtime gain, push, production promotion or fuzz transfer. Andrew report local,not sent.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Localize BVE clause-order drift to the second Vivify call'], cwd=root, check=True)
print('PASS:', len(owned), 'explicit paths landed locally; no push')

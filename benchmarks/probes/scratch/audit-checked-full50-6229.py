#!/usr/bin/env python3
"""Seal ownership, parent replay, checked comparison and protected campaigns."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-checked-full50-audit-6229.json'
assert not target.exists()
owned = set(); local = {}; accounting = {}; checks = {}
def retain(p):
    rel = str(p.relative_to(root)); assert p.is_file()
    if p.stat().st_size >= 5 << 20 or rel.endswith('.decisions.tsv'):
        local[rel] = dict(bytes=p.stat().st_size, sha256=sha(p))
    else: owned.add(rel)

for rel in ('PROGRESS.md', 'benchmarks/reports/2026-09-09-checked-full50-and-retry-producers.md'):
    retain(root / rel)
for number in (6219, *range(6224, 6230)):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.py'): retain(p)
prior = json.loads((root / 'benchmarks/reports/2026-09-09-membership-admission-audit-6220.json').read_text())
for rel, digest in prior['protected_campaign_hashes'].items(): assert sha(root / rel) == digest
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
for name in ('20260909-6217-rank32-retry-members', '20260909-6224-rank32-binary-producers'):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    s = json.loads((c / 'summary.json').read_text()); assert s['parent_replay']
    for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest; retain(root / rel)
    for filename in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch', 'summary.json'): retain(c / filename)
    launch = json.loads((c / 'launch.json').read_text())
    cmd = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
           '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    out = subprocess.check_output(cmd, text=True)
    lines = out.strip().splitlines(); assert len(lines) == 2
    for i, line in enumerate(lines):
        f = line.split('|'); assert f[:3] == [launch['job_id'] + '_' + str(i), 'COMPLETED', '0:0']
        assert f[3] >= launch['accounting_since']
    accounting[name] = dict(command=cmd, stdout=out)
    for arm in ('candidate', 'kissat'):
        r = json.loads((c / 'results' / (arm + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()

for name in ('2026-09-09-fresh-rank50-6226', '2026-09-09-fresh-rank50-final-6227'):
    folder = root / 'benchmarks/reports' / name
    for p in folder.iterdir(): retain(p)
fresh = json.loads((root / 'benchmarks/reports/2026-09-09-fresh-rank50-final-6227/summary.json').read_text())
assert fresh['all_certificates_checked'] and fresh['all_classified_for_scoring'] and fresh['zero_residue']
assert fresh['final_records'] == 100
for path, digest in fresh['record_hashes'].items(): assert sha(Path(path)) == digest; retain(Path(path))
full = root / 'benchmarks/runs/20260909-6181-learned-sort-ranks1-50'
states = {}
for folder in ('provisional', 'results'):
    for p in (full / folder).glob('*.json'):
        r = json.loads(p.read_text()); state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT')
        states[r['task']] = state; retain(p)
assert len(states) == 50
commands = [(str(n), ['python3', 'benchmarks/probes/scratch/' + name, '--self-test'])
    for n, name in ((6219, 'validate-rank32-retry-members-6219.py'), (6228, 'validate-rank32-binary-producers-6228.py'))]
commands += [(name, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/' + name, '-p', 'test_*.py']) for name in ('bench', 'slurm')]
commands += [(name, ['python3', 'tools/docs/' + name + '.py']) for name in ('check-doc-size', 'check-progress',
    'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-0230-checked-full50'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
cmd = ['squeue', '-h', '-j', '1677122', '-o', '%i|%T|%M|%N']
queue = subprocess.check_output(cmd, text=True)
target.write_text(json.dumps(dict(owned_hashes={r: sha(root / r) for r in sorted(owned)},
    local_hash_bound=local, protected_campaign_hashes=prior['protected_campaign_hashes'],
    accounting=accounting, live_queue=dict(command=cmd, stdout=queue), full50_classes=states,
    checks=checks, andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    cleanup='No manual campaign deletion; both terminal diagnostics assert deleted scratch/partial proofs. Large traces/pins retained locally. No frozen runner/source/baseline modifications.',
    failures='Initial report compilation guessed unavailable texlive module; corrected to existing amh-tex/2023 and compiled twice. Initial PROGRESS patch had reverse-ordered hunks and failed before edit; applied ordered separate hunks. Native snapshot selector coverage limitation preserved explicitly.',
    next='Add actual fixed regression for canonical Substitute deleted-binary retry publication; repair general path, prove prefix closure and required gates. Rank50 actual admission inputs still open. No promotion or denied push/transfer retry.'), indent=2) + '\n')
print(target)

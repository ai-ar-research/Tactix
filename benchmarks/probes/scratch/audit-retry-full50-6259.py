#!/usr/bin/env python3
"""Record protected campaign integrity, pending checks, report and owned evidence."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha, score
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-retry-full50-audit-6259.json'
assert not target.exists()
prior = json.loads((root / 'benchmarks/reports/2026-09-09-substitute-retry-audit-6250.json').read_text())
for rel, digest in prior['protected_campaign_hashes'].items(): assert sha(root / rel) == digest
b = json.loads((root / 'benchmarks/runs/20260909-6231-substitute-retry-build/manifest.json').read_text())
for rel, digest in b['files'].items(): assert sha(root / b['source'] / rel) == digest
for rel, digest in b['binaries'].items(): assert sha(root / rel) == digest
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = set(); local = {}; runs = {}; checks = {}
def retain(p):
    rel = str(p.relative_to(root))
    if p.stat().st_size >= 1 << 20 or rel.endswith('.decisions.tsv'):
        local[rel] = dict(bytes=p.stat().st_size, sha256=sha(p))
    else: owned.add(rel)
for rel in ('PROGRESS.md', 'benchmarks/reports/2026-09-09-retry-full50-and-membership.md',
            'benchmarks/reports/2026-09-09-substitute-retry-checkpoint-6255.json'):
    retain(root / rel)
for p in (root / 'benchmarks/reports/2026-09-09-retry-full50-6258').iterdir(): retain(p)
for n in range(6254, 6261):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(n) + '.py'): retain(p)
for name in ('20260909-6236-substitute-retry-BC-gate', '20260909-6239-substitute-retry-ranks1-50',
             '20260909-6181-learned-sort-ranks1-50', '20260909-6248-repaired-rank32-membership',
             '20260909-6256-rank32-remaining-retry'):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m.get('pin_sha256', m.get('hashes')).items(): assert sha(c / rel) == digest
    launch = json.loads((c / 'launch.json').read_text())
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
               '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    accounting = subprocess.check_output(command, text=True)
    states = {}
    for folder in ('provisional', 'results'):
        for p in (c / folder).glob('*.json'):
            r = json.loads(p.read_text()); retain(p)
            if 'task' in r:
                state = classify(r, folder == 'results')
                assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
                states[r['task']] = state
            else:
                assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
                for filename, digest in r['trace_sha256'].items():
                    q = c / 'traces' / filename; assert sha(q) == digest; retain(q)
    for filename in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch', 'summary.json'):
        if (c / filename).exists(): retain(c / filename)
    runs[name] = dict(launch=launch, accounting=dict(command=command, stdout=accounting), states=states)
    if 'ranks1-50' in name: runs[name]['score'] = score(c)
    print(name, len(states), 'records;', sum(s['checked'] for s in states.values()), 'checked', flush=True)
for name, command in [('bench', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
                      ('slurm', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/slurm', '-p', 'test_*.py']),
                      ('observer', ['python3', 'benchmarks/probes/scratch/validate-repaired-membership-6254.py', '--self-test'])] + [
    (n, ['python3', 'tools/docs/' + n + '.py']) for n in
    ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]:
    p = subprocess.run(command, cwd=root, text=True, capture_output=True, check=True)
    checks[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-0311-retry-full50'
assert 'Overfull' not in (report / 'report.log').read_text()
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
result = dict(owned_hashes={rel: sha(root / rel) for rel in sorted(owned)}, local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'], runs=runs, checks=checks,
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    next='Validate6256 both arms and new399915 producer schema before interpretation; capture Rank50 admission inputs at823562. Pending certificate/fuzz/memory/repetition gates stay open.',
    cleanup='No manual deletion. Completed diagnostic scratch absence verified; active checker scratch retained. No source/pin change, no denied push/transfer retry.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(target)

#!/usr/bin/env python3
"""Freeze verified observer evidence and live certificate status without mutation."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha, score
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-09-rank50-admission-audit-6291.json'
assert not target.exists()
prior = json.loads((root/'benchmarks/reports/2026-09-09-vivify-binary-audit-6281.json').read_text())
for rel, digest in prior['protected_campaign_hashes'].items(): assert sha(root/rel) == digest, rel
owned = set(); local = {}; checks = {}; runs = {}
def retain(p):
    rel = str(p.relative_to(root))
    if p.stat().st_size >= 1 << 20 or rel.endswith('.decisions.tsv') or p.suffix == '.log':
        local[rel] = dict(bytes=p.stat().st_size, sha256=sha(p))
    else: owned.add(rel)
for rel in ('PROGRESS.md', 'benchmarks/reports/2026-09-09-rank50-admission-gap.md',
            'benchmarks/reports/2026-09-09-vivify-binary-checkpoint-6284.json',
            'benchmarks/reports/2026-09-09-vivify-binary-checkpoint-6287.json'):
    retain(root/rel)
for p in (root/'benchmarks/reports/2026-09-09-vivify-full50-6290').iterdir(): retain(p)
for n in range(6283, 6293):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.*'):
        if p.suffix in ('.py', '.c', '.cpp'): retain(p)
c = root/'benchmarks/runs/20260909-6283-admission-observer-build'
b = json.loads((c/'manifest.json').read_text())
for source, files in b['files'].items():
    for rel, digest in files.items(): assert sha(root/source/rel) == digest, (source, rel)
for rel, digest in b['binaries'].items(): assert sha(root/rel) == digest, rel
for rel, digest in b['patches'].items(): assert sha(c/rel) == digest, rel
assert b['release_exact_parent']
for kind, count in (('release', 105), ('trace', 104)):
    log = (c/'logs'/f'{kind}-ctest.log').read_text()
    assert f'100% tests passed out of {count}' in log and '***Failed' not in log and '***Exception' not in log
for p in c.rglob('*'):
    if p.is_file() and p.suffix in ('.json', '.patch', '.log'): retain(p)
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
for name in ('20260909-6275-vivify-binary-BC-gate', '20260909-6276-vivify-binary-rank32-prefix',
             '20260909-6278-vivify-binary-ranks1-50', '20260909-6286-rank50-admission',
             '20260909-6236-substitute-retry-BC-gate', '20260909-6239-substitute-retry-ranks1-50',
             '20260909-6181-learned-sort-ranks1-50'):
    c = root/'benchmarks/runs'/name; m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m.get('pin_sha256', m.get('hashes')).items(): assert sha(c/rel) == digest, rel
    launch = json.loads((c/'launch.json').read_text())
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    accounting = subprocess.check_output(command, text=True)
    states = {}
    for folder in ('provisional', 'results'):
        for p in (c/folder).glob('*.json'):
            r = json.loads(p.read_text()); retain(p)
            if 'task' in r:
                state = classify(r, folder == 'results')
                assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
                states[r['task']] = state
            else:
                assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
                for filename, digest in r['trace_sha256'].items():
                    q = c/'traces'/filename; assert sha(q) == digest; retain(q)
    for filename in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch', 'summary.json'):
        if (c/filename).exists(): retain(c/filename)
    runs[name] = dict(launch=launch, accounting=dict(command=command, stdout=accounting), states=states)
    if 'ranks1-50' in name: runs[name]['score'] = score(c)
    print(name, len(states), 'records', sum(s['checked'] for s in states.values()), 'checked', flush=True)
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/'+n, '-p', 'test_*.py']) for n in ('bench', 'slurm')]
commands += [('admission-parser', ['python3', 'benchmarks/probes/scratch/validate-rank50-admission-6289.py', '--self-test'])]
commands += [(n, ['python3', 'tools/docs/'+n+'.py']) for n in ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    p = subprocess.run(command, cwd=root, text=True, capture_output=True, check=True)
    checks[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr); print(name, 'PASS', flush=True)
report = root/'report-to-andrew/20260909-0400-rank50-admission'
assert 'Overfull' not in (report/'report.log').read_text()
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={rel:sha(root/rel) for rel in sorted(owned)}, local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'], runs=runs, checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex', 'report.pdf')},
    cleanup='Three bounded6286 task scratch directories including partial proofs removed by their owners after hashing; no final-proof acceptance inferred. Fixed admission fixtures remove temporary trace files. No pending certificate/checker scratch manually touched; no broad cleanup.',
    next='Audit canonical forward-added lifecycle and implement separate faithful Eliminate material invitation, preserving SP-SLOT-5 events. Required source fuzz, paired timing and memory remain open. Revisit checkers; no promotion or denied transfer/push retry.'), indent=2)+'\n')
print(target)

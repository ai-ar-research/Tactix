#!/usr/bin/env python3
"""Audit source-bound tests, protected campaigns, asynchronous checks and cleanup."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha, score
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-09-vivify-binary-audit-6281.json'
assert not target.exists()
prior = json.loads((root/'benchmarks/reports/2026-09-09-retry-full50-audit-6259.json').read_text())
for rel, digest in prior['protected_campaign_hashes'].items(): assert sha(root/rel) == digest
owned = set(); local = {}; checks = {}; runs = {}
def retain(p):
    rel = str(p.relative_to(root))
    if p.stat().st_size >= 1 << 20 or rel.endswith('.decisions.tsv'):
        local[rel] = dict(bytes=p.stat().st_size, sha256=sha(p))
    else: owned.add(rel)
for rel in ('PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',
            'benchmarks/reports/2026-09-09-vivify-binary-retry.md'):
    retain(root/rel)
for n in range(6261, 6283):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.*'):
        if p.suffix in ('.py', '.c', '.cpp'): retain(p)
for name in ('20260909-6231-substitute-retry-build', '20260909-6269-vivify-binary-retry-build'):
    c = root/'benchmarks/runs'/name; b = json.loads((c/'manifest.json').read_text())
    for rel, digest in b['files'].items(): assert sha(root/b['source']/rel) == digest
    for rel, digest in b['binaries'].items(): assert sha(root/rel) == digest
    for rel, digest in b['patches'].items(): assert sha(c/rel) == digest
    if '6269' in name:
        for kind, count in (('release', 104), ('bc', 104), ('trace', 103)):
            log = (c/'logs'/f'{kind}-ctest.log').read_text()
            assert f'100% tests passed out of {count}' in log
            assert '***Failed' not in log and '***Exception' not in log
        for p in c.rglob('*'):
            if p.is_file() and p.suffix in ('.json', '.patch', '.log'): retain(p)
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
for name in ('20260909-6256-rank32-remaining-retry', '20260909-6264-rank32-removal-clause',
             '20260909-6275-vivify-binary-BC-gate', '20260909-6276-vivify-binary-rank32-prefix',
             '20260909-6278-vivify-binary-ranks1-50', '20260909-6236-substitute-retry-BC-gate',
             '20260909-6239-substitute-retry-ranks1-50', '20260909-6181-learned-sort-ranks1-50'):
    c = root/'benchmarks/runs'/name; m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m.get('pin_sha256', m.get('hashes')).items(): assert sha(c/rel) == digest
    launch = json.loads((c/'launch.json').read_text())
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
               '-o', 'JobID,State%40,ExitCode,Submit', '-P']
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
    for filename in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch', 'matrix.tsv', 'summary.json'):
        if (c/filename).exists(): retain(c/filename)
    runs[name] = dict(launch=launch, accounting=dict(command=command, stdout=accounting), states=states)
    if 'ranks1-50' in name: runs[name]['score'] = score(c)
    print(name, len(states), 'records', sum(s['checked'] for s in states.values()), 'checked', flush=True)
for name in ('20260909-6262-removal-observer-build', '20260909-6267-collapse-retry-fixture',
             '20260909-6268-collapse-retry-native', '20260909-6272-vivify-binary-fixed-proofs',
             '20260909-6273-vivify-binary-references', '20260909-6274-vivify-binary-references'):
    c = root/'benchmarks/runs'/name
    assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
    for p in c.rglob('*'):
        if p.is_file() and p.suffix in ('.json', '.log', '.patch'): retain(p)
p = root/'benchmarks/runs/20260909-6262-removal-observer-build/manifest.json'
b = json.loads(p.read_text())
for rel, digest in b['files'].items(): assert sha(root/b['source']/rel) == digest
assert sha(root/b['source']/'build/kissat') == b['binary_sha256']
assert json.loads((root/'benchmarks/runs/20260909-6272-vivify-binary-fixed-proofs/summary.json').read_text())['checked'] == 8
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/'+n, '-p', 'test_*.py']) for n in ('bench','slurm')]
commands += [(n, ['python3', 'benchmarks/probes/scratch/'+n+'.py', '--self-test']) for n in ('validate-remaining-retry-6261', 'validate-removal-clause-6266')]
commands += [(n, ['python3', 'tools/docs/'+n+'.py']) for n in ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    p = subprocess.run(command, cwd=root, text=True, capture_output=True, check=True)
    checks[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr); print(name, 'PASS', flush=True)
report = root/'report-to-andrew/20260909-0337-vivify-binary-retry'
assert 'Overfull' not in (report/'report.log').read_text()
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={rel:sha(root/rel) for rel in sorted(owned)}, local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'], runs=runs, checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup='Eight checked fixed inputs/proofs3838bytes removed by6272; reproducible sources/hashes/exactoutputs retained. No campaign input/proof or pending checker scratch manually removed.',
    next='Run6280 on6276 only once result exists; validate inherited prefix before trajectory claims. Inspect all50 of6278 and ongoing checks; source fuzz, repeated timing, memory remain open. No promotion or denied push/transfer retry.'), indent=2)+'\n')
print(target)

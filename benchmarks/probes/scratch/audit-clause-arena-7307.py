#!/usr/bin/env python3
"""Freeze exact external peak evidence and the isolated clause-arena gate."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-clause-arena-audit-7307.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    rel = str(p.relative_to(root))
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[rel] = digest
    if own:
        owned.add(rel)
def read(p, own=False):
    bind(p, own=own)
    return json.loads(p.read_text())
prior = read(root / 'benchmarks/reports/2026-09-11-external-memory-audit-7296.json')
for rel in prior['owned_paths']:
    if rel != 'PROGRESS.md':
        bind(root / rel, prior['hashes'][rel])
for rel in ('CLAUDE.md', 'DEVELOPMENT_PLAN.md', 'PROGRESS.md'):
    bind(root / rel, own=rel == 'PROGRESS.md')
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
for stem in ('memory_syscalls', 'proc_memory_task', 'proc_memory_analysis'):
    for name in (stem + '.py', 'test_' + stem + '.py'):
        bind(root / 'tools/bench' / name, own=True)
helpers = ('analyze-memory-syscalls-7297', 'prepare-proc-memory-7298',
    'validate-proc-memory-7298', 'submit-proc-memory-7298',
    'prepare-proc-memory-7299', 'validate-proc-memory-7299', 'submit-proc-memory-7299',
    'analyze-proc-memory-7300', 'prepare-clause-realloc-7301', 'build-clause-realloc-7301',
    'build-clause-realloc-7302', 'check-clause-realloc-sanitizers-7303',
    'check-clause-realloc-proofs-7304', 'prepare-clause-realloc-BC-7305',
    'check-submit-clause-realloc-BC-7305', 'prepare-clause-realloc-runtime-7306',
    'check-submit-clause-realloc-runtime-7306', 'audit-clause-arena-7307')
for name in helpers:
    bind(root / 'benchmarks/probes/scratch' / (name + '.py'), own=True)
for campaign, names in (
    ('20260911-7295-memory-syscalls-rank31', ('summary.json', 'mapping-analysis-7297.json')),
    ('20260911-7299-proc-memory-rank31', ('summary.json', 'resident-analysis-7300.json')),
    ('20260911-7303-clause-realloc-sanitizers', ('summary.json',)),
    ('20260911-7304-clause-realloc-proofs', ('summary.json',))):
    c = root / 'benchmarks/runs' / campaign
    for name in names:
        data = read(c / name, own=True)
        for rel, digest in data['hashes'].items():
            bind(root / rel, digest, own=(root / rel).is_relative_to(c) and (root / rel).suffix in ('.json', '.log'))
        if name == 'resident-analysis-7300.json':
            assert all(a['exact_release_peak_reproduced'] for a in data['arms'].values())
            assert [len(data['arms'][a]['observed_hwm_decreases']) for a in ('candidate', 'previous')] == [7, 3]
    if '7304' in campaign:
        assert data['checked_UNSAT'] == data['validated_SAT_models'] == 156
        assert data['negative_controls'] == 2 and data['removed_bytes'] == 63972
        assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
build = root / 'benchmarks/runs/20260911-7302-clause-realloc-build'
b = read(build / 'manifest.json', own=True)
for rel, digest in b['files'].items():
    bind(root / b['source'] / rel, digest)
for group in ('logs', 'patches'):
    for rel, digest in b[group].items():
        bind(build / rel, digest, own=True)
for rel, digest in b['binaries'].items():
    bind(root / rel, digest)
assert b['oracle']['test_clause_arena_replay'] == 'clause-arena clauses=32768 growth_boundaries=58 collection_rounds=32 digest=c499b64fb8e67383\n'
for name in ('20260911-7298-proc-memory-rank31', '20260911-7301-clause-realloc-build'):
    for p in (root / 'benchmarks/runs' / name / 'logs').iterdir():
        bind(p, own=True)

expected = {name: data['expected'] for name, data in prior['campaigns'].items()}
new = {'20260911-7299-proc-memory-rank31': 2,
       '20260911-7305-clause-realloc-BC': 66,
       '20260911-7306-clause-realloc-runtime': 30}
expected.update(new)
campaigns, jobs = {}, set()
for name, count in expected.items():
    c = root / 'benchmarks/runs' / name
    m = read(c / 'manifest.json', own=name in new)
    for rel, digest in m['pin_sha256'].items():
        bind(c / rel, digest, own=name in new and (c / rel).suffix in ('.py', '.tsv', '.sbatch'))
    if name in new:
        for rel, digest in m.get('dependencies', m.get('evidence_sha256', {})).items():
            bind(root / rel, digest)
        for p in (c / 'logs').glob('preflight.*'):
            bind(p, own=True)
    for p in c.iterdir():
        if p.is_file() and (p.name == 'launch.json' or p.name.endswith('-submission.json')):
            launch = read(p, own=name in new)
            assert launch['manifest_sha256'] == hashes[str((c / 'manifest.json').relative_to(root))]
            jobs.add(str(launch['job_id']))
    states = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = read(p, own=True)
            task = r['task']
            assert task == int(p.stem) and 0 <= task < count
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            states[task] = state
            # Only completed solver records bind their finalized logs.
            for key in ('memory_syscalls', 'proc_memory'):
                if key in r['solve']:
                    evidence = r['solve'][key]
                    q = c / evidence['path']
                    assert q.parent == c / 'logs' and q.stat().st_size == evidence['bytes']
                    bind(q, evidence['sha256'], own=True)
    campaigns[name] = dict(expected=count, published=len(states),
        checked=sum(s['checked'] for s in states.values()), records=states)
assert campaigns['20260911-7305-clause-realloc-BC']['published'] == 66
scheduler = {}
for label, command in (
    ('queue', ['squeue', '-h', '-r', '-j', ','.join(sorted(jobs)), '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', ','.join(sorted(jobs)),
                    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'])):
    p = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert p.returncode == 0, (label, p.stderr)
    scheduler[label] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
report = root / 'report-to-andrew/20260911-0703-clause-arena'
for name in ('report.tex', 'report.pdf', 'report.log'):
    bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
bind(root / 'benchmarks/reports/2026-09-11-clause-arena-peak-7307.md', own=True)
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py'] for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, f'tools/docs/{name}.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-occurrence-full50-7262', 'validate-realloc-full50-7283',
    'validate-memory-syscalls-7295', 'analyze-memory-syscalls-7297',
    'validate-proc-memory-7299', 'analyze-proc-memory-7300',
    'check-submit-clause-realloc-BC-7305', 'check-submit-clause-realloc-runtime-7306')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout[-3000:], p.stderr[-3000:])
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
        previous_turn='No progress toward convergence: the immediate bookkeeping recheck reconfirmed existing evidence. This continuation revalidated it, committed the pending7296 checkpoint, reproduced and localized exact Release RSS externally, implemented an isolated clause-arena repair and advanced correctness/runtime gates.',
        decision='7300 brackets actual candidate high-water rise between allocation of a new93519872-byte clause arena and free of its46764032-byte predecessor.7302 tests existing realloc-backed word ownership for Arena only; unchanged capacities, word contents/refs, ordering/collection, guards, scheduling and proofs. Runtime/RSS gates decide; no gain presumed.',
        diagnostic_corrections='7298 sandbox denied fixed preflight before allocation;7299 allowed fixed preflight passed.7301 configure-only missing copied examples fixed in7302. First7300 reader assumed monotonic raw VmHWM; seven/three observed decreases now retained explicitly and tested, original samples unmodified, maximum checked separately against GNUtime/originalRelease. No solver/model/proof/sanitizer invalidity.',
        newly_deleted_bytes=63972,
        cleanup='Only generated7304 fixed CNFs/proofs removed by checked helper; exact logs and hashes retained. No other deletion or growing-log snapshot. All active campaigns/proof checkers preserved.7307 binds only traces finalized by completed solver records.',
        limitation='Target unmet. No scored prototype performance yet, no promotion, no denied fuzz access/transfer or push retry. Runtime/full50, final independent proofs/zero residue, current-source Claude-owned fuzz and consolidation remain open.'), out, indent=2)
print(target)

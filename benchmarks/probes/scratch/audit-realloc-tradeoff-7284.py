#!/usr/bin/env python3
"""Freeze complete repeated tradeoffs, diagnostic limitations, and live full50."""
import datetime as dt
import gzip
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root / 'benchmarks/reports/2026-09-11-realloc-tradeoff-audit-7284.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()


def bind(p, expected=None, own=False):
    rel = str(p.relative_to(root))
    if rel not in hashes:
        hashes[rel] = sha(p)
    assert expected is None or hashes[rel] == expected, p
    if own:
        owned.add(rel)


def read(p, own=False):
    bind(p, own=own)
    return json.loads(p.read_text())


prior = read(root / 'benchmarks/reports/2026-09-11-occurrence-memory-realloc-audit-7274.json')
for rel in prior['owned_paths']:
    if rel != 'PROGRESS.md':
        bind(root / rel, prior['hashes'][rel])
canonical = root / 'benchmarks/probes/scratch/seal-dense-realloc-build-7271.py'
packed = canonical.with_suffix('.py.gz')
bind(packed)
assert gzip.decompress(packed.read_bytes()) == canonical.read_bytes()
bind(root / 'benchmarks/reports/2026-09-11-occurrence-memory-realloc-landing-7275.json')
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
for rel in ('PROGRESS.md', 'DEVELOPMENT_PLAN.md', 'CLAUDE.md'):
    bind(root / rel, own=rel == 'PROGRESS.md')
build = read(root / 'benchmarks/runs/20260911-7271-dense-realloc-build/manifest.json')
assert build['status'] == 'PASS' and len(build['changed']) == 4
for rel, digest in build['files'].items():
    bind(root / build['source'] / rel, digest)
for rel, digest in build['binaries'].items():
    bind(root / rel, digest)

helpers = (
    'inspect-realloc-propagation-7276', 'prepare-realloc-rank31-memory-7277',
    'submit-realloc-rank31-memory-7277', 'validate-realloc-rank31-memory-7277',
    'prepare-realloc-rank38-profiles-7278', 'submit-realloc-rank38-profile-7278',
    'validate-realloc-rank38-profiles-7278', 'extract-realloc-rank38-profiles-7279',
    'localize-realloc-propagation-7280', 'assess-dense-realloc-7281',
    'compare-realloc-memory-7282', 'prepare-realloc-full50-7283',
    'submit-realloc-full50-7283', 'validate-realloc-full50-7283',
    'audit-realloc-tradeoff-7284')
for name in helpers:
    bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)

summaries = {}
for rel in (
    'benchmarks/runs/20260911-7276-realloc-propagation-code/summary.json',
    'benchmarks/runs/20260911-7277-realloc-rank31-memory/summary.json',
    'benchmarks/runs/20260911-7277-realloc-rank31-memory/comparison-7282.json',
    'benchmarks/runs/20260911-7278-realloc-rank38-profiles/solver-summary-7278.json',
    'benchmarks/runs/20260911-7279-realloc-rank38-samples/wave0/summary.json',
    'benchmarks/reports/2026-09-11-realloc-propagation-cost-7280.json',
    'benchmarks/reports/2026-09-11-dense-realloc-assessment-7281.json'):
    data = read(root / rel, own=True)
    summaries[rel] = data
    for path, digest in data.get('hashes', {}).items():
        bind(root / path, digest)
assessment = summaries['benchmarks/reports/2026-09-11-dense-realloc-assessment-7281.json']
assert assessment['expected_outcomes'] == 30
assert set(assessment['runtime']) == {'9', '25', '31', '35', '38'}
assert all(v['absolute_headline_stable'] for v in assessment['spreads'].values())
memory = summaries['benchmarks/runs/20260911-7277-realloc-rank31-memory/comparison-7282.json']
assert memory['exact_structural_rows'] == 10538
assert memory['trace_peaks_kib'] == {'candidate': 505724, 'previous': 509956}
sample = summaries['benchmarks/runs/20260911-7279-realloc-rank38-samples/wave0/summary.json']
assert sample['temporary_expansions_deleted_bytes'] == 1180017428
sample_dir = root / 'benchmarks/runs/20260911-7279-realloc-rank38-samples/wave0'
assert not list(sample_dir.glob('expanded-*'))
for p in sample_dir.iterdir():
    if p.is_file():
        bind(p, own=True)
erratum = read(root / 'benchmarks/runs/20260911-7283-realloc-ranks1-50/metadata-erratum.json', own=True)
assert erratum['correct'] == sample['complete_propagation_ratio'] == 931495926342 / 856765834224

expected = {name: data['expected'] for name, data in prior['campaigns'].items()}
new = {'20260911-7277-realloc-rank31-memory': 2,
       '20260911-7278-realloc-rank38-profiles': 2,
       '20260911-7283-realloc-ranks1-50': 50}
expected.update(new)
campaigns, jobs = {}, set()
for name, count in expected.items():
    c = root / 'benchmarks/runs' / name
    manifest = read(c / 'manifest.json', own=name in new)
    for rel, digest in manifest['pin_sha256'].items():
        bind(c / rel, digest, own=name in new and (c / rel).suffix in ('.py', '.tsv', '.sbatch', '.patch'))
    for p in c.iterdir():
        if p.is_file() and (p.name == 'launch.json' or p.name.endswith('-submission.json')):
            launch = read(p, own=True)
            jobs.add(str(launch['job_id']))
            assert launch['manifest_sha256'] == hashes[str((c / 'manifest.json').relative_to(root))]
    states = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            record = read(p, own=True)
            task = record['task']
            assert task == int(p.stem) and 0 <= task < count
            state = classify(record, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            states[task] = state
    campaigns[name] = dict(expected=count, published=len(states),
                          checked=sum(s['checked'] for s in states.values()), records=states)
    if name in new:
        for p in (c / 'logs').glob('*.slot-memory.tsv'):
            bind(p, own=True)
        for p in (c / 'profiles').glob('*'):
            if p.is_file():
                bind(p)  # Raw padded perf output remains unchanged and local.
        for p in c.glob('*disassembly.txt'):
            bind(p)
assert campaigns['20260911-7273-dense-realloc-runtime']['published'] == 30
assert campaigns['20260911-7278-realloc-rank38-profiles']['published'] == 2
assert campaigns['20260911-7277-realloc-rank31-memory']['published'] == 2

scheduler = {}
for label, command in (
    ('queue', ['squeue', '-h', '-r', '-j', ','.join(sorted(jobs)), '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', ','.join(sorted(jobs)),
                    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'])):
    p = subprocess.run(command, capture_output=True, text=True, timeout=30)
    scheduler[label] = dict(command=command, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr)
    assert p.returncode == 0, (label, p.stderr)

report = root / 'report-to-andrew/20260911-0543-realloc-tradeoff'
for name in ('report.tex', 'report.pdf', 'report.log'):
    bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
bind(root / 'benchmarks/reports/2026-09-11-realloc-tradeoff-7284.md', own=True)
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py']
            for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, f'tools/docs/{name}.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py', '--self-test'] for name in (
    'inspect-realloc-propagation-7276', 'prepare-realloc-rank31-memory-7277',
    'validate-realloc-rank31-memory-7277', 'assess-dense-realloc-7281',
    'compare-realloc-memory-7282', 'validate-realloc-full50-7283',
    'check-submit-dense-realloc-runtime-7273')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'check-submit-occurrence-runtime-7257', 'validate-occurrence-profiles-7259',
    'validate-occurrence-full50-7262', 'check-submit-dense-realloc-BC-7272',
    'check-submit-dense-realloc-runtime-7273', 'validate-realloc-rank31-memory-7277',
    'validate-realloc-rank38-profiles-7278', 'validate-realloc-full50-7283')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
        previous_turn='No convergence progress: read-only current pinned bookkeeping audit confirmed the earlier diagnostic inventory and no-consistent-gain ablation. This checkpoint completes previously unfinished evidence and reports; live7283 was revalidated, not restarted.',
        decision='All30 targeted outcomes exact; retain9/25 RSS benefits and31 RSS/38 runtime penalties. Rank38 increase localizes to ordinary propagation self, not allocation callees. Rank31 trace fails to reproduce Release RSS regression. Full50 breadth measurement continues without waiting for proofs; no promotion or source change.',
        newly_deleted_bytes=1180017428,
        cleanup='Only previously verified7279 temporary raw expansions removed; compressed raw profiles, extracted streams and receipts retained. Current turn deletes nothing. Final checked records require runner cleanup; active proof scratch remains owned by its campaign.',
        packaging='Prior7271 canonical successful helper remains unchanged locally; tracked gzip is byte-exact. New raw profile/disassembly files are unchanged, local, and hash-bound, not normalized for Git whitespace.',
        non_solver_errors='Andrew TeX overfull paragraphs corrected before final compilation. Initial current-session compile failed because texlive was not the available module; use amh-tex/2023. No solver failure or gate waiver.',
        test_consideration='New wrapper/boundary/structural comparison controls plus existing distribution, callstack, function-envelope, self-PC and checked-comparison suites cover this diagnostic/reporting-only checkpoint. Solver source and accepted correctness gates unchanged.',
        limitation='Original runtime target, memory parity, current-source Claude-owned fuzz, remaining independent checks/residue and consolidation remain open. No denied fuzz access/transfer or push retry.'), out, indent=2)
print(target)

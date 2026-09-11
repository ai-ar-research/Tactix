#!/usr/bin/env python3
"""Checkpoint fresh validation without an integration commit or proof wait."""
import datetime
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-10-vivify-revalidation-audit-7169.json'
assert not target.exists()
hashes, owned = {}, []
def bind(p, expected=None, own=False):
    digest = sha(p)
    assert expected is None or digest == expected, p
    rel = str(p.relative_to(root))
    hashes[rel] = digest
    if own: owned.append(rel)
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
for rel in ('PROGRESS.md', 'DEVELOPMENT_PLAN.md', '.claude/skills/revamp/SKILL.md'):
    bind(root / rel, own=rel == 'PROGRESS.md')
for name in ('rebuild-vivify-validation-7162', 'recheck-vivify-controls-7163',
             'recheck-vivify-sanitizers-7164', 'recheck-vivify-proofs-7165',
             'prepare-vivify-revalidation-BC-7166', 'submit-vivify-revalidation-BC-7166',
             'validate-vivify-revalidation-BC-7166', 'prepare-vivify-revalidation-full50-7167',
             'submit-vivify-revalidation-full50-7167', 'validate-vivify-revalidation-full50-7168',
             'audit-vivify-revalidation-7169'):
    bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)
authority = root / 'benchmarks/runs/20260910-7162-vivify-revalidation-build'
m = json.loads((authority / 'manifest.json').read_text())
original_path = root / 'benchmarks/runs/20260910-7144-vivify-compact-build/manifest.json'
original = json.loads(original_path.read_text())
bind(original_path)
assert m['files'] == original['files']
for rel, digest in m['files'].items(): bind(root / m['source'] / rel, digest)
for kind in ('release', 'bc', 'trace'):
    rel = f'build/pin-7162-vivify-revalidation-{kind}/tactix'
    assert m['binaries'][rel] == original['binaries'][f'build/pin-7144-vivify-compact-{kind}/tactix']
    bind(root / rel, m['binaries'][rel])
for p in authority.rglob('*'):
    if p.is_file(): bind(p, own=True)
for number, stem in ((7163, 'controls'), (7164, 'sanitizers'), (7165, 'proofs')):
    c = root / f'benchmarks/runs/20260910-{number}-vivify-revalidation-{stem}'
    g = json.loads((c / 'summary.json').read_text())
    assert g['status'] == 'PASS'
    for rel, digest in g['hashes'].items(): bind(root / rel, digest)
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json', '.log'): bind(p, own=True)
    if number == 7165:
        assert g['checked'] == 96 and g['negative_controls'] == 192 and g['exact_parent_certificates']
        deleted = g['removed_bytes']
        assert deleted == 179234 and not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))

specs = [('20260910-7166-vivify-revalidation-BC-gate', 66, None),
         ('20260910-7167-vivify-revalidation-ranks1-50', 50, None),
         ('20260910-7150-vivify-compact-runtime', 18, 6),
         ('20260910-7152-vivify-compact-profiles', 6, 2)]
jobs, launches = [], {}
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    fresh = '7166' in name or '7167' in name
    bind(c / 'manifest.json', own=fresh)
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    launches[name] = {}
    for wave in range(3 if width else 1):
        p = c / (f'wave{wave}-submission.json' if width else 'launch.json')
        launch = json.loads(p.read_text())
        bind(p, own=fresh)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        for rel, digest in launch.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
        tasks = range(width * wave, width * (wave + 1)) if width else range(count)
        for task in tasks: launches[name][task] = launch['job_id']
        jobs.append(launch['job_id'])
    if fresh:
        for p in c.iterdir():
            if p.is_file() and p.suffix in ('.json', '.tsv', '.sbatch'): bind(p, own=True)
        for p in (c / 'pins').iterdir():
            if p.suffix in ('.py', '.patch'): bind(p, own=True)
queue = subprocess.check_output(['squeue', '-h', '-r', '-u', 'hwu', '-o', '%i|%T|%j'], text=True)
accounting = subprocess.check_output(['sacct', '-n', '-X', '--array', '-j', ','.join(jobs),
    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'], text=True)
states = {parts[0]: parts[1].strip() for line in accounting.splitlines()
          if len(parts := line.split('|')) >= 2 and parts[0]}
for line in queue.splitlines():
    task, state, _ = line.split('|')
    states[task] = state
campaigns = {}
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name
    records = []
    for task in range(count):
        identity = f'{launches[name][task]}_{task}'
        assert identity in states, identity
        p = c / f'results/{task:03d}.json'
        final = p.exists()
        if not final: p = c / f'provisional/{task:03d}.json'
        r = json.loads(p.read_text()) if p.exists() else None
        state = classify(r, final, states[identity])
        assert state['status'] in ('pending_solver', 'pending_validation', 'checked_SAT', 'checked_UNSAT'), (identity, state)
        if r: bind(p, own=True)
        records.append(dict(task=task, scheduler_state=states[identity], classification=state))
    campaigns[name] = records
report = root / 'report-to-andrew/20260910-2228-vivify-revalidation'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', 'test_*.py']
            for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py'] for name in (
    'validate-vivify-revalidation-BC-7166', 'validate-vivify-revalidation-full50-7168',
    'validate-vivify-compact-profiles-7152')]
checks = {}
for command in commands:
    result = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=result.returncode, output=result.stdout + result.stderr)
    assert result.returncode == 0, (command, result.stdout, result.stderr)
with target.open('x') as out:
    json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        hashes=hashes, owned_paths=sorted(set(owned)), campaigns=campaigns, queue=queue,
        accounting=accounting, checks=checks, newly_deleted_bytes=deleted,
        decision='User-requested fresh complete validation before further integration commit. Identical7144 source/binaries; all fixed suites/controls/sanitizers/proofs repeated. Fresh BC expected outcomes permit full50 while proofs continue. No new solver edit, integration commit or denied push/transfer retry.',
        next_action='Read7168 for every available full50 outcome; classify all mismatches before scoring. Seal once all50 exact expected outcomes, never wait for independent proof completion to continue safe work. Full correctness/cleanup/current-source fuzz and memory/consolidation remain separate.'), out, indent=2)
print(target)
for name, records in campaigns.items():
    print(name, sum(r['classification']['checked'] for r in records), '/', len(records), 'checked-clean')

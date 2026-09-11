#!/usr/bin/env python3
"""Freeze exact full-set transfer, independent experiments and a scoped handoff."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-convergence-transfer-checkpoint-7214.json'
assert not target.exists(), 'successful checkpoint is frozen'
hashes, owned = {}, set()


def bind(p, expected=None, own=False):
    digest = sha(p)
    assert expected is None or digest == expected, p
    rel = str(p.relative_to(root))
    hashes[rel] = digest
    if own:
        owned.add(rel)


assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
prior = root / 'benchmarks/reports/2026-09-11-inprocessing-unit-cost-checkpoint-7203.json'
old = json.loads(prior.read_text()); bind(prior)
# Verify the inherited evidence without restaging its whole frozen history.
for rel in old['owned_paths']:
    if rel != 'PROGRESS.md':
        bind(root / rel, old['hashes'][rel])
receipt = root / 'benchmarks/reports/2026-09-11-sanitizer-raw-log-preservation-7204.json'
bind(receipt)
preserved = json.loads(receipt.read_text())
raw = root / preserved['source']
bind(raw, preserved['source_sha256'])
assert raw.read_bytes() == preserved['verbatim'].encode()
for rel in ('PROGRESS.md', 'DEVELOPMENT_PLAN.md', '.claude/skills/revamp/SKILL.md'):
    bind(root / rel, own=rel == 'PROGRESS.md')
for name in ('prepare-congruence-fused-runtime-7205', 'check-submit-congruence-fused-runtime-7205',
             'prepare-congruence-fused-profiles-7206', 'submit-congruence-fused-profile-wave-7206',
             'validate-congruence-fused-profiles-7206', 'extract-congruence-fused-wave-7207',
             'localize-dense-rebuild-pcs-7208', 'prepare-rank31-retry-producer-7210',
             'submit-rank31-retry-producer-7210', 'validate-rank31-retry-producer-7211',
             'prepare-factor-arena-transfer-controls-7212', 'check-submit-factor-arena-transfer-controls-7212',
             'summarize-factor-transfer-7213', 'audit-convergence-transfer-7214'):
    bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)
for rel in ('benchmarks/runs/20260911-7208-dense-rebuild-pc-localization/summary.json',
            'benchmarks/reports/2026-09-11-factor-arena-full50-7201/summary.json',
            'benchmarks/reports/2026-09-11-factor-transfer-context-7213.json'):
    p = root / rel; bind(p, own=True)
    for key, digest in json.loads(p.read_text())['hashes'].items():
        bind(root / key, digest)
bind(root / 'benchmarks/reports/2026-09-11-factor-arena-full50-7201/comparison.tsv', own=True)
bind(root / 'benchmarks/reports/2026-09-11-convergence-transfer-7214.md', own=True)
specs = [('20260910-7167-vivify-revalidation-ranks1-50', 50, None),
         ('20260910-7186-factor-arena-runtime', 18, 6),
         ('20260910-7187-factor-arena-profiles', 6, 2),
         ('20260911-7201-factor-arena-ranks1-50', 50, None),
         ('20260911-7202-congruence-fused-BC', 66, None),
         ('20260911-7205-congruence-fused-runtime', 18, 6),
         ('20260911-7210-rank31-retry-producer', 2, None),
         ('20260911-7212-factor-arena-transfer-controls', 18, 6)]
jobs, launches = [], {}
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text()); bind(c / 'manifest.json', own=True)
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, digest in m.get(group, {}).items(): bind(root / rel, digest)
    launches[name] = {}
    for wave in range(3 if width else 1):
        p = c / (f'wave{wave}-submission.json' if width else 'launch.json')
        if not p.exists():
            assert width and wave > 0, p
            continue
        launch = json.loads(p.read_text()); bind(p, own=True)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        for task in (range(width*wave, width*(wave+1)) if width else range(count)):
            launches[name][task] = launch['job_id']
        jobs.append(launch['job_id'])
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json', '.tsv', '.sbatch'): bind(p, own=True)
    for p in (c / 'pins').iterdir():
        if p.suffix in ('.py', '.patch'): bind(p, own=True)
# Scope scheduler reads to these benchmark jobs; fuzz belongs to Claude.
job_list = ','.join(jobs)
queue = subprocess.check_output(['squeue', '-h', '-r', '-j', job_list,
                                 '-o', '%i|%T|%j'], text=True)
accounting = subprocess.check_output(['sacct', '-n', '-X', '--array', '-j', job_list,
    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'], text=True)
states = {parts[0]: parts[1].strip() for line in accounting.splitlines()
          if len(parts := line.split('|')) >= 2 and parts[0]}
for line in queue.splitlines():
    task, state, _ = line.split('|'); states[task] = state
campaigns = {}
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name
    records = []
    for task, job in launches[name].items():
        identity = f'{job}_{task}'; assert identity in states, identity
        p = c / f'results/{task:03d}.json'; final = p.exists()
        if not final: p = c / f'provisional/{task:03d}.json'
        r = json.loads(p.read_text()) if p.exists() else None
        state = classify(r, final, states[identity])
        assert state['status'] in ('pending_solver', 'pending_validation', 'checked_SAT', 'checked_UNSAT'), (identity, state)
        if r: bind(p, own=True)
        records.append(dict(task=task, scheduler_state=states[identity], classification=state))
    campaigns[name] = records
report = root / 'report-to-andrew/20260911-0100-convergence-transfer'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', 'test_*.py']
            for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py', '--self-test'] for name in (
    'prepare-rank31-retry-producer-7210', 'validate-rank31-retry-producer-7211',
    'check-submit-congruence-fused-runtime-7205', 'check-submit-factor-arena-transfer-controls-7212',
    'summarize-factor-transfer-7213')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py'] for name in (
    'validate-vivify-revalidation-full50-7168', 'validate-factor-arena-full50-7201',
    'check-submit-congruence-fused-BC-7202', 'check-submit-congruence-fused-runtime-7205',
    'check-submit-factor-arena-transfer-controls-7212')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout+p.stderr)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, queue=queue, accounting=accounting,
        checks=checks, newly_deleted_bytes=0,
        previous_turn='The immediate bookkeeping recheck reconfirmed known evidence without changing the next action; this continuation revalidated live jobs and advanced the available solver gates.',
        decision='Arena7201 all50 exact;8103.859s=1.119470618x fixedKissat,31.800s slower than7167. Preserve all losses/tradeoffs; largest three regressions78.331s correlate with faults but source/system cause is unproven. Matched7212 repeats submitted. Rank31 initial admission355342 redirects to existing retry-producer diagnostic7210, not budget/tie tuning. Independent AND7205 advances after exact66BC without proof waits.',
        limitation='No aggregate arena-gain acceptance, production integration, new fuzz verdict, denied transfer/push retry or deadline invented. Raw sanitizer log remains unchanged/hash-bound local through7204 receipt. Memory/final certificates/consolidation and the full target remain open.',
        next_action='Validate each live campaign without rerunning successful writers. Advance7205 and7212 waves on exact expected solver outcomes; prepare7206 only after the first Rank35 pair, with reviewed addresses.7211 seals only after full7210 replay; inspect producer marks before any policy repair. Preserve established closures/fixed denominator and all50 scoring.'), out, indent=2)
print(target)
for name, rows in campaigns.items():
    print(name, sum(r['classification']['checked'] for r in rows), '/', len(rows), 'checked-clean')

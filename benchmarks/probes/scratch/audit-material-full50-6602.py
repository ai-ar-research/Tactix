#!/usr/bin/env python3
"""Freeze all-fifty evidence and live checks without touching checker scratch."""
import csv
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha, score

target = root / 'benchmarks/reports/2026-09-09-material-full50-audit-6602.json'
assert not target.exists(), 'Frozen audit exists'
prior_path = root / 'benchmarks/reports/2026-09-09-material-schedule-audit-6590.json'
prior = json.loads(prior_path.read_text())
ancestor_path = root / prior['prior_audit']['path']
assert sha(ancestor_path) == prior['prior_audit']['sha256']
ancestor = json.loads(ancestor_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md': assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'benchmarks/reports/2026-09-09-material-full50-outcome.md'}
scripts = ('prepare-material-full50-6594.py', 'submit-material-full50-6595.py',
    'summarize-material-full50-6596.py', 'summarize-material-waves-6597.py',
    'status-material-gates-6598.py', 'prepare-material-adverse-6599.py',
    'submit-material-adverse-6600.py', 'summarize-material-adverse-6601.py',
    'audit-material-full50-6602.py', 'land-material-full50-6603.py')
owned.update('benchmarks/probes/scratch/' + p for p in scripts)
bound = {}

def bind(path, digest=None, own=False):
    actual = sha(path)
    if digest is not None: assert actual == digest, path
    rel = str(path.relative_to(root)); bound[rel] = actual
    if own: owned.add(rel)
    return actual

def read(path, own=False):
    bind(path, own=own)
    return json.loads(path.read_text())

build = read(root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json')
for rel, digest in build['files'].items(): bind(root / build['source'] / rel, digest)
for rel, digest in build['binaries'].items(): bind(root / rel, digest)
full_path = root / 'benchmarks/reports/2026-09-09-material-full50-6596/summary.json'
full = read(full_path, True)
bind(full_path.with_name('comparison.tsv'), own=True)
assert full['score']['expected_solves'] == 50
assert full['changed_certificate_ranks'] == full['changed_non_timing_stat_ranks'] == [38, 50]
assert abs(full['groups']['all50']['candidate'] - 8401.066) < 1e-8
assert abs(full['groups']['all50']['kissat'] - 7239.010) < 1e-8
for rel, digest in full['hashes'].items(): bind(root / rel, digest)
runtime = read(root / 'benchmarks/runs/20260909-6592-rank50-material-three-wave-runtime/solver-checkpoint-6597.json', True)
assert runtime['solver_outcomes'] == runtime['checked_clean'] == 6
assert runtime['exact_per_arm_work_and_proof']
assert runtime['ranks']['50']['absolute_range_within_two_percent']
for rel, digest in runtime['hashes'].items(): bind(root / rel, digest)

accounting = {}; gates = {}
campaigns = {
    '20260909-6584-material-schedule-BC-gate': 66,
    '20260909-6592-rank50-material-three-wave-runtime': 6,
    '20260909-6594-material-ranks1-50': 50,
    '20260909-6599-material-adverse-rank38': 6,
}
for name, count in campaigns.items():
    c = root / 'benchmarks/runs' / name
    m = read(c / 'manifest.json', True)
    for rel, digest in m['pin_sha256'].items():
        bind(c / rel, digest, rel.endswith(('.py', '.patch', '.tsv', '.sbatch')))
    for rel, digest in m.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
    dependencies = m.get('dependencies', {})
    if isinstance(dependencies, list):
        for rel in dependencies:
            digest = prior['local_hash_bound'].get(rel, prior['owned_hashes'].get(rel,
                ancestor['local_hash_bound'].get(rel, ancestor['owned_hashes'].get(rel))))
            assert digest is not None, rel
            bind(root / rel, digest)
    else:
        for rel, digest in dependencies.items(): bind(root / rel, digest)
    for b in m.get('authorities', {}).values(): bind(root / b['path'], b['sha256'])
    for key in ('candidate_authority', 'fixed_reference', 'complete50_selection', 'provisional_gate', 'paired_timing'):
        b = m.get(key)
        if isinstance(b, dict): bind(root / b['path'], b['sha256'])
    for arms in m.get('work_oracles', {}).values():
        for b in arms.values(): bind(root / b['path'], b['sha256'])
    with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
    assert len(matrix) == count
    launches = [c / 'launch.json'] if (c / 'launch.json').exists() else sorted(c.glob('wave*-submission.json'))
    submitted = set()
    for p in launches:
        launch = read(p, True)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        tasks = launch.get('tasks', list(range(count)))
        assert not (submitted & set(tasks))
        submitted.update(tasks)
        command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S',
                   launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P']
        output = subprocess.check_output(command, text=True)
        lines = output.splitlines(); assert len(lines) == len(tasks), (p, output)
        for line in lines:
            fields = line.split('|')
            assert fields[1] in ('COMPLETED', 'RUNNING', 'COMPLETING', 'PENDING') and fields[2] == '0:0', line
        accounting[str(p.relative_to(root))] = dict(command=command, stdout=output, launch=launch)
    records = {}; statuses = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = read(p, True); task = r['task']; assert task == int(p.stem) and task in submitted
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            for key in ('arm', 'instance', 'expected'): assert r[key] == matrix[task][key], (p, key)
            if 'rank' in matrix[task]: assert r['rank'] == int(matrix[task]['rank'])
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            if task in records:
                for key in ('solve', 'certificate_sha256', 'certificate_bytes'):
                    assert r[key] == records[task][key], (p, key)
            if m.get('work_oracles'):
                b = m['work_oracles'][r['arm']][str(r['rank'])]
                oracle = read(root / b['path'])
                for key in ('verdict', 'certificate_sha256', 'certificate_bytes'): assert r[key] == oracle[key]
                nt = lambda v: [s for s in v['solver_statistics'] if not s.startswith('c timing')]
                assert nt(r) == nt(oracle)
            records[task] = r; statuses[task] = state
    if 'adverse' not in name: assert len(records) == count
    gates[name] = dict(submitted_tasks=sorted(submitted), solver_outcomes=len(records),
        checked_clean=sum(s['checked'] and s['clean'] for s in statuses.values()), statuses=statuses,
        seconds={t: r['solve']['wall_seconds'] for t, r in records.items()})
full_current = score(root / 'benchmarks/runs/20260909-6594-material-ranks1-50')
assert full_current['expected_solves'] == full_current['reference_checked'] == 50
assert abs(full_current['candidate_par2'] - 8401.066) < 1e-8

checks = {}
commands = [(name, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/' + name, '-p', 'test_*.py'])
            for name in ('bench', 'slurm')]
commands += [(name, ['python3', 'tools/docs/' + name + '.py']) for name in
    ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    result = subprocess.run(command, cwd=root, text=True, capture_output=True, check=True)
    checks[name] = dict(command=command, stdout=result.stdout, stderr=result.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-1635-material-full50-outcome'
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel: sha(root / rel) for rel in sorted(owned)}, local_hash_bound=bound,
    protected_campaign_hashes=prior['protected_campaign_hashes'], accounting=accounting,
    gates=gates, full50=full_current, checks=checks,
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    cleanup='No deletion this checkpoint; completed published records require zero residue. No live checker scratch inspected. All source/pins/references/protected campaigns unchanged.',
    limitations='Current-source fuzz unavailable; no promotion, FlatLists, fuzz access/transfer or push authorization. Full50/memory goal unmet. One premature adverse wave1 submission was rejected by the expected-outcome guard before sbatch; no duplicate job. Initial PDF overflow corrected before final two compiles. First audit attempt stopped before output because the B/C manifest uses list dependencies, now bound through prior audit hashes; no measurement rerun.')
with target.open('x') as stream: json.dump(out, stream, indent=2)
print(target)

#!/usr/bin/env python3
"""Audit replicated adversity, new diagnostics and pending independent checks."""
import csv
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha, score
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-material-followup-audit-6609.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-material-full50-audit-6602.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md': assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'tools/bench/decision_prefix.py', 'tools/bench/test_decision_prefix.py',
         'benchmarks/reports/2026-09-09-material-adverse-and-rank31.md'}
scripts = ('annotate-propagation-6604.py', 'prepare-material-rank31-6605.py',
    'submit-material-rank31-6606.py', 'validate-material-rank31-6607.py',
    'summarize-propagation-6608.py', 'audit-material-followup-6609.py', 'land-material-followup-6610.py',
    'prepare-material-rank31-early-6611.py', 'submit-material-rank31-early-6612.py',
    'validate-material-rank31-early-6613.py')
owned.update('benchmarks/probes/scratch/' + p for p in scripts)
bound = {}; accounting = {}; gates = {}
def bind(p, digest=None, own=False):
    actual = sha(p)
    if digest is not None: assert actual == digest, p
    rel = str(p.relative_to(root)); bound[rel] = actual
    if own: owned.add(rel)
    return actual
def read(p, own=False):
    bind(p, own=own); return json.loads(p.read_text())
def account(p, count):
    launch = read(p, True)
    assert launch['manifest_sha256'] == sha(p.parent / 'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S',
        launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    out = subprocess.check_output(command, text=True); assert len(out.splitlines()) == count
    for line in out.splitlines():
        fields = line.split('|')
        assert fields[1] in ('RUNNING', 'COMPLETING', 'COMPLETED', 'PENDING') and fields[2] == '0:0', line
    accounting[str(p.relative_to(root))] = dict(command=command, stdout=out)
build = read(root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json')
for rel, digest in build['files'].items(): bind(root / build['source'] / rel, digest)
for rel, digest in build['binaries'].items(): bind(root / rel, digest)
for name, count in (('6594-material-ranks1-50', 50), ('6599-material-adverse-rank38', 6)):
    c = root / 'benchmarks/runs' / ('20260909-' + name); m = read(c / 'manifest.json')
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
    assert len(matrix) == count
    states = {}; records = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = read(p, True); t = r['task']; assert int(p.stem) == t and 0 <= t < count
            status = classify(r, folder == 'results')
            assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, status)
            for key in ('instance', 'arm', 'expected'): assert r[key] == matrix[t][key]
            assert r['rank'] == int(matrix[t]['rank'])
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            if t in records:
                for key in ('solve', 'certificate_sha256', 'certificate_bytes'): assert r[key] == records[t][key]
            if m.get('work_oracles'):
                b = m['work_oracles'][r['arm']][str(r['rank'])]; bind(root / b['path'], b['sha256'])
                oracle = read(root / b['path'])
                for key in ('verdict', 'certificate_sha256', 'certificate_bytes'): assert r[key] == oracle[key]
                nt = lambda v: [x for x in v['solver_statistics'] if not x.startswith('c timing')]
                assert nt(r) == nt(oracle)
            records[t] = r; states[t] = status
    assert len(records) == count
    gates[name] = dict(outcomes=count, checked_clean=sum(s['checked'] and s['clean'] for s in states.values()), statuses=states)
    launches = [c / 'launch.json'] if (c / 'launch.json').exists() else sorted(c.glob('wave*-submission.json'))
    for p in launches: account(p, count if p.name == 'launch.json' else 2)
    if 'adverse' in name:
        s = read(c / 'solver-checkpoint-6601.json', True)
        assert s['solver_outcomes'] == 6 and s['exact_per_arm_work_and_proof']
        for rel, digest in s['hashes'].items(): bind(root / rel, digest)
        assert s['ranks']['38']['absolute_range_within_two_percent']
annotation = root / 'benchmarks/runs/20260909-6604-rank32-propagation-annotation'
am = read(annotation / 'manifest.json', True); a = read(annotation / 'summary-6608.json', True)
for rel, digest in am['authorities'].items(): bind(root / am['parent'] / rel, digest)
for rel, digest in a['hashes'].items(): bind(root / rel, digest)
for p in annotation.iterdir():
    assert p.is_file(), 'Temporary profile expansion remains'
    bind(p, own=True)
assert am['temporary_expansions_deleted_bytes'] == 1010452800
diagnostics = {}
for name, summary_name in (('6605-material-rank31-prefix', 'summary-6607.json'),
                           ('6611-material-rank31-early', 'summary-6613.json')):
    c = root / 'benchmarks/runs' / ('20260909-' + name); m = read(c / 'manifest.json', True)
    for rel, digest in m['hashes'].items(): bind(c / rel, digest, rel.endswith(('.py', '.sbatch')))
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, digest in m[group].items(): bind(root / rel, digest)
    account(c / 'launch.json', 1)
    p = c / 'results/candidate.json'
    if p.exists():
        r = read(p, True); assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes'] > 0
        assert r['binary_sha256'] == sha(c / 'pins/candidate')
        for rel, digest in r['trace_sha256'].items(): bind(c / 'traces' / rel, digest)
        for log in (c / 'logs').iterdir(): bind(log, own=True)
        assert (c / summary_name).exists(), 'Validate the completed diagnostic before sealing audit'
        s = read(c / summary_name, True)
        for rel, digest in s['hashes'].items(): bind(root / rel, digest)
        diagnostics[name] = s.get('status', s.get('decisions'))
    else: diagnostics[name] = 'Running; no published result'
checks = {}
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/' + n, '-p', 'test_*.py']) for n in ('bench', 'slurm')]
commands += [(n, ['python3', 'tools/docs/' + n + '.py']) for n in
    ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    run = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=run.stdout, stderr=run.stderr); print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-1655-material-followup'
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel: sha(root / rel) for rel in sorted(owned)}, local_hash_bound=bound,
    protected_campaign_hashes=prior['protected_campaign_hashes'], accounting=accounting, gates=gates,
    diagnostics=diagnostics, checks=checks, full50=score(root / 'benchmarks/runs/20260909-6594-material-ranks1-50'),
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    cleanup='Only1010452800 bytes of owned temporary cached-profile expansions deleted; compressed originals retained. No live checker scratch inspected or deleted. Completed records require cleanup.',
    limitations='No solver source edit/promotion. Rank31 earlier prefix changed is localization evidence, not invalidity. Source fuzz unavailable; FlatLists approval follow-up unanswered at preparation. No fuzz access/transfer or push retry. First6608 replacement-count assertion stopped before output; corrected exact two occurrences. Initial audit stopped before output because early6611 finished and required validation;6613 now supplies it. PDF overflow corrected before final two compiles. Goal active/unmet.')
with target.open('x') as stream: json.dump(out, stream, indent=2)
print(target)

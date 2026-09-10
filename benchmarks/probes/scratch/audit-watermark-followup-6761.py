#!/usr/bin/env python3
"""Bind the full50 score, adverse repetitions and next causal diagnostics."""
import csv
import datetime as dt
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-09-watermark-followup-audit-6761.json'
assert not target.exists()
prior_path = root/'benchmarks/reports/2026-09-09-reduce-watermark-audit-6747.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root/rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'benchmarks/reports/2026-09-09-watermark-full50-and-next-gaps.md'}
for p in (root/'benchmarks/probes/scratch').iterdir():
    match = re.search(r'-(\d+)\.(py|cpp)$', p.name)
    if match and 6750 <= int(match[1]) <= 6762:
        owned.add(str(p.relative_to(root)))
def own(p):
    owned.add(str(p.relative_to(root)))
def bindings(hashes):
    for rel, digest in hashes.items():
        assert sha(root/rel) == digest, rel
evidence = {}
def summary(p):
    s = json.loads(p.read_text())
    bindings(s.get('hashes', {}))
    own(p)
    evidence[str(p.relative_to(root))] = sha(p)
    return s
view = root/'benchmarks/reports/2026-09-09-reduce-watermark-full50-6750'
s = summary(view/'summary.json')
own(view/'comparison.tsv')
assert s['score']['expected_solves'] == 50 and len(s['rows']) == 50
assert [r['rank'] for r in s['rows'] if not r['non_timing_stats_exact_parent']] == [46]
assert [r['rank'] for r in s['rows'] if not r['certificate_exact_parent']] == [46]
summary(root/'benchmarks/runs/20260909-6760-watermark-layout/summary.json')
for p in (root/'benchmarks/runs/20260909-6760-watermark-layout').iterdir():
    if p.suffix in ('.out', '.log'):
        own(p)
accounting = {}
def account(c, launch, count):
    assert launch['manifest_sha256'] == sha(c/'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
        '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    stdout = subprocess.check_output(command, text=True)
    rows = stdout.splitlines()
    assert len(rows) == count, (c, stdout)
    for row in rows:
        cells = row.split('|')
        assert cells[1] in ('PENDING', 'RUNNING', 'COMPLETING', 'COMPLETED') and cells[2] == '0:0', row
        assert cells[3] >= launch['accounting_since']
    accounting[launch['job_id']] = dict(campaign=str(c.relative_to(root)), command=command, stdout=stdout)
diagnostics = {}
for name, filename in (('20260909-6751-rank21-long-prefix', 'summary-6753.json'),
                       ('20260909-6757-rank21-search-window', 'summary-6759.json')):
    c = root/'benchmarks/runs'/name
    m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m['hashes'].items():
        assert sha(c/rel) == digest, rel
    bindings(m['dependencies'])
    for n in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch'):
        own(c/n)
    account(c, json.loads((c/'launch.json').read_text()), 2)
    records = list((c/'results').glob('*.json'))
    assert len(records) <= 2
    for p in records:
        own(p)
        r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        assert r['binary_sha256'] == m['hashes']['pins/'+r['arm']]
        for rel, digest in r['trace_sha256'].items():
            assert sha(c/'traces'/rel) == digest
    if (c/filename).exists():
        summary(c/filename)
    diagnostics[name] = dict(published=len(records), validated_summary=(c/filename).exists())
states = {}
for name, count in (
    ('20260909-6666-congruence-empty-ranks1-50', 50),
    ('20260909-6737-reduce-watermark-BC-gate', 66),
    ('20260909-6741-rank46-watermark-runtime', 6),
    ('20260909-6743-reduce-watermark-ranks1-50', 50),
    ('20260909-6754-rank38-watermark-cost-runtime', 6)):
    c = root/'benchmarks/runs'/name
    m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m.get('pin_sha256', {}).items():
        assert sha(c/rel) == digest, rel
    for n in ('manifest.json', 'launch.json', 'array.sbatch', 'matrix.tsv'):
        if (c/n).exists():
            own(c/n)
    if count == 6:
        launches = sorted(c.glob('wave*-submission.json'))
        assert len(launches) == 3
        for p in launches:
            own(p)
            launch = json.loads(p.read_text())
            bindings(launch['preceding_records'])
            account(c, launch, 2)
    else:
        account(c, json.loads((c/'launch.json').read_text()), count)
    matrix = list(csv.DictReader((c/'matrix.tsv').open(), delimiter='\t'))
    current = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c/folder).glob('*.json')):
            own(p)
            r = json.loads(p.read_text())
            status = classify(r, folder == 'results')
            assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, status)
            assert r['task'] == int(p.stem)
            row = matrix[r['task']]
            for key in ('arm', 'instance', 'expected'):
                assert r[key] == row[key], (p, key)
            assert r['rank'] == int(row['rank'])
            if folder == 'results':
                assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            current[r['task']] = dict(rank=r['rank'], arm=r['arm'], status=status, seconds=r['solve']['wall_seconds'])
    assert len(current) == count, (name, len(current))
    states[name] = current
    if name.endswith('watermark-cost-runtime'):
        summary(c/'solver-checkpoint-6756.json')
for name in ('6654-congruence-empty-gates', '6730-reduce-watermark'):
    p = root/f'benchmarks/runs/20260909-{name}-build/manifest.json'
    m = json.loads(p.read_text())
    for rel, digest in m['files'].items():
        assert sha(root/m['source']/rel) == digest, rel
    bindings(m['binaries'])
checks = {}
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/'+n, '-p', 'test_*.py']) for n in ('bench', 'slurm')]
commands += [(n, ['python3', 'tools/docs/'+n+'.py']) for n in ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=r.stdout, stderr=r.stderr)
    print(name, 'PASS', flush=True)
report = root/'report-to-andrew/20260909-2215-full50-next-gaps'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], evidence=evidence,
    accounting=accounting, campaign_states=states, diagnostics=diagnostics, checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex', 'report.pdf')},
    cleanup='No manual deletion. Published clean records confirm task-owned cleanup; active checker scratch is untouched. Hash-bound diagnostic streams, native control evidence and fixed layout outputs remain available. No fuzz inspection.',
    limitations='6730 remains unpromoted/unfuzzed and production unchanged. Aggregate goal unmet; retain adverse exact-work timing. Rank21 first split is localization, not proof of a cause. Compiled layout offsets motivate but do not prove a layout-cost attribution.6754 inherits a stale generic66-result validation sentence; its explicit6-task matrix/acceptance and6756 enforce six outcomes with identical shared work/certificates. No baseline refresh, FlatLists rewrite, denied fuzz/transfer/push retry. Deadline05:45 winddown/06:00EDT cutoff; Andrew local/unsent.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(target)
for name, rows in states.items():
    print(name, len(rows), 'published', sum(r['status']['checked'] and r['status']['clean'] for r in rows.values()), 'checked/clean')

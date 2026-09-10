#!/usr/bin/env python3
"""Audit the isolated layout ablation, rejected observer and live successors."""
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
target = root/'benchmarks/reports/2026-09-09-watermark-layout-audit-6779.json'
assert not target.exists()
prior_path = root/'benchmarks/reports/2026-09-09-watermark-followup-audit-6761.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root/rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'tools/bench/test_search_window_truncation.py',
         'benchmarks/reports/2026-09-09-watermark-layout-ablation.md'}
for p in (root/'benchmarks/probes/scratch').iterdir():
    match = re.search(r'-(\d+)\.py$', p.name)
    if match and 6763 <= int(match[1]) <= 6784:
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
for rel in (
    '20260909-6765-watermark-layout-control/summary.json',
    '20260909-6769-watermark-layout-proof-control/summary.json',
    '20260909-6757-rank21-search-window/classification-6773.json',
    '20260909-6774-rank21-light-window/classification-6781.json',
    '20260909-6777-watermark-propagate-code/summary.json',
    '20260909-6777-watermark-propagate-code/relocations-6778.json'):
    summary(root/'benchmarks/runs'/rel)
for name in ('20260909-6765-watermark-layout-control', '20260909-6777-watermark-propagate-code'):
    for p in (root/'benchmarks/runs'/name).iterdir():
        if p.suffix in ('.asm', '.out', '.log'):
            own(p)
c = root/'benchmarks/runs/20260909-6769-watermark-layout-proof-control'
for p in (c/'fixed-proofs').iterdir():
    own(p)
own(c/'logs/root-lock.log')
# Raw independent-checker logs remain hash-bound in6769 and retained locally;
# do not rewrite their emitted trailing spaces or weaken the Git whitespace gate.
builds = {}
for name in ('6730-reduce-watermark', '6763-watermark-layout'):
    c = root/f'benchmarks/runs/20260909-{name}-build'
    m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m['files'].items():
        assert sha(root/m['source']/rel) == digest, rel
    bindings(m['binaries'])
    for rel, digest in m['patches'].items():
        assert sha(c/rel) == digest
    builds[str((c/'manifest.json').relative_to(root))] = sha(c/'manifest.json')
    if name.startswith('6763'):
        for filename in ('manifest.json', 'parent.json', 'incremental.patch', 'cumulative.patch'):
            own(c/filename)
        for rel, digest in m['logs'].items():
            assert sha(c/rel) == digest
            own(c/rel)
accounting = {}
def account(c, launch, count, failed_tasks=()):
    assert launch['manifest_sha256'] == sha(c/'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
        '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    stdout = subprocess.check_output(command, text=True)
    rows = stdout.splitlines()
    assert len(rows) == count, (c, stdout)
    for row in rows:
        cells = row.split('|')
        if any(cells[0].endswith('_'+str(task)) for task in failed_tasks):
            assert cells[1:3] == ['FAILED', '1:0'], row
        else:
            assert cells[1] in ('PENDING', 'RUNNING', 'COMPLETING', 'COMPLETED') and cells[2] == '0:0', row
        assert cells[3] >= launch['accounting_since']
    accounting[launch['job_id']] = dict(campaign=str(c.relative_to(root)), command=command, stdout=stdout)
diagnostics = {}
for name, filename, failed in (
    ('20260909-6757-rank21-search-window', 'classification-6773.json', 'timeout'),
    ('20260909-6774-rank21-light-window', 'classification-6781.json', 'endpoint'),
    ('20260909-6782-rank21-trajectory-stop', 'summary-6784.json', None)):
    c = root/'benchmarks/runs'/name
    m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m['hashes'].items():
        assert sha(c/rel) == digest, rel
    bindings(m['dependencies'])
    for n in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch'):
        own(c/n)
    account(c, json.loads((c/'launch.json').read_text()), 2,
        (0,) if failed == 'timeout' else (0, 1) if failed == 'endpoint' else ())
    records = list((c/'results').glob('*.json'))
    for p in records:
        own(p)
        r = json.loads(p.read_text())
        expected_exit = (124 if failed == 'timeout' and r['arm'] == 'candidate'
                         else -15 if failed == 'endpoint' and r['arm'] == 'kissat' else 0)
        assert r['exit_code'] == expected_exit and r['scratch_deleted']
        assert r['binary_sha256'] == m['hashes']['pins/'+r['arm']]
        for rel, digest in r['trace_sha256'].items():
            assert sha(c/'traces'/rel) == digest
    if (c/filename).exists():
        summary(c/filename)
    diagnostics[name] = dict(published=len(records), summary_present=(c/filename).exists(),
        classified_observer_failure=failed)
states = {}
for name, count in (
    ('20260909-6743-reduce-watermark-ranks1-50', 50),
    ('20260909-6754-rank38-watermark-cost-runtime', 6),
    ('20260909-6766-watermark-layout-BC-gate', 66),
    ('20260909-6770-watermark-layout-runtime', 12)):
    c = root/'benchmarks/runs'/name
    m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m.get('pin_sha256', {}).items():
        assert sha(c/rel) == digest, rel
    for n in ('manifest.json', 'launch.json', 'array.sbatch', 'matrix.tsv'):
        if (c/n).exists():
            own(c/n)
    if count in (6, 12):
        launches = sorted(c.glob('wave*-submission.json'))
        assert len(launches) == 3
        for p in launches:
            own(p)
            launch = json.loads(p.read_text())
            bindings(launch['preceding_records'])
            account(c, launch, count//3)
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
    for filename in ('summary-6768.json', 'solver-checkpoint-6772.json'):
        if (c/filename).exists():
            summary(c/filename)
checks = {}
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/'+n, '-p', 'test_*.py']) for n in ('bench', 'slurm')]
commands += [(n, ['python3', 'tools/docs/'+n+'.py']) for n in ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=r.stdout, stderr=r.stderr)
    print(name, 'PASS', flush=True)
report = root/'report-to-andrew/20260909-2255-watermark-layout'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], evidence=evidence, build_evidence=builds,
    accounting=accounting, campaign_states=states, diagnostics=diagnostics, checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex', 'report.pdf')},
    cleanup='No manual deletion. All terminal diagnostic scratch deleted by owned runners; published checked-clean records audited. Active checker scratch, unique traces, fixed proof witnesses and raw checker logs retained untouched. No fuzz access.',
    limitations='Production unchanged;6763 is isolated unfuzzed layout ablation, neutral and not adopted. Historical watermark6730 remains the semantic baseline.6757 timeout/native cap and6774 competing-stop failures classified;6782 must pass strict6784 full replay before interpretation. Static code placement is not runtime causality. Full50, memory/fuzz and pending certificates remain open. No reference refresh, FlatLists or denied transfer/push retry. Winddown05:45/cutoff06:00EDT; Andrew local/unsent.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(target)
for name, rows in states.items():
    print(name, len(rows), 'published', sum(r['status']['checked'] and r['status']['clean'] for r in rows.values()), 'checked/clean')

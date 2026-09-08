#!/usr/bin/env python3
"""Audit owned evidence and freeze a checkpoint without waiting on proofs."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
def run(args):
    return subprocess.run(args, cwd=root, capture_output=True, text=True, check=True).stdout
jobs = {5395:1669252, 5411:1669628, 5412:None, 5413:1669643, 5414:None,
        5415:1669667, 5416:1669710, 5417:None, 5418:1669808,
        5421:1669878, 5422:1669812, 5423:1669887, 5424:1669919}
queue = run(['squeue', '-u', 'hwu', '-h', '-o', '%i %T %j']).splitlines()
for line in queue:
    base = int(line.split()[0].split('_')[0])
    assert base in set(jobs.values()) | {1668242}, ('Unmapped active job', line)
campaigns = []
for number, job in jobs.items():
    c = next((root / 'benchmarks/runs').glob(f'20260907-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m.get('hashes', m.get('pin_sha256', {})).items(): assert sha(c / rel) == digest
    for rel, digest in m.get('binaries', {}).items(): assert sha(root / rel) == digest
    for rel, digest in m.get('logs', {}).items(): assert sha(c / 'logs' / rel) == digest
    for rel, digest in m.get('patches', {}).items(): assert sha(c / rel) == digest
    for rel, digest in m.get('files', {}).items(): assert sha(root / m['source'] / rel) == digest
    for arm, spec in m.get('sources', {}).items():
        files = spec['files'] if 'files' in spec else {spec['file']: spec['sha256']}
        for rel, digest in files.items(): assert sha(root / spec['source'] / rel) == digest
        assert sha(c / (arm + '.patch')) == spec['patch_sha256']
    records = sorted((c / 'results').glob('*.json'))
    provisional = sorted((c / 'provisional').glob('*.json'))
    classifications = {}
    for p in records:
        r = json.loads(p.read_text())
        if number in (5395, 5421, 5422, 5423):
            assert r['valid'] and r['expected_match'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            check = r['model_check'] if r['solve']['exit_code'] == 10 else r['proof_check']
            assert check['exit_code'] == 0 and not check['timed_out']
            classifications[p.name] = 'checked-clean'
        else:
            assert r['scratch_deleted']
            if number == 5411:
                assert 'exit_code' not in r
                classifications[p.name] = 'deliberate-cancellation-no-solver-result'
                run(['ssh', r['node'], 'test', '!', '-e', r['scratch']])
            elif number == 5413:
                assert r['exit_code'] == 124
                classifications[p.name] = '600-second-timeout-no-accepted-result'
            else:
                assert r['exit_code'] == 0
                classifications[p.name] = 'bounded-diagnostic-stop-not-checked-solve'
            for name, digest in r.get('trace_sha256', {}).items(): assert sha(c / 'traces' / name) == digest
    for p in provisional:
        r = json.loads(p.read_text())
        assert r['expected_match'] and not r['solve']['timed_out'] and r['solve']['exit_code'] in (10, 20)
    residue = [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and p.suffix in ('.cnf', '.drat', '.lrat', '.frat', '.grat')]
    assert not residue, residue
    state = run(['sacct', '-j', str(job), '-X', '--format=JobID,State,ExitCode', '--parsable2']).splitlines() if job else None
    campaigns.append(dict(campaign=str(c.relative_to(root)), job=job, slurm_state=state,
        manifest_sha256=sha(c / 'manifest.json'), final_records=len(records),
        result_sha256={p.name:sha(p) for p in records}, result_classification=classifications,
        provisional_sha256={p.name:sha(p) for p in provisional},
        summary_sha256={p.name:sha(p) for p in c.glob('*summary.json')},
        checkpoint_sha256={p.name:sha(p) for p in c.glob('*checkpoint.json')},
        repository_residue=residue,
        retention='Submitted immutable source/pins/runners or unique evidence; preserve. Active node-local checker scratch remains job-owned.'))
checks = {}
for name in ('check-doc-size', 'check-progress', 'check-section-refs', 'check-option-coverage', 'check-rule-policy-boundary'):
    checks[name] = run(['python3', str(root / 'tools/docs' / (name + '.py'))])
source_log = root / 'build/release/Testing/Temporary/LastTest.log'
assert source_log.read_text().count('Test Passed.') == 77
testlog = root / 'benchmarks/reports/2026-09-07-rephase-root-main-ctest.log'
assert not testlog.exists()
shutil.copy2(source_log, testlog)
report = root / 'report-to-andrew/20260907-2025-rephase-root'
assert 'Overfull' not in (report / 'report.log').read_text()
for n in (1,2): assert 'Output written on report.pdf' in (report / f'compile-{n}.log').read_text()
assert not run(['git', 'diff', '--', 'src', 'tests']), 'Production unexpectedly changed'
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    queue=queue, campaigns=campaigns, checks=checks,
    progress_lines=len((root / 'PROGRESS.md').read_text().splitlines()),
    main_ctest=dict(passed=77, seconds=6.48, log=str(testlog.relative_to(root)), sha256=sha(testlog)),
    candidate_test_authority='benchmarks/runs/20260907-5417-rephase-root/manifest.json',
    unchanged_production_source=True, candidate_status='isolated UNFUZZED policy repair, not promoted',
    additional_manual_removed_bytes=0, known_5413_automatic_removed_bytes=86366503,
    uncertain_deletions=[],
    andrew_report=dict(directory=str(report.relative_to(root)), final_compilations=2,
        tex_sha256=sha(report / 'report.tex'), pdf_sha256=sha(report / 'report.pdf')),
    scope='Normal runtime, fixed tests and bounded diagnostics only; no new fuzz campaign or artifact inspected. Prior push rejection remains in force.')
dest = root / 'benchmarks/reports/2026-09-07-rephase-root-validation.json'
assert not dest.exists()
dest.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: source/pin identities, completed results, cancellation cleanup, docs and twice-compiled report; pending checks preserved')

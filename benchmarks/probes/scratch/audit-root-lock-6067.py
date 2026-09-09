#!/usr/bin/env python3
"""Fresh repair checkpoint; inspect only named normal campaigns, never fuzz."""
from collections import Counter
import datetime as dt
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-root-reason-repair-validation.json'
assert not target.exists()

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

def run_dir(number):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches) == 1, number
    return matches[0]

hashes = {}
def bind(path):
    hashes[str(path.relative_to(root))] = sha(path)

previous = root / 'benchmarks/reports/2026-09-08-retention-origin-validation.json'
bind(previous)
old = json.loads(previous.read_text())
known_status = {r['path']: r['sha256'] for c in old['campaigns']
                for r in c['unscored'] if r.get('reason') != 'checker_timeout'}
assert len(known_status) == 4
for rel, digest in known_status.items():
    assert sha(root / rel) == digest, rel
sp = importlib.util.spec_from_file_location('timeouts', Path(__file__).with_name('classify-checker-timeout-5981.py'))
mod = importlib.util.module_from_spec(sp)
sp.loader.exec_module(mod)
mod.fixtures()

campaigns = []
for number in (5784, 5855, 5858, 5878, 5880, 5906, 5908, 5924, 5926, 5968, 5970, 6059, 6061):
    c = run_dir(number)
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, (number, rel)
    for name in ('manifest.json', 'launch.json', 'array.sbatch', 'matrix.tsv'):
        bind(c / name)
    counts = Counter()
    unscored = []
    for p in (c / 'results').glob('*.json'):
        r = json.loads(p.read_text())
        assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending'], p
        assert r['binary_sha256'] == m['arms'][r['arm']]['sha256'], p
        if r['valid']:
            assert r['expected_match'] and r['solve']['exit_code'] in (10, 20), p
            counts['checked_valid'] += 1
        elif r['solve']['timed_out']:
            counts['solver_timeout'] += 1
        elif r.get('proof_check', {}).get('timed_out'):
            assert mod.checker_timeout(r, m['resource_contract']['checker_timeout']), p
            counts['checker_timeout_unverified'] += 1
            unscored.append(dict(path=str(p.relative_to(root)), sha256=sha(p), reason='checker_timeout'))
        else:
            rel = str(p.relative_to(root))
            assert number == 5784 and rel in known_status and sha(p) == known_status[rel], p
            counts['checker_status_mismatch_unscored'] += 1
            unscored.append(dict(path=rel, sha256=sha(p), reason='frozen_checker_status_case'))
        if number in (6059, 6061):
            bind(p)
    provisional = list((c / 'provisional').glob('*.json'))
    for p in provisional:
        r = json.loads(p.read_text())
        assert r['solve']['timed_out'] or (r['expected_match'] and r['solve']['exit_code'] in (10, 20)), p
        if number in (6059, 6061):
            bind(p)
    campaigns.append(dict(number=number, counts=dict(counts), provisional=len(provisional), unscored=unscored))

c = run_dir(6049)
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['files'].items():
    assert sha(root / m['source'] / rel) == digest, rel
for rel, digest in m['binaries'].items():
    assert sha(root / rel) == digest, rel
for name in ('manifest.json', 'complete-proof-validation.json', 'final-controls/summary.json', 'controls/summary.json'):
    p = c / name
    bind(p)
    for rel, digest in json.loads(p.read_text()).get('hashes', {}).items():
        assert sha(root / rel) == digest, rel
for rel, digest in m['logs'].items():
    assert sha(c / 'logs' / rel) == digest, rel
assert json.loads((c / 'complete-proof-validation.json').read_text())['checked_proofs'] == 62

c = run_dir(6060)
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['diagnostic_source_hashes'].items():
    assert sha(root / m['diagnostic_source'] / rel) == digest, rel
v = json.loads((c / 'summary.json').read_text())
assert v['suffix_comparison']['complete_equal'] and v['matching_decisions'] == 69828932
r = json.loads((c / 'reduce-validation.json').read_text())
assert r['all_reductions_match'] and r['reductions'] == 55
for name in ('manifest.json', 'launch.json', 'results/candidate.json', 'summary.json', 'reduce-validation.json'):
    bind(c / name)

checks = {}
commands = [(name, ['python3', f'tools/docs/{name}.py']) for name in
            ('check-doc-size', 'check-section-refs', 'check-option-coverage', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory')]
commands += [('benchmark-unit-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
             ('reduce-comparator-tests', ['python3', 'tests/test_compare_reduce_trace.py']),
             ('history-comparator-tests', ['python3', 'benchmarks/probes/scratch/validate-retention-history-6044.py', '--self-test'])]
for label, command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert p.returncode == 0, (label, p.stdout, p.stderr)
    checks[label] = p.stdout + p.stderr
jobs = '1673402,1676660,1676661,1676727'
queue = subprocess.run(['squeue', '-h', '-j', jobs, '-o', '%i|%P|%T|%C|%m|%R'], capture_output=True, text=True, check=True).stdout
terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1676660,1676661,1676727', '-o', 'JobID,State,ExitCode', '-P'], capture_output=True, text=True, check=True).stdout
for rel in ('PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md', 'docs/proof-rules.html',
            'benchmarks/reports/2026-09-08-root-reason-repair.md'):
    bind(root / rel)
target.write_text(json.dumps(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), campaigns=campaigns,
    checks=checks, hashes=hashes, queue=queue, terminal=terminal,
    outcome='General6049 root-reason repair:62 checked fixed proofs,four suites,18 old/new cases,3 native cases;6054 observer98/98.6060 closes55 reductions and69828932 decisions through300K. BC66 expected;full6061 submitted.',
    test_consideration='New solver and observer tests cover changed root/non-root/pending/GC/proof paths. Read-only audit reuses tested timeout classification, exact prior status-case hashes, nine existing validator suites and fail-closed pin/result assertions; no acceptance rule changed. Initial pre-publication audit stopped at a manifest-key assumption; corrected to actual diagnostic_source_hashes/diagnostic_source before sealing any output.',
    cleanup='No manual deletion. Published normal task records assert certificate/derivative cleanup. Preserve live checker scratch, all cited authorities, pins and full traces. No fuzz-owner artifacts inspected.',
    limitations='Goal not established. Unfuzzed isolated source,not promoted;denied fuzz transfer and push not retried. Status mismatches and checker timeouts remain invalid/unscored.'), indent=2) + '\n')
print(json.dumps(campaigns, indent=2))

#!/usr/bin/env python3
"""Audit normal evidence only; never inspect fuzz-owner artifacts or live logs."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-rank32-forward-checkpoint-validation.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
hashes = {}
def bind(p): hashes[str(p.relative_to(root))] = sha(p)
old_hashes = {}
for label in ('forward-unit-exit', 'forward-exit-owner-followup'):
    p = root / f'benchmarks/reports/2026-09-08-{label}-validation.json'
    old_hashes.update(json.loads(p.read_text())['hashes'])
    bind(p)
mutable = {'PROGRESS.md', 'tools/bench/fixed_runtime.py', 'tools/bench/test_fixed_runtime.py'}
observed_live_log = 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended/logs/kissat.out'
superseded = {}
for rel, digest in old_hashes.items():
    actual = sha(root / rel)
    if actual != digest:
        assert rel in mutable or rel == observed_live_log, rel
        superseded[rel] = dict(previous_sha256=digest, current_sha256=actual,
            reason='Living source/status updated with tested successor' if rel in mutable else
                   'Earlier audit observed a still-running log;now terminal,not an immutable launch input. Original audit retained.')
    hashes[rel] = actual
campaigns = []
for number in (5678, 5712, 5727, 5754, 5758, 5771, 5779, 5782, 5783, 5786, 5787, 5791, 5794):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches) == 1
    c = matches[0]
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m.get('pin_sha256', m.get('hashes', {})).items():
        assert sha(c / rel) == digest, (number, rel)
        if (c / rel).suffix in ('.py', '.patch', '.sbatch', '.tsv'): bind(c / rel)
    for p in c.glob('*.json'): bind(p)
    results = list((c / 'results').glob('*.json'))
    for p in results:
        r = json.loads(p.read_text())
        if 'solve' in r:
            assert r['valid'] and r['expected_match'] and not r['solve']['timed_out'], p
            assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending'], p
        else:
            assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes'], p
            for rel, digest in r['trace_sha256'].items():
                trace = c / 'traces' / rel
                # Detailed complete streams already validated by the named comparators.
                # Their result hashes remain authority; keep bulky raw traces local.
                if trace.stat().st_size < 5 << 20:
                    assert sha(trace) == digest, (number, rel)
                    bind(trace)
            log = c / 'logs' / (r['arm'] + '.out')
            if log.exists(): bind(log)  # only an arm with a terminal result
        bind(p)
    for p in (c / 'selector-fixtures').glob('*'): bind(p)
    campaigns.append(dict(campaign=str(c.relative_to(root)), final_records=len(results)))
for rel in ('PROGRESS.md', 'docs/scheduling-parity-ledger.md', 'tools/bench/fixed_runtime.py',
            'tools/bench/test_fixed_runtime.py', 'tools/bench/decision_prefix.py', 'tools/bench/test_decision_prefix.py',
            'tools/bench/runtime_task.py', 'tools/bench/test_runtime_task.py',
            'benchmarks/reports/2026-09-08-rank32-forward-checkpoint.md'):
    bind(root / rel)
for number in range(5754, 5797):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'):
        if p.suffix in ('.py', '.txt', '.cpp', '.c'): bind(p)
for name in ('report.tex', 'report.pdf'):
    bind(root / 'report-to-andrew/20260908-1156-rank32-and-tierd' / name)
checks = {}
commands = [('benchmark-unit-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py', '-v']),
            ('snapshot-parser', ['python3', 'benchmarks/probes/scratch/compare-rank32-formula-5785.py', '--self-test']),
            ('binary-parser', ['python3', 'benchmarks/probes/scratch/compare-rank32-forward-binaries-5795.py', '--self-test'])]
commands += [(name, ['python3', f'tools/docs/{name}.py']) for name in
             ('check-doc-size', 'check-section-refs', 'check-option-coverage', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory')]
for name, command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = r.stdout + r.stderr
sys.path.insert(0, str(root / 'tools/bench'))
import fixed_runtime
score = fixed_runtime.score(root / 'benchmarks/runs/20260908-5758-reuse-target-ranks1-40')
experiment = root / 'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s'
em = json.loads((experiment / 'manifest.json').read_text())
assert em['resource_contract']['scheduler_wall_seconds'] == 18000
for rel, digest in em['pin_sha256'].items():
    assert sha(experiment / rel) == digest, rel
    if (experiment / rel).suffix in ('.py', '.patch', '.sbatch', '.tsv'): bind(experiment / rel)
for p in experiment.glob('*.json'): bind(p)
for p in (root / 'benchmarks/probes/scratch/5793-experimental-oracle').iterdir(): bind(p)
experiment_results = list((experiment / 'results').glob('*.json'))
for p in experiment_results:
    r = json.loads(p.read_text())
    assert r['valid'] and not r['cleanup_pending'], p
    bind(p)
queue = subprocess.run(['squeue', '-h', '-j', '1673255,1673329,1673507', '-o', '%i|%T|%R'],
                       capture_output=True, text=True, check=True).stdout
eq = subprocess.run(['squeue', '-h', '-j', '1673402', '-o', '%T|%P|%C|%m'],
                    capture_output=True, text=True, check=True).stdout.splitlines()
from collections import Counter
subprocess.run(['git', 'diff', '--check'], cwd=root, check=True)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes, checks=checks,
           superseded_mutable_observations=superseded, campaigns=campaigns, normal_queue=queue,
           fixed_full_score={k: score[k] for k in ('expected_solves', 'checked_clean', 'reference_checked', 'ratio')},
           fuzz='No terminal5741 verdict.38524 ended interim;96672 failed before inference;external network resume denied and not bypassed. No fuzz artifacts inspected.',
           experimental_tierd=dict(job='1673402', tasks=2332, instances=1166,
               contract='User-approved1200s solver cutoff,18000s allocation,16000s checker;4CPU/32GiB/cpu-q;separate frozen paired arms,not a new12CPU denominator.',
               first_checked_clean=len(experiment_results), queue_counts=dict(Counter(eq)),
               oracle_controls='5793 actual positive/negative SAT and UNSAT checker controlsPASS;UNKNOWN labels never bypass independent checking.'),
           cleanup='No manual deletion. Terminal diagnostic tasks certify their own node-local input/partial-proof cleanup. Raw unique traces/pins retained;uncertain old fixture scratch preserved.',
           limitations='Goal not met;no new solver repair/promoted source. Missing original native snapshots retained as failed observer setup,not solver failure. Pending checks do not pass acceptance. Push and Claude-network resume remain unapproved.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(dict(score=out['fixed_full_score'], campaigns=campaigns, superseded=superseded), indent=2))

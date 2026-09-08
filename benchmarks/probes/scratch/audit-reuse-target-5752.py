#!/usr/bin/env python3
"""Bounded checkpoint audit; no fuzz-owner artifacts or jobs are read."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-reuse-target-fixed-reference-validation.json'
assert not target.exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
hashes = {}
def bind(p):
    hashes[str(p.relative_to(root))] = sha(p)

c = root / 'benchmarks/runs/20260908-5741-reuse-target'
m = json.loads((c / 'manifest.json').read_text())
bind(c / 'manifest.json')
for section in ('binaries', 'validation', 'initial_fixture'):
    for name, digest in m[section].items():
        assert sha(root / name) == digest, name
        bind(root / name)
for name, digest in m['files'].items():
    assert sha(root / m['source'] / name) == digest
for section in ('patches', 'logs'):
    for name, digest in m[section].items():
        p = c / ('logs' if section == 'logs' else '') / name
        assert sha(p) == digest
        bind(p)
campaigns = []
for name, job in [('20260908-5664-propagation-cursor-ranks1-40',1672864),
                  ('20260908-5678-assignment-level-BC-gate',1672960),
                  ('20260908-5712-assignment-level-ranks1-40',1673074),
                  ('20260908-5727-root-reuse-BC-gate',1673169)]:
    folder = root / 'benchmarks/runs' / name
    manifest = json.loads((folder / 'manifest.json').read_text())
    bind(folder / 'manifest.json')
    for rel, digest in manifest['pin_sha256'].items():
        assert sha(folder / rel) == digest, (name, rel)
    results = list((folder / 'results').glob('*.json'))
    provisional = list((folder / 'provisional').glob('*.json'))
    for p in results:
        r = json.loads(p.read_text())
        assert r['valid'] and r['expected_match'] and not r['solve']['timed_out'], p
        assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending'], p
        bind(p)
    for p in provisional:
        r = json.loads(p.read_text())
        assert r['expected_match'] and r['solve']['exit_code'] in (10,20) and not r['solve']['timed_out'], p
    campaigns.append(dict(campaign=name, job=job, solver_records=len(provisional), checked_clean=len(results)))
checks = {}
for name in ('check-doc-size', 'check-section-refs', 'check-option-coverage',
             'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory'):
    checks[name] = subprocess.run(['python3', root / 'tools/docs' / (name + '.py')],
        cwd=root, check=True, capture_output=True, text=True).stdout
test = subprocess.run(['python3', root / 'benchmarks/probes/scratch/test-fixed-reference-5750.py'],
    cwd=root, check=True, capture_output=True, text=True)
checks['fixed_reference_unit_tests'] = test.stdout + test.stderr
for rel in ('DEVELOPMENT_PLAN.md','PROGRESS.md','docs/calculus.md','docs/implementation-notes.md',
            'benchmarks/baselines/rank1-40-kissat-12cpu-v1/manifest.json',
            'benchmarks/reports/2026-09-08-reuse-target-fixed-reference.md',
            'benchmarks/runs/20260908-5735-root-reuse-rank31-prefix/boundary-localization.json',
            'benchmarks/runs/20260908-5726-root-reuse-rank33-prefix/boundary-localization.json',
            'report-to-andrew/20260908-1022-reuse-target-reference/report.tex',
            'report-to-andrew/20260908-1022-reuse-target-reference/report.pdf'):
    bind(root / rel)
report = root / 'report-to-andrew/20260908-1022-reuse-target-reference'
assert 'Output written on report.pdf' in (report / 'report.log').read_text()
assert 'Overfull' not in (report / 'report.log').read_text()
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
    campaigns=campaigns, checks=checks, andrew_compilations=2,
    queue_observation='Explicit squeue for1672864/1672960/1673074/1673169:only5727tasks64,65 and5712tasks42,43,35 remained RUNNING at checkpoint;12CPU/48000M. No fuzz jobs queried.',
    fuzz_owner='4746 terminalPASS5672,60000 cases/55384 checked verdicts,176 reach cases/2823730 hits,0solverfailures;fixture-only GCC stream-read issue. CLI response only,not coverage of5741. Do not repoll.',
    cleanup='Validators deleted5088 passing CNF/proof pairs after checks,hashes/logs retained. No manual deletion. Six failed-fixture scratch directories under exact /tmp/tactix-reuse-target- prefix remain for ownership audit; no submitted pin changed.',
    limitations='No new5741 benchmark submitted,no full policy replay,no latest-source fuzz,no promotion. Existing async checks continue. Fixed denominator is provisional until source checks close. No push attempted.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(campaigns, indent=2))
print('PASS: source,pins,5088 proof records,normal jobs,docs,reference tests and Andrew PDF')

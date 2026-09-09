#!/usr/bin/env python3
"""Audit the observer increment, terminal extension and still-live dependencies."""
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha, score
from checked_comparison import classify, load
target = root / 'benchmarks/reports/2026-09-09-membership-admission-audit-6220.json'
assert not target.exists()
owned = set(); local = {}; campaigns = {}
def retain(p):
    rel = str(p.relative_to(root)); assert p.is_file()
    if p.stat().st_size >= 5 << 20 or rel.endswith('.decisions.tsv'):
        local[rel] = dict(bytes=p.stat().st_size, sha256=sha(p))
    else: owned.add(rel)
for name in ('PROGRESS.md', 'benchmarks/reports/2026-09-09-schedule-membership-and-rank50-admission.md'):
    retain(root / name)
for number in range(6206, 6222):
    for pattern in ('*' + str(number) + '.py', '*' + str(number) + '.c'):
        for p in (root / 'benchmarks/probes/scratch').glob(pattern): retain(p)
build = root / 'benchmarks/runs/20260909-6206-schedule-continue-build'
b = json.loads((build / 'summary.json').read_text()); assert b['release_exact_parent']
parents = json.loads((build / 'parents.json').read_text())['parents']
for path, digest in b['binaries'].items(): assert sha(root / path) == digest
for source, files in b['sources'].items():
    for rel, digest in files.items(): assert sha(root / source / rel) == digest
for rel, digest in b['logs'].items(): assert sha(build / rel) == digest; retain(build / rel)
for name in ('summary.json', 'parents.json'): retain(build / name)
# Keep originally generated context patches unchanged. Store separate minimal
# patches for clean Git whitespace checks; apply them with --unidiff-zero.
for source, changes in b['changes'].items():
    for rel in changes:
        old = root / parents[source]['source'] / rel; new = root / source / rel
        arm = 'native' if source.startswith('tools/') else 'tactix'
        p = build / (arm + '-' + Path(rel).name + '-zero-context.patch')
        assert not p.exists()
        p.write_text(''.join(difflib.unified_diff(old.read_text().splitlines(True), new.read_text().splitlines(True),
            fromfile='a/' + rel, tofile='b/' + rel, n=0)))
        retain(p)
        original = build / (arm + '-' + Path(rel).name + '.patch')
        local[str(original.relative_to(root))] = dict(bytes=original.stat().st_size, sha256=sha(original))
for name in ('20260909-6208-rank32-schedule-membership', '20260909-6211-rank50-extended-prefix',
             '20260909-6217-rank32-retry-members'):
    c = root / 'benchmarks/runs' / name; m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    for filename in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch'): retain(c / filename)
    campaigns[str(c.relative_to(root))] = json.loads((c / 'launch.json').read_text())
    if '6217' in name: continue
    s = json.loads((c / 'summary.json').read_text()); retain(c / 'summary.json')
    if '6208' in name:
        assert s['parent_replay'] and s['shared'] == 30834 and len(s['kissat_only']) == 5
        assert not s['candidate_only'] and not s['shared_score_or_occurrence_differences']
    else: assert s['decisions']['matching_rows'] == 1226946
    for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest; retain(root / rel)
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json'); r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        retain(p)
bc = root / 'benchmarks/runs/20260909-6171-learned-sort-BC-gate/summary.json'
s = json.loads(bc.read_text()); assert s['checked_clean'] == 66 and not s['pending_tasks']
retain(bc)
for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest; retain(root / rel)
full = root / 'benchmarks/runs/20260909-6181-learned-sort-ranks1-50'
states = {}
for folder in ('provisional', 'results'):
    for p in (full / folder).glob('*.json'):
        r = json.loads(p.read_text()); state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT')
        states[r['task']] = state; retain(p)
fresh = load(root / 'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50')
for path, digest in fresh['record_hashes'].items(): assert sha(Path(path)) == digest; retain(Path(path))
prior = json.loads((root / 'benchmarks/reports/2026-09-09-expanded-ranks-audit-6204.json').read_text())
for rel, digest in prior['protected_campaign_hashes'].items(): assert sha(root / rel) == digest
checks = {}
commands = [('membership-fixtures', ['python3', 'benchmarks/probes/scratch/validate-rank32-membership-6210.py', '--self-test']),
    ('benchmark-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
    ('watcher-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/slurm', '-p', 'test_*.py'])]
commands += [(name, ['python3', 'tools/docs/' + name + '.py']) for name in ('check-doc-size', 'check-progress',
    'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-0206-schedule-admission'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={r: sha(root / r) for r in sorted(owned)}, local_hash_bound=local,
    campaigns=campaigns, checks=checks, full50=dict(score=score(full), classes=states), fresh50=fresh,
    protected_campaign_hashes=prior['protected_campaign_hashes'],
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    cleanup='No campaign files manually deleted. Terminal diagnostics attest deleted partial certificates and scratch. Old-control fixture TSV was removed in6215;buildsummary records the limitation. No local core/core.* file exists;system core collector not altered. Context patches and large traces stay local/hash-bound;zero-context source patches are separate successors.',
    scope='Diagnostic-only Release-byte-neutral increment;no promotion,push,denied transfer retry or fuzz artifact inspection. Remaining certificates and retry-producer job continue independently.'), indent=2) + '\n')
print(target)

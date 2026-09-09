#!/usr/bin/env python3
"""Seal the causal repair, tests and published asynchronous campaign state."""
import datetime as dt
import json
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-congruence-empty-gates-audit-6646.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-reorder-rank49-audit-6635.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md': assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md','docs/calculus.md','docs/implementation-notes.md',
         'tools/bench/search_window.py','tools/bench/test_search_window.py',
         'benchmarks/reports/2026-09-09-congruence-empty-gates.md'}
for p in (root / 'benchmarks/probes/scratch').iterdir():
    match = re.search(r'-(\d+)\.(py|cpp)$', p.name)
    if match and 6639 <= int(match[1]) <= 6667: owned.add(str(p.relative_to(root)))
evidence = {}; accounting = {}; build_evidence = {}
def own(p): owned.add(str(p.relative_to(root)))
def bindings(hashes):
    for rel, digest in hashes.items(): assert sha(root / rel) == digest, rel
def account(campaign, launch, count, terminal=False):
    assert launch['manifest_sha256'] == sha(campaign / 'manifest.json')
    command = ['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],
               '-o','JobID,State%40,ExitCode,Submit','-P']
    stdout = subprocess.check_output(command, text=True)
    rows = stdout.splitlines(); assert len(rows) == count, (campaign, stdout)
    for row in rows:
        cells = row.split('|')
        assert cells[1] in ('PENDING','RUNNING','COMPLETING','COMPLETED') and cells[2] == '0:0', row
        assert cells[3] >= launch['accounting_since']
        if terminal: assert cells[1] == 'COMPLETED'
    accounting[launch['job_id']] = dict(campaign=str(campaign.relative_to(root)), command=command, stdout=stdout)
for name, summary, count in (
    ('20260909-6637-rank35-binary-life','summary-6642.json',2),
    ('20260909-6639-rank49-search-window','summary-6641.json',2),
    ('20260909-6643-rank49-probe-input','summary-6645.json',2),
    ('20260909-6648-rank49-binary-life','summary-6650.json',2),
    ('20260909-6651-rank49-congruence-stages','summary-6653.json',1),
    ('20260909-6658-rank49-congruence-empty-prefix','summary-6660.json',1)):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c/'manifest.json').read_text())
    for rel,digest in m['hashes'].items(): assert sha(c/rel) == digest, rel
    bindings(m['dependencies'])
    for filename in ('manifest.json','launch.json','run.py','array.sbatch',summary): own(c/filename)
    s = json.loads((c/summary).read_text()); bindings(s['hashes'])
    evidence[str((c/summary).relative_to(root))] = sha(c/summary)
    results = list((c/'results').glob('*.json')); assert len(results) == count
    for p in results:
        own(p); r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        assert r['binary_sha256'] == m['hashes']['pins/'+r['arm']]
        for rel,digest in r['trace_sha256'].items(): assert sha(c/'traces'/rel) == digest
    account(c, json.loads((c/'launch.json').read_text()), count, True)
for name in ('20260909-6579-material-schedule-build','20260909-6654-congruence-empty-gates-build'):
    c = root/'benchmarks/runs'/name; m = json.loads((c/'manifest.json').read_text())
    for rel,digest in m['files'].items(): assert sha(root/m['source']/rel) == digest, rel
    bindings(m['binaries'])
    for rel,digest in m.get('patches',{}).items(): assert sha(c/rel) == digest
    build_evidence[str((c/'manifest.json').relative_to(root))] = sha(c/'manifest.json')
    if '6654' in name:
        for p in c.iterdir():
            if p.is_file() and p.suffix in ('.json','.patch'): own(p)
        for p in (c/'logs').glob('*.log'): own(p)
        for rel,digest in m['logs'].items(): assert sha(c/rel) == digest
        assert (c/'logs/parent-negative.log').read_text().strip() == 'PASS: frozen parent exposes32 binary-only cleanup failures;32 controls pass'
        for arm,total in (('release',111),('bc',111),('trace',110)):
            assert f'100% tests passed out of {total}' in (c/f'logs/{arm}-ctest.log').read_text()
c = root/'benchmarks/runs/20260909-6657-congruence-empty-proof-control'
s = json.loads((c/'summary.json').read_text()); bindings(s['hashes'])
assert s['cases'] == s['final_proofs_checked'] == s['exact_model_sets'] == 64
assert len(s['records']) == 64 and s['removed_bytes'] == 22584
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
for p in c.iterdir():
    if p.suffix in ('.json','.log'): own(p)
evidence[str((c/'summary.json').relative_to(root))] = sha(c/'summary.json')
states = {}
runtime_names = ('20260909-6594-material-ranks1-50','20260909-6661-congruence-empty-BC-gate',
                 '20260909-6663-rank49-congruence-empty-runtime','20260909-6666-congruence-empty-ranks1-50')
for name in runtime_names:
    c = root/'benchmarks/runs'/name; m = json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',{}).items(): assert sha(c/rel) == digest, rel
    for filename in ('manifest.json','launch.json','array.sbatch','matrix.tsv'):
        if (c/filename).exists(): own(c/filename)
    if '6661' in name:
        own(c/'summary-6665.json'); bindings(json.loads((c/'summary-6665.json').read_text())['hashes'])
    if '6663' in name:
        for p in sorted(c.glob('wave*-submission.json')):
            own(p); launch = json.loads(p.read_text())
            bindings(launch['preceding_records']); account(c, launch, 2)
    else:
        account(c, json.loads((c/'launch.json').read_text()), 66 if '6661' in name else 50)
    current = {}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            own(p); r = json.loads(p.read_text()); status = classify(r, folder == 'results')
            assert status['status'] in ('pending_validation','checked_SAT','checked_UNSAT'), (p,status)
            assert r['task'] == int(p.stem)
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            current[r['task']] = dict(rank=r['rank'],arm=r['arm'],status=status,seconds=r['solve']['wall_seconds'])
    if '6594' in name: assert len(current) == 50
    if '6661' in name: assert len(current) == 66
    states[name] = current
checks = {}
commands = [(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    r = subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name] = dict(command=command,stdout=r.stdout,stderr=r.stderr); print(name,'PASS',flush=True)
report = root/'report-to-andrew/20260909-1907-congruence-empty-gates'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)),sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)}, protected_campaign_hashes=prior['protected_campaign_hashes'],
    evidence=evidence,build_evidence=build_evidence,accounting=accounting,campaign_states=states,checks=checks,
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex','report.pdf')},
    cleanup='6657 removed22584 bytes of exact fixed-micro CNF/proof files after independent checks and hash capture. No other manual deletion; terminal diagnostic runners removed their own hashed partial certificates/scratch. All source pins, observers, histories and pending campaigns retained. No live checker scratch or fuzz artifact inspection.',
    limitations='Experimental6654 remains unpromoted/unfuzzed. One premature return repaired; public capabilities and production unchanged. Three-wave/full50/memory acceptance pending. Wrong UNSAT preflight rejected before creating6663; submit found no manifest and issued no job. Corrected fixed SAT oracle retained. Wave2 preflight refused incomplete wave1, no job submitted. First audit attempt expected an incorrect substring of the successful parent-negative log; corrected to its exact text before publishing any audit. No real solver/checker invalidity, fixed-reference refresh, FlatLists change or denied fuzz/push retry.')
with target.open('x') as stream: json.dump(out,stream,indent=2); stream.write('\n')
print(target)
for name, rows in states.items(): print(name,len(rows),'published',sum(r['status']['checked'] and r['status']['clean'] for r in rows.values()),'checked/clean')

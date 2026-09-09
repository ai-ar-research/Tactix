#!/usr/bin/env python3
"""Current-source replay checkpoint with inherited protected evidence checks."""
from pathlib import Path
source = Path(__file__).with_name('audit-early-congruence-6542.py')
code = source.read_text()
for old, new in {
    '2026-09-09-early-congruence-audit-6542.json': '2026-09-09-current-material-pair-audit-6565.json',
    '2026-09-09-retention-attribution-audit-6533.json': '2026-09-09-material-producers-audit-6559.json',
    '20260909-1240-early-congruence-timing': '20260909-1330-current-material-pair',
    "local[str((report / 'report-overflow.log').relative_to(root))] = sha(report / 'report-overflow.log')": '',
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
begin, end = code.index('owned = {'), code.index('\nlocal, campaigns =')
code = code[:begin] + '''owned = {'PROGRESS.md', 'benchmarks/reports/2026-09-09-rank50-current-material-pair.md'}
owned.update('benchmarks/probes/scratch/' + n for n in (
    'prepare-current-material-pair-6562.py', 'submit-current-material-pair-6563.py',
    'validate-current-material-pair-6564.py', 'audit-current-material-pair-6565.py',
    'land-current-material-pair-6566.py'))
''' + code[end:]
anchor = 'accounting = {}\nfor name, account in accounts.items():'
assert code.count(anchor) == 1
extra = '''landing = read(root / 'benchmarks/reports/2026-09-09-material-producers-landing-6561.json')
assert landing['audit_sha256'] == sha(prior_path)
assert landing['helper_sha256'] == sha(root / 'benchmarks/probes/scratch/land-material-context-6561.py')
assert landing['retained_patch_sha256'] == sha(root / 'benchmarks/runs/20260909-6550-native-material-observer-build/native.patch')
assert len(landing['completed6464_checked_clean']) == 18
for rel, digest in landing['completed6464_checked_clean'].items():
    assert sha(root / rel) == digest
    r = read(root / rel)
    assert classify(r, True)['status'] in ('checked_SAT', 'checked_UNSAT')
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
c = root / 'benchmarks/runs/20260909-6562-rank50-current-material-pair'
m = read(c / 'manifest.json', True)
launch = read(c / 'launch.json', True)
assert launch['manifest_sha256'] == sha(c / 'manifest.json')
s = read(c / 'summary-6564.json', True)
assert s['observer_neutral'] and s['scratch_deleted']
assert s['decision_difference']['matching_rows'] == 1226946
assert not s['boundary_window']['candidate']
assert len(s['lifecycle']['candidate']) == 1 and len(s['lifecycle']['kissat']) == 2
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
    local[rel] = digest
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
    if rel.endswith(('.py', '.sbatch')): owned.add(str((c / rel).relative_to(root)))
for arm in ('candidate', 'candidate_control'):
    r = read(c / 'results' / (arm + '.json'), True)
    assert r['exit_code'] == 0 and r['scratch_deleted']
    assert r['partial_certificate'] == s['partial_certificate']
    assert r['partial_certificate']['bytes'] == 63963136
    for name, digest in r['trace_sha256'].items():
        p = c / 'traces' / name
        assert sha(p) == digest
        if p.stat().st_size < 100000: owned.add(str(p.relative_to(root)))
for p in (c / 'logs').iterdir(): owned.add(str(p.relative_to(root)))
accounts[c.name] = dict(command=['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S',
    launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P'], launch=launch)
diagnostic = dict(observer_neutral=True, partial_certificate=s['partial_certificate'],
    scratch_deleted=True, decision_difference=s['decision_difference'], source_unchanged=True)
'''
code = code.replace(anchor, extra + '\n' + anchor)
begin, end = code.index('    diagnostic=dict('), code.index('    full50=dict(')
code = code[:begin] + '    diagnostic=diagnostic,\n' + code[end:]
begin, end = code.index("    cleanup='"), code.index('\n)\nwith target.open')
code = code[:begin] + '''    cleanup='Two bounded runners hash-deleted 63963136-byte partial proofs each and decompressed inputs; scratch deleted. No final-proof acceptance or live-scratch/fuzz access. Prior6464 all18 checked/clean independently reverified.',
    failure_classification='Initial preparation used a mistyped6439 authority path and stopped before manifest/submission. Exact four-file scaffold and both pins were verified unchanged before resuming. No solver/proof/model failure or result exclusion; no successful writer rerun, campaign mutation or denied transfer/push retry.',
    next='Map separate subsumption-material producer/clear/rearm/watermark design onto current source, preserving all shared touch consumers and SP-SLOT-5; implementation approval remains unanswered. No more native replay of this same boundary. All50/memory goal unmet.',
''' + code[end:]
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

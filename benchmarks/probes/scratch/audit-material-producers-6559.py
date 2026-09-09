#!/usr/bin/env python3
"""Scoped observer/replay audit with exact new and inherited job accounting."""
from pathlib import Path
source = Path(__file__).with_name('audit-early-congruence-6542.py')
code = source.read_text()
for old, new in {
    '2026-09-09-early-congruence-audit-6542.json': '2026-09-09-material-producers-audit-6559.json',
    '2026-09-09-retention-attribution-audit-6533.json': '2026-09-09-factor-material-audit-6548.json',
    '20260909-1240-early-congruence-timing': '20260909-1310-rank50-material',
    "local[str((report / 'report-overflow.log').relative_to(root))] = sha(report / 'report-overflow.log')": '',
}.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
begin, end = code.index('owned = {'), code.index('\nlocal, campaigns =')
code = code[:begin] + '''owned = {
    'PROGRESS.md', 'benchmarks/reports/2026-09-09-rank50-material-producers.md',
    'tools/bench/material_trace.py', 'tools/bench/test_material_trace.py',
}
owned.update('benchmarks/probes/scratch/' + n for n in (
    'prepare-material-observer-6550.py', 'build-material-observer-6551.py',
    'test-material-observer-6552.c', 'prepare-rank50-material-6553.py',
    'submit-rank50-material-6554.py', 'validate-rank50-material-6555.py',
    'prepare-material-pair-6556.py', 'submit-material-pair-6557.py',
    'validate-material-pair-6558.py', 'audit-material-producers-6559.py',
    'land-material-producers-6560.py'))
''' + code[end:]
anchor = 'accounting = {}\nfor name, account in accounts.items():'
assert code.count(anchor) == 1
extra = '''from material_trace import parse_material
from binary_lifecycle import read_lifecycle
observer = root / 'benchmarks/runs/20260909-6550-native-material-observer-build'
ob = read(observer / 'manifest.json', True)
read(observer / 'parents.json', True)
assert sha(observer / 'parents.json') == ob['parents_sha256']
assert sha(observer / 'native.patch') == ob['patch_sha256']
owned.add(str((observer / 'native.patch').relative_to(root)))
for rel, digest in ob['files'].items(): assert sha(root / ob['source'] / rel) == digest, rel
assert sha(root / ob['binary']['path']) == ob['binary']['sha256']
assert sha(observer / 'fixture') == ob['fixture']['binary_sha256']
assert sha(observer / 'fixture.tsv') == ob['fixture']['output_sha256']
assert ob['fixture']['overwrite_rejection']['exit'] == 2
parse_material((observer / 'fixture.tsv').read_text(), 2, 3, 9)
owned.add(str((observer / 'fixture.tsv').relative_to(root)))
for p in (observer / 'logs').iterdir():
    assert p.is_file()
    owned.add(str(p.relative_to(root)))
diagnostics = {}
for name, summary_name in (
    ('20260909-6553-rank50-material-producers', 'summary-6555.json'),
    ('20260909-6556-rank50-material-pair', 'summary-6558.json'),
):
    c = root / 'benchmarks/runs' / name
    m = read(c / 'manifest.json', True)
    launch = read(c / 'launch.json', True)
    assert sha(c / 'manifest.json') == launch['manifest_sha256']
    s = read(c / summary_name, True)
    for rel, digest in s['hashes'].items():
        assert sha(root / rel) == digest, rel
        local[rel] = digest
    for rel, digest in m['hashes'].items():
        assert sha(c / rel) == digest, rel
        if rel.endswith(('.py', '.sbatch')): owned.add(str((c / rel).relative_to(root)))
    r = read(c / 'results/kissat.json', True)
    assert r['exit_code'] == 0 and r['scratch_deleted'] and s['ordinary_replay_exact']
    assert r['partial_certificate'] == s['partial_certificate']
    assert r['partial_certificate']['bytes'] == 63963136
    for p in (c / 'traces').iterdir():
        assert sha(p) == r['trace_sha256'][p.name]
        if p.stat().st_size < 100000: owned.add(str(p.relative_to(root)))
    assert parse_material((c / 'traces/kissat.material.tsv').read_text(), 2508, 2510, 823562) == s['material_events']
    if 'pair_lifecycle' in s:
        with (c / 'traces/kissat.lifecycle.tsv').open() as stream:
            assert read_lifecycle(stream, 'kissat', {(-525, 574)}) == s['pair_lifecycle']
    accounts[name] = dict(command=['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S',
        launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P'], launch=launch)
    diagnostics[name] = dict(ordinary_replay_exact=True, partial_certificate=r['partial_certificate'], scratch_deleted=True)
'''
code = code.replace(anchor, extra + '\n' + anchor)
begin, end = code.index('    diagnostic=dict('), code.index('    full50=dict(')
code = code[:begin] + '    diagnostics=diagnostics,\n' + code[end:]
begin, end = code.index("    cleanup='"), code.index('\n)\nwith target.open')
code = code[:begin] + '''    cleanup='Two bounded runners hash-deleted63963136-byte partial proofs each plus decompressed inputs and report scratch deleted. No final-proof acceptance, manual live-scratch cleanup or fuzz access. Old independent checks remain asynchronous.',
    failure_classification='Fixture compile-only native macro identifier error retained then corrected at the exact unsubmitted fixture stage. First validator wrongly treated all same-conflict admission calls as invitations; failed before output, corrected to retain/validate first invitation plus54 subsequent declines. Intentional no-overwrite control exits2 with original file intact. No solver, proof, model or soundness failure; no successful writer rerun or denied transfer/push retry.',
    next='Confirm selected pair lifecycle at current6424 Tactix through the823562 boundary with its existing observer and exact ordinary-stream/proof control. General material-tracker and FlatLists implementation approvals remain unanswered; goal unmet.',
''' + code[end:]
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

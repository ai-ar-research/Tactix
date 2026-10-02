"""Freeze checked fixed-fixture evidence; never executes a solver."""
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
from collections import Counter

root = next(p for p in Path(__file__).resolve().parents if (p / 'CLAUDE.md').is_file())
out = root / 'benchmarks/reports/2026-10-02-broad-default-correspondence'
out.mkdir(exist_ok=True)
prior = json.loads((root / 'benchmarks/reports/2026-10-02-native-support-audit/validation.json').read_text())
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
dump = lambda path, value: path.write_text(json.dumps(value, indent=2) + '\n')

sources = sorted(set(prior['source_sha256']) | {
    'DEVELOPMENT_PLAN.md', 'tests/test_default_execution_signature.cpp',
    'tests/test_inprocess_execution_signature.cpp'})
for path in sources:
    if path.startswith('src/'):
        assert sha(root / path) == prior['source_sha256'][path], path
for key in ('accepted_authorities_sha256', 'checker_and_inspector_sha256'):
    assert all(sha(root / path) == value for path, value in prior[key].items()), key

def archive(name, files):
    members = {}
    with tarfile.open(out / name, 'w:gz') as tar:
        for member, path in sorted(files.items()):
            members[member] = sha(path)
            tar.add(path, arcname=member, recursive=False)
    dump(out / name.replace('.tar.gz', '-members.json'), members)
    with tarfile.open(out / name, 'r:gz') as tar:
        assert set(tar.getnames()) == set(members)
        for member in tar:
            assert hashlib.sha256(tar.extractfile(member).read()).hexdigest() == members[member.name]
    return len(members)

archive('current-source-snapshot.tar.gz', {p: root / p for p in sources})
baseline = '12e3c4f0e7098af15e855f995da23f6317cd0d58'
preserved = {}
with tarfile.open(out / 'preserved-foundation-sources.tar.gz', 'w:gz') as tar:
    for path in ['src/policies/default_policy.cpp', 'src/policies/default_policy.hpp',
                 'src/policies/default_inprocess.hpp']:
        data = subprocess.check_output(['git', 'show', f'{baseline}:{path}'], cwd=root)
        info = tarfile.TarInfo(path)
        info.size = len(data)
        tar.addfile(info, io.BytesIO(data))
        preserved[path] = hashlib.sha256(data).hexdigest()
dump(out / 'preserved-foundation-members.json', preserved)

for script in ('compare-broad-phase.py', 'compare-broad-search.py',
               'validate-broad-search-controls.py', 'freeze-broad-correspondence.py'):
    shutil.copyfile(root / 'build' / script, out / script)
shutil.copyfile(root / 'benchmarks/reports/2026-10-02-native-support-audit/debug-phase-abi-bridge.cpp',
                out / 'phase-abi-bridge.cpp')
for path in sorted((root / 'build').glob('shared-policy-prep-broad*.log')):
    shutil.copyfile(path, out / path.name.removeprefix('shared-policy-prep-broad-'))

builds = {}
for mode in ('debug', 'diagnostic'):
    build = root / ('build/shared-policy-prep-' + mode)
    totals = Counter()
    groups = {}
    files = {}
    for label in ('phase', 'search'):
        path = build / ('broad-' + label + '-correspondence.json')
        data = json.loads(path.read_text())
        shutil.copyfile(path, out / f'{mode}-{label}-correspondence.json')
        assert data['signatures_equal'] and data['artifacts_byte_equal']
        assert len({x['output_sha256'] for x in data['comparisons']}) == 1
        assert all(x['artifacts'] == data['comparisons'][0]['artifacts'] for x in data['comparisons'])
        assert data['baseline_source_sha256'] == preserved['src/policies/default_policy.cpp']
        headers = data.get('baseline_header_sha256', data.get('header_sha256'))
        assert all(value == preserved['src/' + name] for name, value in headers.items())
        enabled = build / f'broad-{label}-correspondence/enabled'
        for name, value in data['comparisons'][2]['artifacts'].items():
            assert sha(enabled / name) == value
        for entry in data['feedback']:
            name = entry.get('file', entry.get('case', '') + '.jsonl')
            assert sha(enabled / name) == entry['sha256'], name
            summary = entry['summary']
            totals['streams'] += 1
            for action in summary['actions'].values():
                totals['actions'] += action['records']
                totals['support_receipts'] += action['kinds'].get('support', 0)
            for interval in summary['intervals'].values():
                totals['intervals'] += interval if isinstance(interval, int) else interval['records']
        assert all(x['verified'] for x in data['oracles'])
        proofs = sum(x['kind'] == 'UNSAT' for x in data['oracles'])
        models = sum(x['kind'] == 'SAT' for x in data['oracles'])
        assert proofs == models == (6 if label == 'phase' else 7)
        for directory in ('enabled', 'checkers'):
            for path in sorted((build / f'broad-{label}-correspondence' / directory).rglob('*')):
                if path.is_file():
                    member = str(path.relative_to(build / f'broad-{label}-correspondence'))
                    files[f'{label}/{member}'] = path
        groups[label] = {'distinct_fixture_profiles': 12 if label == 'phase' else 14,
            'artifacts_per_arm': len(data['comparisons'][0]['artifacts']),
            'proofs_verified': proofs, 'models_verified': models,
            'strict_streams': len(data['feedback'])}
        if label == 'phase':
            assert len(data['productive_controls']) == 12
            groups[label]['productive_controls'] = data['productive_controls']
    controls = json.loads((build / 'broad-search-controls.json').read_text())
    assert controls == json.loads((build / 'broad-search-correspondence.json').read_text())['explicit_search_controls']
    shutil.copyfile(build / 'broad-search-controls.json', out / f'{mode}-search-controls.json')
    for name in ('compile_commands.json', 'CMakeCache.txt'):
        shutil.copyfile(build / name, out / f'{mode}-{name}')
    count = archive(f'{mode}-checked-artifacts.tar.gz', files)
    focused = (out / f'final-{mode}-focused.log').read_text()
    assert '100% tests passed out of 8' in focused
    seconds = float(re.search(r'Total Test time \(real\) =\s*([0-9.]+)', focused)[1])
    binary_hash = sha(build / 'tactix')
    assert binary_hash == prior['builds'][mode]['tactix_sha256']
    assert dict(totals) == {'streams': 38, 'actions': 9461, 'support_receipts': 1241, 'intervals': 195}
    builds[mode] = {'tactix_sha256': binary_hash, 'focused_passed': 8, 'focused_total': 8,
        'focused_seconds': seconds, 'distinct_fixture_profiles': 26,
        'proofs_verified': 13, 'models_verified': 13, 'groups': groups,
        'feedback_coverage': dict(totals), 'checked_archive_members': count}

docs = (out / 'docs-final.log').read_text()
assert '100% tests passed out of 15' in docs
assert '0 error(s)' in (out / 'progress.log').read_text()
assert '0 error(s), 6 warning(s)' in (out / 'doc-size.log').read_text()
andrew = root / 'report-to-andrew/20261002T154030Z-broad-default-correspondence'
assert all((andrew / name).is_file() for name in ('report.tex', 'report.pdf'))
assert all('Output written on report.pdf' in (andrew / name).read_text()
           for name in ('tex-pass-1.log', 'tex-pass-2.log'))
manifest = {
    'schema': 'tactix-broad-default-correspondence-v1',
    'parent_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    'accepted_checkpoint': baseline,
    'scope': 'Fixed-fixture preservation/coverage increment; production unchanged; full acceptance incomplete.',
    'source_sha256': {p: sha(root / p) for p in sources},
    'accepted_authorities_sha256': prior['accepted_authorities_sha256'],
    'checker_and_inspector_sha256': prior['checker_and_inspector_sha256'],
    'artifact_sha256': {p.name: sha(p) for p in sorted(out.iterdir()) if p.is_file() and p.name != 'validation.json'},
    'report_sha256': sha(root / 'benchmarks/reports/2026-10-02-broad-default-correspondence.md'),
    'builds': builds,
    'documentation_gates': {'passed': 15, 'total': 15, 'seconds': 10.14},
    'initial_failures': ['Sweep fixture cap 4 admitted too few clauses; nonvacuous progress assertion failed.',
                         'Cap 5 admits the four-variable equivalence; no solver change.'],
    'queue': {'task_campaigns_submitted': False, 'unrelated_interactive_job': '1745040', 'untouched': True},
    'benchmark_cleanup': {'deleted': False},
    'remaining': ['Candidate contract audit and immutable validation snapshot', 'New full Debug suite',
                  'Fresh Claude-owned >=100000 seeds per policy plus retained/targeted coverage',
                  'All four default regression gates', 'Both-policy purity and measured feedback overhead',
                  'Whole-goal terminal acceptance and final Andrew handoff'],
    'andrew': {'directory': str(andrew.relative_to(root)),
               'sha256': {p.name: sha(p) for p in sorted(andrew.iterdir()) if p.is_file()}}
}
dump(out / 'validation.json', manifest)
print(json.dumps({'builds': {mode: {'fixtures': data['distinct_fixture_profiles'],
    'proofs': data['proofs_verified'], 'models': data['models_verified'],
    'archived_members': data['checked_archive_members']} for mode, data in builds.items()},
    'companion_files': len(manifest['artifact_sha256']) + 1,
    'total_bytes': sum(p.stat().st_size for p in out.iterdir() if p.is_file())}))

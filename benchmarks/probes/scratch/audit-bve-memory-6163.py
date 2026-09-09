#!/usr/bin/env python3
"""Audit checked memory evidence, current authority and timestamp-bound jobs."""
from pathlib import Path
source = Path(__file__).with_name('audit-current-memory-6133.py')
code = source.read_text()
changes = {
    '2026-09-08-current-memory-validation.json': '2026-09-09-bve-memory-validation.json',
    "f'20260908-{number}-*'": "f'2026090[89]-{number}-*'",
    '(5784, 6108, 6114, 6115, 6121, 6129)': '(5784, 6108, 6114, 6115, 6121, 6129, 6148, 6156)',
    '2026-09-08-fresh-rank50-6132': '2026-09-09-fresh-rank50-6159',
    '2026-09-08-current-memory-diagnostics.md': '2026-09-09-bve-memory-ownership.md',
    "['sacct', '-n', '-X', '-j', ','.join(jobs[1:]), '-o', 'JobID,State,ExitCode', '-P']":
    "['sacct', '-n', '-X', '--array', '-S', '2026-09-09T00:00:00', '-j', ','.join(jobs[1:]), '-o', 'JobID,State%40,ExitCode,Submit', '-P']",
    'Unchanged6107 trace98/98 andjournal99/99;39benchmarktests andsixnegativeparserfixtures. Memory6129/job1676999 queued;noownershiporperformanceclaim. Fresh6132all50retained,74solves58checked,noaggregate.':
    '6148 and6156 each3/3 checked/clean,full-work/proof andprecedingcapacity replay. Peaks precedeGC;densegrowth/compaction intervals identified.6154 Release101/101,trace100/100,Releasebyteidentity.6160rank4 907045decisions/380searchboundaries matchthrough650K.13watcher+39benchmarktests.6159fresh100solver/96checked,noaggregate. Structuralbufferexperimentawaitsapproval.',
    'No manual deletion. All published final normal records assert original-certificate and derivative cleanup; active scratch,pins andlogs untouched. No fuzz-owner artifacts accessed.':
    'Removedtwoexactzero-bytefixed-testtemporaryfiles afterintentional old-object assertions;identities inbuildmanifests. Allpublishedfinalnormalrecords assertcertificate/derivativecleanup. Active scratch,pins andlogs untouched;no fuzz-owner artifactsaccessed.',
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
extra = '''
for number in (6146, 6154):
    c = campaign(number)
    b = json.loads((c / 'summary.json').read_text())
    m = json.loads((c / 'manifest.json').read_text())
    assert b['release_byte_identical_to_6107'] and not m['production_change']
    assert sha(c / 'summary.json') == m['build_summary_sha256']
    assert sha(root / m['parent_authority']) == m['parent_authority_sha256']
    for rel, digest in b['files'].items():
        assert sha(root / b['source'] / rel) == digest, rel
    for rel, digest in b['binaries'].items():
        assert sha(root / rel / 'tactix') == digest, rel
    for rel, digest in b['logs'].items():
        assert sha(c / rel) == digest, rel
        bind(c / rel)
    for rel, digest in m['files'].items():
        assert sha(c / 'source' / rel) == digest, rel
        bind(c / 'source' / rel)
    assert sha(c / 'observer.patch') == m['patch_sha256']
    assert m['cleanup']['bytes'] == 0 and not (root / m['cleanup']['path']).exists()
    for name in ('manifest.json', 'summary.json', 'observer.patch'):
        bind(c / name)
for number in (6148, 6156, 6160):
    c = campaign(number)
    s = json.loads((c / 'summary.json').read_text())
    for rel, digest in s['hashes'].items():
        assert sha(root / rel) == digest, rel
    bind(c / 'summary.json')
    if number == 6160:
        m = json.loads((c / 'manifest.json').read_text())
        for rel, digest in m['hashes'].items():
            assert sha(c / rel) == digest, rel
        assert s['decisions']['complete_equal'] and s['decisions']['matching_rows'] == 907045
        assert s['boundaries_search']['equal'] and s['boundaries_search']['candidate_rows'] == 380
        jobs.append(str(json.loads((c / 'launch.json').read_text())['job_id']))
    else:
        assert len(s['records']) == 3 and all(r['checked_clean'] for r in s['records'])
spec = importlib.util.spec_from_file_location('waiter', root / 'tools/slurm/wait.py')
waiter = importlib.util.module_from_spec(spec); spec.loader.exec_module(waiter)
bound_command = ['sacct', '-n', '-X', '--array', '-j', '1677046', '-S', '2026-09-09T00:19:08',
                 '-o', 'JobID,State%40,ExitCode,Submit', '-P']
bound = subprocess.run(bound_command, capture_output=True, text=True, check=True).stdout
assert waiter.accounting_state(bound, '1677046', '2026-09-09T00:19:08') == 'COMPLETED'
assert len([line for line in bound.splitlines() if line.strip()]) == 2
for rel in ('tools/slurm/wait.py', 'tools/slurm/test_wait.py'):
    bind(root / rel)
'''
marker = 'checks = {}\n'
assert code.count(marker) == 1
code = code.replace(marker, extra + '\n' + marker)
marker = 'for name, command in commands:\n'
additional = '''commands += [
    ('watcher-unit-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/slurm', '-p', 'test_*.py']),
    ('collector-sequence-fixtures', ['python3', 'benchmarks/probes/scratch/seal-collect-memory-6153.py', '--self-test']),
    ('capacity-sequence-fixtures', ['python3', 'benchmarks/probes/scratch/seal-bve-capacity-6158.py', '--self-test'])]
'''
assert code.count(marker) == 1
code = code.replace(marker, additional + marker)
code = code.replace('checks=checks, queue=queue, terminal=terminal,',
                    'checks=checks, queue=queue, terminal=terminal, rank4_bound_accounting=dict(command=bound_command, stdout=bound),')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

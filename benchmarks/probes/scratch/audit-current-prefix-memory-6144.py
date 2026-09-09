#!/usr/bin/env python3
"""Successor audit; retain broad UNKNOWN handling and frozen prior evidence."""
from pathlib import Path

source = Path(__file__).with_name('audit-current-memory-6133.py')
code = source.read_text()
replacements = {
    '2026-09-08-current-memory-validation.json': '2026-09-08-current-prefix-memory-validation.json',
    '2026-09-08-fresh-rank50-6132': '2026-09-08-fresh-rank50-6143',
    '2026-09-08-current-memory-diagnostics.md': '2026-09-08-current-prefix-memory.md',
    'Unchanged6107 trace98/98 andjournal99/99;39benchmarktests andsixnegativeparserfixtures. Memory6129/job1676999 queued;noownershiporperformanceclaim. Fresh6132all50retained,74solves58checked,noaggregate.':
    'Memory6129 all3 checked/clean and full-work/proof replay;6136 journal repair99/99 Release andjournal,Release byte-identical6107. Prefix6139/6140 complete equal through10K/100K conflicts. Fresh6143 retainsall50,100solver/93checked,noaggregate. No runtime improvement orpromotion claim.',
}
for old, new in replacements.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
extra = '''
# Verify newly sealed diagnostics and their exact retained dependencies.
for number in (6129, 6135, 6139, 6140):
    c = campaign(number)
    p = c / 'summary.json'
    summary = json.loads(p.read_text())
    for rel, digest in summary['hashes'].items():
        assert sha(root / rel) == digest, rel
    bind(p)
    if number in (6139, 6140):
        assert summary['decisions']['complete_equal']
        jobs.append(str(json.loads((c / 'launch.json').read_text())['job_id']))
c = campaign(6136)
m = json.loads((c / 'manifest.json').read_text())
assert m['release_byte_identical_to_6107']
assert sha(c / 'build-summary.json') == m['build_summary_sha256']
for rel, digest in m['binaries'].items():
    assert sha(root / rel) == digest, rel
for rel, digest in m['files'].items():
    assert sha(root / m['source'] / rel) == sha(c / 'source' / rel) == digest, rel
    bind(c / 'source' / rel)
for group in ('patches', 'logs'):
    for rel, digest in m[group].items():
        assert sha(c / rel) == digest, rel
        bind(c / rel)
for kind in ('release', 'journal'):
    assert '100% tests passed out of 99' in (c / 'logs' / f'{kind}-ctest.log').read_text()
assert 'transactions == 1' in (c / 'logs/old-negative.log').read_text()
for name in ('manifest.json', 'build-summary.json'):
    bind(c / name)
bind(root / 'docs/implementation-notes.md')
'''
marker = 'checks = {}\n'
assert code.count(marker) == 1
code = code.replace(marker, extra + '\n' + marker)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

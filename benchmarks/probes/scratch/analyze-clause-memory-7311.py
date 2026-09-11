#!/usr/bin/env python3
"""Apply the tested external-memory analysis to the current Release pair."""
from pathlib import Path

base = Path(__file__).with_name('analyze-proc-memory-7300.py')
code = base.read_text()
changes = {
    '20260911-7299-proc-memory-rank31': '20260911-7310-clause-memory-rank31',
    'resident-analysis-7300.json': 'resident-analysis-7311.json',
    "    data['release_rss_kib'] = {'candidate': 562920, 'previous': 495156}[arm]":
        """    manifest = json.loads((c / 'manifest.json').read_text())
    oracle = manifest['release_records'][arm]
    bind(root / oracle['path'], oracle['sha256'])
    reference = json.loads((root / oracle['path']).read_text())
    assert reference['arm'] == arm and reference['rank'] == 31
    assert reference['solve']['peak_rss_kb'] == oracle['peak_rss_kib']
    data['release_rss_kib'] = oracle['peak_rss_kib']""",
    'bind(Path(__file__))':
        "bind(Path(__file__))\nbind(Path(__file__).with_name('analyze-proc-memory-7300.py'))",
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'),
     {'__file__': str(Path(__file__)), '__name__': '__main__'})

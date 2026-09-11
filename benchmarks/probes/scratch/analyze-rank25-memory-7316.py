#!/usr/bin/env python3
"""Locate each solver's own reproduced Rank25 peak; no cross-work assumption."""
from pathlib import Path
base = Path(__file__).with_name('analyze-proc-memory-7300.py')
code = base.read_text()
changes = {
    '20260911-7299-proc-memory-rank31': '20260911-7315-rank25-native-memory',
    'resident-analysis-7300.json': 'resident-analysis-7316.json',
    "    data['release_rss_kib'] = {'candidate': 562920, 'previous': 495156}[arm]":
        """    manifest = json.loads((c / 'manifest.json').read_text())
    oracle = manifest['release_records'][arm]
    bind(root / oracle['path'], oracle['sha256'])
    reference = json.loads((root / oracle['path']).read_text())
    assert reference['rank'] == 25 and reference['expected'] == 'SAT'
    assert reference['solve']['peak_rss_kb'] == oracle['peak_rss_kib']
    data['release_rss_kib'] = oracle['peak_rss_kib']
    data['solver'] = oracle['solver']""",
    'bind(Path(__file__))':
        "bind(Path(__file__))\nbind(Path(__file__).with_name('analyze-proc-memory-7300.py'))",
    'Exact work/certificates permit provisional work while independent proofs remain pending.':
        'Exact own-work/certificates permit provisional interpretation only when own Release RSS is reproduced. These are different solvers and their work is not equal; independent original SAT models and zero residue remain required.',
}
for before, after in changes.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

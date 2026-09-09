#!/usr/bin/env python3
"""Require the entire cached100K observation before locating a later split."""
from pathlib import Path
source = Path(__file__).with_name('validate-priority-prefixes-6142.py')
code = source.read_text()
changes = {
    '((6139, 21), (6140, 4))': '((6211, 50),)',
    "f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'": "'benchmarks/runs/20260909-6211-rank50-extended-prefix'",
    "        assert prefix['common'] == prefix['common_reached'] == 1":
    "        assert prefix['common'] == 139633 and prefix['common_reached'] == 1\n        assert prefix['common_hash'] == '236dcfa1a362af4d'",
    "        assert first[3] == prefix['first'] == prefix['common_last'] and last[3] == prefix['last']":
    "        assert first[3] == prefix['first'] and last[3] == prefix['last']",
}
for old, new in changes.items():
    assert code.count(old) == 1, old
    code = code.replace(old, new)
marker = '    difference = first_stream_difference('
assert code.count(marker) == 1
code = code.replace(marker, """    parent_summary = root / m['parent_summary']['path']
    assert sha(parent_summary) == m['parent_summary']['sha256']
    cached = json.loads(parent_summary.read_text())
    for rel, digest in cached['hashes'].items():
        assert sha(root / rel) == digest, rel
    for arm in ('candidate', 'kissat'):
        assert prefixes[arm]['common_last'] == cached['prefixes'][arm]['last']
        old_result = json.loads((parent_summary.parent / 'results' / (arm + '.json')).read_text())
        for name, digest in old_result['trace_sha256'].items():
            if name.endswith('.prefix.tsv'): continue
            old_path = parent_summary.parent / 'traces' / name
            with old_path.open('rb') as before, (c / 'traces' / name).open('rb') as after:
                while True:
                    block = before.read(1 << 20)
                    if not block: break
                    assert after.read(len(block)) == block, name
    # Cached traces have all been validated; no earlier closure is inferred
    # from selected decision counts or endpoint coincidence alone.
""" + marker)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

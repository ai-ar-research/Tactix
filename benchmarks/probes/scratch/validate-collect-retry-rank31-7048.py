#!/usr/bin/env python3
"""Three-way full-prefix comparison localizes the adverse transfer without guessing."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_stream_accounting, first_stream_difference
reader = Path(__file__).with_name('localize-root-lock-rank37-6073.py')
spec = importlib.util.spec_from_file_location('portable_boundaries', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
c = root / 'benchmarks/runs/20260910-7046-collect-retry-rank31'
target = c / 'summary-7048.json'
assert not target.exists()
if any(not (c / 'results' / (a + '.json')).exists() for a in ('candidate', 'previous')):
    print('Rank31 paired diagnostics pending; no summary written'); sys.exit(0)
h.fixtures()
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or expected == digest, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['hashes'].items(): bind(c / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for p in (c / 'manifest.json', c / 'launch.json', reader, Path(__file__)): bind(p)
streams, prefixes, projected, ignored = {}, {}, {}, {}
for arm, directory in [('candidate', c), ('previous', c), ('kissat', root / m['cached_native'])]:
    path = directory / 'results' / (arm + '.json'); bind(path)
    r = json.loads(path.read_text())
    assert r['exit_code'] == 0 and r['arm'] == arm and r['partial_certificate']['bytes'] > 0
    assert r['scratch_deleted'] and not Path(r['scratch']).exists()
    bind(directory / 'pins' / arm, r['binary_sha256'])
    for name, digest in r['trace_sha256'].items(): bind(directory / 'traces' / name, digest)
    prefix = read_prefix(directory / 'traces' / (arm + '.prefix.tsv'))
    assert prefix['conflict'] == m['stop_conflict'] == 300000
    assert prefix['solver'] == ('kissat' if arm == 'kissat' else 'tactix')
    stream = directory / 'traces' / (arm + '.decisions.tsv')
    count = int(subprocess.check_output(['wc', '-l', stream], text=True).split()[0])
    first = list(map(int, subprocess.check_output(['head', '-n', '1', stream], text=True).split()))
    last = list(map(int, subprocess.check_output(['tail', '-n', '1', stream], text=True).split()))
    validate_stream_accounting(prefix, first, last, count)
    prefixes[arm], streams[arm] = prefix, stream
    with (directory / 'traces' / (arm + '.boundaries.tsv')).open() as inp:
        projected[arm], ignored[arm] = h.parse(inp, arm == 'kissat')
comparisons = {}
for a, b in [('candidate', 'previous'), ('candidate', 'kissat'), ('previous', 'kissat')]:
    difference = first_stream_difference(streams[a], streams[b])
    near = {}
    for arm, key in ((a, 'left'), (b, 'right')):
        row = difference[key]
        if row is not None:
            near[arm] = [r for r in projected[arm] if r['conflicts'] <= row[0]][-16:]
    comparisons[a + '_vs_' + b] = dict(decisions=difference, preceding_boundaries=near,
        boundaries=h.compare(projected[a], projected[b]))
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, prefixes=prefixes, comparisons=comparisons, ignored_boundary_names=ignored,
        limitation='Complete three-way300K prefix, not final runtime attribution or full formula identity. If both Tactix arms remain equal here, the new adverse divergence is later; do not restore rejected5920 simply because an older native split persists.'), stream, indent=2)
    stream.write('\n')
print(json.dumps({name: r['decisions'] for name, r in comparisons.items()}, indent=2))

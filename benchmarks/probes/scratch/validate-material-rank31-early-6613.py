#!/usr/bin/env python3
"""Complete early-stream localization, with no inherited equality assumption."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_stream_accounting, first_stream_difference
c = root / 'benchmarks/runs/20260909-6611-material-rank31-early'
target = c / 'summary-6613.json'; assert not target.exists(), 'Frozen summary exists'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
for rel, digest in m['dependencies'].items(): assert sha(root / rel) == digest, rel
native = root / m['cached_native']; hashes = {}; prefixes = {}; streams = {}; counts = {}; projected = {}; ignored = {}
spec = importlib.util.spec_from_file_location('boundaries', Path(__file__).with_name('localize-root-lock-rank37-6073.py'))
bounds = importlib.util.module_from_spec(spec); spec.loader.exec_module(bounds); bounds.fixtures()
for name in ('manifest.json', 'launch.json'): hashes[str((c / name).relative_to(root))] = sha(c / name)
for arm, directory in (('candidate', c), ('kissat', native)):
    p = directory / 'results' / (arm + '.json'); r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
    assert r['binary_sha256'] == sha(directory / 'pins' / arm) and r['partial_certificate']['bytes'] > 0
    hashes[str(p.relative_to(root))] = sha(p)
    for rel, digest in r['trace_sha256'].items():
        p = directory / 'traces' / rel; assert sha(p) == digest, p
        hashes[str(p.relative_to(root))] = digest
    prefix = read_prefix(directory / 'traces' / (arm + '.prefix.tsv'))
    assert prefix['conflict'] == m['stop_conflict'] == 300000
    stream = directory / 'traces' / (arm + '.decisions.tsv')
    count = int(subprocess.check_output(['wc', '-l', str(stream)], text=True).split()[0])
    with stream.open('rb') as inp: first = list(map(int, inp.readline().split(b'\t')))
    last = list(map(int, subprocess.check_output(['tail', '-n', '1', str(stream)], text=True).split('\t')))
    validate_stream_accounting(prefix, first, last, count)
    prefixes[arm] = prefix; streams[arm] = stream; counts[arm] = count
    with (directory / 'traces' / (arm + '.boundaries.tsv')).open() as inp:
        projected[arm], ignored[arm] = bounds.parse(inp, arm == 'kissat')
difference = first_stream_difference(streams['candidate'], streams['kissat'])
near = {}
if not difference['complete_equal']:
    for arm, key in (('candidate', 'left'), ('kissat', 'right')):
        row = difference[key]
        if row is not None: near[arm] = [r for r in projected[arm] if r['conflicts'] <= row[0]][-16:]
search = {a: [r for r in rs if r['conflicts'] > 0] for a, rs in projected.items()}
out = dict(rank=31, stop=300000, prefixes=prefixes, complete_rows=counts, decisions=difference,
    boundaries_all=bounds.compare(projected['candidate'], projected['kissat']),
    boundaries_search=bounds.compare(search['candidate'], search['kissat']),
    last_boundaries_before_difference=near, ignored_boundary_names=ignored, hashes=hashes,
    limitation='Earliest selected decision difference in the complete0..300K stream, not causal explanation or full-state/effort equality. Partial proofs hashed/deleted; no final proof or acceptance timing.')
with target.open('x') as stream: json.dump(out, stream, indent=2)
print(json.dumps({k: out[k] for k in ('decisions', 'boundaries_search', 'last_boundaries_before_difference')}, indent=2))

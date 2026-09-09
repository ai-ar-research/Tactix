#!/usr/bin/env python3
"""Validate the shorter observer against its same-binary parent before scoring membership."""
import importlib.util
import io
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common

def module(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def exact_prefix(short, long):
    size = 0
    last = b''
    while block := short.read(1048576):
        assert long.read(len(block)) == block, 'observer changed the parent prefix'
        size += len(block)
        last = block[-1:]
    assert size and last == b'\n', 'empty or incomplete frame'
    return size

def main():
    membership = module('validate-rank32-membership-6210')
    selected = module('validate-rank32-retry-members-6219')
    membership.fixtures()
    selected.fixtures()
    assert exact_prefix(io.BytesIO(b'a\nb\n'), io.BytesIO(b'a\nb\nc\n')) == 4
    for a, b in ((b'', b'a\n'), (b'a', b'a\n'), (b'a\n', b'b\n'), (b'a\nb\n', b'a\n')):
        try: exact_prefix(io.BytesIO(a), io.BytesIO(b))
        except AssertionError: pass
        else: raise AssertionError('invalid prefix accepted')
    print('PASS: four negative prefix fixtures')
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6248-repaired-rank32-membership'
    assert not (c / 'summary.json').exists()
    m = json.loads((c / 'manifest.json').read_text())
    p = root / m['parent_summary']['path']
    assert sha(p) == m['parent_summary']['sha256']
    parent = p.parent
    s = json.loads(p.read_text())
    old = json.loads((parent / 'results/candidate.json').read_text())
    r = json.loads((c / 'results/candidate.json').read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins/candidate')
    hashes = {str(p.relative_to(root)): sha(p)}
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    for name, digest in r['trace_sha256'].items():
        p = c / 'traces' / name
        assert sha(p) == digest
        hashes[str(p.relative_to(root))] = digest
    prefixes = {}
    for suffix in ('boundaries', 'macro', 'retry-producer', 'decisions'):
        name = 'candidate.' + suffix + '.tsv'
        p = parent / 'traces' / name
        assert sha(p) == old['trace_sha256'][name] == s['hashes'][str(p.relative_to(root))]
        hashes[str(p.relative_to(root))] = sha(p)
        with (c / 'traces' / name).open('rb') as a, p.open('rb') as b:
            prefixes[name] = exact_prefix(a, b)
    prefix = read_prefix(c / 'traces/candidate.prefix.tsv')
    validate_common(prefix, stop=300000, common=69828932, digest='10d4b4ca139060f5')
    closure = root / 'benchmarks/runs/20260908-6060-root-lock-rank32-prefix/summary.json'
    assert sha(closure) == m['earlier_closure'][str(closure.relative_to(root))]
    prior = json.loads(closure.read_text())['new_prefix']
    for key in ('conflict', 'start_decisions', 'end_decisions', 'count', 'hash', 'first', 'last', 'max_level'):
        assert prefix[key] == prior[key], key
    native = root / 'benchmarks/runs/20260909-6208-rank32-schedule-membership'
    ns = json.loads((native / 'summary.json').read_text())
    assert ns['parent_replay']
    text = (c / 'traces/candidate.schedule.tsv').read_text()
    head, tail = text.split('selected\t', 1)
    states = selected.selected(io.StringIO('selected\t' + tail), 'tactix')
    ah, ar = membership.parse(io.StringIO(head), 'tactix', 386968)
    p = native / 'traces/kissat.schedule.tsv'
    assert sha(p) == ns['hashes'][str(p.relative_to(root))]
    hashes[str(p.relative_to(root))] = sha(p)
    with p.open() as stream: bh, br = membership.parse(stream, 'kissat', 386968)
    a, b = ({r['external']: r for r in rows} for rows in (ar, br))
    for v, state in states.items():
        assert state['position'] == (a[v]['position'] if v in a else None)
    shared = sorted(a.keys() & b.keys())
    for p in (c / 'manifest.json', c / 'results/candidate.json', native / 'summary.json', closure):
        hashes[str(p.relative_to(root))] = sha(p)
    result = dict(same_binary_parent_prefix=True, prefix_bytes=prefixes, prefix=prefix,
        headers=dict(candidate=ah, kissat=bh), selected=states,
        counts=dict(candidate=len(a), kissat=len(b)), shared=len(shared),
        candidate_only=[a[v] for v in sorted(a.keys() - b.keys())],
        kissat_only=[b[v] for v in sorted(b.keys() - a.keys())],
        shared_score_or_occurrence_differences=[dict(candidate=a[v], kissat=b[v]) for v in shared
            if any(a[v][k] != b[v][k] for k in ('score_bits', 'positive', 'negative'))],
        shared_displaced_positions=sum(a[v]['position'] != b[v]['position'] for v in shared),
        hashes=hashes, partial_certificate=r['partial_certificate'],
        limitation='Exact same-binary pre-stop decision/boundary/macro/retry prefixes and inherited 300K hash verified. Different stop lengths preclude partial-proof identity; deleted proof is hash-bound only, not checked. Membership does not establish full formula equivalence or tie causality. No runtime claim.')
    (c / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('hashes', 'headers', 'selected')}, indent=2))

if __name__ == '__main__': main()

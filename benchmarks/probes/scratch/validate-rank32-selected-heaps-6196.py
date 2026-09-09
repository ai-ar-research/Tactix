#!/usr/bin/env python3
"""Validate complete parent replay before interpreting selected heap snapshots."""
import io
import json
import math
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
SUBJECTS = (349460, 132302, 349446)

def parse(stream, solver):
    assert next(stream).rstrip('\n') == '# bve-heap-observer\t1'
    rounds = []
    keys = {'stage', 'trigger', 'subject', 'score', 'positive', 'negative', 'pos', 'root', 'size'}
    for line in stream:
        fields = line.rstrip('\n').split('\t')
        assert fields[0] == solver
        row = dict(f.split('=', 1) for f in fields[1:])
        assert set(row) == keys and len(fields) == len(keys) + 1
        row = {k: v if k == 'stage' else float.fromhex(v) if k == 'score' else int(v)
               for k, v in row.items()}
        assert math.isfinite(row['score']) and row['subject'] in SUBJECTS
        assert row['stage'] and all(v >= 0 for k, v in row.items() if k not in ('stage', 'score', 'pos'))
        assert -1 <= row['pos'] < row['size'] or (row['pos'] == -1 and row['size'] == 0)
        assert (row['root'] == 0) == (row['size'] == 0)
        if row['stage'] == 'initial' and row['subject'] == SUBJECTS[0]:
            if rounds: assert len(rounds[-1]) >= len(SUBJECTS)
            rounds.append([])
        assert rounds
        if len(rounds[-1]) < len(SUBJECTS):
            assert row['stage'] == 'initial' and row['subject'] == SUBJECTS[len(rounds[-1])]
            assert row['trigger'] == 0
        else:
            assert row['stage'] != 'initial'
        rounds[-1].append(row)
    assert rounds and len(rounds[-1]) >= len(SUBJECTS)
    return rounds

def fixtures():
    head = '# bve-heap-observer\t1\n'
    body = ''.join(f'tactix\tstage=initial\ttrigger=0\tsubject={s}\tscore=-0x1p+0\tpositive=1\tnegative=2\tpos=0\troot=9\tsize=3\n' for s in SUBJECTS)
    assert len(parse(io.StringIO(head + body + body), 'tactix')) == 2
    for before, after in (('tactix', 'kissat'), ('score=-0x1p+0', 'score=nan'),
                          ('subject=349460', 'subject=3'), ('size=3', 'size=-1'),
                          ('pos=0', 'pos=3'), ('positive=1', 'positive=-1'),
                          ('trigger=0', 'trigger=9'), ('root=9', 'root=0')):
        try: parse(io.StringIO(head + body.replace(before, after)), 'tactix')
        except (AssertionError, ValueError): pass
        else: raise AssertionError('invalid heap trace accepted')
    try: parse(io.StringIO(head + body.splitlines(True)[0]), 'tactix')
    except AssertionError: pass
    else: raise AssertionError('truncated initial accepted')
    print('PASS: two-round selected heap framing and nine negative fixtures')

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6194-rank32-selected-heaps'
    target = c / 'summary.json'
    assert not target.exists()
    if not all((c / 'results' / (a + '.json')).exists() for a in ('candidate', 'kissat')):
        print('Both final diagnostic records required; do not interpret partial observer output.'); return
    m = json.loads((c / 'manifest.json').read_text())
    p = root / m['parent_summary']['path']
    assert sha(p) == m['parent_summary']['sha256']
    parent = p.parent
    assert sha(root / m['alignment']['path']) == m['alignment']['sha256']
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
    hashes = {}; observed = {}
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json')
        r = json.loads(p.read_text()); old = json.loads((parent / 'results' / p.name).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
        assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
        assert r['partial_certificate'] == old['partial_certificate']
        assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.bve-heap.tsv'}
        for name, digest in old['trace_sha256'].items():
            assert r['trace_sha256'][name] == digest == sha(parent / 'traces' / name), name
        hashes[str(p.relative_to(root))] = sha(p)
        for name, digest in r['trace_sha256'].items():
            p = c / 'traces' / name
            assert sha(p) == digest, p
            hashes[str(p.relative_to(root))] = digest
        with (c / 'traces' / (arm + '.bve-heap.tsv')).open() as stream:
            rounds = parse(stream, 'tactix' if arm == 'candidate' else 'kissat')
        observed[arm] = dict(rounds=len(rounds), rows=[len(v) for v in rounds],
            initial=[v[:len(SUBJECTS)] for v in rounds], first_round=rounds[0])
    result = dict(parent_replay=True, subjects=SUBJECTS, observed=observed, hashes=hashes,
        manifest_sha256=sha(c / 'manifest.json'),
        limitation='Complete per-arm parent trace/proof identity is required. Selected snapshots do not prove full formula or heap identity; update callbacks have different granularity and stages across solvers.')
    target.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({a: {k: v for k, v in o.items() if k != 'first_round'} for a, o in observed.items()}, indent=2))

if __name__ == '__main__': main()

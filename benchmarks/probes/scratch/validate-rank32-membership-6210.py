#!/usr/bin/env python3
"""Validate observer neutrality, then compare complete initial schedule sets."""
import io
import json
import math
from pathlib import Path
import re
import struct
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

def parse(stream, solver, conflict):
    assert next(stream).rstrip('\n') == '# bve-schedule-observer\t1'
    head = next(stream).rstrip('\n').split('\t')
    assert len(head) == 2 and head[0] == solver
    parts = head[1].split('|'); header = dict(p.split('=', 1) for p in parts)
    assert len(header) == len(parts) and int(header['conflicts']) == conflict
    rows = []
    for line in stream:
        fields = line.rstrip('\n').split('\t'); assert fields[0] == 'member'
        row = dict(f.split('=', 1) for f in fields[1:])
        assert set(row) == {'external', 'position', 'positive', 'negative', 'score'} and len(fields) == 6
        assert re.fullmatch('[0-9a-f]{16}', row['score'])
        bits = row['score']; score = struct.unpack('>d', bytes.fromhex(bits))[0]
        assert math.isfinite(score)
        row = {k: int(v) for k, v in row.items() if k != 'score'}
        assert row['external'] > 0 and all(v >= 0 for v in row.values())
        row.update(score_bits=bits, score=score)
        assert row['position'] == len(rows)
        if rows: assert rows[(len(rows)-1)//2]['score'] >= score
        rows.append(row)
    assert len(rows) == int(header['scheduled']) and len({r['external'] for r in rows}) == len(rows)
    return header, rows

def fixtures():
    head = '# bve-schedule-observer\t1\ntactix\tconflicts=7|scheduled=2\n'
    rows = ('member\texternal=1\tposition=0\tpositive=2\tnegative=1\tscore=4010000000000000\n'
            'member\texternal=2\tposition=1\tpositive=1\tnegative=3\tscore=4008000000000000\n')
    assert len(parse(io.StringIO(head + rows), 'tactix', 7)[1]) == 2
    for before, after in (('conflicts=7', 'conflicts=8'), ('scheduled=2', 'scheduled=3'),
                          ('external=2', 'external=1'), ('external=2', 'external=0'),
                          ('position=1', 'position=0'), ('positive=2', 'positive=-1'),
                          ('4010000000000000', '7ff8000000000000'),
                          ('4008000000000000', '4020000000000000')):
        try: parse(io.StringIO((head + rows).replace(before, after)), 'tactix', 7)
        except (AssertionError, ValueError): pass
        else: raise AssertionError('bad schedule accepted')
    print('PASS: full membership framing and eight negative fixtures')

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6208-rank32-schedule-membership'
    assert not (c / 'summary.json').exists()
    if not all((c / 'results' / (a + '.json')).exists() for a in ('candidate', 'kissat')):
        print('Both completed diagnostic records required; interpretation pending.'); return
    m = json.loads((c / 'manifest.json').read_text())
    parent_path = root / m['parent_summary']['path']
    assert sha(parent_path) == m['parent_summary']['sha256']
    parent = parent_path.parent; old_summary = json.loads(parent_path.read_text())
    assert old_summary['parent_replay']
    build_path = root / m['observer_build']['path']
    assert sha(build_path) == m['observer_build']['sha256']
    build = json.loads(build_path.read_text()); assert build['release_exact_parent']
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    hashes = {}; schedules = {}; headers = {}
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json'); r = json.loads(p.read_text())
        old = json.loads((parent / 'results' / p.name).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
        expected = ('build/pin-6206-schedule-continue-trace/tactix' if arm == 'candidate' else
                    'tools/solvers/variants/kissat-schedule-continue-6206/build/kissat')
        assert r['binary_sha256'] == sha(c / 'pins' / arm) == build['binaries'][expected]
        assert r['partial_certificate'] == old['partial_certificate']
        assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.schedule.tsv'}
        for name, digest in old['trace_sha256'].items():
            assert r['trace_sha256'][name] == digest == sha(parent / 'traces' / name), name
        hashes[str(p.relative_to(root))] = sha(p)
        for name, digest in r['trace_sha256'].items():
            p = c / 'traces' / name; assert sha(p) == digest
            hashes[str(p.relative_to(root))] = digest
        with (c / 'traces' / (arm + '.schedule.tsv')).open() as stream:
            headers[arm], rows = parse(stream, 'tactix' if arm == 'candidate' else 'kissat', 386968)
        schedules[arm] = {r['external']: r for r in rows}
        for selected in old_summary['observed'][arm]['initial'][0]:
            row = schedules[arm][selected['subject']]
            assert len(rows) == selected['size'] and rows[0]['external'] == selected['root']
            for a, b in (('position', 'pos'), ('score', 'score'), ('positive', 'positive'), ('negative', 'negative')):
                assert row[a] == selected[b]
    a, b = schedules['candidate'], schedules['kissat']; shared = sorted(a.keys() & b.keys())
    result = dict(parent_replay=True, headers=headers,
        counts={arm: len(v) for arm, v in schedules.items()}, shared=len(shared),
        candidate_only=[a[v] for v in sorted(a.keys() - b.keys())],
        kissat_only=[b[v] for v in sorted(b.keys() - a.keys())],
        shared_score_or_occurrence_differences=[dict(candidate=a[v], kissat=b[v]) for v in shared
            if any(a[v][k] != b[v][k] for k in ('score_bits', 'positive', 'negative'))],
        shared_displaced_positions=sum(a[v]['position'] != b[v]['position'] for v in shared),
        hashes=hashes, manifest_sha256=sha(c / 'manifest.json'),
        limitation='Full changed-observer parent replayPASS. Initial membership/score differences precede attempted order split; their eligibility/retry producer and relative strength remain unproved. No full-formula equivalence, runtime or acceptance claim.')
    (c / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('hashes', 'headers')}, indent=2))

if __name__ == '__main__': main()

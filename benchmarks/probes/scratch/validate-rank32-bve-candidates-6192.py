#!/usr/bin/env python3
"""Require full observer neutrality, then compare BVE attempts without tuning."""
import io
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

def candidates(stream, solver, conflict):
    assert next(stream).rstrip('\n') == '# bve-candidate-observer\t1'
    rows = []
    required = {'external', 'stage', 'conflicts', 'ticks', 'limit', 'applied', 'value'}
    for line in stream:
        fields = line.rstrip('\n').split('\t')
        assert fields[0] == solver and all('=' in f for f in fields[1:])
        row = dict(f.split('=', 1) for f in fields[1:])
        assert len(row) == len(fields)-1 and required <= row.keys()
        assert row['stage'] in ('begin', 'end', 'postflush')
        row = {k: v if k == 'stage' else int(v) for k, v in row.items()}
        assert row['conflicts'] == conflict and row['external'] > 0
        assert row['applied'] in (0, 1) and row['value'] in (-1, 0, 1)
        assert all(v >= 0 for k, v in row.items() if k not in ('stage', 'value'))
        rows.append(row)
    assert rows
    return rows

def attempts(rows):
    groups = []; current = None
    for row in rows:
        stage = row['stage']
        if stage == 'begin':
            assert current is None or 'end' in current
            if current: groups.append(current)
            assert row['applied'] == 0
            current = {'begin': row}
        else:
            assert current is not None and stage not in current
            assert stage == 'end' or 'end' in current
            assert row['external'] == current['begin']['external']
            current[stage] = row
    assert current is not None and 'end' in current
    groups.append(current)
    return groups

def fixtures():
    base = 'tactix\texternal=9\tstage={}\tconflicts=386968\tticks=5\tlimit=6\tapplied=0\tvalue=0\n'
    good = '# bve-candidate-observer\t1\n' + ''.join(base.format(s) for s in ('begin', 'end', 'postflush'))
    parse = lambda s: candidates(io.StringIO(s), 'tactix', 386968)
    assert len(attempts(parse(good))) == 1
    for before, after in (('external=9', 'external=0'), ('stage=begin', 'stage=bogus'),
            ('conflicts=386968', 'conflicts=0'), ('ticks=5', 'ticks=-1'),
            ('applied=0', 'applied=2'), ('value=0', 'value=2'), ('ticks=5', 'value=0')):
        try: parse(good.replace(before, after))
        except (AssertionError, ValueError): pass
        else: raise AssertionError('invalid candidate row accepted')
    for rows in (parse(good)[1:], parse(good)[:1], list(reversed(parse(good)))):
        try: attempts(rows)
        except AssertionError: pass
        else: raise AssertionError('invalid candidate stages accepted')
    print('PASS: candidate schema/grouping and10 negative fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6188-rank32-bve-candidates'
    assert not (c / 'summary.json').exists()
    if not all((c / 'results' / (a + '.json')).exists() for a in ('candidate', 'kissat')):
        print('Parent replay awaits published results from both arms'); return
    m = json.loads((c / 'manifest.json').read_text())
    parent_summary = root / m['parent_summary']['path']
    assert sha(parent_summary) == m['parent_summary']['sha256']
    parent = parent_summary.parent
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
    rows = {}; groups = {}; hashes = {}
    for name in ('manifest.json', 'launch.json'):
        hashes[str((c / name).relative_to(root))] = sha(c / name)
    for arm in ('candidate', 'kissat'):
        path = c / 'results' / (arm + '.json'); r = json.loads(path.read_text())
        old = json.loads((parent / 'results' / path.name).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
        assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
        assert r['partial_certificate'] == old['partial_certificate'], arm
        assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.bve-candidates.tsv'}
        for name, digest in old['trace_sha256'].items():
            assert r['trace_sha256'][name] == digest == sha(parent / 'traces' / name), name
        for name, digest in r['trace_sha256'].items():
            p = c / 'traces' / name
            assert sha(p) == digest, p
            hashes[str(p.relative_to(root))] = digest
        hashes[str(path.relative_to(root))] = sha(path)
        with (c / 'traces' / (arm + '.bve-candidates.tsv')).open() as inp:
            rows[arm] = candidates(inp, 'tactix' if arm == 'candidate' else 'kissat', 386968)
        groups[arm] = attempts(rows[arm])
    fields = ('external', 'stage', 'conflicts', 'applied', 'value')
    first = None
    for i, (a, b) in enumerate(itertools.zip_longest(rows['candidate'], rows['kissat'])):
        if a is None or b is None or any(a[k] != b[k] for k in fields):
            first = dict(index=i, candidate=a, kissat=b,
                preceding_candidate=rows['candidate'][max(0, i-6):i], preceding_kissat=rows['kissat'][max(0, i-6):i])
            break
    effort = None
    for i, (a, b) in enumerate(zip(groups['candidate'], groups['kissat'])):
        if a['begin']['external'] != b['begin']['external']: break
        da = a['end']['ticks'] - a['begin']['ticks']; db = b['end']['ticks'] - b['begin']['ticks']
        assert da >= 0 and db >= 0
        if da != db:
            effort = dict(attempt=i, candidate_delta=da, kissat_delta=db, candidate=a, kissat=b); break
    result = dict(parent_replay=True, compared_fields=fields,
        rows={a: len(v) for a, v in rows.items()}, attempts={a: len(v) for a, v in groups.items()},
        first_portable_difference=first, first_effort_delta_difference=effort, hashes=hashes,
        limitation='Exact full parent stream/proof replay proves observer neutrality. First candidate/outcome/effort difference is localization,not causal attribution or proof of weaker effect. Raw flags,roots,trail,limits retained,not assumed cross-solver equivalents.')
    (c / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'hashes'}, indent=2))

if __name__ == '__main__': main()

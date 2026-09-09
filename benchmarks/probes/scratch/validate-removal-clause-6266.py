#!/usr/bin/env python3
"""Validate full parent replay and complete pre-garbage payloads."""
import io
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

def parse(stream):
    rows = []
    for line in stream:
        assert line.endswith('\n')
        fields = line[:-1].split('\t'); assert fields[0] == 'kissat'
        row = dict(f.split('=', 1) for f in fields[1:])
        assert len(fields) == 9 and set(row) == {'conflicts', 'decisions', 'source', 'redundant', 'size', 'literals', 'values', 'eliminate'}
        for k in ('conflicts', 'decisions', 'redundant', 'size'): row[k] = int(row[k])
        assert row['conflicts'] == 372473 and row['decisions'] == 77569158
        assert row['redundant'] in (0, 1) and row['size'] >= 3 and row['source']
        for k in ('literals', 'values', 'eliminate'):
            assert row[k].endswith(',')
            row[k] = [int(v) for v in row[k][:-1].split(',')]
            assert len(row[k]) == row['size']
        assert all(row['literals']) and 399915 in map(abs, row['literals'])
        assert len(set(row['literals'])) == row['size']
        assert all(v in (-1, 0, 1) for v in row['values'])
        assert all(v in (0, 1) for v in row['eliminate'])
        rows.append(row)
    assert rows
    return rows

def main():
    f = 'kissat\tconflicts=372473\tdecisions=77569158\tsource=fixture\tredundant=0\tsize=3\tliterals=1,-399915,3,\tvalues=0,0,0,\teliminate=1,0,1,\n'
    assert len(parse(io.StringIO(f))) == 1
    bad = ['', f[:-1]] + [f.replace(a, b) for a,b in (
        ('conflicts=372473', 'conflicts=1'), ('decisions=77569158', 'decisions=1'),
        ('size=3', 'size=4'), ('1,-399915,3,', '1,-2,3,'), ('1,-399915,3,', '1,-399915,1,'),
        ('values=0,0,0,', 'values=0,2,0,'), ('eliminate=1,0,1,', 'eliminate=1,0,2,'),
        ('source=fixture', 'source='), ('redundant=0', 'redundant=2'))]
    for text in bad:
        try: parse(io.StringIO(text))
        except (AssertionError, ValueError): pass
        else: raise AssertionError('bad removal payload accepted')
    print('PASS: eleven negative full-payload fixtures')
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6264-rank32-removal-clause'
    assert not (c / 'summary.json').exists()
    if not (c / 'results/kissat.json').exists(): print('Diagnostic result pending'); return
    m = json.loads((c / 'manifest.json').read_text()); hashes = {}
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    p = root / m['parent_summary']['path']; assert sha(p) == m['parent_summary']['sha256']
    assert json.loads(p.read_text())['parent_replay']; parent = p.parent
    old = json.loads((parent / 'results/kissat.json').read_text())
    r = json.loads((c / 'results/kissat.json').read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    b = root / m['observer_build']['path']; assert sha(b) == m['observer_build']['sha256']
    build = json.loads(b.read_text())
    assert sha(c / 'pins/kissat') == r['binary_sha256'] == build['binary_sha256']
    assert r['partial_certificate'] == old['partial_certificate']
    assert set(r['trace_sha256']) - set(old['trace_sha256']) == {'kissat.removed-clause.tsv'}
    for name, digest in old['trace_sha256'].items():
        assert sha(parent / 'traces' / name) == r['trace_sha256'][name] == digest
    for name, digest in r['trace_sha256'].items():
        p = c / 'traces' / name; assert sha(p) == digest; hashes[str(p.relative_to(root))] = digest
    with (c / 'traces/kissat.removed-clause.tsv').open() as stream: rows = parse(stream)
    for p in (c / 'manifest.json', c / 'results/kissat.json', b, parent / 'summary.json'):
        hashes[str(p.relative_to(root))] = sha(p)
    result = dict(parent_replay=True, removed_clauses=rows, hashes=hashes,
        limitation='Complete native6256 traces and partial proof replay exactly. Pre-garbage snapshots identify native producer source, not the corresponding Tactix formula or why it did not publish. No solver timing or final proof claim.')
    (c / 'summary.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(rows, indent=2))

if __name__ == '__main__': main()

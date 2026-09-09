#!/usr/bin/env python3
"""Bind the entire parent observation before reading eligibility/mark history."""
import io
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
SUBJECTS = {337674, 390671, 399915, 409823, 444046}

def selected(stream, solver):
    result = {}
    keys = {'external', 'position', 'positive', 'negative', 'value'} | (
        {'level', 'retired'} if solver == 'tactix' else {'active', 'fixed', 'eliminated'})
    for line in stream:
        fields = line.rstrip('\n').split('\t'); assert fields[0] == 'selected'
        row = dict(f.split('=', 1) for f in fields[1:])
        assert len(fields) == len(keys) + 1 and set(row) == keys
        row = {k: None if k == 'position' and v == '-' else int(v) for k, v in row.items()}
        v = row['external']; assert v in SUBJECTS and v not in result
        assert row['value'] in (-1, 0, 1)
        assert all(x is None or x >= 0 for k, x in row.items() if k != 'value')
        for k in ('retired', 'active', 'fixed', 'eliminated'):
            if k in row: assert row[k] in (0, 1)
        result[v] = row
    assert set(result) == SUBJECTS
    return result

def producers(stream, solver):
    rows = []
    keys = {'conflicts', 'decisions', 'external', 'source', 'before', 'after', 'value'} | (
        {'retired', 'clause'} if solver == 'tactix' else {'probing'})
    for line in stream:
        fields = line.rstrip('\n').split('\t'); assert fields[0] == solver
        row = dict(f.split('=', 1) for f in fields[1:])
        assert len(fields) == len(keys) + 1 and set(row) == keys
        row = {k: v if k in ('source', 'clause') else int(v) for k, v in row.items()}
        assert row['external'] == 337674 and 0 <= row['conflicts'] <= 386968
        assert row['before'] in (0, 1) and row['after'] in (0, 1) and row['value'] in (-1, 0, 1)
        assert row['source'] and row['decisions'] >= 0
        if rows: assert rows[-1]['conflicts'] <= row['conflicts']
        if 'clause' in row:
            assert not row['clause'] or row['clause'].endswith(',')
            row['clause'] = [int(v) for v in row['clause'].split(',') if v]
            assert all(row['clause'])
        rows.append(row)
    return rows

def fixtures():
    text = ''.join(f'selected\texternal={v}\tposition=-\tpositive=2\tnegative=2\tvalue=0\tlevel=0\tretired=0\n' for v in sorted(SUBJECTS))
    assert len(selected(io.StringIO(text), 'tactix')) == 5
    for bad in (text + text.splitlines(True)[0], ''.join(text.splitlines(True)[:-1]),
                text.replace('retired=0', 'retired=2'), text.replace('positive=2', 'positive=-1')):
        try: selected(io.StringIO(bad), 'tactix')
        except (AssertionError, ValueError): pass
        else: raise AssertionError('invalid selected-state frame accepted')
    row = 'kissat\tconflicts=2\tdecisions=3\texternal=337674\tsource=remove\tbefore=0\tafter=1\tvalue=0\tprobing=0\n'
    assert len(producers(io.StringIO(row), 'kissat')) == 1
    for before, after in (('external=337674', 'external=1'), ('before=0', 'before=2'), ('conflicts=2', 'conflicts=386969')):
        try: producers(io.StringIO(row.replace(before, after)), 'kissat')
        except AssertionError: pass
        else: raise AssertionError('invalid producer accepted')
    print('PASS: selected-state and retry-producer schema with seven negative controls')

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6217-rank32-retry-members'
    assert not (c / 'summary.json').exists()
    if not all((c / 'results' / (a + '.json')).exists() for a in ('candidate', 'kissat')):
        print('Both completed records required; no partial interpretation.'); return
    m = json.loads((c / 'manifest.json').read_text())
    p = root / m['parent_summary']['path']; assert sha(p) == m['parent_summary']['sha256']
    parent = p.parent; s = json.loads(p.read_text()); assert s['parent_replay']
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    states = {}; histories = {}; hashes = {}
    for arm, solver in (('candidate', 'tactix'), ('kissat', 'kissat')):
        p = c / 'results' / (arm + '.json'); r = json.loads(p.read_text())
        old = json.loads((parent / 'results' / p.name).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
        assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
        assert r['partial_certificate'] == old['partial_certificate']
        assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.retry-producer.tsv'}
        for name, digest in old['trace_sha256'].items():
            assert sha(parent / 'traces' / name) == digest
            if name.endswith('.schedule.tsv'):
                before = (parent / 'traces' / name).read_bytes()
                with (c / 'traces' / name).open('rb') as stream:
                    assert stream.read(len(before)) == before
                    tail = stream.read().decode()
                assert tail.endswith('\n'); states[arm] = selected(io.StringIO(tail), solver)
            else: assert r['trace_sha256'][name] == digest
        hashes[str(p.relative_to(root))] = sha(p)
        for name, digest in r['trace_sha256'].items():
            p = c / 'traces' / name; assert sha(p) == digest; hashes[str(p.relative_to(root))] = digest
        with (c / 'traces' / (arm + '.retry-producer.tsv')).open() as stream:
            histories[arm] = producers(stream, solver)
    result = dict(parent_replay=True, selected=states, producer_rows={a: len(v) for a, v in histories.items()},
        producer_history=histories, hashes=hashes, manifest_sha256=sha(c / 'manifest.json'),
        limitation='Complete parent trace/proof replay and exact appended selected-state rows verified. Source strings and marks are not assumed equivalent; these histories omit candidate-decline flag clears and do not alone prove the first missing producer.')
    (c / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('hashes', 'producer_history')}, indent=2))
    for arm, rows in histories.items(): print(arm, 'last20', json.dumps(rows[-20:], indent=2))

if __name__ == '__main__': main()

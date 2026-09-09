#!/usr/bin/env python3
"""Bind both parent observations before reading the newly selected producer."""
import io
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix, validate_common
import importlib.util
def module(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m

def producers(stream, solver):
    rows = []
    keys = {'conflicts', 'decisions', 'external', 'source', 'before', 'after', 'value'} | (
        {'retired', 'clause'} if solver == 'tactix' else {'probing'})
    for line in stream:
        assert line.endswith('\n')
        fields = line.rstrip('\n').split('\t'); assert fields[0] == solver
        row = dict(f.split('=', 1) for f in fields[1:])
        assert len(fields) == len(keys) + 1 and set(row) == keys
        row = {k: v if k in ('source', 'clause') else int(v) for k, v in row.items()}
        assert row['external'] == 399915 and 0 <= row['conflicts'] <= 386968
        assert row['before'] in (0, 1) and row['after'] in (0, 1) and row['value'] in (-1, 0, 1)
        assert row['source'] and row['decisions'] >= 0
        if rows: assert rows[-1]['conflicts'] <= row['conflicts']
        assert row.get('retired', row.get('probing')) in (0, 1)
        if 'clause' in row:
            assert not row['clause'] or row['clause'].endswith(',')
            row['clause'] = [int(v) for v in row['clause'].split(',') if v]
            assert all(row['clause'])
        rows.append(row)
    assert rows
    return rows

def main():
    selected = module('validate-rank32-retry-members-6219'); selected.fixtures()
    prior = module('validate-repaired-membership-6254')
    fixture = 'kissat\tconflicts=2\tdecisions=3\texternal=399915\tsource=remove\tbefore=0\tafter=1\tvalue=0\tprobing=0\n'
    assert len(producers(io.StringIO(fixture), 'kissat')) == 1
    bad = ['', fixture[:-1], fixture + fixture.replace('conflicts=2', 'conflicts=1')]
    bad += [fixture.replace(a, b) for a, b in (('external=399915', 'external=337674'), ('before=0', 'before=2'),
        ('probing=0', 'probing=2'), ('value=0', 'value=2'), ('conflicts=2', 'conflicts=386969'))]
    for text in bad:
        try: producers(io.StringIO(text), 'kissat')
        except (AssertionError, ValueError): pass
        else: raise AssertionError('invalid producer frame accepted')
    print('PASS: eight negative selected399915 journal fixtures')
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260909-6256-rank32-remaining-retry'
    assert not (c / 'summary.json').exists()
    m = json.loads((c / 'manifest.json').read_text()); hashes = {}; histories = {}; prefixes = {}; states = {}
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
    for arm, solver, binding in (('candidate', 'tactix', m['parent_summary']), ('kissat', 'kissat', m['native_parent'])):
        p = root / binding['path']; assert sha(p) == binding['sha256']
        parent = p.parent; hashes[str(p.relative_to(root))] = sha(p)
        old = json.loads((parent / 'results' / (arm + '.json')).read_text())
        r = json.loads((c / 'results' / (arm + '.json')).read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
        assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
        for name, digest in r['trace_sha256'].items():
            p = c / 'traces' / name; assert sha(p) == digest; hashes[str(p.relative_to(root))] = digest
        if arm == 'candidate':
            assert r['partial_certificate'] == old['partial_certificate']
            assert set(r['trace_sha256']) == set(old['trace_sha256'])
            for name, digest in old['trace_sha256'].items():
                if name.endswith('.retry-producer.tsv'): continue
                assert sha(parent / 'traces' / name) == r['trace_sha256'][name] == digest
        else:
            for suffix in ('decisions', 'boundaries'):
                name = arm + '.' + suffix + '.tsv'
                assert sha(parent / 'traces' / name) == old['trace_sha256'][name]
                with (c / 'traces' / name).open('rb') as a, (parent / 'traces' / name).open('rb') as b:
                    prior.exact_prefix(a, b)
            before = (parent / 'traces/kissat.schedule.tsv').read_bytes()
            assert sha(parent / 'traces/kissat.schedule.tsv') == old['trace_sha256']['kissat.schedule.tsv']
            with (c / 'traces/kissat.schedule.tsv').open('rb') as stream:
                assert stream.read(len(before)) == before
                states[arm] = selected.selected(io.StringIO(stream.read().decode()), solver)
        prefix = read_prefix(c / 'traces' / (arm + '.prefix.tsv'))
        validate_common(prefix, stop=300000, common=69828932, digest='10d4b4ca139060f5')
        assert prefix['count'] == 69828932 and prefix['hash'] == '10d4b4ca139060f5'
        prefixes[arm] = prefix
        with (c / 'traces' / (arm + '.retry-producer.tsv')).open() as stream: histories[arm] = producers(stream, solver)
        p = c / 'results' / (arm + '.json'); hashes[str(p.relative_to(root))] = sha(p)
    result = dict(parent_replay=True, prefixes=prefixes, selected=states, producer_history=histories,
        hashes=hashes, manifest_sha256=sha(c / 'manifest.json'),
        limitation='Candidate complete parent trace/proof identity; native shorter decision/boundary prefixes and full schedule plus selected tail. Native different stops prohibit proof identity. Producer strings do not identify the containing phase; mark clears are not journaled. No timing or final correctness claim.')
    (c / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(histories, indent=2))

if __name__ == '__main__': main()

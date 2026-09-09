#!/usr/bin/env python3
"""Strict untruncated learning rows and full observer neutrality checks."""
import csv
import hashlib
import io
import json
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
C = ROOT / 'benchmarks/runs/20260908-6035-learned-stage'
KEY = {53, 51, -47, -93630, -4166, -4349, 4172}
FIELDS = 'seq event conflicts decisions level size literals'.split()
EVENTS = {'learned_raw', 'learned_sorted', 'learned_shrunk', 'learned'}
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
def parse(stream, lower=240062, upper=247206):
    assert next(stream).rstrip() == '# learned-stage\t1'
    assert next(stream).removeprefix('# ').rstrip().split('\t') == FIELDS
    rows = []; previous = (lower, 0)
    for fields in csv.reader(stream, delimiter='\t'):
        assert len(fields) == len(FIELDS)
        r = dict(zip(FIELDS, fields))
        for name in ('seq', 'conflicts', 'decisions', 'level', 'size'): r[name] = int(r[name]); assert r[name] >= 0
        assert r['seq'] == len(rows) and r['event'] in EVENTS and lower <= r['conflicts'] <= upper
        assert r['conflicts'] >= previous[0] and r['decisions'] >= previous[1]
        previous = (r['conflicts'], r['decisions'])
        if r['size']:
            r['literals'] = list(map(int, r['literals'].split(',')))
            assert len(r['literals']) == r['size'] and all(r['literals'])
            assert len(set(map(abs, r['literals']))) == r['size']
        else:
            assert r['literals'] == '-'; r['literals'] = []
        rows.append(r)
    assert rows
    return rows
def fixtures():
    head = '# learned-stage\t1\n# ' + '\t'.join(FIELDS) + '\n'
    good = head + '0\tlearned_raw\t1\t5\t0\t3\t1,-2,3\n1\tlearned\t2\t6\t0\t0\t-\n'
    assert len(parse(io.StringIO(good), 1, 2)) == 2
    long = head + '0\tlearned_raw\t1\t5\t0\t2048\t' + ','.join(map(str, range(1, 2049))) + '\n'
    assert parse(io.StringIO(long), 1, 2)[0]['literals'][-1] == 2048
    for old, new in [('1,-2,3', '1,-2'), ('1,-2,3', '1,-1,3'), ('1,-2,3', '1,0,3'),
                     ('1\tlearned', '3\tlearned'), ('learned_raw', 'bad-event'),
                     ('\t2\t6', '\t3\t6'), ('\t2\t6', '\t2\t4'), ('\t0\t-\n', '\t0\t1\n')]:
        try: parse(io.StringIO(good.replace(old, new)), 1, 2)
        except (AssertionError, ValueError, StopIteration): pass
        else: raise AssertionError('Malformed learning row accepted')
    print('PASS: inclusive learning window, empty/2048-literal clauses and eight malformed-row controls', flush=True)
def main():
    fixtures()
    if '--self-test' in sys.argv: return
    target = C / 'summary.json'; assert not target.exists()
    m = json.loads((C / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(C / rel) == digest, rel
    arms, evidence = {}, {}
    for arm in ('candidate', 'kissat'):
        p = C / 'results' / (arm + '.json'); r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'] and r['binary_sha256'] == sha(C / 'pins' / arm)
        parentp = ROOT / m['parent_campaign'] / 'results' / (arm + '.json'); parent = json.loads(parentp.read_text())
        assert r['partial_certificate'] == parent['partial_certificate']
        for name, digest in parent['trace_sha256'].items(): assert r['trace_sha256'][name] == digest, name
        assert set(r['trace_sha256']) - set(parent['trace_sha256']) == {arm + '.learned-stage.tsv'}
        for name, digest in r['trace_sha256'].items(): assert sha(C / 'traces' / name) == digest, name
        with (C / 'traces' / (arm + '.learned-stage.tsv')).open() as stream: rows = parse(stream)
        arms[arm] = rows
        evidence[arm] = dict(result_sha256=sha(p), parent_result_sha256=sha(parentp), trace_sha256=r['trace_sha256'], rows=len(rows))
    selected = {arm: [r for r in rows if set(r['literals']) == KEY] for arm, rows in arms.items()}
    def keyed(rows):
        result, counts = {}, {}
        for r in rows:
            key = (r['conflicts'], r['event']); n = counts.get(key, 0); counts[key] = n + 1
            result[key + (n,)] = r
        return result
    a, b = keyed(arms['candidate']), keyed(arms['kissat'])
    first = {}
    for key in sorted(set(a) | set(b)):
        x, y = a.get(key), b.get(key)
        if key[1] not in first and (x is None or y is None or sorted(x['literals']) != sorted(y['literals'])):
            first[key[1]] = dict(key=key, candidate=x, kissat=y)
    selected_conflicts = {r['conflicts'] for rows in selected.values() for r in rows}
    out = dict(full_parent_replay=True, evidence=evidence, selected_clause_rows=selected,
        selected_conflict_stages={arm: [r for r in rows if r['conflicts'] in selected_conflicts] for arm, rows in arms.items()},
        first_canonical_difference_by_stage=first,
        limitation='Learning stages are observational; missing final rows can result from a reused analysis path. Same conflict number alone does not prove identical incoming trail or reason graph. Require those witnesses before implementing a fix.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps({k:v for k,v in out.items() if k not in ('evidence', 'selected_conflict_stages')}, indent=2))
if __name__ == '__main__': main()

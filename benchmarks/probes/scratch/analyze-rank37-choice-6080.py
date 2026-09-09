#!/usr/bin/env python3
"""Separate score,availability and topology after independently validated replay."""
import hashlib
import json
from pathlib import Path
import sys

def heap(lines):
    assert lines[0].startswith(('# tactix-heap-set\t1', '# kissat-heap-set\t1'))
    assert lines[1] == '# external\tposition\tscore\tassigned\tactive'
    result = {}
    for index, line in enumerate(lines[2:]):
        row = line.split('\t')
        assert len(row) == 5 and int(row[1]) == index
        var = int(row[0])
        assert var > 0 and var not in result and row[3] in ('0', '1') and row[4] in ('0', '1')
        result[var] = dict(position=index, score=row[2], assigned=int(row[3]), active=int(row[4]))
    return result

def compare(left, right):
    shared = sorted(set(left) & set(right))
    changes = lambda key: [dict(variable=v, candidate=left[v][key], kissat=right[v][key]) for v in shared if left[v][key] != right[v][key]]
    return dict(candidate_only=sorted(set(left)-set(right)), kissat_only=sorted(set(right)-set(left)),
                score=changes('score'), assigned=changes('assigned'), active=changes('active'), position=changes('position'))

def fixtures():
    header = ['# tactix-heap-set\t1', '# external\tposition\tscore\tassigned\tactive']
    a = heap(header + ['1\t0\t1\t0\t1', '2\t1\t1\t0\t1'])
    b = heap(header + ['2\t0\t1\t0\t1', '1\t1\t1\t0\t1'])
    d = compare(a, b)
    assert len(d['position']) == 2 and not any(d[k] for k in d if k != 'position')
    b[1]['assigned'] = 1
    assert len(compare(a,b)['assigned']) == 1
    b[1]['score'] = '2'
    assert len(compare(a,b)['score']) == 1
    del b[2]
    assert compare(a,b)['candidate_only'] == [2]
    for bad in (['1\t1\t1\t0\t1'], ['1\t0\t1\t2\t1'], ['1\t0\t1\t0\t1', '1\t1\t1\t0\t1']):
        try:
            heap(header + bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('malformed heap accepted')
    print('PASS: topology-only,assignment,score,membership and three malformed-heap controls')

def main():
    fixtures()
    if '--self-test' in sys.argv:
        return
    root = Path(__file__).resolve().parents[3]
    c = root / 'benchmarks/runs/20260908-6076-root-lock-rank37-choice'
    target = c / 'state-analysis.json'
    assert not target.exists()
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    vpath = c / 'choice-validation.json'
    v = json.loads(vpath.read_text())
    assert 'Both arms replay every cached ordinary trace hash' in v['observer_neutrality']
    for rel, digest in v['hashes'].items():
        assert sha(root / rel) == digest, rel
    heaps, choices, hashes = {}, {}, {str(vpath.relative_to(root)): sha(vpath)}
    for arm in ('candidate', 'kissat'):
        record = json.loads((c / 'results' / (arm + '.json')).read_text())
        for kind in ('heap', 'choice'):
            p = c / 'traces' / f'{arm}.{kind}.tsv'
            assert sha(p) == record['trace_sha256'][p.name]
            hashes[str(p.relative_to(root))] = sha(p)
        heaps[arm] = heap((c / 'traces' / (arm + '.heap.tsv')).read_text().splitlines())
        choices[arm] = [line.split('\t') for line in (c / 'traces' / (arm + '.choice.tsv')).read_text().splitlines()]
        assert choices[arm] and all(len(r) == 18 and r[0] == '272979' and r[4] == '116362' for r in choices[arm])
    diffs = compare(heaps['candidate'], heaps['kissat'])
    selected = {arm: {str(var): data.get(var) for var in (116360,116362)} for arm, data in heaps.items()}
    around = {arm: [r for r in rows if 76953346 <= int(r[1]) <= 76953350] for arm, rows in choices.items()}
    before = {arm: [r for r in rows if int(r[1]) <= 76953348] for arm, rows in choices.items()}
    target_state = {arm: [r for r in rows if int(r[1]) == 76953348] for arm, rows in before.items()}
    assert all(len(rows) == 1 for rows in target_state.values())
    out = dict(heap_sizes={arm: len(h) for arm,h in heaps.items()}, changes=diffs,
        change_counts={key:len(value) for key,value in diffs.items()}, selected_heap_entries=selected,
        last_common_choice_state=target_state, around_first_split=around, hashes=hashes,
        fields='Choice rows:conflict,decision,level,chosen,wanted,value,assignment_level,heap_position,score,reason,stable,queue_stamp,queue_search,search_stamp,wanted_stamp,prev,next,chosen_stamp.',
        limitation='After-decision snapshot precedes subsequent propagation. Equal shared score/assignment/active metadata does not prove earlier formula or operation parity. Queue fields and absent-position sentinels need mode-aware normalization; post-split state may be a consequence.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps({k:v for k,v in out.items() if k not in ('changes','hashes')}, indent=2))

if __name__ == '__main__':
    main()

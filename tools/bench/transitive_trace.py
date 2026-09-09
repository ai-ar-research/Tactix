"""Validate v2 all-source transitive census; never infer formula equivalence."""
from collections import Counter

FIELDS = ('solver event source_sequence conflicts source candidate_ordinal '
          'destination ticks_before ticks_after redundant failed candidate_count '
          'out_degree source_internal destination_internal source_import '
          'destination_import source_reference destination_reference source_active '
          'destination_active source_value destination_value copies skip_reason').split()
EVENTS = {'source-begin', 'candidate', 'delete', 'source-end'}
SIGNED = {'source', 'destination', 'source_value', 'destination_value'}
TEXT = {'solver', 'event', 'skip_reason'}


def read_phase(stream, solver, conflict):
    """Check every frame/group, retaining only the requested conflict's rows.

    This deliberately accepts only ALL-source logs with an unmatched pair
    selector. Selected endpoint/walk/admission traces require another reader.
    Tactix omits source-end on empty sources; native Kissat emits it.
    An effort-limited source may process fewer than candidate_count candidates.
    """
    assert solver in ('tactix', 'kissat') and conflict >= 0
    assert next(stream) == '# tactix-transitive-trace\t2\n', 'schema'
    assert next(stream) == '# ' + '\t'.join(FIELDS) + '\n', 'columns'
    selected = []
    current = None
    sequence = -1
    last_conflict = -1
    last_ticks = -1
    total_rows = 0

    def finish():
        if current is None:
            return
        begin = current['begin']
        empty_tactix = solver == 'tactix' and begin['candidate_count'] == 0
        assert current['end'] is not None or empty_tactix, 'unclosed source'
        reduced = Counter(r['destination'] for r in current['candidates'] if r['redundant'])
        deleted = Counter(r['destination'] for r in current['deletes'])
        if solver == 'tactix':
            assert deleted == reduced, 'deletion/candidate mismatch'
        else:
            assert not deleted, 'unexpected native delete frame'
        if begin['conflicts'] == conflict:
            selected.append(current)

    for line in stream:
        assert line.endswith('\n'), 'truncated row'
        values = line[:-1].split('\t')
        assert len(values) == len(FIELDS), 'row width'
        row = dict(zip(FIELDS, values))
        assert row['solver'] == solver and row['event'] in EVENTS, 'solver/event'
        assert row['skip_reason'] == '-', 'not an all-source census'
        for key in FIELDS:
            if key not in TEXT:
                row[key] = int(row[key])
                if key not in SIGNED:
                    assert 0 <= row[key] <= (1 << 64) - 1, key
        assert row['source'] != 0, 'zero source'
        for key in ('source_value', 'destination_value'):
            assert row[key] in (-1, 0, 1), key
        for key in ('redundant', 'failed', 'source_active', 'destination_active'):
            assert row[key] in (0, 1), key
        assert row['ticks_after'] >= row['ticks_before'], 'negative effort'
        assert row['ticks_after'] >= last_ticks, 'effort counter went backwards'
        last_ticks = row['ticks_after']
        assert row['candidate_count'] <= row['out_degree'], 'candidate degree'
        assert row['conflicts'] >= last_conflict, 'conflict order'
        last_conflict = row['conflicts']
        event = row['event']
        if event == 'source-begin':
            finish()
            assert row['source_sequence'] == sequence + 1, 'source sequence'
            sequence += 1
            assert row['destination'] == row['candidate_ordinal'] == 0, 'begin frame'
            current = dict(begin=row, candidates=[], deletes=[], end=None)
        else:
            assert current is not None and current['end'] is None, 'orphan/closed source'
            begin = current['begin']
            for key in ('source_sequence', 'conflicts', 'source', 'candidate_count', 'out_degree'):
                assert row[key] == begin[key], 'source identity: ' + key
            if event == 'candidate':
                assert not current['deletes'], 'candidate after deletion batch'
                assert row['destination'] != 0, 'zero destination'
                assert row['candidate_ordinal'] == len(current['candidates']), 'candidate ordinal'
                assert len(current['candidates']) < row['candidate_count'], 'too many candidates'
                assert not (row['redundant'] and row['failed']), 'contradictory result'
                current['candidates'].append(row)
            elif event == 'delete':
                assert solver == 'tactix' and row['destination'] != 0 and row['redundant'] == 1
                current['deletes'].append(row)
            else:
                assert row['destination'] == 0, 'end destination'
                assert row['candidate_ordinal'] == len(current['candidates']), 'end ordinal'
                current['end'] = row
        total_rows += 1
    finish()
    assert selected, 'requested phase absent'
    return dict(total_rows=total_rows, total_sources=sequence + 1, sources=selected)


def reduced_pairs(phase):
    """Physical-copy multiset from observed successful candidate walks."""
    return Counter(tuple(sorted((r['source'], r['destination'])))
                   for s in phase['sources'] for r in s['candidates'] if r['redundant'])


def binary_drops(rows, name, conflict):
    """Pair every invocation at one conflict, not merely its last begin/end.

    Net binary drops are a separate oracle from successful walks: callers
    must account for units or other mutations before asserting equality.
    """
    pending = None
    drops = []
    for row in rows:
        if row['name'] != name or int(row['conflicts']) != conflict:
            continue
        if row['stage'] == 'begin':
            assert pending is None, 'nested transitive begin'
            pending = row
        else:
            assert row['stage'] == 'end' and pending is not None, 'orphan transitive end'
            assert int(row['seq']) > int(pending['seq']), 'boundary order'
            assert row['decisions'] == pending['decisions'], 'boundary decisions'
            drops.append(dict(begin_sequence=int(pending['seq']), end_sequence=int(row['seq']),
                begin_binary=int(pending['binary']), end_binary=int(row['binary']),
                drop=int(pending['binary'])-int(row['binary'])))
            pending = None
    assert pending is None and drops, 'unclosed or missing transitive boundary'
    return drops

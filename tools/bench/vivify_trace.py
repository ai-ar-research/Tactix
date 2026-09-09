"""Validate complete standalone Tactix Vivify round/candidate traces."""
ROUND_FIELDS = 'solver seq stage conflicts scope scheduled tried produced ticks limit offset'.split()
CANDIDATE_FIELDS = ('solver seq event conflicts scope ordinal ticks limit ref position '
                    'size redundant glue priority garbage clause pose').split()


def rows(stream, schema, fields, text_fields):
    assert next(stream) == '# tactix-vivify-' + schema + '\t1\n', 'schema'
    assert next(stream) == '# ' + '\t'.join(fields) + '\n', 'columns'
    for sequence, line in enumerate(stream):
        assert line.endswith('\n'), 'truncated row'
        values = line[:-1].split('\t')
        assert len(values) == len(fields), 'row width'
        row = dict(zip(fields, values))
        assert row['solver'] == 'tactix', 'solver'
        for field in fields:
            if field not in text_fields:
                row[field] = int(row[field])
                assert 0 <= row[field] < 1 << 64, field
        assert row['seq'] == sequence, 'sequence'
        yield row


def read_rounds(stream, conflict):
    result, pending = [], None
    for row in rows(stream, 'round', ROUND_FIELDS, {'solver', 'stage'}):
        assert row['conflicts'] == conflict, 'selected conflict'
        assert row['stage'] in ('begin', 'end'), 'round event'
        assert row['produced'] <= row['tried'] <= row['scheduled'], 'round counts'
        if row['stage'] == 'begin':
            assert pending is None and row['tried'] == row['produced'] == 0, 'round begin'
            pending = row
        else:
            assert pending is not None, 'orphan end'
            for key in ('scope', 'scheduled', 'limit', 'offset'):
                assert row[key] == pending[key], 'round identity: ' + key
            assert row['ticks'] >= pending['ticks'], 'round effort'
            result.append({'begin': pending, 'end': row})
            pending = None
    assert pending is None and result, 'unclosed or absent rounds'
    return result


def read_candidates(stream, rounds):
    """Yield validated rows, with round identity, without retaining huge logs.

    Exhaust the iterator to establish completeness. Different rounds may reuse
    scope and ordinal; the paired round frames are the grouping authority.
    """
    source = iter(rows(stream, 'candidate', CANDIDATE_FIELDS,
                       {'solver', 'event', 'clause', 'pose'}))
    row = next(source, None)
    for index, frame in enumerate(rounds):
        start, end = frame['begin'], frame['end']
        ticks = start['ticks']
        expected = []
        for ordinal in range(end['tried']):
            for event in ('vivify_candidate', 'vivify_candidate_end'):
                assert row is not None, 'missing candidate row'
                assert row['event'] == event and row['ordinal'] == ordinal, 'candidate frame'
                for key in ('scope', 'conflicts', 'limit'):
                    assert row[key] == start[key], 'candidate identity: ' + key
                assert ticks <= row['ticks'] <= end['ticks'], 'candidate effort'
                ticks = row['ticks']
                for key in ('redundant', 'priority', 'garbage'):
                    assert row[key] in (0, 1), key
                for key in ('clause', 'pose'):
                    row[key] = tuple(map(int, row[key].split(','))) if row[key] else ()
                    assert len(row[key]) == row['size'] and all(row[key]), key
                assert tuple(sorted(row['pose'])) == row['clause'], 'clause/pose multiset'
                if event == 'vivify_candidate':
                    expected = [row['ref']]
                else:
                    assert row['ref'] == expected[0], 'candidate ref changed within call'
                yield dict(row, round=index)
                row = next(source, None)
        if (end['tried'] < start['scheduled'] and row is not None and
                row['event'] == 'vivify_budget_stop'):
            assert row['ordinal'] == end['tried'] < start['scheduled'], 'stop ordinal'
            for key in ('scope', 'conflicts', 'limit'):
                assert row[key] == start[key], 'stop identity: ' + key
            assert row['ticks'] == end['ticks'] and row['ticks'] > row['limit'], 'stop effort'
            for key in ('redundant', 'priority', 'garbage'):
                assert row[key] in (0, 1), key
            for key in ('clause', 'pose'):
                row[key] = tuple(map(int, row[key].split(','))) if row[key] else ()
                assert len(row[key]) == row['size'] and all(row[key]), key
            assert tuple(sorted(row['pose'])) == row['clause'], 'stop clause/pose multiset'
            yield dict(row, round=index)
            row = next(source, None)
        assert ticks <= end['ticks']
    assert row is None, 'extra candidate rows'

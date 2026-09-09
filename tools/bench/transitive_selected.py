"""Strict selected-pair v2 trace reader; endpoint multiplicity is observer-specific."""
from itertools import chain
import re

from transitive_trace import EVENTS, FIELDS, SIGNED, TEXT, read_phase


def read_selected(stream, solver, pair, conflict):
    assert solver in ('tactix', 'kissat') and conflict >= 0
    pair = tuple(sorted(pair))
    assert len(pair) == 2 and all(pair)
    header = [next(stream), next(stream)]
    assert header == ['# tactix-transitive-trace\t2\n', '# ' + '\t'.join(FIELDS) + '\n']
    selected = []; frames = []; first_source_conflict = None
    total = 0; last_conflict = -1; source = None; walk = None
    for line in stream:
        assert line.endswith('\n'), 'truncated row'
        values = line[:-1].split('\t')
        assert len(values) == len(FIELDS), 'row width'
        row = dict(zip(FIELDS, values))
        event = row['event']
        assert row['solver'] == solver and event in EVENTS | {'endpoint', 'admission', 'walk-node'}, 'solver/event'
        for key in FIELDS:
            if key not in TEXT:
                row[key] = int(row[key])
                if key not in SIGNED:
                    assert 0 <= row[key] < 1 << 64, key
        assert row['source'] and row['conflicts'] >= last_conflict, 'source/conflict'
        last_conflict = row['conflicts']
        for key in ('source_value', 'destination_value'):
            assert row[key] in (-1, 0, 1), key
        for key in ('redundant', 'failed', 'source_active', 'destination_active'):
            assert row[key] in (0, 1), key
        assert row['ticks_after'] >= row['ticks_before'], 'negative effort'
        if event in EVENTS:
            frames.append(line)
            if event == 'source-begin':
                assert source is None and walk is None, 'nested source'
                source = row
                if first_source_conflict is None:
                    first_source_conflict = row['conflicts']
            elif event == 'source-end':
                assert walk is None, 'unfinished walk'
                source = None
            elif event == 'candidate' and walk is not None:
                assert (row['source'], row['destination']) == walk['pair'], 'walk/candidate identity'
                assert row['ticks_after'] >= walk['ticks'], 'walk/candidate effort'
                walk = None
        else:
            assert row['source_sequence'] == 0, 'auxiliary source sequence'
            assert row['redundant'] == row['failed'] == row['candidate_count'] == row['out_degree'] == 0, 'auxiliary frame'
            if event == 'endpoint':
                assert source is None and row['source'] in pair, 'endpoint placement'
                assert row['destination'] == row['candidate_ordinal'] == row['copies'] == 0, 'endpoint shape'
                assert row['skip_reason'] == '-' and row['ticks_before'] == row['ticks_after'] == 0
            elif event == 'admission':
                assert source is None and tuple(sorted((row['source'], row['destination']))) == pair, 'admission placement/pair'
                assert row['copies'] > 0 and row['candidate_ordinal'] == 0
                assert row['ticks_before'] == row['ticks_after'] == 0
                assert row['skip_reason'] in ('admitted', 'duplicate', 'ownership', 'destination-fixed'), 'admission reason'
            else:
                assert source is not None and row['source'] == source['source'], 'walk source'
                assert row['conflicts'] == source['conflicts'], 'walk conflict'
                assert tuple(sorted((row['source'], row['destination']))) == pair, 'walk pair'
                assert re.fullmatch(r'node=-?[1-9][0-9]*', row['skip_reason']), 'walk node payload'
                ordinal = row['candidate_ordinal']
                if walk is None:
                    assert ordinal == 0, 'walk first ordinal'
                else:
                    assert ordinal == walk['ordinal'] + 1 and row['ticks_before'] >= walk['ticks'], 'walk order'
                walk = dict(pair=(row['source'], row['destination']), ordinal=ordinal, ticks=row['ticks_after'])
        if row['conflicts'] == conflict:
            selected.append(row)
        total += 1
    assert source is None and walk is None, 'unclosed selected source'
    if frames:
        # Keep the maintained complete source/candidate/deletion framing oracle.
        read_phase(iter(chain(header, frames)), solver, first_source_conflict)
    assert selected and any(r['event'] == 'endpoint' for r in selected), 'target endpoint observation absent'
    return dict(total_rows=total, source_frame_rows=len(frames), rows=selected)

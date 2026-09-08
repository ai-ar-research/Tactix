"""Read version-one decision-prefix records without positional-field ambiguity."""
from pathlib import Path

FIELDS = ('solver conflict start_decisions end_decisions count hash common '
          'common_reached common_hash first common_last last max_level').split()


def read_prefix(path):
    lines = Path(path).read_text().splitlines()
    assert lines[0] == '# tactix-decision-prefix\t1', 'unsupported prefix schema'
    assert lines[1] == '# ' + '\t'.join(FIELDS), 'unexpected prefix columns'
    records = [line.split('\t') for line in lines[2:] if line.strip()]
    assert len(records) == 1 and len(records[0]) == len(FIELDS), 'expected one complete prefix'
    record = dict(zip(FIELDS, records[0]))
    for field in FIELDS:
        if field not in ('solver', 'hash', 'common_hash'):
            record[field] = int(record[field])
    assert record['common_reached'] in (0, 1), 'common_reached must be Boolean'
    return record


def validate_common(record, *, stop, common, digest):
    assert record['conflict'] == stop, 'wrong stop conflict'
    assert record['common'] == common, 'wrong common-prefix request'
    assert record['common_reached'] == 1, 'common prefix was not reached'
    assert record['count'] >= common, 'truncated common prefix'
    assert record['common_hash'] == digest, 'earlier common prefix changed'

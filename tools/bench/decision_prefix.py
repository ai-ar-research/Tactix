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


def validate_stream_accounting(record, first, last, count):
    """Check full-stream edges without equating cumulative rows to counters.

    The observer emits end_decisions-count as start_decisions modulo2**64.
    Preprocessing may reset the solver counter while the observer keeps rows;
    this derived field therefore need not precede the first row's counter.
    """
    assert count == record['count'] and count > 0, 'wrong full-stream row count'
    assert len(first) == len(last) == 5, 'decision stream must have five columns'
    assert first[3] == record['first'], 'first literal does not match prefix'
    assert last[3] == record['last'], 'last literal does not match prefix'
    assert last[1] == record['end_decisions'], 'last counter does not match prefix'
    assert 0 <= first[0] <= last[0] <= record['conflict'], 'stream outside prefix boundary'
    assert record['start_decisions'] == (record['end_decisions'] - count) % (1 << 64), 'wrong derived start counter'


def validate_suffix_accounting(record, first, last, count, *, common_count,
                               overlap_count, stream_from):
    """Validate a threshold-filtered suffix after a separately verified prefix.

    The threshold can have zero decisions: do not require or invent one overlap
    row. Callers bind complete files and verify any actual cached overlap bytes;
    this function checks counts and edges, not the interior rows or prefix hash.
    """
    assert 0 <= overlap_count <= common_count <= record['count'], 'invalid overlap/common count'
    assert count == record['count'] - common_count + overlap_count and count > 0, 'wrong suffix row count'
    assert len(first) == len(last) == 5, 'decision stream must have five columns'
    assert 0 <= stream_from <= first[0] <= last[0] <= record['conflict'], 'suffix outside boundary'
    assert (first[0] == stream_from) == (overlap_count > 0), 'threshold overlap mismatch'
    assert last[3] == record['last'], 'last literal does not match prefix'
    assert last[1] == record['end_decisions'], 'last counter does not match prefix'
    assert record['start_decisions'] == (record['end_decisions'] - record['count']) % (1 << 64), 'wrong derived start counter'


def first_stream_difference(left, right, *, chunk_bytes=1 << 20):
    """Compare bound, headerless decision streams without parsing equal rows.

    Returns the first differing five-integer row and the exact common row count.
    This checks framing and observed rows, not every equal row's schema; callers
    must separately bind complete files to their solver's trace/prefix records.
    """
    assert chunk_bytes > 0
    def row(data):
        if not data:
            return None
        fields = data.split(b'\t')
        assert len(fields) == 5, 'decision stream must have five columns'
        return list(map(int, fields))

    with Path(left).open('rb') as a, Path(right).open('rb') as b:
        for stream in (a, b):
            stream.seek(0, 2)
            if stream.tell():
                stream.seek(-1, 2)
                assert stream.read(1) == b'\n', 'truncated decision row'
                stream.seek(0)
                row(stream.readline().rstrip(b'\n'))
            stream.seek(0)
        count, carry = 0, b''
        while True:
            x, y = a.read(chunk_bytes), b.read(chunk_bytes)
            if x == y:
                if not x:
                    assert not carry
                    return dict(matching_rows=count, left=None, right=None, complete_equal=True)
                count += x.count(b'\n')
                carry = x.rsplit(b'\n', 1)[-1] if b'\n' in x else carry + x
                continue
            pos = next((i for i, (u, v) in enumerate(zip(x, y)) if u != v), min(len(x), len(y)))
            common = x[:pos]
            count += common.count(b'\n')
            start = common.rfind(b'\n') + 1
            def first(data, stream):
                tail = (b'' if start else carry) + data[start:]
                if b'\n' not in tail:
                    tail += stream.readline()
                return row(tail.split(b'\n', 1)[0])
            return dict(matching_rows=count, left=first(x, a), right=first(y, b), complete_equal=False)

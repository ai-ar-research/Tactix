"""Read the existing cumulative Congruence stage-timer diagnostic.

Fields overlap (notably closure/match/proof); do not sum them as disjoint work.
These are instrumented wall times, not uninstrumented performance acceptance.
"""
import re


_ROW = re.compile(
    r'\[cong-time\] cumulative ms: apply (?P<apply>\d+) \| '
    r'extract\{and (?P<and>\d+) index (?P<index>\d+) sort (?P<sort>\d+) '
    r'xor (?P<xor>\d+) ite (?P<ite>\d+)\} \| rebuild (?P<rebuild>\d+) '
    r'binary\{extract (?P<binary_extract>\d+) units (?P<binary_units>\d+) '
    r'import (?P<binary_import>\d+)\} setup (?P<setup>\d+) \| '
    r'closure (?P<closure>\d+) \{canon (?P<canon>\d+) match (?P<match>\d+) '
    r'\(of which proof (?P<proof>\d+)\)\} \| forward (?P<forward>\d+) '
    r'\| items (?P<items>\d+)')


def parse_cumulative(text, expected_rows):
    if not isinstance(expected_rows, int) or isinstance(expected_rows, bool) or expected_rows <= 0:
        raise ValueError('expected row count must be positive')
    rows = []
    for line in text.splitlines():
        if '[cong-time]' not in line:
            continue
        match = _ROW.fullmatch(line)
        if not match:
            raise ValueError('malformed Congruence timer row')
        row = {name: int(value) for name, value in match.groupdict().items()}
        if rows and any(value < rows[-1][name] for name, value in row.items()):
            raise ValueError('cumulative timer counter decreased')
        rows.append(row)
    if len(rows) != expected_rows:
        raise ValueError(f'expected {expected_rows} timer rows, found {len(rows)}')
    return rows


def deltas(rows):
    """Subtract cumulative rows; overlaps and integer-ms rounding remain."""
    result, previous = [], None
    for row in rows:
        if previous is not None and row.keys() != previous.keys():
            raise ValueError('inconsistent timer fields')
        delta = {name: value - (previous[name] if previous else 0) for name, value in row.items()}
        if any(value < 0 for value in delta.values()):
            raise ValueError('cumulative counter decreased')
        result.append(delta)
        previous = row
    return result

"""Read perf's flat symbol rows, never summing its repeated call trees.

Percentages are rounded sample-period shares, not operation costs or timing
confidence intervals. Inclusive rows can overlap; callers own scope mapping.
"""
import re


def read_report(lines, inclusive=False):
    total = lost = event = None
    header = False
    rows = {}
    pattern = re.compile(
        r'(\d+\.\d{2})%\s+' + (r'(\d+\.\d{2})%\s+' if inclusive else '') +
        r'(\S+)\s+\[([.k])\]\s+(.+?)\s+-\s+-')
    for raw in lines:
        line = ' '.join(raw.split())
        if line.startswith('# Event count (approx.):'):
            if total is not None:
                raise ValueError('duplicate event total')
            value = line.removeprefix('# Event count (approx.): ').strip()
            if not value.isdecimal() or int(value) <= 0:
                raise ValueError('invalid event total')
            total = int(value)
        elif line.startswith('# Total Lost Samples:'):
            if lost is not None or line != '# Total Lost Samples: 0':
                raise ValueError('missing zero-loss authority or duplicate')
            lost = 0
        elif line.startswith('# Samples:'):
            if event is not None or not line.endswith("of event 'cycles:u'"):
                raise ValueError('unexpected or duplicate event')
            event = 'cycles:u'
        elif line.startswith(('# Children Self Shared Object Symbol',
                              '# Overhead Shared Object Symbol')):
            if header or line.startswith('# Children') != inclusive:
                raise ValueError('wrong or duplicate table header')
            header = True
        elif re.match(r'^(?:[+-]?\d|[+-]?(?:nan|inf)%)', line):
            match = pattern.fullmatch(line)
            if not header or not match:
                raise ValueError('malformed symbol row')
            values = match.groups()
            if inclusive:
                children, own, dso, kind, symbol = values
            else:
                own, dso, kind, symbol = values
                children = own
            children, own = float(children), float(own)
            if not 0 <= own <= children <= 100:
                raise ValueError('invalid shares')
            key = (dso, kind, symbol)
            if key in rows:
                raise ValueError('duplicate symbol row')
            rows[key] = dict(inclusive_percent=children, self_percent=own)
    if total is None or lost is None or event is None or not header or not rows:
        raise ValueError('incomplete report')
    return dict(total_cycles=total, rows=rows)


def select(report, dso, symbols, inclusive=True):
    """Select explicit symbols; no prefix matching, missing-as-zero or nesting inference."""
    if not symbols or len(set(symbols)) != len(symbols):
        raise ValueError('empty or duplicate selection')
    field = 'inclusive_percent' if inclusive else 'self_percent'
    try:
        percent = sum(report['rows'][dso, '.', symbol][field] for symbol in symbols)
    except KeyError as error:
        raise ValueError('missing exact symbol') from error
    if percent > 100:
        raise ValueError('selected share exceeds whole profile')
    return dict(symbols=list(symbols), percent=percent,
                estimated_cycles=report['total_cycles'] * percent / 100,
                rounding_cycles=report['total_cycles'] * 0.005 * len(symbols) / 100)

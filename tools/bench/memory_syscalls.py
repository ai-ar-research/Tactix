"""Strict, loss-accounted reader for strace -ttt -k allocator syscall logs.

The interval ledger measures traced virtual extents, NOT RSS or live malloc
objects. Unknown records and incomplete stacks are retained explicitly.
"""
from collections import Counter
from dataclasses import dataclass, field
import re


HEADER = re.compile(r'(\d+) (\d+)\.(\d{6}) (brk|mmap|mremap|munmap)\((.*)\)\s+= (.*)')


@dataclass
class Event:
    line: int
    pid: int
    time_us: int
    operation: str
    arguments: str
    result: str
    frames: list = field(default_factory=list)


def parse(text):
    events, unknown = [], []
    for line_number, line in enumerate(text.splitlines(), 1):
        match = HEADER.fullmatch(line)
        if match:
            pid, seconds, micros, operation, arguments, result = match.groups()
            events.append(Event(line_number, int(pid), int(seconds) * 1000000 + int(micros),
                                operation, arguments, result))
        elif line.startswith(' > ') and events:
            events[-1].frames.append(line[3:])
        else:
            unknown.append(dict(line=line_number, text=line))
    return events, unknown


def owner(event, binary):
    prefix = str(binary) + '('
    for frame in event.frames:
        if frame.startswith(prefix):
            # The full frame remains in Event; aggregation removes PC/offset.
            return re.sub(r'\+0x[0-9a-f]+$', '', frame[len(prefix):].rsplit(') [', 1)[0])
    return '<no solver frame>'


def integer(word):
    if not re.fullmatch(r'0x[0-9a-f]+|[0-9]+', word):
        raise ValueError('not an unsigned integer: ' + word)
    return int(word, 16 if word.startswith('0x') else 10)


class Intervals:
    def __init__(self):
        self.ranges = []  # start, end, allocation event index, anonymous

    def remove(self, start, end):
        kept, removed = [], 0
        for a, b, event, anonymous in self.ranges:
            overlap = max(0, min(end, b) - max(start, a))
            removed += overlap
            if not overlap:
                kept.append((a, b, event, anonymous))
                continue
            if a < start:
                kept.append((a, start, event, anonymous))
            if b > end:
                kept.append((end, b, event, anonymous))
        self.ranges = sorted(kept)
        return removed

    def add(self, start, end, event, anonymous, replace=False):
        if start >= end:
            raise ValueError('empty mapping')
        if replace:
            self.remove(start, end)
        elif any(start < b and a < end for a, b, _, _ in self.ranges):
            raise ValueError('unexpected overlapping mapping')
        self.ranges.append((start, end, event, anonymous))
        self.ranges.sort()

    def size(self):
        return sum(b - a for a, b, _, _ in self.ranges)


def analyze(events, unknown, binary, page_size):
    if unknown or not events:
        raise ValueError('unknown records or empty trace: ' + str(unknown[:3]))
    if len({e.pid for e in events}) != 1:
        raise ValueError('ledger requires one process; fork semantics not inferred')
    if any(b.time_us < a.time_us for a, b in zip(events, events[1:])):
        raise ValueError('nonmonotonic trace timestamps')
    if not isinstance(page_size, int) or isinstance(page_size, bool) or page_size <= 0 or page_size & (page_size - 1):
        raise ValueError('invalid page size')
    rounded = lambda n: (n + page_size - 1) // page_size * page_size
    maps = Intervals()
    initial_brk = current_brk = None
    peak, jumps, failures, records = None, [], [], []
    counts, callers = Counter(), Counter()
    for i, e in enumerate(events):
        counts[e.operation] += 1
        who = owner(e, binary)
        callers[(e.operation, who)] += 1
        if e.result.startswith('-1 '):
            failures.append(dict(event=i, line=e.line, result=e.result))
        elif e.operation == 'brk':
            result = integer(e.result)
            if initial_brk is None:
                if e.arguments != 'NULL':
                    raise ValueError('missing initial brk query')
                initial_brk = current_brk = result
            else:
                if result < initial_brk:
                    raise ValueError('brk below original extent')
                if e.arguments != 'NULL' and result != integer(e.arguments):
                    failures.append(dict(event=i, line=e.line, result=e.result,
                                         reason='brk request not granted'))
                delta = result - current_brk
                if delta:
                    jumps.append(dict(event=i, line=e.line, time_us=e.time_us,
                                      delta_bytes=delta, heap_extent_bytes=result-initial_brk,
                                      owner=who))
                current_brk = result
        elif e.operation == 'mmap':
            args = e.arguments.split(', ')
            if len(args) != 6:
                raise ValueError('unsupported mmap arguments')
            start, length = integer(e.result), rounded(integer(args[1]))
            maps.add(start, start + length, i, 'MAP_ANONYMOUS' in args[3],
                     replace='MAP_FIXED' in args[3] and 'MAP_FIXED_NOREPLACE' not in args[3])
        elif e.operation == 'munmap':
            args = e.arguments.split(', ')
            if len(args) != 2 or e.result != '0':
                raise ValueError('unsupported munmap')
            start, length = integer(args[0]), rounded(integer(args[1]))
            removed = maps.remove(start, start + length)
            if removed != length:
                raise ValueError('unmap includes untracked bytes')
        else:
            args = e.arguments.split(', ')
            if len(args) != 4 or args[3] not in ('0', 'MREMAP_MAYMOVE'):
                raise ValueError('unsupported remap flags/alias semantics')
            old, old_len, new_len = integer(args[0]), rounded(integer(args[1])), rounded(integer(args[2]))
            old_ranges = [r for r in maps.ranges if r[0] < old + old_len and old < r[1]]
            if len(old_ranges) != 1 or old_ranges[0][:2] != (old, old + old_len):
                raise ValueError('remap source is not one exact tracked mapping')
            maps.remove(old, old + old_len)
            new = integer(e.result)
            maps.add(new, new + new_len, i, old_ranges[0][3])
        heap = 0 if initial_brk is None else current_brk - initial_brk
        extent = heap + maps.size()
        state = dict(event=i, line=e.line, elapsed_us=e.time_us-events[0].time_us,
                     heap_extent_bytes=heap, traced_mapping_bytes=maps.size(),
                     traced_virtual_extent_bytes=extent)
        if peak is None or extent > peak['traced_virtual_extent_bytes']:
            peak = dict(state, mappings=list(maps.ranges))
        records.append(state)
    if initial_brk is None:
        raise ValueError('no brk baseline')
    peak['mapping_owners'] = [dict(bytes=b-a, anonymous=anonymous,
        allocation_event=index, allocation_line=events[index].line,
        owner=owner(events[index], binary)) for a, b, index, anonymous in peak.pop('mappings')]
    return dict(events=len(events), counts=dict(counts), pid=events[0].pid,
        no_solver_frame_events=[i for i, e in enumerate(events) if owner(e, binary) == '<no solver frame>'],
        incomplete_stack_events=[i for i, e in enumerate(events)
            if not e.frames or any('no matching address range' in f or 'unwind' in f.lower() for f in e.frames)],
        failed_syscalls=failures, peak=peak, final=records[-1], heap_changes=jumps,
        caller_counts=[dict(operation=op, owner=who, count=n)
            for (op, who), n in sorted(callers.items())], trajectory=records,
        limitation='Traced virtual extents only: not RSS, not malloc live bytes; no attribution of resident high-water to a syscall. Unwinding gaps and failed requests retained.')

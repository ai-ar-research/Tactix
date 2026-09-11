"""Join sampled VMAs to traced mapping origins, without dividing their RSS.

Syscall timestamps are entry times. Even stable mapping topology does not make
smaps atomic, or identify live malloc objects within the process heap.
"""
from memory_syscalls import Intervals, analyze, integer, owner


def mapping_state(events, before_ns, page_size=4096):
    """Replay an already validated trace through a syscall-entry time cutoff."""
    maps = Intervals()
    rounded = lambda n: (n + page_size - 1) // page_size * page_size
    for i, e in enumerate(events):
        if e.time_us * 1000 >= before_ns:
            break
        if e.result.startswith('-1 ') or e.operation == 'brk':
            continue
        args = e.arguments.split(', ')
        if e.operation == 'mmap':
            start = integer(e.result)
            maps.add(start, start + rounded(integer(args[1])), i,
                     'MAP_ANONYMOUS' in args[3],
                     replace='MAP_FIXED' in args[3] and 'MAP_FIXED_NOREPLACE' not in args[3])
        elif e.operation == 'munmap':
            start = integer(args[0])
            maps.remove(start, start + rounded(integer(args[1])))
        elif e.operation == 'mremap':
            start, length = integer(args[0]), rounded(integer(args[1]))
            old = [r for r in maps.ranges if r[:2] == (start, start + length)]
            assert len(old) == 1
            maps.remove(start, start + length)
            start = integer(e.result)
            maps.add(start, start + rounded(integer(args[2])), i, old[0][3])
        else:
            raise ValueError('unsupported mapping operation')
    return maps


def join(events, unknown, binary, snapshot, page_size=4096):
    validated = analyze(events, unknown, binary, page_size)
    before, after = snapshot['smaps_before_ns'], snapshot['smaps_after_ns']
    if type(before) is not int or type(after) is not int or before > after:
        raise ValueError('invalid snapshot time interval')
    maps = mapping_state(events, before, page_size)
    # Conservatively flag the entire snapshot if any mapping/heap call starts
    # during it, including unsuccessful calls. Do not silently assign old VMAs.
    concurrent = [i for i, e in enumerate(events) if before <= e.time_us*1000 <= after]
    rows = []
    for region in snapshot['regions']:
        a, b = region['start'], region['end']
        if not (type(a) is int and type(b) is int and 0 <= a < b):
            raise ValueError('invalid VMA range')
        origins = []
        for start, end, i, anonymous in maps.ranges:
            overlap = max(0, min(b, end) - max(a, start))
            if overlap:
                origins.append(dict(event=i, overlap_bytes=overlap, anonymous=anonymous,
                                    owner=owner(events[i], binary), frames=events[i].frames))
        covered = sum(o['overlap_bytes'] for o in origins)
        assert covered <= b-a
        rows.append(dict(start=a, end=b, name=region['name'], kib=region['kib'],
                         covered_bytes=covered, untracked_bytes=b-a-covered, origins=origins))
    return dict(regions=rows, concurrent_syscalls=concurrent,
                topology_stable_by_entry_timestamps=not concurrent,
                total_rss_kib=sum(r['kib']['Rss'] for r in rows),
                syscall_counts=validated['counts'], failed_syscalls=validated['failed_syscalls'],
                incomplete_stack_events=validated['incomplete_stack_events'],
                limitation='VMA RSS belongs to the whole sampled region, never apportioned to origins. Heap objects and allocator-retained bytes are not resolved. No atomic-peak or syscall-completion claim.')

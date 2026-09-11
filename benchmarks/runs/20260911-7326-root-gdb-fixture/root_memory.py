"""Decode externally read Release kernel storage; no inferior function calls."""
import struct

SELECTED = ('values', 'assigned', 'watch_heads', 'jumped_over', 'unit_ids',
            'touched', 'removed', 'touched_kept')


def vector(header, offset, item_bytes):
    if item_bytes <= 0:
        raise ValueError('invalid element size')
    first, end, limit = struct.unpack_from('<QQQ', header, offset)
    if not first <= end <= limit or (not first and (end or limit)):
        raise ValueError('invalid vector pointers')
    if (end-first) % item_bytes or (limit-first) % item_bytes:
        raise ValueError('unaligned vector extent')
    return dict(address=first, end=end, limit=limit, size=(end-first)//item_bytes,
                capacity=(limit-first)//item_bytes, size_bytes=end-first,
                capacity_bytes=limit-first, item_bytes=item_bytes)


def populations(values, assigned, status, nvars, assigned_bytes, level_offset):
    if nvars < 0 or len(values) != 2*nvars or len(assigned) != assigned_bytes*nvars:
        raise ValueError('incomplete population arrays')
    if len(status) not in (0, nvars) or any(s not in (0, 1, 2) for s in status):
        raise ValueError('invalid variable status')
    if assigned_bytes < 4 or not 0 <= level_offset <= assigned_bytes-4:
        raise ValueError('invalid assignment layout')
    result = dict(vars=nvars, fixed=0, fixed_active=0, retired=0, unassigned=0,
                  nonroot_active=0)
    for v in range(nvars):
        pos, neg = struct.unpack_from('<bb', values, 2*v)
        if pos not in (-1, 0, 1) or neg != -pos:
            raise ValueError('invalid literal values')
        retired = bool(status and status[v])
        level = struct.unpack_from('<I', assigned, assigned_bytes*v+level_offset)[0]
        fixed = pos != 0 and level == 0
        result['fixed'] += fixed
        result['fixed_active'] += fixed and not retired
        result['retired'] += retired
        result['unassigned'] += pos == 0 and not retired
        result['nonroot_active'] += pos != 0 and level != 0 and not retired
    assert sum(result[k] for k in ('fixed_active', 'retired', 'unassigned', 'nonroot_active')) == nvars
    return result


def snapshot(read, address, layout):
    """read(address,size) must be a read-only process-memory provider."""
    def exact(at, size):
        data = bytes(read(at, size)) if size else b''
        if len(data) != size:
            raise ValueError('short process memory read')
        return data
    header = exact(address, layout['kernel_bytes'])
    nvars = struct.unpack_from('<I', header, layout['nvars_offset'])[0]
    vectors = {name:vector(header, *spec) for name, spec in layout['vectors'].items()}
    if vectors['values']['size'] != 2*nvars or vectors['assigned']['size'] != nvars:
        raise ValueError('population size mismatch')
    if vectors['var_status']['size'] not in (0, nvars):
        raise ValueError('status size mismatch')
    arrays = [exact(vectors[name]['address'], vectors[name]['size_bytes'])
              for name in ('values', 'assigned', 'var_status')]
    return dict(populations=populations(*arrays, nvars, layout['assigned_bytes'], layout['level_offset']),
        vectors=vectors, selected_size_bytes=sum(vectors[k]['size_bytes'] for k in SELECTED),
        selected_capacity_bytes=sum(vectors[k]['capacity_bytes'] for k in SELECTED),
        conflicts=struct.unpack_from('<Q', header, layout['conflicts_offset'])[0],
        decisions=struct.unpack_from('<Q', header, layout['decisions_offset'])[0])

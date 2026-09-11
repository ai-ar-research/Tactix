"""Read external process RSS observations without equating VmHWM and smaps.

Each smaps region is retained with its original address, name and fields.
The summary reports the actual sampling window and whether smaps reads raced
with a new high-water. It never assigns the historic peak to a sampled map.
"""
import re
from proc_memory_task import status_values


MAP_HEADER = re.compile(r'([0-9a-f]+)-([0-9a-f]+) ([rwxps-]{4}) ([0-9a-f]+) ([0-9a-f]+:[0-9a-f]+) (\d+)\s*(.*)')
REQUIRED = ('Size', 'Rss', 'Anonymous', 'Private_Dirty', 'Swap')


def smaps(text):
    regions = []
    for number, line in enumerate(text.splitlines(), 1):
        match = MAP_HEADER.fullmatch(line)
        if match:
            a, b, permissions, offset, device, inode, name = match.groups()
            region = dict(start=int(a, 16), end=int(b, 16), permissions=permissions,
                          offset=int(offset, 16), device=device, inode=int(inode),
                          name=name, fields={})
            if region['start'] >= region['end']:
                raise ValueError('invalid map range')
            if regions and region['start'] < regions[-1]['end']:
                raise ValueError('unordered/overlapping smaps')
            regions.append(region)
            continue
        key, sep, value = line.partition(':')
        if not regions or not sep or key in regions[-1]['fields']:
            raise ValueError(f'invalid smaps field on line {number}')
        regions[-1]['fields'][key] = value.strip()
    if not regions:
        raise ValueError('empty smaps')
    for region in regions:
        kib = {}
        for name in REQUIRED:
            value = region['fields'].get(name, '')
            match = re.fullmatch(r'(\d+) kB', value)
            if not match:
                raise ValueError('missing/invalid smaps metric: ' + name)
            kib[name] = int(match[1])
        if kib['Size'] * 1024 != region['end'] - region['start']:
            raise ValueError('smaps size/range mismatch')
        if kib['Rss'] > kib['Size']:
            raise ValueError('resident pages exceed region size')
        region['kib'] = kib
    return regions


def summarize(rows):
    if not rows or rows[-1]['kind'] != 'observer_stop':
        raise ValueError('missing completed observer record')
    if any(r['kind'] not in ('sample', 'gap', 'observer_stop') for r in rows):
        raise ValueError('unknown observer record kind')
    if any(r['kind'] == 'observer_stop' for r in rows[:-1]):
        raise ValueError('early observer stop')
    samples = [(i, r) for i, r in enumerate(rows) if r['kind'] == 'sample']
    if not samples or len({(r['pid'], r['starttime_ticks']) for _, r in samples}) != 1:
        raise ValueError('missing samples or inconsistent identity')
    identity = samples[0][1]
    if any(rows[-1][k] != identity[k] for k in ('pid', 'starttime_ticks')):
        raise ValueError('stop identity mismatch')
    before = -1
    high = -1
    peaks = []
    decreases = []
    previous_sample = None
    for index, row in samples:
        if row['before_ns'] < before or row['after_ns'] < row['before_ns']:
            raise ValueError('nonmonotonic observer timestamp')
        before = row['after_ns']
        if status_values(row['status']) != row['kib']:
            raise ValueError('derived status differs from raw evidence')
        hwm = row['kib']['VmHWM']
        if previous_sample is not None and hwm < previous_sample[1]['kib']['VmHWM']:
            decreases.append(dict(previous_row=previous_sample[0], row=index,
                previous_kib=previous_sample[1]['kib']['VmHWM'], observed_kib=hwm))
        previous_sample = (index, row)
        if hwm > high:
            high = hwm
            peaks.append((index, row))
    if rows[-1]['maximum_sampled_hwm_kib'] != high:
        raise ValueError('stop high-water mismatch')
    at, peak = peaks[-1]
    preceding = next((r for i, r in reversed(samples) if i < at), None)
    snapshot = peak.get('high_water_snapshot')
    region_summary = None
    if snapshot:
        for key in ('pid', 'starttime_ticks'):
            if snapshot[key] != peak[key]:
                raise ValueError('snapshot identity mismatch')
        if status_values(snapshot['status']) != snapshot['kib'] or status_values(snapshot['status_after_smaps']) != snapshot['kib_after_smaps']:
            raise ValueError('snapshot status differs from raw evidence')
        stamps = [peak['before_ns'], peak['after_ns'], snapshot['before_ns'],
                  snapshot['after_ns'], snapshot['smaps_before_ns'], snapshot['smaps_after_ns']]
        if stamps != sorted(stamps):
            raise ValueError('snapshot chronology mismatch')
        regions = smaps(snapshot['smaps'])
        region_summary = dict(
            status_before_kib=snapshot['kib'], status_after_kib=snapshot['kib_after_smaps'],
            smaps_before_ns=snapshot['smaps_before_ns'], smaps_after_ns=snapshot['smaps_after_ns'],
            hwm_unchanged_through_snapshot=(snapshot['kib']['VmHWM'] == high == snapshot['kib_after_smaps']['VmHWM']),
            smaps_total_rss_kib=sum(r['kib']['Rss'] for r in regions),
            heap=[r for r in regions if r['name'] == '[heap]'],
            regions=regions)
    return dict(samples=len(samples), gaps=[r for r in rows if r['kind'] == 'gap'],
        pid=identity['pid'], starttime_ticks=identity['starttime_ticks'],
        peak_hwm_kib=high, first_maximum_row=at,
        observed_hwm_decreases=decreases,
        previous_sample_after_ns=preceding['after_ns'] if preceding else None,
        maximum_sample_before_ns=peak['before_ns'], maximum_sample_after_ns=peak['after_ns'],
        maximum_sample_kib=peak['kib'], maximum_snapshot=region_summary,
        snapshot_error=peak.get('snapshot_error'),
        high_water_rises=[dict(row=i, before_ns=r['before_ns'], after_ns=r['after_ns'], kib=r['kib']) for i, r in peaks],
        limitation='VmHWM readings can decrease; every observed decrease is retained, never normalized away. The maximum is a maximum over raw samples and must be checked against GNUtime and the original Release measurement separately. The preceding/maximum samples bound detection, not an exact allocation or atomic peak-state snapshot; status counters and smaps can race. No malloc liveness/fragmentation classification from RSS alone.')

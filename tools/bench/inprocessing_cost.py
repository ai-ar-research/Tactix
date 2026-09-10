"""Read Tactix's slot timers without treating them as native operation prices."""
import re
import math
import statistics as stats


def read_timers(statistics):
    values = {}; rules = {}
    for line in statistics:
        if not line.startswith('c timing'):
            continue
        match = re.fullmatch(r'c timing\s+(\d+) ms solve', line)
        if match:
            if 'solve_ms' in values: raise ValueError('duplicate solve timer')
            values['solve_ms'] = int(match[1]); continue
        match = re.fullmatch(r'c timing\s+(\d+) ms \([\d.]+%\) inprocessing slot, (\d+) ms \([\d.]+%\) search\+scaffolding outside it', line)
        if match:
            if 'slot_ms' in values: raise ValueError('duplicate slot timer')
            values.update(slot_ms=int(match[1]), outside_ms=int(match[2])); continue
        match = re.fullmatch(r'c timing\s+([A-Za-z]+)\s+(\d+) ms \(\s*[\d.]+%\) in (\d+) applications', line)
        if match:
            if match[1] in rules: raise ValueError('duplicate rule timer')
            rules[match[1]] = dict(ms=int(match[2]), applications=int(match[3])); continue
        match = re.fullmatch(r'c timing\s+\(unattributed\)\s+(\d+) ms \(\s*[\d.]+%\)', line)
        if match:
            if 'unattributed_ms' in values: raise ValueError('duplicate unattributed timer')
            values['unattributed_ms'] = int(match[1]); continue
        raise ValueError('unrecognized timing row: '+line)
    if not {'solve_ms','slot_ms','outside_ms'} <= values.keys():
        raise ValueError('missing solve or slot timer')
    # main.cpp prints each quantity independently with %.0f (nearest integer).
    if abs(values['solve_ms']-values['slot_ms']-values['outside_ms']) > 1:
        raise ValueError('inconsistent slot partition')
    values['rules'] = rules
    return values


def paired_costs(rows, ranks, waves, rules):
    """Price a complete declared matrix; exact work/proof checks are upstream.

    Zero denominators stay unpriced, not zero-cost wins. Per-wave ratios and
    their full ranges are retained; no selection of favorable repetitions.
    """
    if (not ranks or len(set(ranks)) != len(ranks) or type(waves) is not int or
            waves < 1 or not rules or len(set(rules)) != len(rules)):
        raise ValueError('invalid declared matrix or rules')
    width = 2 * len(ranks)
    by_task = {}
    grouped = {}
    for row in rows:
        task = row['task']
        if type(task) is not int or task < 0 or task >= width * waves or task in by_task:
            raise ValueError('duplicate or out-of-range task')
        rank, arm = row['rank'], row['arm']
        if rank not in ranks or arm not in ('candidate', 'previous'):
            raise ValueError('unexpected rank or arm')
        key = (task // width, rank, arm)
        if key in grouped:
            raise ValueError('duplicate wave/rank/arm')
        timers = read_timers(row['solver_statistics'])
        if not set(rules) <= timers['rules'].keys():
            raise ValueError('missing requested rule timer')
        solve = row['solve']
        wall, rss = solve['wall_seconds'], solve['peak_rss_kb']
        if (isinstance(wall, bool) or not isinstance(wall, (int, float)) or
                not math.isfinite(wall) or wall <= 0 or type(rss) is not int or rss <= 0):
            raise ValueError('invalid wall time or RSS')
        values = dict(wall_seconds=wall, peak_rss_kib=rss,
                      slot_ms=timers['slot_ms'], outside_ms=timers['outside_ms'])
        values.update({rule + '_ms': timers['rules'][rule]['ms'] for rule in rules})
        grouped[key] = dict(values=values, applications={
            rule: timers['rules'][rule]['applications'] for rule in rules})
        by_task[task] = row
    if set(by_task) != set(range(width * waves)):
        raise ValueError('incomplete declared matrix')
    out = {}
    for rank in ranks:
        pairs = []
        for wave in range(waves):
            try:
                arms = {arm: grouped[wave, rank, arm] for arm in ('candidate', 'previous')}
            except KeyError as error:
                raise ValueError('incomplete wave/rank pair') from error
            if arms['candidate']['applications'] != arms['previous']['applications']:
                raise ValueError('changed rule application counts')
            values = {arm: item['values'] for arm, item in arms.items()}
            ratios = {key: values['candidate'][key] / value if value else None
                      for key, value in values['previous'].items()}
            pairs.append(dict(wave=wave, values=values, ratios=ratios,
                              applications=arms['previous']['applications']))
        paired = {}
        for key in pairs[0]['ratios']:
            ratios = [p['ratios'][key] for p in pairs]
            complete = all(ratio is not None for ratio in ratios)
            paired[key] = dict(priced_waves=sum(r is not None for r in ratios),
                median=stats.median(ratios) if complete else None,
                min=min(ratios) if complete else None,
                max=max(ratios) if complete else None)
        out[rank] = dict(waves=pairs, paired=paired)
    return out

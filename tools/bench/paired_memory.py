"""Complete three-wave solver memory summaries; never infer missing samples."""
import statistics


def summarize_memory(records):
    records = list(records)
    indexed = {}
    for record in records:
        wave, arm = record['wave'], record['arm']
        if type(wave) is not int or wave not in range(3) or arm not in ('candidate', 'previous'):
            raise ValueError('invalid wave or arm')
        if (wave, arm) in indexed:
            raise ValueError('duplicate wave/arm')
        indexed[wave, arm] = record['solve']
    if set(indexed) != {(w, a) for w in range(3) for a in ('candidate', 'previous')}:
        raise ValueError('complete three paired waves required')
    result = {}
    for field in ('peak_rss_kb', 'sampled_process_vm_peak_kb'):
        values = {a: [indexed[w, a].get(field) for w in range(3)]
                  for a in ('candidate', 'previous')}
        missing = [(w, a) for (w, a), solve in indexed.items() if solve.get(field) is None]
        for group in values.values():
            if any(v is not None and (type(v) is not int or v <= 0) for v in group):
                raise ValueError('memory samples must be positive integer KiB')
        if missing:
            result[field] = dict(complete=False, missing=sorted(missing), values=values)
            continue
        ratios = [values['candidate'][w] / values['previous'][w] for w in range(3)]
        result[field] = dict(complete=True, values=values,
            arms={a: dict(min=min(v), median=statistics.median(v), max=max(v))
                  for a, v in values.items()},
            paired=dict(values=ratios, min=min(ratios), median=statistics.median(ratios), max=max(ratios)))
    return result

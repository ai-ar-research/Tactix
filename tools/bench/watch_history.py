"""Strict projection and contiguous state blocks for selected watch histories."""
from itertools import groupby
from watch_observer import watch_payload


def validate_projection(original, observed, *, window, scan):
    """The old selected window must survive exactly, including duplicates."""
    if not original or not observed:
        raise ValueError('empty watch history')
    for row in observed:
        if row['event'] != 'watch':
            raise ValueError('non-watch event in strict history')
        watch_payload(row['payload'], scan)
    old = [r for r in original if r['event'] == 'watch']
    new = [r for r in observed if window[0] <= r['conflicts'] <= window[1]]
    normalize = lambda rows: [{k:v for k,v in r.items() if k != 'seq'} for r in rows]
    if not old or normalize(old) != normalize(new):
        raise ValueError('selected watch projection changed')
    return len(new)


def selected_blocks(rows, *, scan, clause_keys):
    """Group contiguous states, not purported unique scans; retain ambiguity."""
    fields = ('conflicts','decisions','propagations','level','mode','search_external')
    result = []
    for state, group in groupby(rows, key=lambda r:tuple(r[k] for k in fields)):
        group = list(group)
        parsed = [watch_payload(r['payload'],scan) for r in group]
        hits = {name:[dict(position=i,row=r,fields=p) for i,(r,p) in enumerate(zip(group,parsed))
                      if p['clause']==key] for name,key in clause_keys.items()}
        if not any(hits.values()):
            continue
        result.append(dict(state=dict(zip(fields,state)),first_seq=group[0]['seq'],
            entries=len(group),hits=hits,unique_pair=all(len(h)==1 for h in hits.values())))
    return result

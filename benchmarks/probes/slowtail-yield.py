#!/usr/bin/env python3
"""Simplification-YIELD comparison: do we simplify LESS than kissat/cadical?

If our conflict count is high because the formula we search is less simplified,
the yields differ. If the yields match and conflicts still differ, the residual
is search shape.
"""
import glob
import re
import sys

rawdir, jid = sys.argv[1], sys.argv[2]
by = {}
for p in sorted(glob.glob('%s/slowtail1-%s_*.out' % (rawdir, jid))):
    txt = open(p, errors='replace').read()
    m = re.search(r'=== label=(\S+) instance=\S+ solver=(\S+)', txt)
    if m:
        by.setdefault(m.group(1).rsplit('-', 1)[0], {})[m.group(2)] = txt

# (label, our regex, kissat stat name, cadical stat name)
METRICS = [
    ('BVE elim vars', r'^c bve +\d+ rounds, (\d+) eliminated', 'eliminated', 'eliminated'),
    ('substituted', r'^c substitute +\d+ rounds, (\d+) substituted', 'substituted', 'substituted'),
    ('congruent', r'^c congruence +\d+ rounds.*?(\d+) merged', 'congruent', None),
    ('factored/added', r'^c factor +\d+ rounds, (\d+)', 'factored', 'factored'),
    ('vivified', r'^c vivify +\d+ rounds, \d+ checked, (\d+) ', 'vivified', 'vivified'),
    ('subsumed', r'^c subsume +\d+ rounds \(\d+ checked, \d+ visits, (\d+) subsumed', 'subsumed', 'subsumed'),
    ('sweep equivs', r'^c sweep .*?(\d+) equivalences', None, 'sweep equivs'),
]

print('%-11s %-16s %12s %12s %12s' % ('inst', 'metric', 'ours', 'kissat', 'cadical'))
for k, v in by.items():
    o = v.get('ours', '')
    ki = v.get('kis', '')
    ca = v.get('cad', '')
    for name, ourpat, kname, cname in METRICS:
        m = re.search(ourpat, o, re.M)
        ours_v = int(m.group(1)) if m else None
        kv = cv = None
        if kname:
            mm = re.search(r'^c %s: +(\d+)' % re.escape(kname), ki, re.M)
            kv = int(mm.group(1)) if mm else None
        if cname:
            mm = re.search(r'^c %s: +(\d+)' % re.escape(cname), ca, re.M)
            cv = int(mm.group(1)) if mm else None
        if ours_v is None and kv is None and cv is None:
            continue
        print('%-11s %-16s %12s %12s %12s' % (
            k, name,
            ours_v if ours_v is not None else '-',
            kv if kv is not None else '-',
            cv if cv is not None else '-'))
    print()

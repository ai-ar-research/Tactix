#!/usr/bin/env python3
"""Collate an arms wave: one row per instance, one column per arm.

Usage: slowtail-arms-collate.py <rawdir> <prefix> <jobid> [arm,arm,...]
"""
import glob
import re
import sys

rawdir, prefix, jid = sys.argv[1], sys.argv[2], sys.argv[3]
want = sys.argv[4].split(',') if len(sys.argv) > 4 else None

rows = {}
arms = []
for p in sorted(glob.glob('%s/%s-%s_*.out' % (rawdir, prefix, jid))):
    txt = open(p, errors='replace').read()
    m = re.search(r'=== label=(\S+) instance=\S+ solver=(\S+)', txt)
    if not m:
        continue
    inst, arm = m.group(1).rsplit('-', 1)
    if arm not in arms:
        arms.append(arm)
    w = re.search(r'wall ([\d.]+) s', txt)
    verdict = ('SAT' if '\ns SATISFIABLE' in txt else
               'UNS' if '\ns UNSATISFIABLE' in txt else 'TO')
    c = re.search(r'^c conflicts   (\d+)', txt, re.M)
    if not c:
        c = re.search(r'^c conflicts: +(\d+)', txt, re.M)
    sl = re.search(r'^c timing +([\d.]+) ms \([\d.]+%\) inprocessing slot', txt, re.M)
    bve = re.search(r'^c bve +(\d+) rounds, (\d+) eliminated', txt, re.M)
    kel = re.search(r'^c eliminated: +(\d+)', txt, re.M)
    rows.setdefault(inst, {})[arm] = dict(
        wall=float(w.group(1)) if w else None, verdict=verdict,
        confl=int(c.group(1)) if c else None,
        slot=float(sl.group(1)) / 1e3 if sl else None,
        elim=int(bve.group(2)) if bve else (int(kel.group(1)) if kel else None),
        txt=txt)

if want:
    arms = [a for a in want if a in arms]

for title, key, fmt in [('WALL (s)', 'wall', '%9.1f'),
                        ('CONFLICTS', 'confl', '%9d'),
                        ('SLOT (s)', 'slot', '%9.1f'),
                        ('vars eliminated', 'elim', '%9d')]:
    print('=== %s ===' % title)
    print('%-11s' % 'inst' + ''.join('%10s' % a for a in arms))
    for inst, v in rows.items():
        line = '%-11s' % inst
        for a in arms:
            d = v.get(a)
            if not d or d.get(key) is None:
                line += '%10s' % ('TO' if d and d['verdict'] == 'TO' else '-')
            else:
                line += ('%10s' % (fmt % d[key])) if d['verdict'] != 'TO' \
                    else '%10s' % 'TO'
        print(line)
    print()

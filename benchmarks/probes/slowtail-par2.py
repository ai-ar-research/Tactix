#!/usr/bin/env python3
"""Per-arm PAR-2 over the slow-solved tail, against the SAME-WINDOW base arm.

Usage: slowtail-par2.py <rawdir> <prefix> <jobid> [--base ARM]
A wave without a `base` arm is scored against the wave-2 base (passed via
--baseline prefix:jobid) so cross-window inflation is at least named.
"""
import glob
import re
import sys

rawdir, prefix, jid = sys.argv[1], sys.argv[2], sys.argv[3]
TO = 300.0  # Tier B timeout: PAR-2 charges 2*TO for an unsolved instance

# reference numbers from the tier run 20260806-102432-B-ledgerfix
KIS = dict(nw=6.0, crypto1=4.2, divs=18.7, qwh=11.6, timetable=11.5,
           prp=17.0, b22=8.6, veer=3.7)
CAD = dict(nw=19.3, crypto1=18.8, divs=5.7, qwh=16.8, timetable=31.1,
           prp=30.9, b22=6.7, veer=5.1)

rows, arms = {}, []
for p in sorted(glob.glob('%s/%s-%s_*.out' % (rawdir, prefix, jid))):
    txt = open(p, errors='replace').read()
    m = re.search(r'=== label=(\S+) instance=\S+ solver=(\S+)', txt)
    if not m:
        continue
    inst, arm = m.group(1).rsplit('-', 1)
    if arm not in arms:
        arms.append(arm)
    w = re.search(r'wall ([\d.]+) s', txt)
    solved = ('\ns SATISFIABLE' in txt) or ('\ns UNSATISFIABLE' in txt)
    rows.setdefault(inst, {})[arm] = (float(w.group(1)) if w else None, solved)

insts = list(rows.keys())
print('instances (%d): %s' % (len(insts), ' '.join(insts)))
print('kissat total on them: %.1f    cadical: %.1f' %
      (sum(KIS[i] for i in insts if i in KIS),
       sum(CAD[i] for i in insts if i in CAD)))
print()
print('%-10s %10s %8s  %s' % ('arm', 'PAR2', 'vs base', 'per-instance delta vs base'))
base = {}
if 'base' in arms:
    base = {i: rows[i]['base'] for i in insts if 'base' in rows[i]}


def par2(arm):
    t = 0.0
    for i in insts:
        d = rows[i].get(arm)
        if d is None:
            return None
        t += d[0] if d[1] else 2 * TO
    return t


bp = par2('base') if base else None
for a in arms:
    p = par2(a)
    if p is None:
        print('%-10s %10s' % (a, 'incomplete'))
        continue
    deltas = []
    if base:
        for i in insts:
            bw, bs = base[i]
            aw, as_ = rows[i][a]
            b = bw if bs else 2 * TO
            x = aw if as_ else 2 * TO
            deltas.append('%s %+.0f' % (i[:4], x - b))
    print('%-10s %10.1f %8s  %s' % (
        a, p, ('%+.1f' % (p - bp)) if bp else '-', ' '.join(deltas)))

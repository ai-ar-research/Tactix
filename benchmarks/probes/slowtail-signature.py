#!/usr/bin/env python3
"""Shared-signature test across the slow-but-solved tail.

(a) per-rule spent/handed + completed (is the effort ledger BINDING?)
(b) clause-quality / restart / reduction comparison vs kissat
"""
import glob
import re
import sys

rawdir, jid = sys.argv[1], sys.argv[2]
ours, kis = {}, {}
for p in sorted(glob.glob('%s/slowtail1-%s_*.out' % (rawdir, jid))):
    txt = open(p, errors='replace').read()
    m = re.search(r'=== label=(\S+) instance=\S+ solver=(\S+)', txt)
    if not m:
        continue
    key = m.group(1).rsplit('-', 1)[0]
    (ours if m.group(2) == 'ours' else kis if m.group(2) == 'kis' else {})[key] = txt

RULES = ['Strengthen', 'SubsumeForward', 'Vivify', 'BVE', 'Factor', 'Congruence',
         'Sweep', 'CCE', 'BCE', 'Instantiate', 'HyperTernaryRes', 'Probe',
         'Backbone', 'TransitiveReduce', 'Substitute']

print('=== (a) is the effort ledger BINDING?  spent/handed, and completed/adm ===')
print('    "handed 0" = unmetered.  spent/handed >> 1 with completed 0 = the')
print('    budget was handed out and then ignored: the round is uninterruptible.')
hdr = '%-18s' % 'rule'
keys = list(ours.keys())
for k in keys:
    hdr += ' %14s' % k
print(hdr)
for r in RULES:
    row = '%-18s' % r
    any_ = False
    for k in keys:
        m = re.search(r'^c slot %s +occ +(\d+) adm +(\d+) ok +(\d+).*?'
                      r'handed (\d+) spent (\d+) completed (\d+)' % r,
                      ours[k], re.M)
        if not m:
            row += ' %14s' % '-'
            continue
        occ, adm, ok, handed, spent, comp = (int(x) for x in m.groups())
        if handed == 0:
            cell = 'unmet a%d' % adm
        else:
            cell = '%.0fx c%d/a%d' % (spent / handed, comp, adm)
            any_ = True
        row += ' %14s' % cell
    print(row)

print()
print('=== (b) search-shape comparison vs kissat ===')
print('%-11s %-22s %10s %10s' % ('inst', 'metric', 'ours', 'kissat'))
for k in keys:
    o, ki = ours[k], kis.get(k, '')

    def go(pat, cast=float):
        m = re.search(pat, o, re.M)
        return cast(m.group(1)) if m else None

    def gk(name, col=1):
        m = re.search(r'^c %s: +([\d.]+) +([\d.]+)?' % re.escape(name), ki, re.M)
        if not m:
            return None
        return float(m.group(col))

    confl = go(r'^c conflicts   (\d+)')
    kconfl = gk('conflicts')
    rows = [
        ('conflicts', confl, kconfl),
        ('learned avg lits', go(r'^c learnedsize ([\d.]+) avg lits'),
         gk('clause size', 2)),
        ('learned avg glue', go(r'^c learnedsize [\d.]+ avg lits, ([\d.]+) avg glue'),
         gk('clause glue', 2)),
        ('restarts', go(r'^c restarts +(\d+)'), gk('restarts')),
        ('conflicts/restart',
         (confl / go(r'^c restarts +(\d+)')) if confl and go(r'^c restarts +(\d+)') else None,
         gk('restarts', 2)),
        ('reduce rounds', go(r'^c reduce +(\d+) rounds'), gk('reductions')),
        ('minimize+shrink %', go(r'^c reduction +([\d.]+)%'), None),
    ]
    print('%-11s' % k, end='')
    first = True
    for name, a, b in rows:
        if a is None and b is None:
            continue
        pre = '' if first else '%-11s' % ''
        first = False
        print('%s%-22s %10s %10s' % (pre, name,
                                     ('%.2f' % a) if a is not None else '-',
                                     ('%.2f' % b) if b is not None else '-'))
    print()

#!/usr/bin/env python3
"""Collate slowtail-decompose raw outputs into the shared decomposition table.

Usage: slowtail-collate.py <rawdir> <jobid>
"""
import glob
import re
import sys

rawdir, jid = sys.argv[1], sys.argv[2]

recs = {}
for p in sorted(glob.glob('%s/slowtail1-%s_*.out' % (rawdir, jid))):
    txt = open(p, errors='replace').read()
    m = re.search(r'=== label=(\S+) instance=(\S+) solver=(\S+)', txt)
    if not m:
        continue
    label, inst, solver = m.groups()
    key = label.rsplit('-', 1)[0]
    d = {'file': p, 'solver': solver}
    w = re.search(r'wall ([\d.]+) s', txt)
    d['wall'] = float(w.group(1)) if w else None
    d['verdict'] = ('SAT' if '\ns SATISFIABLE' in txt
                    else 'UNS' if '\ns UNSATISFIABLE' in txt else 'TO/UNK')
    if solver == 'ours':
        def g(pat, cast=int):
            mm = re.search(pat, txt, re.M)
            return cast(mm.group(1)) if mm else None
        d['confl'] = g(r'^c conflicts   (\d+)', )
        d['dec'] = g(r'^c decisions   (\d+)')
        pm = re.search(r'^c propagations (\d+) \((\d+) search, (\d+) inprocess\)',
                       txt, re.M)
        if pm:
            d['props'] = int(pm.group(1))
            d['sprops'] = int(pm.group(2))
            d['iprops'] = int(pm.group(3))
        d['restarts'] = g(r'^c restarts +(\d+)')
        # timing block
        tm = re.search(r'^c timing +([\d.]+) ms solve', txt, re.M)
        d['solve_ms'] = float(tm.group(1)) if tm else None
        tm = re.search(r'^c timing +([\d.]+) ms \([\d.]+%\) inprocessing slot, '
                       r'([\d.]+) ms', txt, re.M)
        if tm:
            d['slot_ms'] = float(tm.group(1))
            d['search_ms'] = float(tm.group(2))
        d['rules'] = {}
        for rm in re.finditer(r'^c timing +(\S+) +([\d.]+) ms \( *([\d.]+)%\)'
                              r'(?: in (\d+) applications)?', txt, re.M):
            d['rules'][rm.group(1)] = (float(rm.group(2)), float(rm.group(3)),
                                       int(rm.group(4)) if rm.group(4) else 0)
        d['slots'] = {}
        for sm in re.finditer(r'^c slot (\S+) +occ +(\d+) adm +(\d+) ok +(\d+).*?'
                              r'handed (\d+) spent (\d+) completed (\d+)', txt, re.M):
            d['slots'][sm.group(1)] = dict(
                occ=int(sm.group(2)), adm=int(sm.group(3)), ok=int(sm.group(4)),
                handed=int(sm.group(5)), spent=int(sm.group(6)),
                completed=int(sm.group(7)))
        d['unacc'] = 'UNACCOUNTED' in txt
        d['raw'] = txt
    else:
        def gk(name):
            mm = re.search(r'^c %s: +(\d+)' % re.escape(name), txt, re.M)
            return int(mm.group(1)) if mm else None
        d['confl'] = gk('conflicts')
        d['dec'] = gk('decisions')
        d['props'] = gk('propagations')
        d['restarts'] = gk('restarts')
        d['raw'] = txt
    recs.setdefault(key, {})[solver] = d

print('%-11s %-4s %8s %8s %8s | %12s %12s %12s | %6s %6s' %
      ('inst', 'v', 'ours', 'kis', 'cad', 'confl-ours', 'confl-kis', 'confl-cad',
       'xwall', 'xconf'))
for k, v in recs.items():
    o, ki, ca = v.get('ours', {}), v.get('kis', {}), v.get('cad', {})
    xw = (o['wall'] / ki['wall']) if o.get('wall') and ki.get('wall') else None
    xc = (o['confl'] / ki['confl']) if o.get('confl') and ki.get('confl') else None
    print('%-11s %-4s %8s %8s %8s | %12s %12s %12s | %6s %6s' % (
        k, o.get('verdict', '?'),
        '%.1f' % o['wall'] if o.get('wall') else '-',
        '%.1f' % ki['wall'] if ki.get('wall') else '-',
        '%.1f' % ca['wall'] if ca.get('wall') else '-',
        o.get('confl'), ki.get('confl'), ca.get('confl'),
        '%.2f' % xw if xw else '-', '%.2f' % xc if xc else '-'))

print()
print('--- per-conflict decomposition (props/confl, dec/confl) ---')
print('%-11s %9s %9s %9s | %8s %8s | %7s %7s' %
      ('inst', 'p/c ours', 'p/c kis', 'p/c cad', 'd/c our', 'd/c kis',
       'xp/c', 'srch-p/c'))
for k, v in recs.items():
    o, ki, ca = v.get('ours', {}), v.get('kis', {}), v.get('cad', {})
    def pc(d):
        return d['props'] / d['confl'] if d.get('props') and d.get('confl') else None
    po, pk, pca = pc(o), pc(ki), pc(ca)
    spc = (o['sprops'] / o['confl']) if o.get('sprops') and o.get('confl') else None
    print('%-11s %9s %9s %9s | %8s %8s | %7s %7s' % (
        k, '%.0f' % po if po else '-', '%.0f' % pk if pk else '-',
        '%.0f' % pca if pca else '-',
        '%.1f' % (o['dec'] / o['confl']) if o.get('dec') and o.get('confl') else '-',
        '%.1f' % (ki['dec'] / ki['confl']) if ki.get('dec') and ki.get('confl') else '-',
        '%.2f' % (po / pk) if po and pk else '-',
        '%.0f' % spc if spc else '-'))

print()
print('--- TB5 test: raw propagation THROUGHPUT (M props/s) ---')
print('%-11s %10s %10s %10s | %7s   %s' %
      ('inst', 'ours-srch', 'kis', 'cad', 'xours', 'note'))
for k, v in recs.items():
    o, ki, ca = v.get('ours', {}), v.get('kis', {}), v.get('cad', {})
    mo = mk = mc = None
    if o.get('sprops') and o.get('search_ms'):
        mo = o['sprops'] / (o['search_ms'] * 1e3)
    if ki.get('props') and ki.get('wall'):
        mk = ki['props'] / (ki['wall'] * 1e6)
    if ca.get('props') and ca.get('wall'):
        mc = ca['props'] / (ca['wall'] * 1e6)
    print('%-11s %10s %10s %10s | %7s' % (
        k, '%.2f' % mo if mo else '-', '%.2f' % mk if mk else '-',
        '%.2f' % mc if mc else '-', '%.2f' % (mo / mk) if mo and mk else '-'))

print()
print('--- wall-ratio attribution: xwall vs xconf * x(srch-time/conf) ---')
print('%-11s %7s %7s %7s %7s  %s' %
      ('inst', 'xwall', 'xconf', 'slot-s', 'srch-s', 'srch-s/kis-s'))
for k, v in recs.items():
    o, ki = v.get('ours', {}), v.get('kis', {})
    if not (o.get('search_ms') and ki.get('wall')):
        continue
    print('%-11s %7.2f %7.2f %7.1f %7.1f  %.2f' % (
        k, o['wall'] / ki['wall'], o['confl'] / ki['confl'],
        o['slot_ms'] / 1e3, o['search_ms'] / 1e3,
        (o['search_ms'] / 1e3) / ki['wall']))

print()
print('--- where OUR wall clock goes (ms) ---')
for k, v in recs.items():
    o = v.get('ours', {})
    if not o.get('solve_ms'):
        continue
    print('%-11s solve %8.0f  slot %8.0f (%4.1f%%)  search %8.0f (%4.1f%%)%s' % (
        k, o['solve_ms'], o.get('slot_ms', 0),
        100 * o.get('slot_ms', 0) / o['solve_ms'], o.get('search_ms', 0),
        100 * o.get('search_ms', 0) / o['solve_ms'],
        '   ***UNACCOUNTED SLOT ENTRY***' if o.get('unacc') else ''))
    rs = sorted(o['rules'].items(), key=lambda x: -x[1][0])
    for name, (ms, pct, calls) in rs:
        if ms >= 0.005 * o['solve_ms']:
            print('              %-18s %8.0f ms (%4.1f%%) in %d applications' %
                  (name, ms, pct, calls))

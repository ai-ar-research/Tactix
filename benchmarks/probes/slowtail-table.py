#!/usr/bin/env python3
"""Tabulate a bench run: ours vs baselines, ratios, PAR-2 decomposition."""
import json, sys

run = sys.argv[1]
d = json.load(open(run + '/results.json'))
r = d['results']
TO = d['timeout']
arms = list(r.keys())
ours = arms[0]
print('tier', d['tier'], 'timeout', TO, 'git', d['git'], 'date', d['date'])


def by_inst(a):
    return {x['instance']: x for x in r[a]}


maps = {a: by_inst(a) for a in arms}
o = maps[ours]
rows = []
for inst, a in o.items():
    ot = a['time'] if a['solved'] else None
    kt = maps.get('kissat', {}).get(inst)
    ct = maps.get('cadical', {}).get(inst)
    ktt = kt['time'] if kt and kt['solved'] else None
    ctt = ct['time'] if ct and ct['solved'] else None
    ratio = (ot / ktt) if (ot and ktt) else None
    rows.append((inst, a['verdict'], a['solved'], ot, ktt, ctt, ratio))

rows.sort(key=lambda x: (0 if x[2] else 1, -(x[6] or 0)))
print('%-64s %-4s %8s %8s %8s %6s' % ('instance', 'v', 'ours', 'kis', 'cad', 'xkis'))
for inst, v, s, ot, kt, ct, ra in rows:
    if not s or (ra and ra >= 1.5):
        f = lambda t: ('%.1f' % t) if t else 'TO'
        print('%-64s %-4s %8s %8s %8s %6s' % (
            inst.replace('tierB/', '').replace('tierC/', '').replace('.cnf.xz', '')[:64],
            v[:3], f(ot), f(kt), f(ct), ('%.2f' % ra) if ra else '-'))


def par2(a):
    return sum(x['time'] if x['solved'] else 2 * TO for x in r[a])


print()
for a in arms:
    solved = sum(1 for x in r[a] if x['solved'])
    tos = [x for x in r[a] if not x['solved']]
    solved_time = sum(x['time'] for x in r[a] if x['solved'])
    print('%-12s solved %3d/%3d  PAR2 %9.1f   (TO term %7.1f, solved term %8.1f)' % (
        a, solved, len(r[a]), par2(a), 2 * TO * len(tos), solved_time))

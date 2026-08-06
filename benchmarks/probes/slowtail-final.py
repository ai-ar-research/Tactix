#!/usr/bin/env python3
"""Final arithmetic: per-arm PAR-2 on the tail, rescaled to the TIER window.

Each probe wave carries its own `base`, so an arm is scored as
  arm_tier_equivalent = arm_in_window * (tier_base / wave_base)
which cancels the wave's contention inflation. Tier numbers come from
benchmarks/runs/20260806-102432-B-ledgerfix.
"""
TIER = dict(nw=43.5, crypto1=26.1, divs=113.2, qwh=58.4, timetable=46.5,
            prp=69.5, b22=32.7, veer=15.8)
KIS = dict(nw=6.0, crypto1=4.2, divs=18.7, qwh=11.6, timetable=11.5,
           prp=17.0, b22=8.6, veer=3.7)
CAD = dict(nw=19.3, crypto1=18.8, divs=5.7, qwh=16.8, timetable=31.1,
           prp=30.9, b22=6.7, veer=5.1)

# in-window measurements, keyed wave -> arm -> per-instance seconds
W = {}
W['w2'] = {
    'base':  dict(nw=44.7, crypto1=25.2, divs=135.4, qwh=67.6, timetable=46.1, prp=77.0, b22=38.4, veer=15.7),
    'mb':    dict(nw=45.2, crypto1=3.2, divs=22.5, qwh=56.1, timetable=14.7, prp=103.4, b22=40.9, veer=16.9),
    'noC1':  dict(nw=12.7, crypto1=7.3, divs=125.8, qwh=56.3, timetable=13.7, prp=57.2, b22=35.6, veer=16.7),
    'viv10': dict(nw=89.7, crypto1=11.1, divs=138.3, qwh=17.8, timetable=10.2, prp=50.2, b22=38.5, veer=14.9),
}
W['w4'] = {
    'base':  dict(nw=44.5, crypto1=25.2, divs=150.5, qwh=57.6, timetable=47.6, prp=76.9, b22=38.7, veer=16.3),
    'mbC1':  dict(nw=12.9, crypto1=2.8, divs=21.9, qwh=57.9, timetable=15.0, prp=49.8, b22=41.4, veer=17.8),
    'all30': dict(nw=74.7, crypto1=2.7, divs=18.6, qwh=23.4, timetable=15.0, prp=49.7, b22=33.9, veer=15.6),
}
W['w5'] = {
    'base':    dict(nw=43.7, crypto1=25.4, divs=160.0, qwh=68.4, timetable=60.5, prp=75.0, b22=36.8, veer=16.4),
    'eos':     dict(nw=15.5, crypto1=2.8, divs=164.9, qwh=67.4, timetable=56.0, prp=44.8, b22=13.3, veer=51.8),
    'mbC1eos': dict(nw=58.8, crypto1=4.8, divs=22.9, qwh=50.8, timetable=49.1, prp=58.0, b22=14.1, veer=60.1),
    'allX30':  dict(nw=9.3, crypto1=2.9, divs=19.7, qwh=16.4, timetable=36.7, prp=81.8, b22=11.8, veer=46.8),
}

I = list(TIER)
tier_tot = sum(TIER.values())
print('TIER window (20260806-102432-B-ledgerfix): ours %.1f  kissat %.1f  cadical %.1f'
      % (tier_tot, sum(KIS.values()), sum(CAD.values())))
print('excess over kissat on these 8: %.1f s of PAR-2\n' % (tier_tot - sum(KIS.values())))

print('%-9s %8s %8s %9s   %s' % ('arm', 'in-win', 'rescaled', 'recovered',
                                 'per-instance (rescaled)'))
out = []
for wname, arms in W.items():
    b = arms['base']
    scale = tier_tot / sum(b.values())
    for a, v in arms.items():
        if a == 'base':
            continue
        resc = {i: v[i] * scale for i in I}
        tot = sum(resc.values())
        out.append((tot, a, wname, sum(v.values()), resc))
for tot, a, wname, inwin, resc in sorted(out):
    per = ' '.join('%s %.0f' % (i[:4], resc[i]) for i in I)
    print('%-9s %8.1f %8.1f %9.1f   %s' % (a, inwin, tot, tier_tot - tot, per))

print()
print('residual over kissat, per instance, for the top arm (mbC1):')
b = W['w4']['base']
scale = tier_tot / sum(b.values())
mb = W['w4']['mbC1']
for i in I:
    r = mb[i] * scale
    print('  %-11s %6.1f  (kissat %5.1f, cadical %5.1f)  excess %+6.1f   x%.2f'
          % (i, r, KIS[i], CAD[i], r - KIS[i], r / KIS[i]))
print('  %-11s %6.1f  (kissat %5.1f)                    excess %+6.1f'
      % ('TOTAL', sum(mb[i] * scale for i in I), sum(KIS.values()),
         sum(mb[i] * scale for i in I) - sum(KIS.values())))

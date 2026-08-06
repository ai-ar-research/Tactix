#!/usr/bin/env python3
"""Wave 4: do the three wave-2 winners COMPOSE, or do they overlap?

Wave 2 (all 8 instances, one window) found three separable levers:
  mb    = TACTIX_MIDROUND_BUDGET=1 -- enforce the C1/C2 handed budget, which
          SCHED-B4's meter re-pricing made bind 1000-2500x harder than when
          the 2026-08-04 A/B declared it inert
  noC1  = drop SubsumeForward, which SCHED-G2 measured inert in YIELD
  viv10 = Vivify 100 -> 10 per mille
Each wins on a DIFFERENT subset, so composition is the question a ranked plan
needs answered. `base` runs again here as the in-window control.
"""
INSTS = [
    ('nw', '2013113162201nw'),
    ('crypto1', 'combined-crypto1-wff-seed-1-wffvars-450'),
    ('divs', 'DivS_862_11'),
    ('qwh', 'qwh.50.1250'),
    ('timetable', 'SC25_Timetable_C_392'),
    ('prp', 'PRP_30_38'),
    ('b22', 'b22_1'),
    ('veer', 'veer_axi_yosyshq'),
]

S = 'TACTIX_SLOT_STATS=1'
MB = 'TACTIX_MIDROUND_BUDGET=1'
NOC1 = 'TACTIX_SLOT_DISABLE=SubsumeForward'
V10 = 'TACTIX_SLOT_PERMILLE=Vivify:10'
V30 = 'TACTIX_SLOT_PERMILLE=Vivify:30'

ARMS = [
    ('base', [S]),
    ('mbC1', [S, MB, NOC1]),
    ('mbviv', [S, MB, V10]),
    ('C1viv', [S, NOC1, V10]),
    ('all3', [S, MB, NOC1, V10]),
    ('viv30', [S, V30]),
    ('all30', [S, MB, NOC1, V30]),
]

with open(__file__.replace('-combo-gen.py', '-combo.mf'), 'w') as f:
    for iname, stem in INSTS:
        for aname, env in ARMS:
            f.write('%s-%s|%s|400|ours|env:%s\n' % (iname, aname, stem, ';'.join(env)))
print(len(INSTS) * len(ARMS), 'tasks')

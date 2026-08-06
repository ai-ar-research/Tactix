#!/usr/bin/env python3
"""Wave 5: do the C1/C2 family and the ELIMINATION family compose?

They are independent mechanisms, so a ranked plan needs the cross terms:
  mb    = TACTIX_MIDROUND_BUDGET=1        (bound C1/C2 mid-round)
  noC1  = TACTIX_SLOT_DISABLE=SubsumeForward
  eos   = TACTIX_BVE_DOSE_DENSITY=0 + TACTIX_BVE_ONESHOT=1
          (let BVE run, and let its first dose run to COMPLETION -- wave 3
          showed the bounded drip is what damages the trajectory: crypto1
          32.4 s dosed vs 2.8 s completed)
  viv   = Vivify 100 -> 10 / 30 per mille
`base` is the in-window control.
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
EOS = 'TACTIX_BVE_DOSE_DENSITY=0;TACTIX_BVE_ONESHOT=1'
V10 = 'TACTIX_SLOT_PERMILLE=Vivify:10'
V30 = 'TACTIX_SLOT_PERMILLE=Vivify:30'

ARMS = [
    ('base', [S]),
    ('eos', [S, EOS]),
    ('mbC1eos', [S, MB, NOC1, EOS]),
    ('allX', [S, MB, NOC1, V10, EOS]),
    ('allX30', [S, MB, NOC1, V30, EOS]),
]

with open(__file__.replace('-cross-gen.py', '-cross.mf'), 'w') as f:
    for iname, stem in INSTS:
        for aname, env in ARMS:
            f.write('%s-%s|%s|400|ours|env:%s\n' % (iname, aname, stem, ';'.join(env)))
print(len(INSTS) * len(ARMS), 'tasks')

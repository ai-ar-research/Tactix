#!/usr/bin/env python3
"""Generate the wave-2 manifest: the arms that price each candidate cause.

Same 8 instances as wave 1, ALL arms in ONE submission so contention cancels.
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
ARMS = [
    # control, in THIS window
    ('base', 'ours', 'env:' + S),
    # enforce the C1/C2 handed budget at the SHIPPED 100 per-mille dose
    ('mb', 'ours', 'env:%s;TACTIX_MIDROUND_BUDGET=1' % S),
    # ... and at 10 / 1 per mille (the dose axis D-STRENGTHEN-BUDGET left live)
    ('mb10', 'ours',
     'env:%s;TACTIX_MIDROUND_BUDGET=1;TACTIX_SLOT_PERMILLE=Strengthen:10,SubsumeForward:10' % S),
    ('mb1', 'ours',
     'env:%s;TACTIX_MIDROUND_BUDGET=1;TACTIX_SLOT_PERMILLE=Strengthen:1,SubsumeForward:1' % S),
    # SCHED-G2 measured C1 INERT -- price dropping it (DIAGNOSTIC, not a config)
    ('noC1', 'ours', 'env:%s;TACTIX_SLOT_DISABLE=SubsumeForward' % S),
    # Vivify is the one rule in the top cost block on all 8 -- is it buying it?
    ('viv10', 'ours', 'env:%s;TACTIX_SLOT_PERMILLE=Vivify:10' % S),
    # the debug-performance step-3 baseline ablation
    ('kisoff', 'kisoff', 'shipped'),
]

with open(__file__.replace('-arms-gen.py', '-arms.mf'), 'w') as f:
    for iname, stem in INSTS:
        for aname, solver, arm in ARMS:
            f.write('%s-%s|%s|400|%s|%s\n' % (iname, aname, stem, solver, arm))
print(len(INSTS) * len(ARMS), 'tasks')

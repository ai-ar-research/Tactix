#!/usr/bin/env python3
"""Wave 3: price the ELIMINATION gap that wave 1 exposed.

Wave 1 finding: `c bve 0 rounds, 0 eliminated` on 7 of the 8 slow-solved
instances, while kissat eliminates 32-89 % of variables and cadical 16-82 %.
Two gates produce it (both visible in `c slot BVE`): `defer_preprocess`
(conflicts==0) takes the 3 preprocessing occasions, the binary-density dose gate
(binaries < 6*outputs) takes every during-search one.

Both directions, in ONE window:
  * force our elimination ON  -> does it help or (as the density gate's own
    warrant claims) hurt on exactly this set?
  * turn the reference's elimination OFF -> does its conflict count rise to
    ours (debug-performance step 3/4)?
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
    # dose everywhere: the density gate's documented 0 = off value
    ('dns0', 'ours', 'env:%s;TACTIX_BVE_DOSE_DENSITY=0' % S),
    # ... and with the first dose effectively unbounded, so it runs its whole
    # schedule instead of drip-feeding
    ('dns0os', 'ours', 'env:%s;TACTIX_BVE_DOSE_DENSITY=0;TACTIX_BVE_ONESHOT=1' % S),
    # learn-then-decide admission instead of the static separator
    ('admcost', 'ours', 'env:%s;TACTIX_BVE_ADMIT=cost' % S),
    # the reference-side attribution
    ('kisnoelim', 'kisnoelim', 'shipped'),
    ('cadnoelim', 'cadnoelim', 'shipped'),
]

with open(__file__.replace('-elim-gen.py', '-elim.mf'), 'w') as f:
    for iname, stem in INSTS:
        for aname, solver, arm in ARMS:
            f.write('%s-%s|%s|400|%s|%s\n' % (iname, aname, stem, solver, arm))
print(len(INSTS) * len(ARMS), 'tasks')

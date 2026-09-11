#!/usr/bin/env python3
"""Assess all declared AND runtime and complete Congruence profile repeats."""
from pathlib import Path
base = Path(__file__).with_name('assess-factor-arena-7192.py')
code = base.read_text()
for before,after in {
    '2026-09-11-factor-arena-assessment-7192.json':'2026-09-11-congruence-fused-assessment-7227.json',
    '20260910-7186-factor-arena-runtime':'20260911-7205-congruence-fused-runtime',
    '20260910-7188-factor-arena-samples':'20260911-7207-congruence-fused-samples',
    'check-submit-factor-arena-runtime-7186.py':'check-submit-congruence-fused-runtime-7205.py',
    'validate-factor-arena-profiles-7187.py':'validate-congruence-fused-profiles-7206.py',
    "['Factor', 'Vivify', 'BVE']":"['Congruence', 'Vivify', 'BVE']",
    'factor_ratios':'congruence_ratios',
    'complete_factor':'complete_congruence',
    'full Factor envelope':'full Congruence envelope',
    'Complete Factor':'Complete Congruence',
    'Arena assessment:':'Congruence assessment:',
    'Decide whether the measured net gain supports retaining the isolated prototype and a fresh all50 transfer. Keep larger Vivify/Sweep/Congruence and31/43 search-work gaps in priority; do not keep iterating tiny Factor changes by default.':
        'Judge whether all three paired runtime distributions support a net gain despite the local Congruence reduction. Preserve adverse whole-runtime results, exact work, RSS and phase decomposition; no favorable-wave selection. Continue the larger Reduce/collection7218 track independently; no AND/arena/root-binary composition or production promotion from phase-only gains.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

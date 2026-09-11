#!/usr/bin/env python3
"""Assess every runtime/RSS pair against the complete Eliminate phase."""
from pathlib import Path
base = Path(__file__).with_name('assess-factor-arena-7192.py')
code = base.read_text()
for before, after in {
    '2026-09-11-factor-arena-assessment-7192.json':'2026-09-11-occurrence-assessment-7261.json',
    '20260910-7186-factor-arena-runtime':'20260911-7257-occurrence-runtime',
    '20260910-7188-factor-arena-samples':'20260911-7260-occurrence-samples',
    'check-submit-factor-arena-runtime-7186.py':'check-submit-occurrence-runtime-7257.py',
    'validate-factor-arena-profiles-7187.py':'validate-occurrence-profiles-7259.py',
    "['Factor', 'Vivify', 'BVE']":"['BVE', 'Strengthen', 'Substitute']",
    "ratio = r['function_envelopes']['candidate']['selected_period'] / r['function_envelopes']['previous']['selected_period']":
        "ratio = r['arms']['candidate']['phase_periods']['eliminate'] / r['arms']['previous']['phase_periods']['eliminate']",
    "waves.append(dict(wave=wave, envelopes=r['function_envelopes'], phase_ratios=r['phase_ratios']))":
        "waves.append(dict(wave=wave, inprocess_envelopes=r['function_envelopes'], phase_ratios=r['phase_ratios'], phase_periods={arm:r['arms'][arm]['phase_periods'] for arm in ('candidate','previous')}))",
    'factor_ratios':'eliminate_ratios',
    'complete_factor':'complete_eliminate',
    'full Factor envelope':'full Eliminate phase',
    'Complete Factor':'Complete Eliminate',
    'Arena assessment:':'Occurrence assessment:',
    'Decide whether the measured net gain supports retaining the isolated prototype and a fresh all50 transfer. Keep larger Vivify/Sweep/Congruence and31/43 search-work gaps in priority; do not keep iterating tiny Factor changes by default.':
        'Judge the single-image cutover from all net runtime/RSS pairs and complete Eliminate cost, preserving Probe/outside-phase effects. A lower payload is not automatically lower RSS or runtime. Do not cherry-pick, repeat to favorability or tune alignment. Decide promotion/full50 only from net evidence and required correctness gates.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

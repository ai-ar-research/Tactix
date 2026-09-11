#!/usr/bin/env python3
"""Complete Reduce envelopes without dropping their non-phase ancestry."""
from pathlib import Path
base = Path(__file__).with_name('extract-vivify-compact-wave-7153.py')
code = base.read_text()
for before,after in {
    'validate-vivify-compact-profiles-7152.py':'validate-root-binary-profiles-7230.py',
    '20260910-7153-vivify-compact-samples':'20260911-7231-root-binary-samples',
    "'tactix::vivify_apply('":"'tactix::reduce_apply_refs('",
    "    for phase_name, components in summary['components'].items():\n        assert envelope['by_phase'].get(phase_name, 0) == components.get('vivify', 0), phase_name":
        "    assert set(envelope['by_phase']) == {'no_phase_frame'}, 'Reduce unexpectedly inside Probe/Eliminate: review before attribution'\n    assert envelope['selected_period'] <= summary['phase_periods']['no_phase_frame']",
    'complete Vivify cycles':'complete Reduce cycles',
    'complete_vivify_ratio=':'complete_reduce_ratio=',
}.items():
    assert code.count(before) == 1,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

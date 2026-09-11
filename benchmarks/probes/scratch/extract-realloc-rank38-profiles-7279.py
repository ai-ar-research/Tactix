#!/usr/bin/env python3
"""Conserve phases and the complete propagation envelope for one diagnostic pair."""
from pathlib import Path
base=Path(__file__).with_name('extract-vivify-compact-wave-7153.py')
code=base.read_text()
for before,after in {
    'validate-vivify-compact-profiles-7152.py':'validate-realloc-rank38-profiles-7278.py',
    '20260910-7153-vivify-compact-samples':'20260911-7279-realloc-rank38-samples',
    "sys.argv[1] in ('0', '1', '2')":"sys.argv[1] == '0'",
    "'tactix::vivify_apply('":"'tactix::kernel_propagate('",
    "    assert all(summary['phase_periods'].get(name, 0) > 0 for name in ('initial_probe', 'probe', 'eliminate'))":
        "    assert set(summary['phase_periods']) <= {'initial_probe','probe','eliminate','no_phase_frame'}\n    assert summary['phase_periods'].get('no_phase_frame',0)>0",
    "    for phase_name, components in summary['components'].items():\n        assert envelope['by_phase'].get(phase_name, 0) == components.get('vivify', 0), phase_name":
        "    assert sum(envelope['by_phase'].values())==envelope['selected_period']\n    for phase_name,value in envelope['by_phase'].items():\n        assert 0<=value<=summary['phase_periods'].get(phase_name,0),phase_name",
    'complete Vivify cycles':'complete ordinary-propagation cycles',
    'complete_vivify_ratio=':'complete_propagation_ratio=',
}.items():
    assert before in code,before
    code=code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

#!/usr/bin/env python3
"""Sanitize actual fused AND extraction plus the Congruence rule fixtures."""
from pathlib import Path
base = Path(__file__).with_name('check-runtime-telemetry-sanitizers-7176.py')
code = base.read_text()
changes = {
    'runtime-telemetry-ablation-7172': 'runtime-congruence-fused-7196',
    '20260910-7176-runtime-telemetry-sanitizers': '20260911-7197-congruence-fused-sanitizers',
    'pin-7176-runtime-telemetry-sanitizers': 'pin-7197-congruence-fused-sanitizers',
    "    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',\n": '',
    "fixtures = ['test_runtime_telemetry', 'test_backbone']":
        "fixtures = ['test_congruence_fused_extract', 'test_congruence', 'test_congruence_empty_gates', 'test_congruence_index_cleanup']",
    'Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.':
        'Actual DebugASan/UBSan/LSan fused AND library;648 new fixed extraction cases and complete Congruence/empty-gate/index-cleanup fixtures twice each outside ptrace. No fuzz or real-instance execution.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

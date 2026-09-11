#!/usr/bin/env python3
"""Actual Debug-sanitized Factor arena and full focused Factor fixtures."""
from pathlib import Path
base = Path(__file__).with_name('check-runtime-telemetry-sanitizers-7176.py')
code = base.read_text()
changes = {
    'runtime-telemetry-ablation-7172': 'runtime-factor-arena-7180',
    '7176-runtime-telemetry-sanitizers': '7182-factor-arena-sanitizers',
    "    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',\n": '',
    "fixtures = ['test_runtime_telemetry', 'test_backbone']":
        "fixtures = ['test_factor_occurrence_arena', 'test_factor', 'test_factor_n1', 'test_factor_root_support']",
    'Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.':
        'Actual DebugASan/UBSan/LSan Factor arena library;34 storage layouts and complete Factor/N1/root-support fixtures twice each outside ptrace. No fuzz or real-instance execution.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

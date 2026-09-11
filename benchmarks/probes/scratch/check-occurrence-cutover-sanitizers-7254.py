#!/usr/bin/env python3
"""Exercise the actual cutover and public dense consumers under ASan/UBSan/LSan."""
from pathlib import Path
base = Path(__file__).with_name('check-runtime-telemetry-sanitizers-7176.py')
code = base.read_text()
for before, after in {
    'runtime-telemetry-ablation-7172': 'runtime-single-occurrence-cutover-7253',
    '20260910-7176-runtime-telemetry-sanitizers': '20260911-7254-occurrence-cutover-sanitizers',
    'pin-7176-runtime-telemetry-sanitizers': 'pin-7254-occurrence-cutover-sanitizers',
    "    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',\n": '',
    "fixtures = ['test_runtime_telemetry', 'test_backbone']":
        "fixtures = ['test_connected_occurrence', 'test_occurrence_cutover', 'test_replacement_cursor', 'test_kernel', 'test_bve', 'test_sweep', 'test_substitute', 'test_strengthen']",
    'Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.':
        'Actual cutover DebugASan/UBSan/LSan library with eight storage/kernel/rule fixtures twice each outside ptrace. No leak suppression, fuzz or real-instance execution; independent proof and runtime gates remain required.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

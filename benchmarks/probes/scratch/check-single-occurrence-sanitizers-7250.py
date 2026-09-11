#!/usr/bin/env python3
"""Sanitize the new owning stream and filtered ranges outside ptrace."""
from pathlib import Path
base = Path(__file__).with_name('check-runtime-telemetry-sanitizers-7176.py')
code = base.read_text()
for before, after in {
    'runtime-telemetry-ablation-7172': 'runtime-single-occurrence-7249',
    '20260910-7176-runtime-telemetry-sanitizers': '20260911-7250-single-occurrence-sanitizers',
    'pin-7176-runtime-telemetry-sanitizers': 'pin-7250-single-occurrence-sanitizers',
    "    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',\n": '',
    "fixtures = ['test_runtime_telemetry', 'test_backbone']":
        "fixtures = ['test_connected_occurrence']",
    'Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.':
        'New single-occurrence storage fixture runs twice with DebugASan/UBSan/LSan outside ptrace, including exhaustive mixed entries, filtered iterators, cached counts, stable mutations, growth/packing and reuse. The component is not yet connected to Kernel. No solver, proof or fuzz acceptance for a future cutover.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

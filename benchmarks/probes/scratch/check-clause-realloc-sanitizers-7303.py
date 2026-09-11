#!/usr/bin/env python3
"""Check actual clause-arena realloc consumers with unsuppressed sanitizers."""
from pathlib import Path
base = Path(__file__).with_name('check-runtime-telemetry-sanitizers-7176.py')
code = base.read_text()
for before, after in {
    'runtime-telemetry-ablation-7172': 'runtime-clause-realloc-7301',
    '20260910-7176-runtime-telemetry-sanitizers': '20260911-7303-clause-realloc-sanitizers',
    'pin-7176-runtime-telemetry-sanitizers': 'pin-7303-clause-realloc-sanitizers',
    "    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',\n": '',
    "fixtures = ['test_runtime_telemetry', 'test_backbone']":
        "fixtures = ['test_clause_arena_replay', 'test_realloc_words', 'test_collect_memory', 'test_occurrence_cutover', 'test_replacement_cursor', 'test_kernel', 'test_bve', 'test_sweep', 'test_substitute', 'test_strengthen']",
    'Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.':
        'Actual clause-arena realloc library: ten allocation/kernel/collection/rule fixtures twice each under DebugASan/UBSan/LSan outside ptrace, no suppression. Fixed tests only, not source fuzz or real-instance execution.'
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

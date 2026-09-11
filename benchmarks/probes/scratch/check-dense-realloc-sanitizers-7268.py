#!/usr/bin/env python3
"""Check the allocator and actual dense consumers with unsuppressed sanitizers."""
from pathlib import Path
base = Path(__file__).with_name('check-runtime-telemetry-sanitizers-7176.py')
code = base.read_text()
for before,after in {
    'runtime-telemetry-ablation-7172':'runtime-dense-realloc-7267',
    '20260910-7176-runtime-telemetry-sanitizers':'20260911-7268-dense-realloc-sanitizers',
    'pin-7176-runtime-telemetry-sanitizers':'pin-7268-dense-realloc-sanitizers',
    "    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',\n":'',
    "fixtures = ['test_runtime_telemetry', 'test_backbone']":
        "fixtures = ['test_realloc_words', 'test_flat_payload_replay', 'test_connected_occurrence', 'test_occurrence_cutover', 'test_replacement_cursor', 'test_kernel', 'test_bve', 'test_sweep', 'test_substitute', 'test_strengthen']",
    'Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.':
        'Actual realloc-backed dense library with ten allocator/storage/kernel/rule fixtures twice each under DebugASan/UBSan/LSan outside ptrace. No suppression, source fuzz or real-instance execution. Forced-moving and failed allocation plus capacity/state replay are tested; intentional fatal controls are separate non-sanitizer build evidence.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

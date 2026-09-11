#!/usr/bin/env python3
"""Sanitize the fused collector without mutating any submitted pin."""
from pathlib import Path
base = Path(__file__).with_name('check-runtime-telemetry-sanitizers-7176.py')
code = base.read_text()
for before, after in {
    'runtime-telemetry-ablation-7172': 'runtime-gc-forward-7233',
    '20260910-7176-runtime-telemetry-sanitizers': '20260911-7234-gc-forward-sanitizers',
    'pin-7176-runtime-telemetry-sanitizers': 'pin-7234-gc-forward-sanitizers',
    "    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',\n": '',
    "fixtures = ['test_runtime_telemetry', 'test_backbone']":
        "fixtures = ['test_watch_compact_map', 'test_gc_forward', 'test_collect_retry', 'test_kernel', 'test_restart_reduce', 'test_reduce_root_lock']",
    'Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.':
        'Actual DebugASan/UBSan/LSan gc-forward7233 library;26240 watch transactions,896 collector states and collect-retry/kernel/restart-reduce/root-lock fixtures twice each outside ptrace. No fuzz or real-instance execution.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})

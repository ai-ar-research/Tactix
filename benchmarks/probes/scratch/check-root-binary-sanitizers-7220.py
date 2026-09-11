#!/usr/bin/env python3
"""Sanitize the isolated root-true binary collector without changing its pin."""
from pathlib import Path
base = Path(__file__).with_name('check-runtime-telemetry-sanitizers-7176.py')
code = base.read_text()
for before, after in {
    'runtime-telemetry-ablation-7172': 'runtime-root-binary-7218',
    '20260910-7176-runtime-telemetry-sanitizers': '20260911-7220-root-binary-sanitizers',
    'pin-7176-runtime-telemetry-sanitizers': 'pin-7220-root-binary-sanitizers',
    "    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',\n": '',
    "fixtures = ['test_runtime_telemetry', 'test_backbone']":
        "fixtures = ['test_root_binary_collect', 'test_collect_retry', 'test_kernel', 'test_restart_reduce', 'test_reduce_root_lock']",
    'Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.':
        'Actual DebugASan/UBSan/LSan root-binary7218 library;5400-state old/new collector oracle and collect-retry/kernel/restart-reduce/root-lock fixtures twice each outside ptrace. No fuzz or real-instance execution.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})

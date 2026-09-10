#!/usr/bin/env python3
"""Inspect whether invariant outer-pair loads leave the ITE inner loop."""
from pathlib import Path
source = Path(__file__).with_name('compile-watch-dispatch-6894.py')
code = source.read_text()
for before,after in {
    '20260910-6893-watch-dispatch-build':'20260910-6969-ite-pair-hoist-build',
    'runtime-watch-dispatch-6893':'runtime-ite-pair-hoist-6969',
    '6894':'6970',
    "arm+'-kernel.o'":"arm+'-congruence.o'",
    'src/kernel/kernel.cpp':'src/rules/congruence/congruence.cpp',
    "('propagate_literal' in fields[-1] or 'kernel_propagate' in fields[-1])":
        "('extract_gates' in fields[-1] or 'extract_ite' in fields[-1])",
    'Same-layout source change restricted to full watch consumption and one shared blocker test; no budget, scheduling, reason or layout change intended. Actual old/new fixed watch-state/counter controls and full suites/proofs/B/C required before timing.':
        'Move immutable lst[i] pair reads outside the inner j loop. Local list storage never changes during pairing; encounter/verification/seen/gate order and all guards/ticks remain unchanged. No index/layout/sort redesign or composition with pending6950. Compile-only: exact actualold/new gate-vector, budget/stop, fullbuild, strongproof and B/C controls required before solvers. Existing tests alone are not claimed sufficient; add targeted repeated inner-loop/pair-order cases if compiled premise holds.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

#!/usr/bin/env python3
"""Compile actual old/new Factor objects before running fixtures or solvers."""
from pathlib import Path
source = Path(__file__).with_name('compile-watch-dispatch-6894.py')
code = source.read_text()
for before, after in {
    '20260910-6893-watch-dispatch-build':'20260910-6925-factor-count-build',
    'runtime-watch-dispatch-6893':'runtime-factor-count-6925',
    '6894':'6926',
    "arm+'-kernel.o'":"arm+'-factor.o'",
    'src/kernel/kernel.cpp':'src/rules/factor/factor.cpp',
    "('propagate_literal' in fields[-1] or 'kernel_propagate' in fields[-1])":
        "'factor_apply_seeded' in fields[-1]",
    'Same-layout source change restricted to full watch consumption and one shared blocker test; no budget, scheduling, reason or layout change intended. Actual old/new fixed watch-state/counter controls and full suites/proofs/B/C required before timing.':
        'Same-layout saved cofactor reference/value across separate touched-vector push; discovery publication still precedes increment. No predicate/order/budget/guard/proof/API change. No benchmark gain or correctness claimed from compilation. Actual old/new full-state controls, full independent suites, strict native fixed proofs and B/C are required before paired timing.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

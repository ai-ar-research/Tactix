#!/usr/bin/env python3
"""Actual Factor object controls, with hash-identical shared source/headers."""
from pathlib import Path
source = Path(__file__).with_name('control-watch-rebuild-6903.py')
code = source.read_text()
for before, after in {
    '20260910-6902-watch-rebuild-build':'20260910-6925-factor-count-build',
    'runtime-watch-rebuild-6902':'runtime-factor-count-6925',
    '6903':'6927',
    'src/kernel/kernel.cpp':'src/rules/factor/factor.cpp',
    '/kernel/kernel.cpp.o':'/factor/factor.cpp.o',
    "f'{arm}-kernel-6927.o'":"f'{arm}-factor-6927.o'",
    "('test_watch_rebuild','test_kernel','test_replacement_cursor')":
        "('test_factor_count','test_factor','test_rule_instances')",
    'tests/test_watch_rebuild.cpp':'tests/test_factor_count.cpp',
    '144 complete physical-image configurations per actual kernel arm, conservative order-bit normalization, empty/binary/mixed/garbage/duplicate lists, connected/drop/flush modes, both flush flags, root values and attached ordered dense views. Existing kernel and replacement-cursor suites agree. Shared objects are from6730 whose entire source/header tree except kernel.cpp is hash-identical. No ABI change. Not final independent proof/runtime/fuzz acceptance.':
        '270 actual Factor grid configurations cover binary/ternary/five-literal clauses,2-65 rows,3-19 columns,reverse encounter order,first/repeated counts,touched-vector growth,effort cuts and second applications. Semantic assertions and callback sequence plus watch/arena/mark/root/counter digests agree. Existing Factor and rule-instance suites agree. Shared6730 objects have hash-identical source/headers outside factor.cpp. No ABI change; not independent full build/proof/runtime/fuzz acceptance.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

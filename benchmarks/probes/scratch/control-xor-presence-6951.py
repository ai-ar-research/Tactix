#!/usr/bin/env python3
"""Actual old/new Congruence objects; no approximate extraction model."""
from pathlib import Path
source = Path(__file__).with_name('control-watch-rebuild-6903.py')
code = source.read_text()
for before, after in {
    '20260910-6902-watch-rebuild-build':'20260910-6950-xor-presence-build',
    'runtime-watch-rebuild-6902':'runtime-xor-presence-6950',
    '6903':'6951',
    'src/kernel/kernel.cpp':'src/rules/congruence/congruence.cpp',
    '/kernel/kernel.cpp.o':'/congruence/congruence.cpp.o',
    "f'{arm}-kernel-6951.o'":"f'{arm}-congruence-6951.o'",
    "('test_watch_rebuild','test_kernel','test_replacement_cursor')":
        "('test_xor_presence','test_congruence','test_congruence_index_cleanup')",
    'tests/test_watch_rebuild.cpp':'tests/test_xor_presence.cpp',
    '144 complete physical-image configurations per actual kernel arm, conservative order-bit normalization, empty/binary/mixed/garbage/duplicate lists, connected/drop/flush modes, both flush flags, root values and attached ordered dense views. Existing kernel and replacement-cursor suites agree. Shared objects are from6730 whose entire source/header tree except kernel.cpp is hash-identical. No ABI change. Not final independent proof/runtime/fuzz acceptance.':
        'Exhaustive256 ternary sign subsets across root-effective size3/4/5, duplicates, reverse encounters, repeated-variable prefixes and larger interfering records; all full/missing parity groups of sizes4/5. Complete ordered gate payload and charged-tick digests agree, pure extraction preserves arena/watch arena/trail. Existing Congruence and1200 index-cleanup cases agree. Shared6730 source/headers hash-identical outside congruence.cpp; no ABI change. Further unfinished-index, full-build, proof and B/C/runtime gates required; no fuzz or promotion.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

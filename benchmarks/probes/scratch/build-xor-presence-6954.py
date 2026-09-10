#!/usr/bin/env python3
"""Independent full pins for the behavior-preserving XOR membership experiment."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
subprocess.run(['python3',str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
assert (root/'benchmarks/runs/20260910-6950-xor-presence-build/control-6951.json').exists()
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for before, after in {
    '.claude/worktrees/runtime-substitute-retry-6231':'.claude/worktrees/runtime-xor-presence-6950',
    '.claude/worktrees/runtime-learned-sort-6169':'.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build':'20260910-6950-xor-presence-build',
    'build/pin-6231-substitute-retry-':'build/pin-6950-xor-presence-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n":'',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/rules/congruence/congruence.cpp', 'tests/test_xor_presence.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Reuse existing ternary_presence for all four parity-membership questions of distinct-variable size3 XOR bases; truth-table-derived0x69/0x96 masks. Preserve canonical record/seen/gate order, upfront combinations effort, incomplete-index rejection, size4/5 fallback, proof, budget and API. Independent6730 source; no new representation or strategy change.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        '3176 fixed extraction cases, exhaustive256 sign subsets with duplicates/root-effective3-5/reversed encounters/repeated-variable and larger interfering prefixes plus full/missing4-5 parity groups. Actualold/new gate/tick digest cb9ff4553209f52b; public extraction arena/watch/trail unchanged. Existing Congruence and1200cleanup cases agree. Additional actual unfinished-index controls, independent DRAT/cake_lpr proofs and B/C then matched47/42/38 runtime required. Source fuzz unavailable; no promotion.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

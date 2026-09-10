#!/usr/bin/env python3
"""Independent full pins after actual Factor controls agree."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
subprocess.run(['python3',str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
assert (root/'benchmarks/runs/20260910-6925-factor-count-build/control-6928.json').exists()
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for before, after in {
    '.claude/worktrees/runtime-substitute-retry-6231':'.claude/worktrees/runtime-factor-count-6925',
    '.claude/worktrees/runtime-learned-sort-6169':'.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build':'20260910-6925-factor-count-build',
    'build/pin-6231-substitute-retry-':'build/pin-6925-factor-count-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n":'',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":
        "{'src/rules/factor/factor.cpp', 'tests/test_factor_count.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'Save the cofactor reference and value across a separate touched-vector push, keeping first-discovery publication before increment. Remove redundant reloads without changing predicates, encounter/tie order, counter values, effort cutoff, guards, proof or API. Independent6730 candidate, no composition or layout/representation redesign.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        'New270 actual Factor matrix cases cover first/repeated candidate counts, touched-vector growth, binary/ternary/five-literal grids,2-65 rows,3-19 columns,reverse encounter order,effort cuts and second applications.6928 actual old/new Factor objects agree on callback sequence and watch/arena/mark/root/counter digest plus full Factor/kernel suites.6927 passed old270/Factor but optional nonexistent test path prevented completion; retained failure, corrected6928. Strict independent native proofs and B/C required before threewave32/39/38 exact-work time/RSS gate. No source fuzz or promotion.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

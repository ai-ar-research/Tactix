#!/usr/bin/env python3
"""Build independent pins after actual old/new reconnect controls agree."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
subprocess.run(['python3',str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
assert (root/'benchmarks/runs/20260910-6902-watch-rebuild-build/control-6904.json').exists()
source = Path(__file__).with_name('build-substitute-retry-6233.py')
code = source.read_text()
for before,after in {
    '.claude/worktrees/runtime-substitute-retry-6231':'.claude/worktrees/runtime-watch-rebuild-6902',
    '.claude/worktrees/runtime-learned-sort-6169':'.claude/worktrees/runtime-reduce-watermark-6730',
    '20260909-6231-substitute-retry-build':'20260910-6902-watch-rebuild-build',
    'build/pin-6231-substitute-retry-':'build/pin-6902-watch-rebuild-',
    "    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)\n":'',
    "{'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}":"{'src/kernel/kernel.cpp', 'tests/test_watch_rebuild.cpp'}",
    'Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.':
        'At large-watch reconnect, skip word compaction only for lists whose existing exact binary population proves no large entries. Normalize the conservative order bit exactly as compact did, including empty lists. Mixed lists retain old compaction. No layout/API/guard/proof/budget/scheduler change; independent6730, no composition.',
    'Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.':
        '6904 actual old/new kernel controls agree on144 complete physical-image configurations (watch words including dead storage, all head fields, holes/capacity, clause arena, roots, ticks and ordered dense views), digest6d4a6f3bd2f702ef. Existing kernel suite and840 replacement-cursor cases agree.6903 first fixture compile failed on misspelled constructor; no fixture or solver execution, logs preserved;6904 corrects API spelling. Full independent suites/fixed proofs/B28+C5/threewave32-39-38 same-work runtime/RSS gate required. Unfuzzed/unpromoted; no gain claimed.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

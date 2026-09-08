#!/usr/bin/env python3
"""Recheck all independent builds while retaining initial fixture failure logs."""
from pathlib import Path
source = Path(__file__).with_name('recheck-root-conflict-reuse-5723.py').read_text()
source = source.replace('20260908-5718-root-conflict-reuse', '20260908-5741-reuse-target')
source = source.replace('-root-conflict-reuse-{arm}', '-reuse-target-{arm}')
source = source.replace("('release',5718),('trace',5719),('bc',5720)",
                        "('release',5741),('trace',5742),('bc',5743)")
exec(compile(source, __file__, 'exec'))

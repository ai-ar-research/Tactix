#!/usr/bin/env python3
"""Build independent target-dispatch candidates and run complete fixed suites."""
from pathlib import Path
source = Path(__file__).with_name('build-sweep-small-5522.py').read_text()
source = source.replace('runtime-sweep-small-5522', 'runtime-reuse-target-5741')
source = source.replace('20260908-5522-sweep-small', '20260908-5741-reuse-target')
source = source.replace('-sweep-small-{arm}', '-reuse-target-{arm}')
source = source.replace("('release', 5522), ('trace', 5523), ('bc', 5524)",
    "('release', 5741), ('trace', 5742), ('bc', 5743)")
exec(compile(source, __file__, 'exec'))

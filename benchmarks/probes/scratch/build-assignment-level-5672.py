#!/usr/bin/env python3
"""Independent full Release/BC/trace fixed suites for the exact-level change."""
from pathlib import Path
source = Path(__file__).with_name('build-sweep-small-5522.py').read_text()
source = source.replace('runtime-sweep-small-5522', 'runtime-assignment-level-5672')
source = source.replace('20260908-5522-sweep-small', '20260908-5672-assignment-level')
source = source.replace('-sweep-small-{arm}', '-assignment-level-{arm}')
source = source.replace("('release', 5522), ('trace', 5523), ('bc', 5524)",
    "('release', 5672), ('trace', 5673), ('bc', 5674)")
exec(compile(source, __file__, 'exec'))

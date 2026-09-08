#!/usr/bin/env python3
"""Independent Release/trace/BC builds and complete fixed suites."""
from pathlib import Path
source = Path(__file__).with_name('build-sweep-small-5522.py').read_text()
source = source.replace('runtime-sweep-small-5522', 'runtime-propagation-cursor-5641')
source = source.replace('20260908-5522-sweep-small', '20260908-5641-propagation-cursor')
source = source.replace('-sweep-small-{arm}', '-propagation-cursor-{arm}')
source = source.replace("('release', 5522), ('trace', 5523), ('bc', 5524)",
                        "('release', 5641), ('trace', 5642), ('bc', 5643)")
exec(compile(source, __file__, 'exec'))

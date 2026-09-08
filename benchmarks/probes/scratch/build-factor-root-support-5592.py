#!/usr/bin/env python3
"""Build three independent immutable destinations and run their complete suites."""
from pathlib import Path
source = Path(__file__).with_name('build-sweep-small-5522.py').read_text()
source = source.replace('runtime-sweep-small-5522', 'runtime-factor-root-support-5592')
source = source.replace('20260908-5522-sweep-small', '20260908-5592-factor-root-support')
source = source.replace('-sweep-small-{arm}', '-factor-root-support-{arm}')
source = source.replace("('release', 5522), ('trace', 5523), ('bc', 5524)",
                        "('release', 5592), ('trace', 5593), ('bc', 5594)")
exec(compile(source, __file__, 'exec'))

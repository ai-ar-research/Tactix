#!/usr/bin/env python3
"""Three independent builds and full fixed suites for the isolated cleanup repair."""
from pathlib import Path
source = Path(__file__).with_name('build-sweep-small-5522.py').read_text()
source = source.replace('runtime-sweep-small-5522', 'runtime-congruence-index-5619')
source = source.replace('20260908-5522-sweep-small', '20260908-5619-congruence-index')
source = source.replace('-sweep-small-{arm}', '-congruence-index-{arm}')
source = source.replace("('release', 5522), ('trace', 5523), ('bc', 5524)",
                        "('release', 5619), ('trace', 5620), ('bc', 5621)")
exec(compile(source, __file__, 'exec'))

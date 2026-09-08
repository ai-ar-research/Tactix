#!/usr/bin/env python3
"""Build independent Release/BC and trace pins with all fixed tests."""
from pathlib import Path
source=Path(__file__).with_name('build-sweep-small-5522.py').read_text()
source=source.replace('runtime-sweep-small-5522','runtime-root-conflict-reuse-5718')
source=source.replace('20260908-5522-sweep-small','20260908-5718-root-conflict-reuse')
source=source.replace('-sweep-small-{arm}','-root-conflict-reuse-{arm}')
source=source.replace("('release', 5522), ('trace', 5523), ('bc', 5524)",
    "('release', 5718), ('trace', 5719), ('bc', 5720)")
exec(compile(source,__file__,'exec'))

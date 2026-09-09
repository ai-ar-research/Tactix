#!/usr/bin/env python3
"""Correct prototype ordering; preserve failed compile logs in logs/."""
from pathlib import Path
source = Path(__file__).with_name('build-subsumption-producer-6295.py')
code = source.read_text()
code = code.replace("c/'logs'", "c/'logs-6296'")
marker = "assert not (c/'manifest.json').exists()"
assert code.count(marker) == 1
code = code.replace(marker, marker + "\n(c/'logs-6296').mkdir()")
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})

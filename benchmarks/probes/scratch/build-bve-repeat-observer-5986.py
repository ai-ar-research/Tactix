#!/usr/bin/env python3
"""Build independent repeated-pop observers and run the full trace suite."""
from pathlib import Path
source = Path(__file__).with_name('build-bve-full-heap-5957.py')
code = source.read_text().replace('20260908-5956-bve-full-heap-controls', '20260908-5985-bve-repeat-heap-controls')
code = code.replace('runtime-bve-full-heap-5956', 'runtime-bve-repeat-heap-5985')
code = code.replace('pin-5956-bve-full-heap-trace', 'pin-5985-bve-repeat-heap-trace')
code = code.replace('kissat-bve-full-heap-5956', 'kissat-bve-repeat-heap-5985')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

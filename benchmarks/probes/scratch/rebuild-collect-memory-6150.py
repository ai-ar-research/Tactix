#!/usr/bin/env python3
"""Retry neutrality after preprocessor exclusion, retaining first build logs."""
from pathlib import Path

source = Path(__file__).with_name('build-collect-memory-6147.py')
code = source.read_text()
start = code.index("old = root / 'build/pin-6127-publication-trace'")
end = code.index('def build(number, kind, options):')
code = code[:start] + code[end:]
code = code.replace("(name + '.log')", "('retry-' + name + '.log')")
code = code.replace("pool.submit(build, 6146, 'release'", "pool.submit(build, 6150, 'release'")
code = code.replace("pool.submit(build, 6147, 'trace'", "pool.submit(build, 6151, 'trace'")
code = code.replace('build/pin-6146-collect-memory-release', 'build/pin-6150-collect-memory-release')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

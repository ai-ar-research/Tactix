#!/usr/bin/env python3
"""Seal completed6296 build; only the old fixture-log lookup was wrong."""
from pathlib import Path
source = Path(__file__).with_name('build-subsumption-producer-6295.py')
code = source.read_text()
start = code.index("run('configure',")
end = code.index('files = ', start)
code = code[:start] + "assert 'PASS: actual added-literal flips' in (c/'logs-6296/fixture.log').read_text()\n" + code[end:]
code = code.replace("c/'logs/fixture.log'", "c/'logs-6296/fixture.log'")
code = code.replace("(c/'native.patch').write_text(patch)", "assert (c/'native.patch').read_text() == patch")
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})

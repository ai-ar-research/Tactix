#!/usr/bin/env python3
"""Submit one paired long-prefix diagnostic, preserving every older job."""
from pathlib import Path
source = Path(__file__).with_name('submit-priority-prefixes-6141.py')
code = source.read_text()
code = code.replace('((6139, 21), (6140, 4))', '((6160, 4),)')
code = code.replace("f'benchmarks/runs/20260908-{number}-rank{rank}-current-prefix'",
                    "f'benchmarks/runs/20260909-{number}-rank{rank}-current-prefix'")
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

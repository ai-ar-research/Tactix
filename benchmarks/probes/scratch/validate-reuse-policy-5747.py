#!/usr/bin/env python3
"""Keep native-policy parity independent of the repaired public target surface."""
from pathlib import Path
source = Path(__file__).with_name('validate-root-conflict-reuse-5724.py').read_text()
source = source.replace('20260908-5718-root-conflict-reuse', '20260908-5741-reuse-target')
source = source.replace('runtime-root-conflict-reuse-5718', 'runtime-reuse-target-5741')
source = source.replace('pin-5718-root-conflict-reuse-release', 'pin-5741-reuse-target-release')
exec(compile(source, __file__, 'exec'))

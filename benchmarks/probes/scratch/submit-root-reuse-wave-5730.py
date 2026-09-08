#!/usr/bin/env python3
"""Advance changed-work timing after solver outcomes,not proof checking."""
from pathlib import Path
source=Path(__file__).with_name('submit-factor-support-wave-5603.py').read_text()
source=source.replace('20260908-5599-factor-support-rank32-runtime','20260908-5728-root-reuse-rank33-runtime')
source=source.replace('20260908-5598-factor-support-BC-gate','20260908-5727-root-reuse-BC-gate')
exec(compile(source,__file__,'exec'))

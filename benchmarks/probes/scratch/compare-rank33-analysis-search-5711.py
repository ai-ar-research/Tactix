#!/usr/bin/env python3
"""Apply the unchanged strict replay/window validator to corrected selectors."""
from pathlib import Path
source=Path(__file__).with_name('compare-rank33-analysis-window-5705.py').read_text()
source=source.replace('5704','5710').replace('rank33-analysis-window','rank33-analysis-search')
exec(compile(source,__file__,'exec'))

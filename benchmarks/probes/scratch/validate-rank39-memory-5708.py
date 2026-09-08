#!/usr/bin/env python3
"""Use the source-backed complete schema for the pending full ownership run."""
from pathlib import Path
import runpy
source=Path(__file__).with_name('validate-rank39-memory-5688.py').read_text()
anchor="if __name__=='__main__':main()"
assert source.count(anchor)==1
source=source.replace(anchor,"""support=runpy.run_path(str(Path(__file__).with_name('memory-row-parser-5706.py')))
parse,fixtures=support['parse'],support['fixtures']
if __name__=='__main__':main()""")
source=source.replace('parser_fixtures=11','parser_fixtures=24')
exec(compile(source,__file__,'exec'))

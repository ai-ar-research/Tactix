#!/usr/bin/env python3
"""Price all predeclared reconnect controls after the strict18-result gate."""
from pathlib import Path
source = Path(__file__).with_name('price-subsolver-waves-6887.py')
code = source.read_text()
for before,after in {
    '20260910-6864-subsolver-shrink-runtime':'20260910-6912-watch-rebuild-runtime',
    "',6866)":"',6914)",
    'timer-cost-6887.json':'timer-cost-6915.json',
    "('Sweep','BVE','Vivify')":"('Sweep','BVE','Vivify','Substitute')",
}.items():
    assert before in code,before
    code = code.replace(before,after)
# The inherited CLI uses shrink for this substituted branch; never permit the
# other experiment arm to write a misleading artifact through this helper.
import sys
assert len(sys.argv) == 1
sys.argv.append('shrink')
code = code.replace('experiment=sys.argv[1]','experiment="watch-rebuild"')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

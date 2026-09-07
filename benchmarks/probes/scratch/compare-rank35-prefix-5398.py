#!/usr/bin/env python3
"""Keep the same portable diagnostic comparison for the two-arm family witness."""
from pathlib import Path
source = Path(__file__).with_name('compare-rank27-prefix-5396.py').read_text()
source = source.replace('20260907-5396-rank27-prefix', '20260907-5398-rank35-prefix')
source = source.replace("'5396 pending'", "'5398 pending'")
source = source.replace("arms = ('previous', 'candidate', 'kissat')", "arms = ('candidate', 'kissat')")
source = source.replace("(('previous', 'kissat'), ('candidate', 'kissat'), ('previous', 'candidate'))", "(('candidate', 'kissat'),)")
source = source.replace('clean_stops=3', 'clean_stops=2')
source = source.replace("if arm == 'kissat' and r[2] not in names:",
                        "if int(r[4]) == 0 or (arm == 'kissat' and r[2] not in names) or (arm != 'kissat' and r[2] not in names.values()):")
exec(compile(source, __file__, 'exec'))

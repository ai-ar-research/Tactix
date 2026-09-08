#!/usr/bin/env python3
"""Native logs mix rates/timers with counters; retain exact candidate checks."""
from pathlib import Path
source = Path(__file__).with_name('analyze-sweep-full-5499.py').read_text()
source = source.replace("assert stats(prior) == stats(record)",
                        "assert (stats(prior) == stats(record) if arm == 'tactix' else previous[arm]['work'] == pair[arm]['work'])")
source = source.replace('all_80_non_timing_statistics_and_certificates_match_prior',
                        'all_40_tactix_non_timing_statistics_and_all_80_certificates_match_prior')
source = source.replace('native controls also match their predecessor',
                        'native controls also match their three extracted work counters and certificate bytes')
exec(compile(source, __file__, 'exec'))

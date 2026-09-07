#!/usr/bin/env python3
"""Retain the checked four-arm and B28+C5 protocol for dense publication."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[3]
source = (root / 'benchmarks/probes/scratch/prepare-vivify-retry-gates-5379.py').read_text()
mapping = dict([
    ('20260907-5371-congruence-prefix-runtime', '20260907-5379-vivify-retry-runtime'),
    ('20260907-5379-vivify-retry-runtime', '20260907-5388-dense-publication-runtime'),
    ('20260907-5380-vivify-retry-BC-gate', '20260907-5389-dense-publication-BC-gate'),
    ('20260907-5372-congruence-prefix-BC-gate', '20260907-5380-vivify-retry-BC-gate'),
    ('build/pin-5379-vivify-retry-release', 'build/pin-5388-dense-publication-release'),
    ('build/pin-5380-vivify-retry-bc', 'build/pin-5389-dense-publication-bc'),
    ('.claude/worktrees/runtime-vivify-retry-5378', '.claude/worktrees/runtime-dense-publication-5387'),
])
# Substitute once so a predecessor is never remapped into the new campaign.
source = re.sub('|'.join(map(re.escape, mapping)), lambda m: mapping[m[0]], source)
source = source.replace('Native learned-large Vivify retry advice closes the BVE frontier; measure full-solve effects against exact preceding candidate and production.',
                        'Suppress sparse watch publication while physically disconnected; measure runtime and peak RSS against the exact preceding candidate, production and Kissat.')
source = source.replace('5378 fixed native-positive/old-negative/new-positive, 76 serial trace CTests, all 2107 BVE attempts and 786753 decisions match through 345034.',
                        '5387 actual-native/old-negative/repaired-positive publication fixture and 76/76 trace CTests; fresh Release tests retained in 5387 logs.')
source = source.replace('Path-specific native retry advice preserves checked solves; quantify regressions.',
                        'Physical connection-mode publication must preserve checked solves; quantify time and memory regression.')
source = source.replace('isolated cumulative Vivify retry repair', 'isolated cumulative dense-publication repair')
source = source.replace('immutable 5378 source', 'immutable 5387 source')
exec(compile(source, __file__, 'exec'), {'__file__': __file__})

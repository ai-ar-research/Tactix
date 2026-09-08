#!/usr/bin/env python3
"""Extend the repaired causal witness to the unchanged full Rank1--40 view."""
from pathlib import Path
import re
import json

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
c = runs / '20260908-5604-factor-support-ranks1-40'
assert not c.exists(), 'Do not recreate or resubmit an existing full campaign'
trace = runs / '20260908-5597-factor-support-rank32-prefix/summary.json'
timing = runs / '20260908-5599-factor-support-rank32-runtime'
if not trace.exists() or not all((timing / 'provisional' / f'{i:03d}.json').exists() for i in (0, 1)):
    print('5604 pending causal validation or first expected timing pair'); raise SystemExit(0)
source = Path(__file__).with_name('prepare-sweep-small-full-5535.py').read_text()
changes = {
    '20260908-5485-sweep-cursor-ranks1-40': '20260908-5535-sweep-small-ranks1-40',
    '20260908-5522-sweep-small': '20260908-5592-factor-root-support',
    '20260908-5529-sweep-small-rank32-trace/repair-localization.json': '20260908-5597-factor-support-rank32-prefix/summary.json',
    '20260908-5531-sweep-small-rank32-runtime': '20260908-5599-factor-support-rank32-runtime',
    '20260908-5530-sweep-small-BC-gate': '20260908-5598-factor-support-BC-gate',
    '20260908-5535-sweep-small-ranks1-40': '20260908-5604-factor-support-ranks1-40',
    'build/pin-5522-sweep-small-release': 'build/pin-5592-factor-root-support-release',
    'build/pin-5524-sweep-small-bc': 'build/pin-5594-factor-root-support-bc',
}
for old in changes: assert old in source, old
source = re.sub('|'.join(re.escape(k) for k in sorted(changes, key=len, reverse=True)), lambda match:changes[match[0]], source)
start = source.index("comparison = v['decision_comparisons']['native']")
end = source.index("\nt = [json.loads", start)
source = source[:start] + "assert v['matching_rows'] > 18696140, 'Localize an earlier or unchanged divergence before extending this repaired witness'" + source[end:]
source = source.replace('Full unchanged Rank1-40 paired Kissat comparison of retained eligibility and real small-environment Sweep solves on5473.',
    'Full unchanged Rank1-40 paired Kissat comparison of the general Factor root-fixed support repair on5522.')
source = source.replace('General Sweep lifecycle repairs close the observed Rank32 decision divergence; measure net full-suite runtime, work and memory effects, including regressions, without multiplying cross-window ratios.',
    'General active-endpoint translation repairs Factor candidate support; measure the full-suite runtime/work/memory effect including regressions. Do not infer aggregate performance from the causal witness or multiply cross-window ratios.')
source = source.replace('Release86/86 twice with matching hashes; trace85/85; old one-clause counterexample,12 native/SubSolver lifecycle cases and budget/state/model tests. UNFUZZED.',
    'Release87/87 in two hash-matched builds;trace86/86;96 actual old/new support cases,64 actual native connection cases. UNFUZZED.')
source = source.replace('Frozen tested Release5522, observers disabled; same native Kissat, matrix, runner, checkers and resource contract as5485.',
    'Frozen tested Release5592, observers disabled; same native Kissat, matrix, runner, checkers and resource contract as5535.')
exec(compile(source, __file__, 'exec'))

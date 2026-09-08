#!/usr/bin/env python3
"""Successor causal/timing and unchanged B28+C5 contracts for real small solves."""
from pathlib import Path
import re
source = Path(__file__).with_name('prepare-sweep-retained-gates-5515.py').read_text()
changes = {
    '20260908-5510-sweep-retained': '20260908-5522-sweep-small',
    '20260908-5490-rank32-sweep': '20260908-5515-sweep-retained-rank32-trace',
    '20260908-5515-sweep-retained-rank32-trace': '20260908-5529-sweep-small-rank32-trace',
    'build/pin-5511-sweep-retained-trace': 'build/pin-5523-sweep-small-trace',
    'tactix-5490-': 'tactix-5515-', 'tactix-5515-': 'tactix-5529-',
    '.claude/worktrees/runtime-sweep-retained-5510': '.claude/worktrees/runtime-sweep-small-5522',
    '20260908-5476-sweep-cursor-BC-gate': '20260908-5516-sweep-retained-BC-gate',
    '20260908-5516-sweep-retained-BC-gate': '20260908-5530-sweep-small-BC-gate',
    'build/pin-5510-sweep-retained-release': 'build/pin-5522-sweep-small-release',
    'build/pin-5512-sweep-retained-bc': 'build/pin-5524-sweep-small-bc',
    '5476-sweep-cursor-BC': '5516-sweep-retained-BC',
    '5516-sweep-retained-BC': '5530-sweep-small-BC',
    '20260908-5475-sweep-cursor-rank35-runtime': '20260908-5517-sweep-retained-rank32-runtime',
    '20260908-5517-sweep-retained-rank32-runtime': '20260908-5531-sweep-small-rank32-runtime',
    'build/pin-5473-sweep-cursor-release': 'build/pin-5510-sweep-retained-release',
    '20260908-5473-sweep-cursor': '20260908-5510-sweep-retained',
    'Entry-only retained eligibility removes the known extra seed836; determine the earliest remaining decision and Sweep boundary difference against native after the general repair.':
        'Execute previously skipped empty/single-clause environments. Determine whether the33 missing initial solves, extra seven seeds and earliest remaining Rank32 decision divergence close against native.',
    '85/85 twice, trace84/84; actual old pure-seed counterexample; entry/cap/ledger/N1 regression fixtures.':
        '86/86 twice, trace85/85; actual old one-clause counterexample and12 direct native/SubSolver lifecycle cases; empty/small/budget/state tests.',
    'Unchanged cumulative B28+C5 gate for retained Sweep entry admission against production.':
        'Unchanged cumulative B28+C5 gate for complete small-environment Sweep solving against production.',
    'Preserve all expected verdicts and cumulative regression behavior after the general retained-seed scheduling repair.':
        'Preserve all expected verdicts and cumulative regression behavior after removing small-environment solve shortcuts.',
    'Independent Release5510/5512, identical SHA256,85/85 each; no event trace/conflict stop/journal/shadow.':
        'Independent Release5522/5524, identical SHA256,86/86 each; no event trace/conflict stop/journal/shadow.',
    'Restoring retained-entry eligibility reduces divergent Sweep work and improves Rank32 proof-on runtime against the exact cumulative predecessor5473.':
        'Restoring real small-environment solves repairs the enclosing Sweep effort trajectory and improves Rank32 proof-on runtime against exact predecessor5510.',
    'Release85/85 twice, trace84/84; pure retained old-binary counterexample and focused entry/ledger/N1 tests; UNFUZZED.':
        'Release86/86 twice, trace85/85; old one-clause counterexample,12 native engine lifecycle cases, tiny-env/model/budget tests; UNFUZZED.',
}
for old in changes:
    assert old in source, old
source = re.sub('|'.join(re.escape(key) for key in sorted(changes, key=len, reverse=True)),
                lambda match: changes[match[0]], source)
exec(compile(source, __file__, 'exec'))

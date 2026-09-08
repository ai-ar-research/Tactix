#!/usr/bin/env python3
"""Pin root-reuse causal replay,B28+C5 and three predeclared Rank33 pairs."""
from pathlib import Path
source=Path(__file__).with_name('prepare-congruence-gates-5627.py').read_text()
changes={
 '20260908-5626-congruence-index-validated':'20260908-5718-root-conflict-reuse',
 "builds = {arm:root/('build/pin-congruence-index-repair-5623-'+arm) for arm in ('release','bc','trace')}":"builds={arm:root/f'build/pin-{n}-root-conflict-reuse-{arm}' for arm,n in [('release',5718),('trace',5719),('bc',5720)]}",
 '20260908-5611-current-congruence-output':'20260908-5677-assignment-level-rank33-prefix',
 "sha(old/'clause-localization.json')":"sha(old/'summary.json')",
 "replace('tactix-5611-','tactix-5627-')":"replace('tactix-5677-','tactix-5726-')",
 '20260908-5627-congruence-index-rank33-prefix':'20260908-5726-root-reuse-rank33-prefix',
 '20260908-5598-factor-support-BC-gate':'20260908-5678-assignment-level-BC-gate',
 '20260908-5628-congruence-index-BC-gate':'20260908-5727-root-reuse-BC-gate',
 '20260908-5599-factor-support-rank32-runtime':'20260908-5679-assignment-level-rank39-runtime',
 '20260908-5629-congruence-index-rank33-runtime':'20260908-5728-root-reuse-rank33-runtime',
 'build/pin-5592-factor-root-support-release':'build/pin-5672-assignment-level-release',
 '20260908-5592-factor-root-support':'20260908-5672-assignment-level',
 '20260908-5604-factor-support-ranks1-40':'20260908-5664-propagation-cursor-ranks1-40',
 'Applying root-satisfied index cleanup should close the five retained Congruence9457 ternaries on current Rank33. Validate actual whole100K decisions and formula outputs; do not assume later182093 split closes.':'Faithful zero-jump conflict reuse should remove the extra inactive analyzed transaction at182092 and close the first score/decision divergence at182093. Compare native complete parent replay and current portable decision prefix through1M;do not assume later differences close.',
 'Two clean100K stops and complete hashes. Native whole5611 traces/proof replay; candidate may change. Full candidate/native decision comparison and exact framed formula/binary outputs. No acceptance timing or complete verdict.':'Two clean1M stops and immutable hashes. Native complete5677 traces/proof replay;candidate may change. Compare all recorded decision rows plus earlier common literal-prefix digest. No acceptance timing or complete verdict.',
 'Unchanged B28+C5 cumulative regression gate for general Congruence index cleanup.':'Unchanged B28+C5 cumulative regression gate for zero-jump conflict reuse.',
 'Retire only visited root-satisfied records with valid deletions and preserve all expected verdicts/cumulative gate behavior.':'General root-driving conflict reuse preserves soundness and legal API schedules while removing unnecessary learning andscore decay. Include all regressions.',
 'Independent repaired Release/BC88/88 each,matching binary SHA;original fixture failure explained and fixed,solver unchanged by review.':'Independent Release/BC91/91,trace90/90;24 exact native transitions and288 independently checked micro-proofs. Test-only canonical-target expectation corrected;solver source unchanged.',
 'Measure net Rank33 runtime effect of visited satisfied-clause index cleanup against actual5592 on the largest full-window loss. No tuned schedule or budget.':'Measure net Rank33 effect of root-conflict reuse against immediate5672. General reference repair,no tuned schedule/budget. Cross-arm search work may change;all three independent waves andfull40 remain required.'}
for old,new in changes.items():
    assert old in source,old
    source=source.replace(old,new)
exec(compile(source,__file__,'exec'))

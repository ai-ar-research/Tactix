# Rank899 trajectory attribution

The September `G/LG/LG2` result is explained by the now-retired MAB search
controller, not by a useful large-domain symmetry consequence. It is therefore
historical trajectory evidence, not a technique to restore. The remaining
general lead is the ordinary Boolean-search boundary against bundled Kissat
4.0.4 on the exact Satsuma residual.

## What the historical rows establish

The frozen September binaries are SHA256
`1f86ff9d6b199e4d0f01c1811f08abc6375b43fee6d9d30de0565881773f8b29`
and `2d7294ed3e5dfb47c30f88bba083510f3bd9c38edabbf5c085f95fcdd10765c3`.
Both retain the MAB/CHB controller, its configuration fields and its activation
counters. The `G` option sets only `TACTIX_EXP_GEN_COMPOUND=2`; `LG` adds the
large-domain lean schedule and `LG2` selects its second form.

On `mp1-Nb7T42.cnf.xz`, `A0/L` report one symmetry application and time out at
1,200 seconds. `G/LG/LG2` report zero applications and all finish SAT at
2,150,089 conflicts in 174.5/173.3/174.4 seconds. Because all three
singleton-declining arms have the same conflict count, the lean preprocessing
variants did not cause the recovery. The retained September source tree shows
the relevant general control: a successful structured-root application
suppresses delayed MAB admission, while a zero-application pass admits it after
the configured stagnation interval. That mutable tree was later reused, so it
cannot serve as an exact source pin for the old binaries; the binary symbols,
frozen command lines, counters and same-conflict outcomes bind the attribution
without treating that tree as immutable provenance.

The stronger confirmation is the later exact-input immediate-MAB run. Result
`benchmarks/runs/20260928-am521-immediate-mab-wv2-89/results/035.json`
(SHA256 `f580d0e8f0ea8de04dd54da7510a751e35dea14ba007da55a59319ea73f4f01c`)
is SAT in 66.579 seconds, 683,909 conflicts and 17,032,142 decisions. Its
independent checker verifies all 208,936 original clauses over 52,731 variables.
The solver reports one MAB activation, 167 stable restarts, 27 switches and
13/154 EVSIDS/CHB selections. This verifies that the historical search feature
can solve this input, but the immediate-MAB four-tranche campaign failed Rank,
B28+C5, WV100 and WV2--89 acceptance. The user retired MAB and accepted saved
no-MAB `d170b4a5`; it must not be rerun or reintroduced.

Saved no-MAB result
`benchmarks/runs/20260928-am549-no-mab-hybrid-wv2-89/results/035.json`
(SHA256 `92bd667dce0a0961e3ff5fe05a636ff7f9483124164261f3961c80315ec07a3b`)
is UNKNOWN at 600.005 seconds despite the same 28,741 startup eliminations and
822,766 resolutions. Current solver SHA `10942f...` is also UNKNOWN at 300
seconds with and without a proof destination; those equal-time arms differ only
in output destination. Final FirstOpen SHA `b26efc...` has not been timed on
Rank899, so this report makes no final-candidate runtime claim.

## Why the reference path remains useful

The independently checked Satsuma+Kissat result solves this original CNF in
107.102 seconds. Actual Satsuma adds only root unit `40310` and otherwise
preserves the input clause order. The user now requires every valid certified
symmetry-breaking unit to be admitted; the direct policy owner is implementing
that general rule. The September unit-applying arms time out while the
singleton-declining MAB arms solve, but that search history does not justify
withholding a valid unit. The remaining question is where generic preprocessing
or ordinary search first differs from Kissat on the identical residual, without
restoring MAB or adding a benchmark-specific recognizer.

SLURM job **1755320** was the first bounded comparison from final source archive
SHA256 `ab70cbb5...`, changing only the typed default for initial FastEliminate
from on to off to match bundled Kissat 4.0.4. Tactix retains `--auto-proof` and
`--default-graph-labeling=on`; integrated structured symmetry is off because
the input is the actual Satsuma residual. Instrumented Kissat uses
`--fastel=0`. Both arms run sequentially on one allocated CPU, with 60 seconds,
32 GiB, a 64 MiB per-file cap and a first-50-conflict trace limit. The frozen
campaign is `benchmarks/runs/20261007-rank899-fastel0-reference-trace/`.
That attempt is INCOMPLETE: both traces consumed the 200,000-row cap during
conflict 0, before Tactix emitted `preprocess_end`, and Kissat then reached the
64 MiB proof-file cap. It establishes no solver difference.

Successor **1755460** keeps the same source, residual, solver options, CPU,
memory and time limits, records the portable trajectory without implications,
and does not write proof payloads. Tactix still selects automatic-proof mode;
only the unused output destination is absent. Its frozen campaign is
`benchmarks/runs/20261007-rank899-fastel0-reference-trace-r2/`.

That successor completed on node015, CPU 23. Tactix stopped cleanly after
conflict 50 in 5.829 seconds; Kissat continued beyond the recorded window and
was terminated by the external 60-second diagnostic limit. Both complete,
untruncated first-50 windows contain exactly 12,723 decisions, 50
`analysis_begin` events and 50 learned clauses. Every compared payload,
conflict count, decision count and decision level is identical. The portable
comparison is SHA256
`a07f433365d6bdd573f7b16a8271336f59a98e57a77512dfab1d2b43f451efec`.

Trajectory-only filtering omitted Tactix's startup snapshots, so this successor
does not compare `preprocess_end` or implications. The generic semantic
comparator consequently reports a superficial first-row mismatch between a
Tactix decision and Kissat's `trace_begin`; that is a trace-shape artifact, not
a solver divergence. The custom portable comparison of the three selected
search streams is the authoritative result.

The exact 50-conflict match supplies no new search-policy patch and no runtime
claim. It narrows the next causal test to the user's required general change:
admit the independently proof-checked unit `40310`, then verify the integrated
clause multiset, presentation order, activation queue and actual startup
FastEliminate behavior against the frozen Satsuma residual. Only after the
focused proof gate passes should the final candidate and its unit-admission
successor receive a same-CPU bounded Rank899 comparison with full original-CNF
model checking for SAT. No result here licenses a benchmark rule, MAB, a new
withholding heuristic or an inference from one case to the default policy.

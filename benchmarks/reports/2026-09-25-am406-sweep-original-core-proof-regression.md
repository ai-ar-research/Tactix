# AM406 — current-source Sweep original-core-unit proof regression

The September 7 seed-563 witness is implemented in current source, but its
isolated proof-interface fixture had not been carried into the maintained
suite. AM406 adds a white-box regression for the existing saved-core adapter;
it changes no rule semantics, public registry action, scheduling choice, or
proof vocabulary. `sweep_capture_core_units_for_test` and
`sweep_emit_round_for_test` expose only the already-existing internal transaction
to the test binary; neither is declared in the public Sweep header.

The four-variable fixture poses original clause `(-1,2,3,4)` in the saved
clausal core after host root units `-2,-3,-4` have arrived. It requires a
single original-sourced captured unit `-1`, a one-step proof citing original
id 1, host assignment, and an independently checked refutation against the
exact five-clause checker CNF (which adds checker-only opposite unit `1`).
The negative control with only `-2,-3` leaves two non-false literals and must
capture nothing. This is a general original-core eligibility boundary, not a
seed-563/id-44 special case. The emitter writes DRAT, LRAT, and native DSR;
the maintained `sweep_proofs.sh` checks all three through the project's
independent pipelines.

The first diagnostic fixture (`1721167`) intentionally exposed an invalid
assumption: a clause already unit before environment construction is omitted
from that environment. It was replaced by the saved-core boundary above.
The second invocation (`1721168`) used zero brute-force oracle rounds and
therefore hit that oracle's existing `envs > 0` assertion; the new regression
had already passed. The complete current-source job `1721170` is terminal
`COMPLETED/0:0` in 19 seconds on hwulab, 2 CPUs and 8 GiB. The Sweep unit
suite passed (86,501 small environments, 5,344 checked claims); the proof
suite verified 48 derivations by drat-trim and lrat-check, including six
strict native DSR→LSR→Trestle cases. The new original-core case is among the
DRAT/LRAT/DSR cases. Exact command, binary hashes and all three job outputs
are retained in `benchmarks/probes/am406-sweep-core-proof/`.

This validates the adapter and proof for a posed saved core, not the full
embedded search trajectory that produced the 5317 witness. The latter has
isolated trace evidence in
`benchmarks/reports/2026-09-07-original-core-convergence.md`. This test
does not close cumulative B28+C5/Rank/WV acceptance, performance parity,
or N1--N4. The N4 ledger row remains open for acceptance, though the
current-source exact proof-regression criterion is now satisfied.

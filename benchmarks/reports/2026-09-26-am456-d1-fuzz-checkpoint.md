# AM456: current-source Opus fuzz and D1 proof handoff checkpoint

Date: 2026-09-26 local. This records current evidence, not N1--N4 acceptance.
The canonical native search proof contract remains no-option DSR → LSR →
pinned Trestle; no public X7 action or checker acceptance rule changed.

Opus owner `9578ed6e` is running a combined campaign on solver source
`66fed7e5` (main now `d86ecd81`, a status/request-only successor). Its
`OWNER_STATUS.md` reports fixed controls F1--F6 passed in job `1724946`,
including complete original-CNF CakeLPR verification of AM444 C1 API UNSAT
cases on every current build, parent-negative controls, and checked H1/H2
replays. Stock counted CLI legs were initially void because the owner's
submission omitted a required NAME argument; the owner canceled them and
resubmitted correctly as `1726463`--`1726475`. The latest queue still contains
running and pending legs. No terminal fuzz `REPORT.md` exists yet.

The owner handed off a deterministic new N2 failure class, D1. The bounded
artifact is `benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/handoffs/D1-dsr-probe-cce-unit-rejection/`:
29 variables, 27 clauses, public program `I 13 971874; A 3; V; Q` (Probe,
CCE, solve). Release, Debug, ASan and witness builds from both candidate and
parent output byte-identical UNSAT native DSR certificates, rejected by
no-option dsr-trim backward and forward; explicit LRAT and DRAT independently
verify on the same CNF/program. Thus D1 predates AM455 and is not a CaDiCaL/
Kissat verdict disagreement. The proof begins with a pending Probe unit
`-14`, then ten deferred support deletions; the first failing witnessed
addition is the auxiliary clause `30 -21` after root units. Diagnostic
`dsr-trim -f -V` shows checker root `21` from the prior `-10` unit and input
clause `21 10`, with `-30` also derived from the first auxiliary clause.
This indicates a root-closure/proof-order mismatch in the DSR-only general
encoding. It is a diagnostic inference, not a verified repair. Removing the
first SR transaction or delaying the intervening root units did not yield an
accepted proof; the original no-option checker route remains the gate.

The current-source full Debug job `1724325` finished all 187 tests in 4:26:51
and failed 3: `event_trace` missed `factor_mark_transition`,
`event_trace_zero_overhead` saw an unrecognized diagnostic symbol, and
`comment_hygiene` flagged the historical `AM444 C1` ticket in a new CMake
comment. The latter was changed to state the invariant and cite the AM455
report; `check-comment-hygiene.py` now reports zero hits. The two event-trace
failures still need causal classification and rerun. In the earlier AM452
full Debug `1723753`, only demo3 remained at timeout; this job confirms demo3
itself passes (15,926.71 seconds), but the full suite is not green.

The owner also reports parent-identical internal API UBSan at
`proof.cpp:625` (negating `INT32_MIN`) and Debug dense-binary assertions under
the `TACTIX_STOP_FUZZ=101` knob, with valid Release/ordinary outcomes. These
are separate bounded follow-ups, not an excuse to waive D1 or claim N2.
Next: repair D1 generally without narrowing X7 or changing the checker
contract, add an exact public-action regression, and rerun independent
DSR/Trestle, LRAT/DRAT and model oracles. Consume the terminal Opus report,
resolve remaining Debug failures, then complete N1--N4 and online-learning
readiness gates. Tier D has not been submitted.

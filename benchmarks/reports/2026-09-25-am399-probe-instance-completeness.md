# AM399: Probe public instance completeness

Date: 2026-09-25. Parent source `8cc7c7da`. This is an isolated N1 surface
repair checkpoint, not universal N1/N2 acceptance or a performance result.

## Causal gap

V1 Probe's abstract legal action is to test any unassigned literal by full
propagation. The explicit `failed_literal_try` path accepted one, but the
public `Probe` instance callback reused the canonical BIG-head filter from
`failed_literal_instances`. That filter is sound as a *schedule* only. At a
clean root, a root-false literal can leave a stored three-literal clause
physically large while making it act as a binary reason. The fixed formula
`(-1 ∨ 2 ∨ 3), (-1 ∨ 2 ∨ -3), (-2)` has no binary watch on `-1`, yet assuming
`1` propagates `3` and `-3`, proving `-1`. The candidate's test-only fixture
run against the pre-repair callback aborted at `assert(offered)` (exit 134);
the direct explicit application was already valid. This is a general
public-enumeration omission, not a missing proof inference.

`Probe` now has its own caller-bounded instance callback over both phases of
every unassigned variable. The canonical Probe round and its BIG-head
candidate schedule are unchanged; V2 Backbone keeps the binary-only callback,
and X6's separate semantic surface is unchanged. A nonfailed explicit probe
still returns NoOp. The calculus, implementation guard ledger, and rule header
now distinguish the public action surface from canonical scheduling.

## Bounded verification

The new internal fixture checks that the canonical list still omits `1`, the
public list contains exactly `1,-1,3,-3` in stable order and truncates at the
caller cap, explicit `1` derives `-1`, and the formula solves SAT. A public C
API fixture independently checks enumeration, application and a reconstructed
model satisfying all three original clauses. Debug and Release `probe` plus
`api_n1` pass 2/2 each; ASan+UBSan with leak detection passes the same 2/2.
Current Debug `probe`, `backbone`, `hyperbinary`, `api_n1`, `api_instances`
pass 5/5. The current Debug `micro_proofs` suite passes 1/1 against frozen
AM369 DSR/DRAT/LRAT checker pins, including native DSR elaboration and
Trestle. Its first six-test invocation reported three `Not Run` entries
because their executables had not yet been built; those exact three and the
two previously passing focused tests were then built and passed 5/5. This
was a test-build setup error, not a solver failure.

The Debug `test_probe` binary SHA-256 is
`f7f08f3a17a2081618b346cc09f35c69865609c77905a1baa707928165a121f9`;
Debug `test_api_c` is
`33e79984cc8b158345131bf7d057edf726df4074b497b88a6be737eced46d43e`.
Documentation size/sync, comment hygiene, rule-policy boundary and diff checks
have zero errors; six pre-existing document-capacity warnings remain.

These fixed tests do not prove all N1 states or arbitrary proof schedules.
Independent affected-scope Opus fuzz, a fresh full Debug suite and combined
N1/N2 gate are required. H2's historical no-option native DSR rejection and
the post-finalization API wrong-SAT contract remain open; N3/N4 and the later
Tier-D comparison are not promoted.

# Propagation cursor: exact-state implementation and pending runtime gates

The Rank1–40 objective remains at most1.01x Kissat, with B28+C5 regression
gates and general implementation repairs. The latest complete solver view is
still5592/full5604 at1.13758865x;78/80 checks were complete at this checkpoint.
No replicated gain is established yet for propagation implementation5649.

## General unit-cost change

Source `.claude/worktrees/runtime-propagation-cursor-repair-5647` descends from
validated Congruence5626. `kernel_propagate` now selects the ignored-clause
template once per call and keeps a local numeric trail cursor. It publishes
the cursor on fixpoint and conflict exits. Event-trace, rule-journal and
conflict-stop builds retain per-scan publication for observer visibility.
Binary-only debt settlement, watch order, reasons, ticks, phase saving, public
guards/actions and proofs are unchanged. This is not a schedule or budget edit.

Motivation is the checked Rank39 profile5544: main propagation is a dominant
self-cost. Its earlier candidate still has current5592's full work/proof
identity, but not a same-window unit-cost attribution. Both native propagation
sources were read. Kissat `propsearch.c` selects search propagation at compile
time and publishes its local cursor after the scan; CaDiCaL keeps a field
cursor. Tactix's actual old machine code repeated ignore dispatch and cursor
publication within the trail loop. In the new Release, ignore dispatch is at
0x41370e before either loop; ordinary scan increments local ebp at0x413835.
Whole old/new disassemblies are retained in5649/logs. Removing those operations
is verified; a speedup is not yet measured.

The scan and assignment callees neither consume nor rewind the cursor; clause
addition, which could rearm it, does not occur inside this scan. ReservedTrail
grows its used range within preallocated backing storage. The calculus M1 is
unchanged; the implementation notes record the local/diagnostic publication
distinction. No new kernel primitive or global contract is introduced.

## Tests, failure and bounded repair

The new fixture covers224 combinations: empty scans, binary chains, large units,
large conflicts, binary conflicts, relocation and garbage watches; root and
nonroot assignments, ignored clauses, binary-only debt, search/inprocessing,
and32/257 chain lengths. It checks conflict/cursor/accounting/values and hashes
trail reasons, watch/arena state and counters. Existing proof/model/API suites
cover the unchanged semantics; canonical dispatch coverage was added too.

The first build used nonexistent `ReservedTrail::shrink_to_fit`; that ordinary
fixture API error was corrected, preserving original logs. The subsequent new
test segfaulted in all three builds, so development and candidate submission
paused under§8.2. Claude5647's terminal outcome is PASS: the fixture called the
raw nullable `RuleVTable::instances` slot for canonical-only Propagate. GDB
proved the null call; production callers guard it and the public API supplies
the canonical fallback. Solver code was unchanged by repair.

The separate repaired fixture checks the documented null-slot contract, ties
the guard to the settled cursor, and exercises actual canonical dispatch via
`apply_rule`, including root refutation. Both independent Release suites pass
89/89 and trace88/88. The224-case state digest is `f82475e400a7f62b` for both
actual predecessor-linked and candidate fixtures. Main production CTest passes
77/77 in5.96s. Original5641 source, binaries and all failing logs are preserved;
5647 owns the exact review/diff. Neither reviewer nor parent deleted anything.

Authority5649 binds source, tests, patches, review, logs and assembly:

| Pin | SHA256 |
|---|---|
|Release and independent BC|`032ef0ba2d3cac6f3579b2e158de36488a1110e2b586e81d5ca7c62f12cdbbb8`|
|Trace|`b42f0e54533c5032db5b318b4baab9e508c513fff92d454f692acbfc89f19f4b`|

5650/job1672790 passes full per-arm replay through1M: every observed decision,
prefix, macro/portable boundary, formula/binary snapshot and partial-proof byte
is identical to5634. The pre-existing native decision split182093 is unchanged.
This validates behavior preservation on a long diagnostic, not competitiveness.

## Sweep evidence and observer limits

5639/job1672751 validates exact full per-arm5634 replay. The first Sweep input
at120330 has equal binary multisets and equal large-clause multisets. At exit,
native alone has `(-143631,143590)` and `(-143590,143631)`: a unique equivalence,
not duplicate copies. The next Sweep input/output binary multisets agree again.
The cause and effect on the later182093 split are not established.

Detailed5644/job1672779 reached its1200s diagnostic timeout on both arms (124),
with partial certificates and scratch cleanup recorded. It did not reach the
requested1M stop and is not a full-replay or timing authority. Successor5663
validates every captured decision-stream byte against the parent:54,714,182
complete candidate lines and47,407,718 native lines (including headers), plus
the native10-byte final partial row matching the exact next parent row's prefix.
Only source-backed observer macro/native analysis-end rows are separate; all
ordinary macro/boundary fields and selected formula/binary snapshots match.
Strict framing failures5658–5661 remain preserved;5663 passes fourteen window
and six partial-row fixtures. A timeout is not a solver soundness failure.

The selected first call has2574 matching seeds, environment sizes, effort deltas
and solve counts. All170 unit literals have the same order, including both
`-143590` and `-143631`: the extra native equivalence is root-satisfied. The
second call has no seeds or units. Native equivalence events require the separate
commit selector, which was unset, so zero native events is not zero equivalences.
There is no evidence here of missing Sweep search work or a budget/schedule gap.
A satisfied-binary lifecycle difference is plausible; its exact cause and effect
on the later split remain unproved. Do not chase physical identity without an
effect on useful work or cost.

## Runtime and asynchronous correctness

5651/job1672791 is the unchanged full-width B28+C5 gate with independent matching
builds;64/66 records are checked. Rank39 timing5652 against5626 has two exact-work
pairs:421.395/423.540s and419.674/420.777s, slight improvements but not yet a
replicated result. Final pair job1672918 runs. Helper5653 advances after solver
outcomes, not certificates. Scorer5655 requires cross-arm and within-arm complete
non-timing statistics/proof identity, all ratios, median and2% spread.
Full5664/job1672864 runs all40 pairs against Kissat; snapshot5665 freezes only
all80 solver records, and5666 closes only all80 checked/clean records.
Earlier Congruence timing5629 has three pairs, median0.99579758x predecessor,
range0.995774–1.001058;spreads0.634%/0.601%. This is small and mixed, not a
breakthrough. All six checks remain pending;5628 has64/66 checked.
Parent5599 now has all6 timing proofs checked, median0.824875x5522.

Claude5610 returned terminal PASS on5592:50000 cases,45752 checked verdicts,
27038 original-CNF SAT models and18714 UNSAT proofs,zero failures;300 corpus
replays and144 direct reachability cases. Parent observed only the owner response,
not fuzz artifacts. That pass does not cover5626 or5649. New Claude5657 owns one
changed-source background checkpoint under live session33894, covering both
recent changes; parent must poll only the terminal owner response.

Next: score5652's remaining timing pair and the complete5664 Rank1–40 view,
inspect all B28+C5/checker outcomes, then choose the next general fix from the
remaining loss/work-cost decomposition. Do not promote isolated/unfuzzed
sources. Audit5662 owns the final explicit non-fuzz states and report hashes.
Docs remain within required limits; no push or production promotion occurred.

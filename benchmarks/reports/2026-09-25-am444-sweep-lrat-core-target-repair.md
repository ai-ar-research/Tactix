# AM444: Sweep core units replay LRAT hints under their own target

Date: 2026-09-25. This is a focused N2 repair checkpoint, not combined
acceptance. The canonical proof routes and public Sweep action are unchanged.

Claude/Opus AM442 found a plain-solve UNSAT whose native LRAT fails CakeLPR.
Its bounded `min.cnf` has 17 variables and 28 clauses. In job `1723622`, an
immutable Debug binary from AM431 reproduced the handoff's LRAT byte for byte.
The rule journal records Sweep `general-core` unit consequences; the first bad
addition is line 47, clause 60. Its old chain was only `11` and CakeLPR
reported `clause index has no reduction sequence: 19`, with no verified status.
The owner attributes 1,727 rejected UNSAT LRATs to this class on its old pin;
that population is not transferred to this repair.

Sweep had normalized the embedded derivation's raw LRAT hints in place under
the direction's final target. It then shared a prefix with a different core
unit target. A hint redundant under the first negated target cube can be
necessary under the second. The repair keeps shared raw steps immutable and
stores a separate normalized chain for each proof target. Before admitting a
core unit, it replays that prefix against the unit's own target cube and
declines the derived unit if this proof cannot close. This changes proof
admission, not the public Sweep instance surface, semantic effect, schedule or
model reconstruction. No calculus precondition or checker contract changed.

Job `1723631` used the pinned patched Debug binary (`775e98da...`) and the
exact handoff CNF. It returned UNSAT and pinned CakeLPR printed
`s VERIFIED UNSAT`. Clause 60 now has hints
`11 52 26 17 56 2 13`; the old exact proof still rejects under the same
checker, even though that checker returns process status zero on rejection.
The added micro regression asserts that raw prefix hints remain intact and
that direction and unit targets obtain different normalized chains. The
complete `sweep` CTest passes, and `sweep_proofs` passes through pinned
DRAT/LRAT and no-option DSR/Trestle routes. A test-only original-core fixture
now supplies the environment it claims to replay; without it, the first
patched `sweep_proofs` run crashed before proof checking.

Provenance and exact commands are in `benchmarks/probes/am444-h1-journal/`.
Job `1723643` replayed the original 129-clause H1 artifact on the same pinned
binary; its LRAT also returns `s VERIFIED UNSAT` under CakeLPR. Opus affected
fuzz, full Debug and performance measurements remain open. Target-specific
normalization may cost more memory when one core yields many different units;
the affected fuzz and benchmark gates must measure that path before promotion.

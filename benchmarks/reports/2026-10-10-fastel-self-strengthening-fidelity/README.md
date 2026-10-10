# FastEliminate self-strengthening: fidelity against Kissat-SC2024

**Question.** Commit `efdced45a` ported SC2024 `fastel.c` self-strengthening into the FastEliminate
feasibility scan. The bisection (`benchmarks/probes/scratch/search-divergence-bisect`) shows that this
commit moves the search early on 12 Tier D SAT rows that the no-MAB baseline solved. Does the port match
Kissat-SC2024 on those rows, and if it does not, where does it differ?

**Answer.** The port did not match. Six concrete differences were found:

- (a) The strengthened clause is placed differently.
- (a') A mixed strengthened clause gets a different literal order.
- (c) The `flags.eliminate` lifecycle differs, and with it rounds 2–4.
- (d) A per-round admission gate cancelled rounds 2–4.
- (e) The candidate gather walks variables in a different order.
- (b) The suspected scoring quirk turned out not to be a difference.

With all fixes applied, Tactix on `par32-4` (row 162) makes the same first 300 decisions as Kissat (it
diverged at decision 69 before). Its fastel event counts match Kissat almost exactly.

The premise that Kissat solves the most striking row is wrong. Kissat-SC2024 **times out** on
`connm-ue-csp-sat-n1200-d-0.02-s405595518` (am540 `results/01116.json`, 5,000 s). The 20 s Kissat result
belongs to a different instance, `connm-...-n600-d0.04` (UNSAT, `results/03848.json`). Of the 12 rows
`efdced45a` perturbs, Kissat-SC2024 solves 8: 162, 271, 273, 469, 487, 723, 772 and 980. It times out on
199, 279, 440 and 1002.

## Instrumented reference

`tools/solvers/variants/kissat-fastel-strengthen-trace/` contains `fastel-trace.patch` and `build.sh`. The
build applies the patch to a copy of `winners/kissat-sc2024` (commit 1e56df6) and uses the pin's
`--competition` configuration. Setting `KISSAT_FT=1` logs the following:

- each round's radix-ordered candidates;
- each attempt (`try`, `elim`, `keep`);
- each self-strengthening (`str bl` / `str ll`, with the old clause, the new clause and the arena end);
- each fast forward subsumption;
- the formula size at search start;
- the first 300 decisions.

**Verdict check.** The pristine pin (sha `8a1a5b7a…`), the instrumented build, and the instrumented build
with `KISSAT_FT=1` all return UNSAT on connm-n600 and produce byte-identical DRAT (sha `a6d23c82…`).

The Tactix side uses a trace-only patch (`benchmarks/probes/scratch/fastel-fidelity/ft-diag.patch`, never
committed) that prints the same events, together with `TACTIX_DECISION_STREAM_TRACE`. Every run was a
60 s SLURM probe on `cpu-dense-hwulab-q`.

## Comparison

Cells give the per-round candidate counts, then strengthenings, flush subsumptions, attempts and
eliminations.

| Row | Kissat-SC2024 | Tactix main (`aca63751b`) | Tactix + fix |
|---|---|---|---|
| 162 par32-4 | 1214/176; str 26, sub 170, try 1387, elim 417 | 1214/**0**; 26, 177, 1211, 418 | 1214/176; 25, 180, 1387, 417 |
| 273 bp4_TCO_CSO_ZR | 45816/3713; 293, 445, 25841, 11097 | 45785/**0**; 283, 448, 23987, 11115 | 45785/3734/2; 284, 450, 25832, 11116 |
| 279 connm n1200 | 1665/462/23; 49, 2, 2150, 219 | 1738/**0**; 36, 8, 1738, 231 | 1738/492/39; 51, 12, 2269, 235 |

First decision divergence from Kissat:

| Row | Before the fix | After the fix |
|---|---|---|
| 162 | decision 69 | none in 300 decisions |
| 273 | decision 78 (conflict 16) | unchanged |
| 279 | decision 8 | unchanged |

On rows 273 and 279 the formula already differs before fastel starts: Kissat has 46,329 / 1,665 active
variables, Tactix 46,299 / 1,738. That difference comes from the initial probe stage (`probe_initially`),
so it lies outside fastel.

## Differences and fixes

**(a) Position of the strengthened clause.** Kissat's `can_fast_resolve_{binary_large,large_large}` calls
`kissat_new_irredundant_clause`. That allocates a NEW clause at the end of the arena and, in dense mode,
connects it at the end of every literal's occurrence list. The antecedent is then marked garbage. Tactix
called `kernel_replace_clause`, which `kernel_inherit_clause_position`s the antecedent's logical slot. This
models an in-place shrink, and both sparse resume and the watch rebuild then follow that slot.

*Fix:* the strengthening path in `bve.cpp` now adds a fresh clause with
`kernel_add_binary_clause`/`kernel_add_large_clause` (proof line first, as before) and then calls
`kernel_delete_clause` on each subsumed antecedent. The deletion is a semantic removal, so every literal of
the antecedent is re-armed, as `kissat_mark_removed_literals` does. `kernel_replace_clause` stays in use for
genuine in-place shrinks.

**(a') Literal order of a mixed strengthened clause.** Kissat copies the large antecedent's own order
without the pivot. Tactix built the binary-first image whenever the binary was the outer antecedent: on 162
it produced `[-2035 2137]` where Kissat produced `[2137 -2035]`. This changes the watched pair.

*Fix:* in the mixed case the image is rebuilt from the large antecedent.

**(b) Candidate score (`not_lit = LIT(pivot)`, score = 2·pos).** This was already reproduced deliberately.
Ties are broken stably by import position, which matches the radix sort over `all_variables`. Not a
difference.

**(c) The `flags.eliminate` lifecycle.** Kissat runs it like this:

- A round gathers every active variable whose `eliminate` flag is set.
- After each attempt the pivot's flag is cleared, even when it fails the occurrence caps at pop time.
- Every garbage clause or deleted binary sets the flag again on all of its literals.
- At the end of fastel, every flag is set.

Tactix kept `initial_fast_elimination_attempted_`, a flag set once per variable for the whole pass. It also
never restored `bve_retry` at the end, so variables attempted in fastel and kept stayed out of the first
search-time BVE schedule.

*Fix:* candidates are read from `bve_retry`, the kernel's image of `eliminate`. Clearing one variable goes
through a new public support action, `TACTIX_SUPPORT_RETIRE_BVE_CANDIDATE` (code 26, executed by
`RetireBveCandidateRequest`, wired into the twin port and the action map). Fastel ends with
`RescheduleBveCandidates`.

**(d) Admission in every round.** The strict-majority admission gate ran in every round. Re-armed rounds
are a minority by construction, so the gate declined every later round. Tactix gathered 0 candidates in
round 2 on all three rows, where Kissat gathered 176 / 3,713 / 462.

*Fix:* the gate now decides only round 1, that is, whether the owner pass runs at all.

**(e) Gather traversal order.** Kissat gathers over `all_variables`, which is import order. Tactix walked
the instances in variable order. The gather flush has side effects (it removes satisfied and subsumed
occurrences), so the walk order decided which clause of a subsuming pair died. On 162 this changed the
scores of 1342, 2006, 2137, 2320 and 2550, and those score changes caused the decision-69 divergence.

*Fix:* the gather now walks candidates by `compact_import_position`.

**Residual on 162.** Kissat makes 26 strengthenings and 170 flush subsumptions; Tactix makes 25 and 180.
For example, Tactix's flush subsumes `[-1974 120 -3066]` where Kissat strengthens it. The likely cause is
the occurrence-size measure from which `fast_forward_subsumed` picks the literals it marks.

## Patch and validation

The combined patch is `benchmarks/probes/scratch/fastel-fidelity/combined2.patch`, against base `aca63751b`.
It also removes DefaultPolicy's graph-labeling hint installation, by user decision. Only rows where graph
labeling authenticates and publishes a useful hint can be affected: the five Tier D install rows
abw-K-dwt__234, rook-56-1-1, rook-42-0-1, rook-52-0-1 and at-least-two. abw-K-dwt is solved by graph
labeling before search.

Validation of `build/pin-fastelfid-fix2-rel` (same `src` as the final patch):

- **Proofs and models.** All 24 quick Tier D rows verify. 17 are UNSAT, through both the DSR route and the
  witness-free DRAT route, and include connm-n600. 7 are SAT, and their models check. Fastel ran 2–3
  rounds on most of these rows.
- **Focused tests.** An asserts-on build of the focused ctest set passes everything except
  `option_coverage` and `comment_hygiene`. Both fail identically on the base: the reference clones are
  missing from the archive, and `test_online_inprocess.cpp:2340` has a pre-existing dead pointer.
- **Full ctest.** The first full run exposed two expectations that encoded the old semantics:
  - `test_scheduling`'s owner-order fixture: x is re-armed and is now correctly eliminated in a later
    owner round, so the fixture now runs one round.
  - `test_default_action_map` still listed `local-search-guidance` as a required record.

  Both expectations are updated in `combined2.patch`. With them, the full ctest on the corrected tree
  passes 348 of 361 tests. The 13 failures are exactly the failure set of the unpatched base. Among the
  passing tests are `scheduling`, `default_action_map` (internal 0, declared 2),
  `default_public_correspondence` and `default_public_replay`.

Still open: the 5,000 s A/B on the 12 `efdced45a` rows plus the 13 rows gained since no-MAB, full Debug,
fuzz, and the §8.4 gates. All of these run on the committed version.

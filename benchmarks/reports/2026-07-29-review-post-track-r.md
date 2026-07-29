# Codebase review — after Track R R3/R4/R5(S-a,S-b) and the all-rules-ON flip

**Date:** 2026-07-29 · scope: `src/`, `include/`, `tests/`, `tools/`, docs.
Trigger: the revisit-codebase cadence ("after a rule family or subsystem lands"),
run against a session that added two rule families, a sub-solver, and changed
the shipped default composition.

Method note: reviewed by focused passes rather than a fan-out, so the axes are
covered but not independently. A phase-gate run should fan out.

## Applied (behaviour-preserving, `simplify:` commit)

| # | Axis | Site | Finding |
|---|---|---|---|
| 1 | drift | `docs/implementation-notes.md` §1 rows X1/X2/X4; `src/rules/instantiate/instantiate.hpp:32` | **Four sites documented the opposite of what ships.** All said the Track R rules are "not scheduled by DefaultPolicy" / "ships OFF until its own A/B says otherwise"; they have been ON by default since `3fb85bd`. This is the highest-severity finding of the pass — the guard table is the document a reader trusts for what the shipped solver does. |
| 2 | dead-end | `src/kernel/kernel.hpp` `Stats::learned_raw_lits` | Declared, **never written and never read** — a leftover of the clause-quality probe, which L14 closed. Removed. |
| 3 | §5.6 coverage | `include/tactix.h`, `src/api/api.cpp`, `tests/test_api_c.c` | **The three rules that just joined the default schedule had no reward ids.** Every other inprocessing rule exposes its output (ids 13, 17–25); BCE / HyperTernaryRes / Instantiate exposed nothing, so a phase-6 learner could not read the yield of the rules it most needs to dose. Added ids 26–31 as **(yield, effort) pairs**, because the 2026-07-28 campaign showed cost-per-unit-output — not yield alone — is the axis that orders these rules' outcomes (BCE: 13 ticks/deletion where it buys a 50× speedup, 7,700–43,000 where it costs us). Test extended to 1..31. |

## Kept, with reason (recorded at the site)

- `SubSolver::conflicts()` — no consumer yet **by design**: S-f reports it beside
  `ticks()` when Sweep gets an effort spec, as every other rule reports a
  (yield, effort) pair. Comment added so the next reader does not "clean" it.
- `TACTIX_SLOT_PERMILLE` — no default consumer; it is a §5.6 dosing arm in the
  shape of `TACTIX_WALK_PERMILLE`, and the learner's action space needs it.
- The rule-introspection surface (`instances`/`apply_instance` on rules
  DefaultPolicy never poses) — N1 by construction, never a simplification target.

## Routed / open findings

| # | Axis | Severity | Finding | Owner |
|---|---|---|---|---|
| R1 | coverage | **med** | `SubSolver` (S-a/S-b, ~400 lines) is exhaustively unit-tested (1 M brute-force iterations, 207 k derivations replayed) but **never fuzzed**, because nothing in the solver reaches it yet. The moment S-c wires it in it becomes reachable, and its derivations become proof obligations on the MAIN formula. **S-c's definition of done must include fuzz legs on both proof formats**, not just unit tests. | rule-dev / fuzz |
| R2 | coverage | low | S7 `Reorder` landed without a dedicated stress policy, against the stress-policy skill's "use whenever a new rule/feature lands". `--random-policy` does apply it (10 k/0 on both formats), and it is `H`-only so the failure mode is H corruption, which the model-check oracle catches. Acceptable, but a `reorder_stress_policy` would exercise mid-trail application densely. | stress-policy |
| R3 | verbosity | low | `benchmarks/probes/` now holds **10 scripts**, several with closed questions (`t105-lrat-*` ×3 — the L13 defect is fixed and regression-guarded by asserts; `bce-wrongsat-bisect` — closed). They are cited as evidence by frozen reports, so deleting them is not obviously safe. Decide a retention rule (keep the `.out`, drop the `.sh`? keep both?) rather than accreting. | revisit-codebase (next pass) |
| R4 | duplication | low | The `TACTIX_SLOT_PERMILLE` parser (`default_policy.cpp:596`) re-implements the comma-list name matching of the `env_list` lambda 60 lines above, with a different terminator (`:` vs `,`/NUL). Two copies is inside the rule of three; a third env list of this shape should extract a helper. | note |
| R5 | data structure | low | `reorder_weights` allocates two `O(nvars)` vectors per call. Cold path (the op is off by default and rate-limited), and the call is `O(arena)` anyway, so the allocation is noise — but it is the shape the per-call-linear-scratch hazard warns about. Watch item only; no profile justifies action. | note |

## Axis verdicts

- **Rule-vs-policy leakage (the priority axis): improved this session, no new
  leaks.** Removing the hardcoded `strcmp(nm, "HyperTernaryRes") != 0 && …`
  exclusion list deleted a genuine leak — a *scheduling* decision (which rules
  run) baked into the policy as a hardcoded rule-name test rather than posed
  through the slot surface. S7 `Reorder` is clean: WHEN lives in
  `DefaultPolicy::want_reorder` (including kissat's `level == 0` trigger, kept
  out of the guard on purpose), WHICH ranking is the instance surface, and only
  the weight formula is rule-internal.
- **Redundancy:** no proof-emitting logic is duplicated. The S-b chain
  construction deliberately mirrors `kernel_lrat_chain`'s ordering rule in a
  separate solver — that is not duplication to remove (different state, no
  shared type), and both sites cite each other.
- **Data structures:** the sub-solver's `vector<vector<SubWatch>>` and
  arena-with-header layout are documented deviations from the kernel's flat
  scheme with the reason at the top of the file. Nothing to escalate.

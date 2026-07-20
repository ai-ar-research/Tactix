---
name: fuzz
description: Run or extend the soundness fuzzing harness, investigate a fuzz failure, shrink a failing CNF, or add regression cases. Use whenever testing solver soundness, after any kernel/rule change, when a solver verdict is suspected wrong, or when proof checking fails on a fuzzed instance.
---

# Fuzzing and soundness testing

Authoritative spec: DEVELOPMENT_PLAN.md §8.2. Harness lives in `tools/fuzz/` (if a piece doesn't exist yet, build it to this spec).

## Pipeline

```
generator ──cnf──▶ tactix ──┬─ SAT ──▶ model checker (independent tool)
    │                       └─ UNSAT ─▶ proof checker (drat-trim / lrat-check)
    └──────────▶ oracle solver (kissat) ─▶ verdicts must agree
failure ─▶ ddmin shrinker ─▶ minimal CNF + seed + rule trace ─▶ corpus/regressions/
```

## The four oracles — all must pass on every case

1. **Model soundness (SAT):** `tools/fuzz/checkmodel` verifies the `v`-line assignment satisfies every clause of the *original* input. Never trust solver bookkeeping.
2. **Proof soundness (UNSAT):** DRAT via `drat-trim formula.cnf proof.drat` (accept only `s VERIFIED`); LRAT via `lrat-check`. **A rejected proof is a release-blocking bug even when the UNSAT verdict is correct.**
3. **Differential status:** verdict must match Kissat; on disagreement, bring in CaDiCaL as tiebreaker and assume our solver is wrong until the model/proof oracle proves otherwise.
4. **Internal invariants:** run debug builds (`-DTACTIX_CHECK`) with ASan/UBSan; assertion failures are fuzz failures.

## Generators (all seeded — record the seed in every artifact)

Per-batch mix (`generators.py`, rev 31), random problems capped at 100 vars so
verdicts stay reachable under weak/random schedules:
- **15% random k-SAT** sweeping density: m/n ∈ {3.0, 4.0, 4.26, 4.6, 5.5} for k=3, n ∈ [20, 100]; also k ∈ {2,4,5}.
- **30% cnfuzz-style structured** CNFs (variable-reuse clusters, mixed clause lengths) — uniform random alone misses whole bug classes.
- **25% mutation** of a base instance: flip/drop/duplicate literals and clauses.
- **30% FuzzSAT-style** random Boolean circuits Tseitin-encoded to CNF (AND/OR/XOR gates, root asserted ~50% negated) — definitional chains that stress watchers, equivalence reasoning, and BVE (≤120 vars incl. Tseitin aux).
- **The 10 fixed adversarial degeneracies run every batch** (indices 0..9, exempt from the `--max-vars` re-roll): empty formula, empty clause, units only, contradictory units, tautology, duplicate literals, max var index (500000), single huge clause, huge negative clause, unit-empty mix.
- **Rule-order fuzzing (tests the flexibility guarantee):** drive the solver with a random legal macro-policy (random rule order/instances/budgets via the policy API). Soundness and proof validity must survive arbitrary legal schedules. Record the rule trace; replay must reproduce exactly.

## Cadence

Cases run on a thread pool (`--jobs`, default 8, added 2026-07-14): fuzzing
is correctness testing, not timing, so contention is harmless; case `i` is
fully determined by `(seed, i)`, so results are identical at any job count.

- Per commit / before declaring any change done: `tools/fuzz/run.sh --quick` (~1k cases, sanitizer build, 5 s/case default) + full regression corpus + Tier A.
- Random-policy leg (per commit for kernel/rule changes):
  `tools/fuzz/run.sh --quick --solver "build/debug/tactix --random-policy" --n 400 --max-vars 100`.
- Native-LRAT leg (per commit for proof/analyze changes):
  `tools/fuzz/run.sh --quick --solver "build/debug/tactix --lrat" --n 400 --max-vars 150`
  — when the solver command contains `--lrat`, the harness checks the
  solver's own LRAT with lrat-check directly (no drat-trim conversion).
  The size cap matters: a random schedule cannot finish large instances
  within the timeout, and the semantic oracles (differential/model/proof)
  only fire on a verdict — measured 2026-07-14, uncapped 150-case legs
  reached verdicts on only ~46 cases (104 silent TIMEOUTs). Watch the
  `verdicts:` line in the summary: TIMEOUT cases are sanitizer+invariant
  coverage only.
- **Random-policy + native-LRAT leg (per commit for ANY proof/rule change —
  the strongest proof test; promoted to standard 2026-07-15):**
  `tools/fuzz/run.sh --quick --solver "build/debug/tactix --random-policy --lrat" --n 400 --max-vars 100`.
  This is the only leg that checks our native LRAT *chains* under arbitrary
  legal schedules — including the canonical BATCH/round path of every
  inprocessing rule (the random policy draws `INSTANCE_CANONICAL`, not only
  single instances, since 2026-07-15). **Test both DRAT and LRAT, always:**
  they exercise different emission code and catch different bugs — drat-trim
  re-derives each clause by RUP and *ignores our chains* (so it validates the
  clause/delete stream + binary serialization), while lrat-check validates
  *our explicit antecedent chain* (a wrong chain that still yields a RUP-valid
  clause passes drat-trim but fails lrat-check). Neither subsumes the other,
  so the non-`--lrat` legs (native DRAT) and the `--lrat` legs (native LRAT)
  are both mandatory.
- **ASCII-DRAT leg (per commit for proof/serialization changes; added 2026-07-15):**
  `tools/fuzz/run.sh --quick --solver "build/debug/tactix --random-policy --no-binary" --n 400 --max-vars 100`.
  `--no-binary` emits ASCII DRAT (`ProofFormat::DratAscii`) — a DISTINCT serializer
  from binary DRAT; drat-trim auto-detects the format, so the existing check
  applies. Without this leg the ASCII branch of proof.cpp is only exercised by
  the tiny fixed `micro_proofs.sh` cases, so a regression in it would ship
  invisibly (flagged by the N1/N2/N3 adversarial audit, 2026-07-15).
- Nightly: run BOTH formats under the random policy at scale — native DRAT
  (`run.sh --nightly --solver "build/debug/tactix --random-policy"`) and
  native LRAT (`run.sh --nightly --solver "build/debug/tactix --random-policy --lrat"`),
  ~20k cases each (~12 h wall; sequentially, or split to fit the window) +
  all generators + differential testing. Before 2026-07-15 the nightly ran
  native DRAT only (its `--lrat` was the fuzz.py cross-check flag =
  drat-trim→lrat-check on the DRAT, which validates the checker conversion,
  not our LRAT emitter) — native LRAT under random schedules must be soaked
  too. Durable outcome: failures auto-shrink into `tools/fuzz/failures/`
  (empty of newer-than-launch entries + green log tail ⇒ green).

## Failure handling — never skip steps

1. Reproduce with the recorded seed.
2. Shrink with `tools/fuzz/ddmin` (clause-level then literal-level) while preserving the failure predicate (which oracle failed).
3. Save minimal CNF + seed + rule trace + one-line diagnosis to `tools/fuzz/corpus/regressions/NNN-<slug>/`. This directory is tracked in git and grows monotonically — regression cases are never deleted.
4. Fix; confirm the regression case and a quick fuzz round pass; mention the corpus entry in the commit message.

When the harness itself is being built (phase 0), validate it by fuzzing Kissat and CaDiCaL as stand-in solvers: the pipeline must run end-to-end and correctly flag deliberately-broken solvers (e.g., a wrapper that lies UNSAT→SAT, or corrupts one proof line).

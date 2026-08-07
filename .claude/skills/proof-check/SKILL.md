---
name: proof-check
description: Verify DRAT/LRAT proof certificates, set up or run the checker toolchain (drat-trim, lrat-check, cake_lpr), debug a rejected proof, or measure proof size/check time. Use whenever a proof fails verification, when adding proof logging to a rule, or when validating certificates the way the SAT Competition does.
---

# Proof certificate checking

Authoritative spec: DEVELOPMENT_PLAN.md §4 (proof obligations), §6 (competition rules). Competition reality: an UNSAT answer with an unverifiable proof counts as **unsolved**, and buggy proofs can disqualify — proofs are correctness, not an add-on.

## Checker toolchain

Keep checkers in `tools/checkers/` (build once, commit the setup script `tools/checkers/setup.sh`, not the binaries):
- **drat-trim** (github.com/marijnheule/drat-trim): `drat-trim formula.cnf proof.drat` — accept only exit 0 with `s VERIFIED`. Handles ASCII and binary DRAT (auto-detected). Flags: `-b` force binary parse issues visible, `-L out.lrat` to convert DRAT→LRAT.
- **lrat-check** (ships with drat-trim repo): `lrat-check formula.cnf proof.lrat` — fast because hints avoid search.
- **cake_lpr** (verified, what the competition used in 2025): run periodically and before any milestone claim, not on every fuzz case (slower to a degree, but its verdict is the trusted one).

## The accept-condition is part of the checker — get it wrong and every proof passes

**Match the checker's success line exactly, and prove the match can FAIL.**
- `"VERIF" not in output.upper()` matches `c NOT VERIFIED` as well as `c VERIFIED`. That exact bug made **every LRAT rejection in this project's history record as a pass**, hiding two real N2 breaches under green legs.
- A `^`-anchored regex on drat-trim's output matches nothing: drat-trim prefixes its `s` lines with `\r`, so the inverse failure (everything reported unverified) is one character away.
- **A checker whose rejection you have never observed is not yet a checker.** Feed it a known-bad artefact (`tools/fuzz/broken/truncate-proof.sh`, or a hand-corrupted line) and require a red before you trust a green. `tools/fuzz/selftest.sh` is the standing mechanism; run it whenever a checker, its invocation, or its accept-condition changes.
- The checkers disagree on legal input: drat-trim **deletes duplicate literals** while parsing a CNF and `drat-trim -L` cites input clauses by id, so on a duplicate-literal formula the *converted* LRAT is rejected for every solver, ours and the references alike. That is a checker disagreement, not a solver defect — the conversion cross-check stands down (visibly, counted) while the native-LRAT oracle keeps checking.

## Conventions for our solver

- Binary DRAT is the default output; `--lrat` emits native LRAT with antecedent hints captured during conflict analysis (not reconstructed post-hoc).
- Every rule family emits its own proof delta; a rule that cannot state its DRAT/LRAT justification does not merge (see rule-dev skill).
- Deletion lines matter: emit `d` lines when clauses are forgotten/strengthened — missing deletions make checking quadratically slower and can exhaust checker memory.
- Track proof size (bytes, bytes/conflict) and check time as metrics alongside PAR-2; the plan bets on native LRAT for any combined solver+checker scoring.

## Debugging a rejected proof — standard procedure

1. Reproduce: exact CNF + proof file + checker command + full checker output (drat-trim reports the first failing line number).
2. Shrink the *instance* with the fuzz skill's ddmin so the failing proof is small enough to read.
3. Convert to LRAT if the failure is in DRAT (`drat-trim -L`): LRAT failures localize to a specific clause and hint list, DRAT failures don't.
4. Map the failing proof line back to the emitting rule via the rule trace (traces and proof cursor positions are logged together in `--trace` mode).
5. Classify: (a) rule derived a genuinely unsound clause — kernel bug, highest severity; (b) clause sound but justification wrong/missing hints — proof-logging bug in that rule; (c) deletion emitted for a clause still in use — bookkeeping bug. Fix in the rule module, add the shrunk case to the regression corpus.

## Micro-formula rule tests

Each rule family has unit tests that apply the rule to tiny hand-written formulas and pipe the emitted delta (wrapped as a complete proof) through drat-trim. Run these first when touching any rule's proof emission — they fail in milliseconds, long before fuzzing would find it.

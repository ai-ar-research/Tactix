# The Sweep N2 breach was one inverted bit: S-e decoded sub-solver literals with the parity flipped

**Date:** 2026-08-05 · **Frozen on writing** (doc discipline: reports are never edited).
**Follows:** `2026-08-05-sweep-proof-rejection.md` (the investigation that found the breach) and
`2026-08-05` survey commit `434140f` (blast radius).

**Verdict:** the report's premise was right. There was a real defect, it was in **S-e**
(`sweep_emit_general`), and it is fixed. The **same** defect also accounts for the second signature
reported independently from the SCHED-C5 agent's `--random-policy` × native-DRAT run — that is **one
bug, not two**. Closing the coverage gap turned up two further things that are **not** Sweep: a second
live N2 defect in the DIMACS frontend (§7, reported, not fixed — it wants its own change) and a
**broken oracle in the fuzz harness itself** — `fuzz.py`'s native-LRAT check accepted the string
`NOT VERIFIED` (§6a, fixed here).

---

## 1. Root cause, in one paragraph

The sub-solver and the kernel share the same literal encoding, `2*v | negated`. `sweep_detect_seed`
encodes kernel → sub with the parity carried through unchanged
(`sub_lit(sub_of_var[var_of(l)], negated(l))`), and `test_subsolver`'s DIMACS writer agrees (even sub
literal = positive). But **both** decode sites on the way back — the local `to_lit` inside
`sweep_capture_dir`, and the inline decode in `sweep_emit_general`'s lemma loop — spelled the inverse
as `make_lit(vars[sub_var_of(sl)], (sl & 1u) == 0)`, i.e. *negated when the sub literal is positive*.
Every lemma S-e replayed into the main proof was therefore **the elementwise negation of the clause the
sub-solver actually derived**, carrying that clause's antecedent chain. `env ⊆ F` was intact, the core
mapping was intact, the chain order was intact; the clause on the line was wrong.

**Fix** (`src/rules/sweep/sweep.cpp`): one shared

```cpp
static inline Lit sweep_kernel_lit(const std::vector<Var> &vars, SubLit sl) {
  return make_lit(vars[sub_var_of(sl)], (sl & 1u) != 0);
}
```

and both sites go through it. **Soundness argument:** the fix is the literal inverse of the encode, so
the emitted lemma *is* the sub-solver lemma, and the sub-solver's lemmas are already checker-verified in
sub-solver space by the `sweep_proofs` ctest (S-b, 40 cases, both formats). Nothing about **which**
inferences Sweep draws changes — the same pairs are proved and the same two binaries are added — so the
two builds run the same search: on `asconhashv12` both give 97 549 conflicts, an identical
`c sweep` line, and a DRAT proof of the same 12 470 666 bytes. Only the literals printed on the S-e
lemma lines differ. **No inference is dropped**, so no D-CONG-SKIP-style yield trade was needed; the
narrower option (decline the pair) was not necessary because the derivation was always valid — only its
transcription was not.

## 2. Why nothing else caught it — the failure was *self-consistent*

This is the interesting part, and it is why the defect survived S-e's whole increment plus 14 000 green
fuzz cases. The two decode sites agreed **with each other**, which made every internal check pass:

1. Direction 1 proves `(¬x ∨ y)`. Negated, that is `(x ∨ ¬y)` — which is *direction 2's binary*. So the
   pair still looks like a pair, and `sweep_apply`'s `is_pair` check accepts the two targets **in either
   order** (deliberately, and correctly — the order genuinely depends on whether the seed literal sorts
   below `y`). The roles silently swap and the check is happy.
2. `sweep_emit_general`'s own final-step guard compares the emitted target against `p.t0/p.t1` — which
   were read off the **same inverted decode** in `sweep_capture_dir`. It is comparing the bug to itself.
3. Both binaries are still added, so **`F` is correct and the solver's verdict is correct**. Nothing in
   the model oracle, the differential oracle, or the brute-force equivalence oracle can see it.
4. `tests/test_sweep.cpp`'s `test_se_general_path_emits` asserts S-e *fired* and that `F` now states the
   equivalence. Both remain true. It never checked the proof.
5. `tests/sweep_proofs.sh` did check proofs with real checkers — but only `test_subsolver --emit`, i.e.
   the derivations **in sub-solver space**, where the translation does not appear at all. It was
   checking the half that was right.

## 3. Reproduction and gates (`asconhashv12_opt64_H4_M2-LOD9_m0_2_U19.c`, local, single core)

| leg | solve | conflicts | proof | checker | result |
|---|---|---|---|---|---|
| pre-fix DRAT | 169.6 s | 97 549 | 12 470 666 B | drat-trim | **NOT VERIFIED**, line 252 026, lemma `1436 -1004 -1666 -329 0` (9.99 s) |
| post-fix DRAT | 172.6 s | 97 549 | 12 470 666 B | drat-trim | **s VERIFIED** (103.9 s; 96 499/147 409 lemmas in core) |
| post-fix LRAT | 232.7 s | 97 549 | 6 114 242 021 B | cake_lpr | **s VERIFIED UNSAT** (268.8 s) |

Both builds print the identical `c sweep` line — 12 rounds, 490 environments, 6967 solves, 9
equivalences — and the post-fix build's new counter line reads `c sweepemit 18 binaries, 1 not-rup,
1 general pairs`. So **one** general-path pair out of nine equivalences was enough to invalidate a
12.5 MB refutation. That is also why rejection does not track equivalence volume (the survey's
`goldcrest-and-9`, 127 equivalences, verifies): exposure tracks **non-RUP pairs**, the only ones S-e
sees. The pre-fix line and lemma reproduce the prior report exactly.

## 4. The `--random-policy` × native-DRAT signature is the SAME bug

Cases 888 / 2785 / 3296 from `fuzz.py run --solver "tactix --random-policy" --n 4000` (default seed):

| case | vars/clauses | general pairs | pre-fix drat-trim | post-fix drat-trim |
|---|---|---|---|---|
| 888 | 32 / 147 | 2 | NOT VERIFIED, line 150, `26 -3 0` | **s VERIFIED** |
| 2785 | 47 / 216 | 1 | NOT VERIFIED, line 610, `26 0` | **s VERIFIED** |
| 3296 | 46 / 193 | 2 | NOT VERIFIED, line 719, `-34 -18 0` | **s VERIFIED** |

No change outside `sweep.cpp`'s two decode sites was needed, so `SubsumeForward`'s deletion stream is
**exonerated** for these three. The width evidence agrees independently: `sweep_emit_equiv` (S-d) can
only ever emit width-2 lemmas, and the rejected lemmas here and on `asconhashv12` include widths 1
(`-15 0` on the shrunk case), 3, 4 and 5.

**The "one half of the pair verifies, the other does not" shape resolves too.** On case 888 the two
lines around the failure are `(-26 3)` at 149 and `(26 -3)` at 150 — the two halves of one equivalence —
with 149 a duplicate of a clause already at line 45, and 150 the rejection. That is not "emitting the
already-present half broke RAT for the other". The two **targets** are the correct pair either way
(inverting both just swaps which direction claims which), so both are entailed; what is missing is the
run-up. drat-trim verifies each target by RUP over what precedes it, and what precedes 150 is the
*negated* intermediate lemma, which does not make it RUP. Half 149 verifies only because it happens to
restate a clause `F` already had. Fix the parity and both halves get the derivation they were supposed
to have.

The reported asymmetry — "`--random-policy` DRAT fails, `--random-policy` LRAT passes" — is **not**
evidence that the chains were right. The LRAT arm was **vacuous**: on all three cases the LRAT run
reports `c sweepemit 0 binaries, 0 not-rup, 0 general pairs`. (The two arms genuinely diverge because
several rules decline an inference under LRAT that they take under DRAT, which changes the trajectory;
on case 888 the LRAT arm refutes in 1 conflict with 112/112 environments already unsatisfiable.) This is
[[ab-arm-identical-artefacts]] in its purest form and it is why §6 adds the counters.

## 5. The shrunk regression case

`tools/fuzz/corpus/regressions/012-sweep-se-inverted-literal-polarity/` — **19 variables, 22 clauses,
47 literals**, ddmin-shrunk from case 888 (147 clauses, 1474 evaluations) under the predicate "UNSAT ∧
`sweep_general > 0` ∧ drat-trim rejects". The second conjunct matters: without it ddmin is free to
shrink toward any other rejection and the minimal case stops being an S-e case.

| build | verdict | sweepemit | drat-trim |
|---|---|---|---|
| pre-fix | UNSAT | 46 binaries, 1 not-rup, **1 general pair** | **NOT VERIFIED**, proof line 1, lemma `-15 0` |
| post-fix | UNSAT | 46 binaries, 1 not-rup, 1 general pair | **s VERIFIED** |

Whole-corpus replay (`fuzz.py corpus`): **pre-fix 12 entries / 1 failure** (this one), **post-fix
12 entries / 0 failures**. A regression test that does not fail before the fix is not a regression test;
this one does.

`meta.json` carries `solver_args: ["--random-policy"]`, and **`cmd_corpus` now honours
`solver_args`/`solver_env`** (`tools/fuzz/fuzz.py`). Those two keys have been in the corpus format since
entry 011 and the replay silently ignored them, so any entry needing a non-default schedule was passing
for the wrong reason. Entry 011 (which needs `--lrat` + `TACTIX_CHRONO_LEVELS=0`) now replays as
documented and passes.

## 6. Why fuzz was blind — measured, not guessed

200 CNFs from `generators.generate` (the fuzz distribution), pre-fix binary, all four
policy × format quadrants, counting only cases that answer UNSAT (the only ones with a proof oracle):

| quadrant | UNSAT cases | general pairs | cases with ≥1 | proofs REJECTED |
|---|---|---|---|---|
| DefaultPolicy × DRAT — *the standing per-commit leg* | 76 | **0** | 0 | 0 |
| DefaultPolicy × native LRAT | 76 | **0** | 0 | 3 — a **different** defect, §7 |
| `--random-policy` × DRAT — *the leg that is documented but was not run* | 63 | 5 | 2 | **1** |
| `--random-policy` × native LRAT — *the standing leg* | 63 | **0** | 0 | 3 — again §7, not Sweep |

(The rejections are counted by running the checkers directly; the fuzz harness itself did not see the
LRAT ones — see §6a.)

**And the quadrant that catches it is a documented per-commit leg that was not run.** The `fuzz` skill
lists `run.sh --quick --solver "build/debug/tactix --random-policy" --n 4000 --max-vars 100` as a
per-commit leg. Across the whole `tools/fuzz/slurm-*.log` archive, `--random-policy` **without**
`--lrat` appears **twice** (`build/fuzz-s5def/tactix --random-policy`, `build/fixdbg/tactix
--random-policy`), both predating S-e (`ca3522e`, 2026-08-05). The two legs actually run at HEAD were
`pin-sd-fuzz/tactix` (DefaultPolicy × native DRAT, 10 000 cases / 3958 UNSAT, job 1583886) and
`pin-sd-fuzz/tactix --random-policy --lrat` (4000 / 1208 UNSAT, job 1583885) — precisely the two
quadrants the table shows are blind. In practice the pairing has settled into
"DefaultPolicy × DRAT + random × LRAT", and the diagonal has been skipped since S-e existed.

Three independent reasons stack, and all three had to hold:

1. **S-e is rare on fuzz-scale formulas under DefaultPolicy**, because it only sees the pairs S-d
   declines. Over all 200 cases (including SAT ones) DefaultPolicy does emit 39 general pairs in 7
   cases — but restricted to the UNSAT cases, where a proof exists to check, it is **zero**. Sweep's
   emission at fuzz scale happens mostly on satisfiable instances, which carry no proof oracle at all.
2. **drat-trim checks backward**, so a bogus lemma outside the refutation's core is never examined. The
   DRAT leg therefore needs the negated lemma to be *needed*, not merely present.
3. **The native-LRAT leg — the one format that checks every chain forward — runs `--random-policy`,
   where Sweep emitted nothing.** A green arm over zero emissions.
4. And underneath all three, **the native-LRAT oracle could not fail at all** (§6a) — so even a leg that
   had reached the defect in LRAT would have reported green.

The fix for (3) is the standing-leg change in §10; the fix for (1)+(2) is the stress policy; (4) is
fixed here.

### 6a. And a fourth thing, found while measuring the above: the native-LRAT oracle never failed

`fuzz.py`'s two native-LRAT checks read

```python
if c is None or "VERIF" not in o.upper():        # native LRAT
if c is None or "VERIFIED" not in o:             # drat-trim -L cross-check
```

`lrat-check` prints `c VERIFIED` on success and **`c NOT VERIFIED`** on rejection. Both substring tests
match both strings, so **every LRAT rejection in the project's fuzz history was recorded as a pass**;
only a crash or a timeout (`c is None`) could ever fail a native-LRAT leg. That is consistent with the
record: every LRAT defect fuzz has ever caught — the `kernel_lrat_chain` abort in corpus entry 011, the
43 crashes that S-d's own N2 defect produced at `ca3522e` — was an **abort**, never a checker verdict.
The DRAT branches were never affected (`s NOT VERIFIED` does not contain `s VERIFIED`).

The measurement in the table above was made with an independent script and cake_lpr/lrat-check read
directly, which is why it sees rejections the harness did not. Fixed here (`lrat_verified()`, rejection
tested first). **Consequence, and it is not comfortable: with the oracle repaired, the native-LRAT legs
are no longer green** — they now surface the §7 duplicate-literal defect at roughly the rate the table
shows. Every such failure is classified in §8; none of them involve Sweep emission.

## 7. A SECOND, unrelated N2 defect, found here and NOT fixed

The three `DefaultPolicy × native LRAT` rejections above are **not** Sweep — they reject identically on
the fixed build with `c sweepemit 0 binaries, 0 not-rup, 0 general pairs`. In all three the rejected
line's chain cites an input clause whose **file form carries a repeated literal**: case c127 cites
clauses 1 = `-4 -4 0` and 17 = `4 4 0`; c137 cites 81 = `-24 -24 0`; c195 cites 168 = `21 21 0`.
Minimal reproducer, two clauses:

```
p cnf 1 2
-1 -1 0
1 1 0
```

We answer `s UNSATISFIABLE`; the whole native-LRAT proof is `3 0 1 2 0`; cake_lpr says
`clause not empty or singleton after reduction` and `lrat-check` says `NOT VERIFIED`. DRAT verifies.

**Mechanism.** `ingest_clause` (`src/frontend/parse.cpp`) applies set semantics to every input clause —
`std::sort` + `std::unique` — so `(-1 ∨ -1)` becomes the unit `¬1` and is recorded as
`unit_ids[1] = 1`. The **checker** holds input clause 1 as the literal two-literal clause it read from
the file. Every later chain citing that id asks the checker to unit-propagate a clause that is not unit
for it. Same for a raw `(a ∨ a ∨ b)` stored as a binary, or any large clause whose file form has a
repeat. Invisible to DRAT, which re-derives rather than citing.

**How often.** With the §6a oracle repaired, this is what a native-LRAT fuzz leg now reports: 2 proof
failures in 130 UNSAT cases on the `sweepstress-gen-lrat` leg below, and 3 in 76 / 3 in 63 in the §6
quadrant samples — call it **1.5–4 % of UNSAT fuzz cases**, because the generators emit repeated
literals freely. Every one of those failures classifies the same way: the CNF has a clause with a
repeated literal and the run reports `c sweepemit 0 binaries, 0 not-rup, 0 general pairs`. Until it is
fixed, the native-LRAT legs will be red at that rate, and **that is the oracle working**, not a
regression from this change.

**Why it is not a one-liner.** The obvious repair — emit the deduped clause as its own `add_rup` line
with the original as its single antecedent (which *is* a valid one-antecedent chain) — needs a fresh id,
and ids cannot be consumed mid-parse: LRAT numbers input clauses `1..N` in file order and
`ingest_clause` deliberately burns exactly one id per input clause to keep that alignment. A correct fix
buffers the restatements, emits them after parsing, and re-points the stored clause's id. That is a
separate change to the frontend and the kernel, in nobody's current scope, so it is recorded here rather
than bundled into a Sweep fix.

## 8. The stress policy, and its per-path counts

`--sweep-stress-policy` (`src/policies/sweep_stress_policy.{hpp,cpp}`) sweeps every root to a fixpoint
with **no effort budget**, interleaving V3 Substitute so each round sees the formula the previous round
changed, and checks `kernel_check_invariants` per step. `TACTIX_SWEEP_FORCE_GENERAL=1`
(`sweep_apply`) makes the S-d fast path decline so **every** proved equivalence is routed through S-e —
a coverage knob, not a dosing one: both paths justify the same two binaries, so `F` is identical either
way and only the proof lines differ.

**Reachability at the source**, same 200 CNFs, fixed release binary, counters read off `c sweepemit`:

| arm | sweep rounds | S-d+S-e binaries | not-RUP (→ S-e) | **general pairs** | cases with ≥1 general |
|---|---|---|---|---|---|
| DefaultPolicy | 146 | 590 | 177 | 39 | 7 / 200 |
| `--sweep-stress-policy` | 458 | 4052 | 1009 | **143** | 28 / 200 |
| `+ TACTIX_SWEEP_FORCE_GENERAL=1` | 443 | 590 | 3477 | **295** | **59 / 200** |

Both paths are non-zero in every arm, and the forced arm turns "8 non-RUP claims in 4380" into every
equivalence. (In the forced arm `sweep_binaries` counts only S-e's two-per-pair, hence 2 × 295 = 590.)

**Fuzz legs** — `build/debug` (ASan/UBSan, assertions), oracle kissat, `--max-vars 60`, `--jobs 4`,
n = 400 each, run locally (no SLURM: the cluster queue belongs to another agent):

| leg | proof format | failures | verdicts |
|---|---|---|---|
| `--sweep-stress-policy` | native DRAT | **0** | SAT 240 / UNSAT 131 / TO 29 |
| `--sweep-stress-policy --lrat` | native LRAT | **2**, both §7 | SAT 240 / UNSAT 131 / TO 29 |
| `TACTIX_SWEEP_FORCE_GENERAL=1 --sweep-stress-policy` | native DRAT | **0** | SAT 240 / UNSAT 130 / TO 30 |
| `TACTIX_SWEEP_FORCE_GENERAL=1 --sweep-stress-policy --lrat` | native LRAT | **2**, both §7 | SAT 240 / UNSAT 130 / TO 30 |
| `--random-policy` — *the previously-skipped quadrant* | native DRAT | **0** | SAT 223 / UNSAT 119 / TO 58 |

Every UNSAT case is proof-checked, so the UNSAT column is the oracle's real coverage. Both LRAT legs
fail on **the same two cases** (23 `struct-c5x9-m140`, 129 `struct-c6x7-m161`) and both are §7: each CNF
carries a repeated input literal, and each run reports `c sweepemit 0 binaries, 0 not-rup,
0 general pairs`. **No Sweep emission is involved in any failure on any leg.** Regression corpus replays
clean under DefaultPolicy (12/0) and under `--lrat` with the repaired oracle (12/0).

*(Caveat on provenance: the `--sweep-stress-policy --lrat` row is the RE-RUN, after the §6a oracle fix
landed mid-session. Its first pass reported 0 failures against the broken oracle and is discarded. The
`gen-lrat` row already ran against the fixed oracle.)*

## 9. What changed

- `src/rules/sweep/sweep.cpp` — the fix (`sweep_kernel_lit`, both decode sites) + the
  `TACTIX_SWEEP_FORCE_GENERAL` coverage knob + `r.proofs.clear()` in `sweep_round_begin`. That last one
  is a latent trap of the same family, found while auditing: `SweepRound::proofs` is **parallel** to
  `found` (`sweep_apply` indexes both with one `idx`) and only `found` was being reset, so any caller
  that reuses a round across rounds would read the previous round's refutation for every pair. Harmless
  today only because `sweep_apply` builds a fresh round per apply, and it would have failed *closed*
  (the `is_pair` target check declines) rather than emitting — but it is two things kept in step by
  hand, which is exactly what produced the breach.
- `src/main.cpp` — a `c sweepemit` line reporting `sweep_binaries` / `sweep_not_rup` /
  `sweep_general`. Those counters existed at `kernel.hpp:200-202` and were never printed, so "did S-e
  fire on this run?" was unanswerable from a solve and the survey had to infer it from lemma width.
- `src/policies/sweep_stress_policy.{hpp,cpp}` + `--sweep-stress-policy` — kept (a durable N1 stress
  axis: Sweep is the only rule whose proof lines come from a *second* solver's search).
- `tests/test_sweep.cpp --emit` + `tests/sweep_proofs.sh` — the ctest that fails pre-fix: a complete
  UNSAT refutation whose middle is literally S-e's output, checked by drat-trim **and** lrat-check.
  Pre-fix, lrat-check rejects on lemma id 11 — `multiple literals unassigned in hint 1: -2 2`, clause
  `3 -2 1 0` against hint `-1 2 -3 -4 0`. Post-fix that same lemma reads `-3 2 -1 0`: the bug in one
  line, as a diff. Note the two regression artefacts catch it through **different formats** and neither
  is redundant — on this 4-variable fixture drat-trim's backward pass prunes the bogus lemmas and
  accepts, so only LRAT fails it, whereas the shrunk corpus case (§5) is caught by DRAT.
- `tools/fuzz/fuzz.py` — `lrat_verified()` (§6a: the native-LRAT oracle accepted `NOT VERIFIED`) and
  `cmd_corpus` honouring `meta.json`'s `solver_args`/`solver_env` (§5).
- `tools/fuzz/corpus/regressions/012-sweep-se-inverted-literal-polarity/`.
- `docs/implementation-notes.md` — V5 Sweep was missing from the implemented-guard table; added, with
  the sub↔kernel parity contract. `docs/calculus.md` is unchanged: no precondition, effect or proof
  delta moved — S-d/S-e are described there exactly as they are implemented.

## 10. Standing recommendation

**Actually run `--random-policy` × native DRAT per commit.** It is already in the `fuzz` skill's leg
list; what is missing is the habit, not the definition (§6). The practised pair — DefaultPolicy × DRAT
and random × LRAT — is exactly the two blind quadrants, and the diagonal is where both this defect and
the §7 one show. The `--sweep-stress-policy` legs (both formats, with and without
`TACTIX_SWEEP_FORCE_GENERAL`) belong in the nightly, not the per-commit gate — they are slow and their
value is coverage of a path that is rare by construction.

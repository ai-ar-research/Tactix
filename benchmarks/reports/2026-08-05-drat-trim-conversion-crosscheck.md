# The 38 "LRAT" failures are drat-trim's converter, not our emitter — and the leg was mislabeled

**Date:** 2026-08-05 · **Frozen on writing** (doc discipline: reports are never edited).
**Follows:** `2026-08-05-input-clause-normalization-n2.md` (the frontend N2 fix) and the merged-tree fuzz
run that reported 38 residual failures against it (jobs `1584460` DRAT / `1584461` "LRAT").

**Verdict: NOT our path, and not a solver defect at all.** The failing leg ran the solver **without
`--lrat`**, so the proof under test was native **DRAT**; drat-trim **verified** it. What failed was the
harness's *cross-check* — `drat-trim -L` converting that DRAT to LRAT, then `lrat-check` reading the
converted file. **kissat's and cadical's own DRAT proofs are rejected by that pipeline in exactly the
same way, on the same CNFs, with byte-identical messages.** Our native LRAT verifies on every one of
those cases under both `lrat-check` and `cake_lpr`. The frontend fix is complete for what it covers;
`HEAD` is **not** fuzz-red on native LRAT.

Two things did need fixing, and both are in the harness: the cross-check is not a valid oracle on two
classes of degenerate-but-legal CNF, and `selftest.sh` was passing over two of them.

---

## 1. Which oracle actually failed

`fuzz.py` has two different LRAT oracles and two different messages for them:

| oracle | when | message on rejection |
|---|---|---|
| **native LRAT** | `--lrat` is in the *solver command* | `lrat-check rejected the native LRAT (exit N)` |
| **conversion cross-check** | `--lrat` is a *fuzz.py flag*: `drat-trim -L` then `lrat-check` | `lrat-check rejected the LRAT proof` |

The bundle's own `meta.json` settles it:

```json
"solver": ["/home/hwu/SAT-solving/build/pin-wrapfuzz/tactix", "--random-policy"],
"failures": [["proof", "lrat-check rejected the LRAT proof"]]
```

No `--lrat` in the solver argv, and the second message. So this leg is **native DRAT + conversion
cross-check** — the arrangement the fuzz skill explicitly describes as validating "the checker
conversion, not our LRAT emitter". Calling it the native-LRAT leg is what made it look like a residual
of the frontend fix.

## 2. The decisive experiment

Same CNF (`003706-proof-seed20264415/min.cnf`, 269 clauses, 10 with a repeated literal), same pipeline,
three different solvers' own proofs:

| arm | drat-trim on the DRAT | converted LRAT under lrat-check |
|---|---|---|
| tactix @ merged HEAD | VERIFIED | **NOT VERIFIED** — `multiple literals unassigned in hint 161: 80 80` |
| **kissat** (own proof) | VERIFIED | **NOT VERIFIED** — `multiple literals unassigned in hint 161: 80 80` |
| **cadical** (own proof) | VERIFIED | **NOT VERIFIED** — `multiple literals unassigned in hint 161: 80 80` |
| tactix **native LRAT** | — | **VERIFIED** (and `cake_lpr`: `s VERIFIED UNSAT`) |

Line 162 of the file (clause 161) is `80 80 0`. Three independent solvers, one identical rejection: the
proof is not the variable.

Five-clause reproducer, auditable at a glance — kissat, cadical and tactix all rejected, native LRAT
verified:

```
p cnf 3 5
1 1 0
-1 2 0
-1 3 0
-2 -3 0
2 3 0
```

## 3. Mechanism, in the checkers' own source

**Class 1 — a repeated literal.** `drat-trim`'s parser sorts every clause and *deletes* duplicate
literals as it reads the CNF (`drat-trim.c` `parse()`, the `buffer[i] == buffer[i+1]` loop, which even
prints `WARNING: detected and deleted duplicate literal`). So drat-trim holds file clause 161 as the
unit `80` and its RUP check passes. `-L` then writes the converted LRAT citing input clauses **by id**.
`lrat-check` re-reads the **original file** and does not dedupe, so id 161 names a two-entry clause it
cannot propagate. Two tools from the same repository disagree about what the file says.

This is the same *shape* as the frontend breach — an id naming one clause for the producer and another
for the consumer — with drat-trim's converter in the producer role. It is not reachable through
anything we emit.

**Class 2 — the empty clause in the input.** drat-trim short-circuits (the formula is refuted at parse)
and `-L` writes a **zero-byte** proof; `lrat-check` then never sees an empty-clause derivation and says
`Verification incomplete` → `NOT VERIFIED`. Also solver-independent.

Measured *not* to break the cross-check, so the predicate stays narrow: tautologies, verbatim-duplicate
clauses, and plain UNSAT.

## 4. Why nobody saw this before today

The same reason the frontend breach hid: until `lrat_verified()` landed this morning, this oracle tested
`"VERIF" not in o.upper()`, which matches `c NOT VERIFIED` too. Every rejection read as a pass. The
harness's **own selftest** was affected — `expect_clean kissat` runs 150 cases *with* the cross-check
and passed, while in fact two of its fixed edge degeneracies (`edge-empty-clause`, `edge-unit-empty-mix`)
were being rejected. Running it after the oracle repair is what surfaced class 2.

## 5. What changed

Nothing in the solver. Two things in `tools/fuzz/`:

- **`fuzz.py`** — `conversion_crosscheck_unsound(cnf)` returns a reason when the cross-check is not a
  valid oracle for a case (the two classes above, each documented with its checker-source citation), and
  `run_case` then **skips it visibly**: `CaseOutcome.skips` is a new counted channel, surfaced per-leg as
  `oracles skipped: {'lrat-convert': N}` and per-case in `check-one`. A dropped oracle that is not
  counted is indistinguishable from a passing one, which is the failure mode this project has been burned
  by; it is now impossible to lose this one quietly.
  **The native-LRAT oracle is deliberately NOT gated** — it reads ids our own emitter wrote, it is the
  oracle that covers the frontend restatement, and it passes on both classes.
- **`selftest.sh`** — a new section that pins all three properties: the cross-check stands down on a
  duplicate-literal CNF *and* does not blame the solver; the raw pipeline really is rejected there (so
  the skip is not vacuous, and the test fails loudly if a future checker release fixes the
  disagreement and the cross-check should come back); and the native-LRAT oracle still fires and passes
  on the same CNF.

## 6. Gates

- **Both bundles stop reproducing**, at the recorded seed, on `min.cnf` and `case.cnf`:
  conversion cross-check `failures=[] skipped=[('lrat-convert', …)]`; **native-LRAT oracle
  `failures=[]`**; and the path is live — `c input 10` / `c input 6` clauses restated, so these are not
  vacuous passes. `cake_lpr` says `s VERIFIED UNSAT` on all four native proofs.
- **`selftest.sh` green**, including the `kissat`/`cadical` clean arms that were red before this change,
  with the three broken-wrapper arms still correctly flagged (the oracle is gated, not weakened).
- **Fuzz at two seeds** (the coordinator's methodological point, and a fair one — the frontend branch was
  validated at seed 20260805 only): six legs, 6000 cases each, jobs `1584462`–`1584467`.

All six **0 failures**, `build/pin-n2resid` at the merged HEAD, 32 cores each:

| leg | seed | cases | failures | verdicts | reached (input-restated) | oracles skipped |
|---|---|---|---|---|---|---|
| `--random-policy --lrat` (native) | **20260709** | 6000 | **0** | S 3041 / U 1916 / TO 1043 | 1697 (1321 UNSAT) | — |
| `--random-policy --lrat` (native) | 20260805 | 6000 | **0** | S 3046 / U 1922 / TO 1032 | 1708 (1334 UNSAT) | — |
| `--random-policy` DRAT + cross-check | **20260709** | 6000 | **0** | S 3042 / U 1917 / TO 1041 | 1697 (1321 UNSAT) | `lrat-convert: 1471` |
| `--random-policy` DRAT + cross-check | 20260805 | 6000 | **0** | S 3047 / U 1922 / TO 1031 | 1708 (1334 UNSAT) | `lrat-convert: 1482` |
| DefaultPolicy `--lrat` (native) | **20260709** | 6000 | **0** | S 3447 / U 2386 / TO 167 | 1669 (1296 UNSAT) | — |
| DefaultPolicy `--lrat` (native) | 20260805 | 6000 | **0** | S 3450 / U 2386 / TO 164 | 1679 (1308 UNSAT) | — |

Seed **20260709** is the one that produced the 38-failure report, so the top row is the direct
refutation: same seed, same case population, same 6000 cases, **0 native-LRAT failures** with the
restatement path reached by **1321 of 1916 UNSAT answers (69 %)**. Not a vacuous green.

The skip counts are worth reading as a measurement in their own right: **~1475 of 6000 cases (25 %)**
were losing the conversion cross-check silently before this change — 24 % of the corpus population is
degenerate enough to trip one of the two classes. That is the size of the hole the counted-skip channel
now keeps visible.

## 7. What I did NOT do

No regression-corpus entry. The corpus contract is a case that **fails on the current binary** and
passes after a fix; here the current binary is correct and the rejected artifact is produced by
`drat-trim -L`. A corpus entry would assert a solver defect that does not exist. The invariant worth
protecting is the *oracle's* behaviour, so it is pinned in `selftest.sh` (§5) instead — including the
check that the underlying checker disagreement still exists, so the skip cannot outlive its reason.

Left alone deliberately: the drat-trim/lrat-check disagreement itself. They are vendored reference tools
(`drat-trim/`), the competition checks DRAT with drat-trim directly or takes LRAT from the solver — both
paths are clean for us — and patching a checker to make our harness happy would be the wrong instinct.

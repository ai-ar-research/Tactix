# Sweep's RUP admission gate was a no-op outside native LRAT

**Date:** 2026-08-20 · **Frozen on writing** (doc discipline: reports are never edited).
**Found by:** a 400-case `--random-policy` DRAT fuzz leg run while validating the
corpus-004/023 repair (plan §8.2 external escalation).
**Regression:** `tools/fuzz/corpus/regressions/026-sweep-drat-admission-gate` and
`tests/test_sweep.cpp::test_rup_admission_is_format_independent`.

---

## 1. Signature

`drat-trim` rejected the DRAT proof of a run that answered `UNSATISFIABLE`:

```
c WARNING: RAT check on proof pivot failed : [2137] -3 19 0
c RAT check failed on all possible pivots
c failed at proof line 33 (modulo deletion errors)
s NOT VERIFIED
```

The input has 1035 clauses, so proof line 33 is clause id 1068. The rule journal
places that id inside the run's single `Sweep` transaction as
`add-binary(id=1068;lits=-3,19)` — the first half of Sweep's equivalence pair
`3 == 19`. The run's own statistics say where it came from:

```
c sweepemit   94 binaries, 0 not-rup, 0 general pairs, 0 strip units cited
```

All 47 emitted equivalences took the S-d RUP fast path; the captured-refutation
general path emitted none (every pair declined `forward-capture-missing`).

## 2. Cause

`sweep_emit_equiv` decides whether a proved equivalence may be *stated* as two
binaries by asking `congruence_replay_rup`, whose implementation
`congruence_rup_chain` began:

```cpp
if (!k.proof || !k.proof->lrat())
  return true;
```

That short-circuit is correct for Congruence's own lemma families. They are RUP
by construction, so the replay is only how their antecedent chain is *read*, and
nothing needs it when no chain is written — Kissat likewise emits those lines
directly rather than propagating before each one.

Sweep reuses the same primitive for a different job. Kitten's refutation is the
semantic certificate; RUP over the main formula is the separate question of
whether a checker can replay the line. Outside native LRAT that question always
answered yes, so Sweep emitted equivalences unit propagation cannot re-derive.
The counter meant to make this visible, `sweep_not_rup`, could not move outside
LRAT for exactly the same reason — which is why a run whose very first fast-path
pair was unverifiable still reported `0 not-rup`.

Two further callers shared the hole, each with the required contract already
stated in its own comment: `sweep_emit_unit` and HyperBinaryResolution's `emit`
("the replay is both the RUP test and the chain source ... declined rather than
emitted unchecked").

Direct measurement, not inference: with the replay forced under DRAT in
observe-only mode — decision unchanged, so the trajectory is identical — the
solver reported **32 of its 94 emitted binaries as not RUP**, the 32nd being
`-3 19`, the lemma `drat-trim` rejects.

## 3. Repair

`congruence_replay_rup` / `congruence_rup_chain` take an explicit purpose:

| purpose | replay | chain |
|---|---|---|
| `RupReplay::ChainOnly` | only when a chain is written (native LRAT) | yes |
| `RupReplay::Decide` | every proof format, and with no proof | only under LRAT |

Under `Decide` the reason walk is skipped outside LRAT — it cites `unit_ids`,
which no other format fills in, so walking there would put the format's line
terminator into a chain — and the replay's verdict is returned directly. The
`chain.empty()` refusal stays LRAT-only: it is a rule about writing an LRAT
line, not part of the RUP verdict, so it is the one place the two formats may
legitimately answer differently.

The `TACTIX_ENABLE_PROOF_SHADOW`-only `force_drat_replay` parameter is folded
into the same enum; it was the same request under a diagnostic name.

Native-LRAT behaviour is unchanged by construction: the only altered branch is
the non-LRAT one.

## 4. What the repair costs

The fix makes DRAT and proofless runs decline exactly what native LRAT already
declined. On instances that expose the defect that loss is large and visible:
the first witness (`mut-n48-m1381`, seed 990486) solved in 15.9 s before and
does not finish in 600 s after — but pre-fix **native LRAT already timed out on
it at 200 s**, so this is format parity, not a new cost. Sweep's general path
declining every pair (`forward-capture-missing`) is a separate, pre-existing
completeness question that affects LRAT and DRAT identically; it is not touched
here.

The corpus entry was chosen from a later leg precisely because it is small
enough (100 variables, 419 clauses after ddmin) to stay well inside the corpus
gate's 60 s budget on a sanitized build after the repair.

## 5. Validation

| check | result |
|---|---|
| corpus entry 026, pre-fix binary | `drat-trim did not verify (exit 1)` |
| corpus entry 026, repaired debug (ASAN+UBSAN) | UNSAT, proof verified, oracle agrees |
| full corpus, pre-fix binary | 26 entries, **1 failure** (026 only) |
| full corpus, repaired debug — DRAT / native LRAT / random-policy+LRAT | 26 entries, 0 failures each |
| corpus 004 (`--random-policy`) and 023 (`--random-policy=20262096 --lrat`) | model and lrat-check clean |
| `test_rup_admission_is_format_independent` | aborts pre-fix, passes after |
| fuzz 1200 cases, random-policy DRAT, seed range that produced 3 pre-fix failures | 0 failures; Sweep 309 cases, HyperBinaryRes 383 |
| fuzz 800 cases, random-policy native LRAT | 0 failures; Sweep 227, HyperBinaryRes 303 |
| Release / Trace / ASAN-O3 / Debug suites | see the session's commit message |

The first witness (seed 990486) is not the corpus entry and is not retained: it
is regenerable from its recorded seed, and the mechanism it exposes is pinned by
entry 026 and by the unit regression above.

# N2 incident: frozen-at-0 retired variables falsify reconstructed models

**Status:** root cause confirmed and fixed; closing-pair retake pending.
**Defect window:** commits `7a457cf..25f1cd7` (introduced by `7a457cf`).
**Void windows:** `20260808-120257-C-portsfin`, `20260808-120325-B-portsfin`
(both carry `CORRECTNESS-BUG`; their performance numbers stay unusable).

## Incident

The closing Tier B/C pair built from `25f1cd7` produced three invalid SAT
models — an original input clause falsified by the printed model:

| instance | falsified clause | BveBackward | BveBackwardUnit | Factor |
|---|---|---|---|---|
| tierC/EDP3-11000 | 35954 `(-15227 -4231 15229)` | 24 851 | 0 | 2 718 |
| tierB/circuit_48in64out…seed4 | 53 `(-9 -10 28 39 49 -50 -51 -52)` | 7 783 | 0 | 5 |
| tierB/PRP_30_38 | 1004994 `(31966 -31965)` | 979 464 | 1 200 | 11 113 |

The same three instances passed independent model checks in the clean
windows `20260808-094434-C-ablfix` / `20260808-094512-B-p3fix` built from
`eece089`, with comparable rule activity (PRP clean: BveBackward=710 671;
EDP3: Factor identical at 2 718). `git merge-base` confirms `eece089`
precedes all five code commits in the range: `7da23a2`, `7a457cf`,
`15daf5c`, `baa8b9e`, `25f1cd7`.

## Root cause

`7a457cf` retired BVE-eliminated pivots (`VarStatus::Eliminated`,
`unassigned_active--`), so SAT is declared on `unassigned_active == 0` and an
eliminated variable can reach model extension **unassigned** (`values[] == 0`).
Before that commit, eliminated pivots stayed in the decision pool and always
held a definite value at SAT time.

`kernel_extend_model`'s segment test (`values[lit] > 0`) reads a value-0
variable as false in *both* polarities. Tactix pushes **both-sides** witness
segments per eliminated pivot (no opposite-phase unit segment as in
kissat/cadical); the soundness of that scheme rests on the
tautological-resolvent exclusion, which requires every non-pivot literal to
be definite at replay. With a frozen-0 partner `t`, a pos-side and a
neg-side window of pivot `x` can both look unsatisfied; the reverse walk
then flips `¬x` and re-flips `x`, leaving the neg-side original clause
falsified. `print_model` compounded it by printing value-0 variables as
**true** (`values[pos] >= 0`) while `tactix_value` reads them as false.

Two confirmed manifestations, each with an executable trace
(`tests/test_reconstruct.cpp`, `tests/test_bve.cpp`):

1. **Direct BVE double-flip** — toy `(x∨t), (¬x∨¬t)`: eliminating `x` drops
   the tautological resolvent, retires `x`; `t` loses all occurrences and is
   never assigned. Replay double-flips `x`; the EDP3/circuit shape.
2. **Substitute pair with representative later eliminated** — `l ≡ s` pushes
   the symmetric pair; if `s` is later BVE-eliminated with rest-satisfied
   windows, `s` stays 0 and replay forces `l` false while `s` prints true,
   falsifying `(l ∨ ¬s)` — the PRP shape `(31966 -31965)`, a classic SCC
   pair of adjacent variables.

`BveBackward`/`BveBackwardUnit` growth was a scheduling side effect of the
repair commits, not the mechanism; the binary backward arm (`25f1cd7`) was
reviewed and adversarially verified sound.

## Fix

- `kernel_extend_model` (src/kernel/kernel.cpp): complete every value-0
  variable to definite false **before** the reverse replay. Retired
  variables occur in no live clause, so the completion keeps the active
  formula satisfied and restores the total-assignment premise of the
  both-sides replay argument (docs/calculus.md §6c).
- `print_model` (src/main.cpp): strict `> 0`, agreeing with `tactix_value`.
- Regression tests: `test_frozen_partner_no_double_flip`,
  `test_substitute_repr_later_eliminated` (kernel-level, both traces),
  `test_eliminate_frozen_pivot_reconstruction` (through the real BVE apply).
  All three fail on the pre-fix tree and pass with the fix.

## Fuzz vacuity finding

Every fuzz log in `tools/fuzz/` shows `BveBackward=0!` — all five batteries
cited by the repair commits never executed `bve_backward`, because the
stress/random drivers and trace replay never connect the dense occurrence
index (`bve.cpp` guard `if (!k.dense) return`). The dedicated
`slurm-hunt-bwdunit` leg could not reproduce a *known* backward bug for the
same reason. Fix: the stress driver connects dense for half of all seeds
(derived from the seed so traces replay identically); backward-BVE evidence
batteries must pass `--require-reached BveBackward`.

## Validation

- Full debug ctest suite green with the fix (59/59).
- Post-fix fuzz battery and the closing Tier B/C retake are recorded in
  PROGRESS and the follow-up report when they land.

# `mp1`: Vivify implied-candidate frontier

**Date:** 2026-08-10

## Result

The normalized Tactix/Kissat search trajectory remains identical through
conflict 2504.  The conflict-2505 learned-clause mismatch is caused by a
different reason retained after the first search-time Probe, not by branching,
HyperBinaryRes, HyperTernaryRes, sparse watch reselection, or Vivify's
equal-count ordering tie.

At conflict 2503 Tactix scans and uses the learned clause
`(-12435,-7821,-3491,-3489,-7)` to imply `-12435`.  Kissat no longer has that
clause and instead uses `(-12435,-10193,-10192)`.  Binary DRAT decoding locates
the long clause at Kissat proof add event 2817 and deletion event 3377, inside
the first search-time Vivify.  Tactix adds the corresponding clause but retains
it.  Conflict 2505 is the first analysis that observes the different reason.

The missing behavior is therefore not a standalone deletion test.  It is the
complete Kissat Vivify deduction unit (`kissat/src/vivify.c:472-638,1078-1200`):

1. select the earliest satisfied candidate literal on the current trail;
2. analyze its complete reason graph while tracking whether a redundant reason
   was used and whether a reason clause subsumes the candidate;
3. classify the derived clause with `vivify_shrinkable` using analyzed flags,
   retained decisions, unassigned literals, and the number of implied
   literals; then
4. dispatch, in order, to subsumption/promotion, strict rewrite,
   independently-implied redundant deletion, asymmetric-tautology deletion, or
   instantiation/no-op.

Tactix currently has only the strict-rewrite paths and a separate Instantiate
rule.  `SP-VIVIFY-DEDUCE-1` remains open until the whole coupled state machine,
including proof/model transitions and counters for every terminal arm, is
ported and tested.  No local positional proxy is retained.

## Excluded roots

| experiment | trace result | production result | disposition |
|---|---|---|---|
| exact Kissat equal-count comparator | mismatch remains at conflict 2505 | 85.72 s | rejected |
| comparator + in-place candidate permutation + rewatch | mismatch remains at conflict 2505 | 53.20 s | rejected |
| flush a root-satisfied binary during sparse collection | mismatch remains at conflict 2505 | 39.68 s vs Kissat 12.20 s | rejected |

The two Vivify-order experiments are jobs `1606205`-`1606208`; the binary-flush
experiment is jobs `1606209`/`1606210`.  Every complete run produced a verified
SAT model and proof.  These changes are neither the causal root nor strictly
better, so they were removed instead of becoming arms.

## Rejected partial ports

Three proof-safe implementations were tested and removed:

| implementation | `mp1` vs parent | `tr-n280` vs parent | `mod4` vs parent | reason rejected |
|---|---:|---:|---:|---|
| probe-position proxy for non-shrinkability | 4.61 vs 21.76 s | 16.85 vs 13.91 s | timeout vs 263 s | proxy omits deduction state |
| generic reason walk + derived-size test | 55.04 vs 21.70 s | 15.45 vs 13.94 s | 41.80 vs 263.19 s | thousands of encounters misclassified; not Kissat's deduction state machine |
| reason walk with deletion restricted to late eager implications | 68.65 vs 21.94 s | not rerun | 146.14 vs 263.69 s | local restriction remains an approximation and regresses the target |

Jobs `1606219`, `1606224`-`1606226`, `1606228`-`1606232` contain the matched
comparisons.  All completed candidate and parent answers verified.  The first
proxy also improved `tr-n260` and `tr-n300`, but those downstream wins do not
justify shipping a semantically incomplete Vivify arm.

## Cumulative-guard discovery

The matched parent exposed an earlier-fixture regression independent of the
Vivify experiment.  Commit `95bcea4`, the closed `mod4` pin, solves the fixture
in 10.96 s with 236,264 conflicts.  Commit `c5a98e4`, which precedes the sparse
watch repair, takes 261.40 s and 5,028,372 conflicts; both SAT models and proofs
verify (job `1606235`).  Thus `3acd5b9`'s sparse watch repair is not the source:
the regression entered in the bundled `mp1` convergence increment.

The first old/new semantic difference is inside preprocessing Sweep at seed
171.  The corrected embedded solver invalidates its SAT model when a subsequent
solve under assumptions returns UNSAT; the old pin incorrectly lets
`flip_model_literal` inspect that stale model.  Trace job `1606245` records the
boundary.  A same-source diagnostic rollback of only this lifetime rule is
terminally neutral—260.77 s and 5,028,372 conflicts on both sides in job
`1606256`—so the first semantic difference is not by itself the performance
root.  The stale behavior still cannot be restored: Kissat's `kitten_status`
has the corrected lifetime, and the shortcut is not a stronger mechanism under
the same correctness contract.

Fresh Tactix/Kissat tracing then localized the complete coupled boundary.  Both
process the same seed sequence and seed 171 reaches the same 256 variables in
the same BFS order.  Kissat encodes 1,024 clauses; Tactix encodes 1,023 because
it deduplicates the equal-depth reverse encounter of binary `(-3,-5)` while
Kissat retains both directed encounters.  Porting the exact depth-sensitive
construction makes the complete environment hash match
`d87ab864bb8914b4`, but the initial embedded SAT models still differ.  On the
identical environment Kissat performs 13 solves/13,793 ticks, beginning with
backbone candidates `-297,-282,-5`; Tactix performs 19 solves/13,862 ticks,
beginning with `-9,297,223,278,254,-6,-1,-5`.  Jobs `1606278`-`1606304` contain
the clause and query traces.

The environment-only port was therefore removed.  It leaves `mod4`'s terminal
trajectory unchanged (264.22 versus 264.94 s against the parent) and regresses
`mp1` from 21.73 to 65.07 s (jobs `1606290`/`1606310`).  The implementation
unit is `SP-SWEEP-KITTEN-1`: depth-sensitive environment construction plus the
complete Kitten-compatible initial model, phase/decision order, flips,
backbone/partition query state, and effort/resumption handoff.  This cumulative
guard precedes further `mp1` Vivify implementation.

## Reopening and completion conditions

- `SP-VIVIFY-ORDER-1` reopens only when a fixed fixture localizes its first
  meaningful divergence to the comparator or arena-order edge and cumulative
  evidence reverses the measured loss.
- `SP-VIVIFY-DEDUCE-1` closes only with the complete deduction semantic unit,
  focused tests for every terminal arm, per-arm reach counters, DRAT/LRAT and
  SAT-model checks, a target-clause deletion trace, and cumulative guards over
  all previously closed P4-CVG fixtures.
- `mod4` closes again only when `SP-SWEEP-KITTEN-1` lands as a complete unit,
  the corrected model lifetime is retained, and the divergence from Kissat is
  terminally matched or localized with a valid strict-strength argument; the
  10.96-second old pin is diagnostic evidence, not an admissible baseline
  behavior.

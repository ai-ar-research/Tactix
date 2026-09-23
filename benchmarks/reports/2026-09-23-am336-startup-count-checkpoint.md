# AM336 — startup count-preservation implementation checkpoint

This is an ungated checkpoint, not promotion over AM333 and not completion of
the active goal. The approved startup-PBP contract now has a general counting
action in addition to parity. Two independent N2 failures reported by Claude
block promotion; all four combined-candidate regression gates remain due.

## Mechanism and provenance

Shared Lucky instance 20 recognizes complete exactly-one selector rows and
paired ternary guarded signed equalities. Complete stars form a bipartite
network; internal leaves cancel and unit-fixed unequal boundary counts refute.
Fresh selector-and-leaf conjunctions provide shared flows. Selector ALO gives
lower bounds; pairwise AMO and integer rounding give upper bounds. Summation
eliminates internal flows and leaves. This is polynomial cutting-plane reasoning,
not the owner's exponential enumeration of involutive matchings or its exact
generator census. Signed renaming and clause order do not define recognition.

The action is public/API-selectable at the original-input boundary and uses
the shared kernel work budget. Discovery and certificate preflight are
interruptible; failed discovery changes no formula. Automatic proof publication
is terminal and atomic. Explicit clausal modes decline. Original raw literals
are kept only until that boundary closes, preserving clauses simplified by
input units. This allocation's regression cost remains an open gate obligation.
Semantics and engineering restrictions are owned by calculus S5 and the
implementation notes, not this report.

Immutable campaign: `benchmarks/runs/20260923-am336-startup-count`.

- Base commit: `f0ad5a8ec5576ee9c91dfef95ea6ba4ca1ec5389`.
- Source patch SHA256: `6aa5af9a41330790e168f1390a75b77781323cacb1844d24b1f55c896b6246ba`.
- Release SHA256: `8aa84a97c73b15c1f2a323f8b33deeceb77954ad04409ef5e8ac11e54c3c0628`.
- Debug SHA256: `dba05ba74f6529406e8b9b3042db908d77bf27b0286ac291fe2b6a353e23a47e`.
- Debug uses `-g -O1 -D_GLIBCXX_ASSERTIONS`. Source archive, complete patch,
  original-CNF hash, runners and DSR/DRAT/PB checker hashes are in the manifest.

## Diagnostic evidence

Fmla job `1715952`, one full-width SLURM array row on `cpu-dense-hwulab-q`, no
node constraint, 12 allocated CPUs, 32 GiB, solver pinned to one CPU, 600-second
solve and 14,400-second checker limits. The provisional result records:

| quantity | observed |
|---|---:|
| solver wall seconds | 3.322 |
| proof bytes | 388,840,230 |
| peak solver RSS KiB | 384,800 |
| admitted stars | 229,376 |
| count-rule work | 20,036,944 |
| PB additions (including subproof identities) | 10,641,137 |

Independent VeriPB/CakePB checking is **pending at this checkpoint**. Solver exit
20 alone is not acceptance. The frozen owner comparison/proposal records
436,975,142 bytes for LymphoSAT and 4,503,640,567 bytes for AM333 clausal DSR on
this row. These retained cross-campaign observations are not a matched-window
performance gate. No reference was rerun.

AM335's retry job `1715866` has now ended, completing the previously pending
three-row certificate diagnostic. All results have `valid=true`, with both
checker stages successful against original CNF:

| Tseitin row | solve s | proof bytes | VeriPB s | CakePB s | total check s |
|---|---:|---:|---:|---:|---:|
| grid250 | 0.817 | 103,104,441 | 39.280 | 113.494 | 152.775 |
| d3 n100000 | 0.617 | 73,298,895 | 59.611 | 307.171 | 366.783 |
| grid400 | 2.070 | 273,035,686 | 219.414 | 897.949 | 1117.364 |

Authority: `...-am335-startup-parity-retry/results/000.json` through `002.json`,
AM335 v1 Release `94cd4710...`. These are not results for AM336's combined pin.
The earlier frozen AM335 checkpoint report is unchanged.

## Validation and failures

Five focused tests pass: kernel, parser, proof writer, count rule, and independent
count certificates. Count coverage includes width-3/4 two-layer networks,
signed renaming, variable/clause/literal reversal, equal-count SAT controls,
API enumeration/application, strict DSR/DRAT/LRAT declines, stop/work-budget
no-mutation controls, and permanent boundary closure through ordinary actions.
Four API and four default CLI proofs pass both PB checkers; no-proof verdicts
agree. Duplicate literals/clauses and tautologies also verify against raw CNF;
a missing-derivation PBP is rejected.

The full 175-entry Debug-O1 CTest run is active: 174 passed at reporting time,
including Lucky, micro and Sweep proofs with immutable AM333 DSR checker pins;
the scripted-default demo remains. The preceding O0 suite also still
has its long-running scripted-default demo; its earlier fixture/checker-path
failures have passing reruns, as recorded by AM335. Do not replace pending test
results with inferred passes. All documentation checks pass, with reserve
warnings; no documentation compaction was performed.

Claude's AM335 owner supplied a bounded interim handoff in
`...-am335-startup-parity-claude-fuzz/REPORT.md`. Its reported findings are:

1. Random-policy XOR runs produce a rejected native DSR certificate after
   ordinary inprocessing; Lucky 19 did not fire. The owner shrank a case to
   17 variables/41 clauses, seed `335110095`, with `min.cnf`, `min.trace`, proof
   and checker diagnostics in `triage/reproducer/`. The owner reports the same
   failure on AM333 and both AM335 pins, not an AM335 regression.
2. Text writers (`--lrat`, `--no-binary`) print UNSAT on `/dev/full`; binary
   writers abort. The owner also reproduces this on AM333.

These failures remain open and block promotion despite their pre-existing
classification. Codex read only the owner status/report and the designated
bounded handoff, not campaign internals. Other AM335 owner legs remain active.
A separate Claude process owns focused AM336 count testing from
`...-am336-startup-count/claude-fuzz-request.md`; its campaign is
`...-am336-startup-count-claude-fuzz`. This does not substitute for broad
combined-candidate acceptance after repairs.

## Next and hygiene

Repair both handed-off N2 failures, finish independent Fmla checking and focused
count fuzz, then have Claude validate a repaired combined immutable candidate.
Run Rank 1--50, B28+C5, WV100 and WV2-89 with all required checker pins and the
maintained routed runner. Only afterward pursue hcp/6g matched-work timing.
Excluded rows were not reopened. No performance dosing or filename dispatch
was introduced. No additional material was deleted: active campaigns, pins,
bounded failures and fixed test artifacts are retained; original CNF/proof
diagnostic data use the runner's anonymous scratch lifecycle.

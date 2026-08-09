# Analyze minimization-depth parity

**Date:** 2026-08-09

**Gap:** `ANALYZE-MIN-DEPTH-1`

## Source comparison and strength judgment

Kissat ships `minimizedepth=1000` (`options.h:82`) and rejects a recursive
minimization node when `depth >= 1000` (`minimize.c:102-103`).  Its
`minimize_binary` loop (`minimize.c:58-94`) traverses an arbitrary run of
binary reasons iteratively: those links neither consume the recursive depth
budget nor consume the C call stack.  CaDiCaL also ships 1000
(`options.hpp:157`), but rejects only when `depth > 1000` and recursively
counts every reason (`minimize.cpp:12-52`).  The references therefore differ,
and the project tie-break selects Kissat.

Tactix used an unbacked default of 900, rejected only above it, counted every
binary link recursively, and retained `TACTIX_MINIMIZE_DEPTH` as a comparison
arm.  None of those differences was strictly stronger.  An earlier rejection
can only retain a learned literal whose complete reason walk would prove
redundant; it cannot produce a strict subclause.  The binary recursion also
carried avoidable native-stack risk.  Prior probe `1570502` found 900, 1000 and
100000 byte-identical on its selected workload, so the arm's attribution
question was already closed.

## Repair

The authored analysis constant is now 1000 with the exact `>=` boundary.
Binary reasons use a reusable kernel scratch stack and publish removable or
poisoned state deepest-first, preserving Tactix's cleanup and native-LRAT
antecedent order.  If the run ends at a large reason, its children enter two
logical depths below the caller, matching Kissat's accounting.  Root
antecedents remain captured for LRAT.  `minimize_depth_cutoffs` is the reach
counter, printed as `c min-depth`; the environment arm and its production
parsing overhead are gone.

## Verification

SLURM job `1605932` pins all three relevant boundaries:

- a 1,100-link binary chain reaches the learned anchor, removes the literal,
  and records zero recursive cutoffs;
- an 1,000-link large-reason chain admits its deepest node at depth 999 and
  removes the literal;
- an 1,001-link large-reason chain rejects its deepest node at depth 1000,
  records exactly one cutoff, and retains the literal.

The assertion-enabled focused test and all 63 Release tests passed.  The
production binary is `build/pin-kissat-min-depth/tactix`, SHA-256
`470c63e69e5c0a2c256adbea28e19e8e4c6a70dfa8dc55f68b2f444e6cee2e4f`.
It contains neither event-trace code nor the removed environment-arm string.

Job `1605933` compared only the remaining diagnostic Tier-A case.  The prior
and repaired Tactix binaries both produced 815,632 conflicts, 1,279,693
decisions, 48,240,090 propagations, 815,092 learned clauses and a 47,118,870
byte proof.  Their 29.05 and 28.63 second runtimes are not treated as a
performance difference.  Kissat took 22.64 seconds with 927,770 conflicts.
All three DRAT proofs verified.  Thus this required parity repair is neutral on
the selected trajectory and does not explain the remaining per-conflict cost
gap; continue by extending the semantic trace on `tr-n300-s055` alone.

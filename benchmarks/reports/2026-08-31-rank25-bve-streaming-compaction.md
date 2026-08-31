# Rank 25 BVE streaming-compaction repair

Date: 2026-08-31
Status: mechanism and decision-grade timing complete; remaining cumulative
proof checks continue asynchronously

## Objective

Continue from the accepted Rank 33 Factor repair without reverting any general
lifecycle correction or the Kissat creation-order default.  Localize the
largest new homogeneous loss, repair only a general Kissat mechanism, preserve
N1--N4 and proof/model validity, and carry the same Release binary through
Rank 33, B28+C5, and homogeneous Rank 1--40 validation.

## Exact cost localization

Rank 25 is
`tierD24-26/180e6d8b61fe6bab26126b78591e324e-baseballcover11with22_and2positions.cnf.xz`
(SHA-256 `32011f48...d528`, expected SAT).

Historical campaigns `3760` and `4270` had the exact same search and BVE work:
1,031,005 decisions, 296,976 conflicts, 3,254,966,922 propagations,
3,296,386 BVE applications, 566,514 eliminated variables, and 32,958,556
resolution attempts.  Wall time nevertheless changed 270.955→760.379 s and
BVE time 13,803→509,433 ms.

Matched campaign `20260831-4290-tierd-rank25-bve-unit-cost`, job `1657922`,
confirmed the same boundary in one window:

| Arm | Wall s | BVE ms | Model | Cleanup |
|---|---:|---:|:---:|:---:|
| previous | 263.337 | 13,779 | verified | clean |
| current | 755.590 | 507,738 | verified | clean |

Every listed work counter remained exact.  The discrepancy was therefore BVE
implementation unit cost, not admission, schedule, effort, or search.

## Source mechanism and discriminating ablations

Pinned Kissat `resolve.c::occurrences_literal` compacts the inspected mixed
watch prefix with read/write cursors.  It deletes satisfied binaries and stale
large references only before the first live clause above `eliminateclslim`,
retains that oversized occurrence and its suffix, then inspects the opposite
polarity after the first mutation is complete.

Tactix already matched those semantics, but implemented stale-large cleanup as
one `find` plus `vector::erase` in both its ordered and large mirrors for every
garbage reference.  That repeatedly searched and shifted the same occurrence
vectors.  The initial whole-vector snapshot was also unnecessary, but campaign
`20260831-4300-tierd-rank25-bve-cursor-diagnostic`, job `1657924`, proved it was
not material by itself:

| Arm | Wall s | BVE ms | Result |
|---|---:|---:|---|
| current | 760.644 | 507,739 | verified SAT, clean |
| cursor-only | 760.931 | 507,581 | verified SAT, clean |

The final implementation first records only satisfied binaries and garbage in
the legal prefix, applies binary deletion in encounter order, and then stably
compacts each mirror once by matching that ordered garbage subsequence.  It
therefore preserves the exact survivor order, sequential polarity mutation,
occurrence-count image, score notifications, oversized stopping boundary, and
untouched suffix while reducing cleanup from repeated erase/shift work to a
linear pass.  It does not change a guard, policy deadline, effort allowance,
candidate class, formula classification, or any benchmark-visible selector.

Single-arm causal campaign
`20260831-4310-tierd-rank25-bve-streaming-diagnostic`, job `1657926`, returned
verified SAT in 261.641 s with 14,457 ms BVE, the exact work signature, and zero
residue.  Relative to matched current, BVE cost fell 35.12x and wall time to
0.346x; relative to the pre-regression arm, wall time was 0.994x.

## Decision-grade timing

Campaign `20260831-4320-tierd-rank25-bve-streaming-waves`, job `1657927`, ran
three independent current/candidate pairs on node017 and node018 with 12 logical
CPUs and 48,000 MiB per single-threaded task.  All six SAT models verified and
all tasks deleted their artifacts.

| Wave | Current s | Candidate s | Ratio | Current BVE ms | Candidate BVE ms |
|---:|---:|---:|---:|---:|---:|
| 1 | 757.378 | 263.643 | 0.348100 | 507,474 | 14,358 |
| 2 | 754.741 | 263.154 | 0.348668 | 506,346 | 14,428 |
| 3 | 762.438 | 264.546 | 0.346974 | 507,724 | 14,394 |

The paired median is 0.348100 and range 0.346974--0.348668.  Candidate absolute
range is 0.53%; current absolute range is 1.02%, so both satisfy the 2% absolute
stability criterion.  Every pair has the exact work signature and more than
10x BVE recovery.  `summary.json` SHA-256 is
`02adcc91...2166`.

## Correctness and architectural boundary

- Focused ASan/UBSan BVE tests pass, including satisfied-binary deletion,
  inspected garbage before an oversized clause, untouched suffix state,
  occurrence counts, and partner-only score notification.
- Release passes 76/76 and Trace passes 75/75, including N1 API, proof,
  configuration, rule/policy-boundary, replay, and documentation checks.
- The production creation-order default is unchanged.  The repair is uniform
  and contains no formula, benchmark, rank, size, density, ordering,
  classification, admission, score, conflict, or effort gate.
- SAT timing claims above are backed by independently verified models and zero
  residue.  UNSAT proof checking in downstream gates is nonblocking for
  provisional timing but remains mandatory for final classification.

## Remaining gate sequence

The identical candidate pin is
`c18133ceea5c1ac5ee676387ef4c4f1e0d6f57e12011baab6b8940ed292cab00`.
Rank 33 campaign `4330` is valid and clean at 467.925 s with 708,436 KiB peak
RSS and 29,767 ms BVE, versus the prior checked 468.533 s.  Its
2,137,360,540-byte DRAT verified independently in 2,969.817 s before the
certificate and derived artifacts were deleted.
The full B28+C5 paired guard `4340` has all 66 expected solver verdicts at
2,003.260/2,010.711 s candidate/current (0.996294x) with equal peak memory;
all 66 rows are valid and clean, including 17 verified SAT models and 16
verified UNSAT proofs per arm.  The T105 proofs took 3,420.707/3,567.190 s for
shipped/candidate, and the final audit found zero residue.

Homogeneous campaign `4350`, job `1658000`, has all 40 expected solver verdicts
in 10,266.505 s: SAT 4,585.064 and UNSAT 5,681.441.  Total ratios are 0.946336x
current, 0.914783x accepted, and 1.337614x pinned Kissat; geometric-mean ratios
are 0.970442x, 0.926970x, and 1.325358x.  Rank 25 is 265.403 s versus current
760.379 and Kissat 253.813; Rank 38 is stable at 790.264 versus current
789.986.  All 22 SAT models and twelve UNSAT proofs are verified; six UNSAT
proof checks remain active.  At the user's direction they continue
asynchronously and are not blocking this checkpoint commit; their final records
and residue audit remain required follow-up evidence.  This increment
materially improves the homogeneous candidate but does not yet match Kissat
overall, so the persistent convergence goal remains open after this checkpoint.

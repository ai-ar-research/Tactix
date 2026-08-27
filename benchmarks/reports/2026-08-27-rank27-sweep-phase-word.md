# Rank-27 Sweep phase-word expansion

## Outcome

The accepted Rank-27 candidate replaces `SubSolver::randomize_phases()`'s
little-endian scalar 64-byte phase expansion with the eight masked word values
used by upstream Kitten, copied through alignment-safe `memcpy`.  The scalar
mapping remains the fallback on other byte orders.  Generator draws and values,
phase bytes, Sweep/Kitten ticks, candidate and decision order, scheduling,
learning, pruning, proof behavior, and N1--N4 are unchanged.

The source candidate is based on `ec6ef1c`; its isolated solver diff SHA-256 is
`263cd847300c64764fe73d04cdcc78faf1b6b77ea1ca871458ddd65fd29e4463`.
The final Release pin used for the complete guard is
`a76182a0...e85d6`.  No benchmark identity, rank, deadline, policy arm, or
tuned strategy constant enters the solver.

## Exact common-prefix evidence

Frozen campaign `benchmarks/runs/20260826-2320-rank27-sweep-phase-word-prefix/`
ran predecessor and candidate as separate tasks in array `1650366` on node018
with `-c 12`, exactly 48,000 MiB scheduler memory, and the 16 GiB process cap.
Both intentionally stopped at conflict 9,450 and reproduced normalized semantic
SHA-256 `3b87bb36da711bcef3ca877f3922e9fa5f8a3c6ae9d638c1ae18d4965bd92c5f`.
The changed owner fell from 849,084,660 to 150,625,144 sampled cycles
(-82.26%); complete prefix cycles fell 49,883,395,824 to 49,315,979,966
(-1.1375%).  Sweep fell 910 to 744 ms at the exact same 41,351 solves and
37,087,279 ticks.  perf lost no samples and cleanup/certificate residue was
zero.

## Decision-grade Rank-27 timing

Campaign `benchmarks/runs/20260826-2340-rank27-sweep-phase-word-timing/`
used three paired waves, six independent one-solver tasks in array `1650368`,
on an otherwise empty node018.  Each task used `-c 12`, exactly 48,000 MiB,
the 16 GiB process cap, and proof-off Release.  Every arm returned UNSAT with
the exact full normalized non-time SHA-256
`10372d94d2cb1ae86893408435e0c3ffa2338053b11b7012c2107a3bebf1ffd0`.

Candidate/predecessor internal seconds were 358.542/358.587,
357.916/358.759, and 357.941/359.460.  The candidate won all three waves;
the median paired ratio is 0.997650 (-0.2350%), with complete ratio range
0.995774--0.999875.  Candidate and predecessor ranges are 0.1749% and
0.2433% of their medians, so the absolute observations satisfy the 2% stability
rule and every candidate run is below 400 seconds.  Median cycles fall 0.2414%
and instructions 0.4084%.  Certificate residue is zero.

## Complete B28+C5 gate

Campaign `benchmarks/runs/20260827-0010-BC-sweep-phase-word-guard/` used the
frozen 28 Tier-B plus fixed five Tier-C matrix as a full-width, unthrottled,
unconstrained 66-task array `1650774`, followed by aggregate `1650840`.  Every
task ran one solver with `-c 2`, 16 GiB, and the 1,200-second contract.  The
aggregate passed 66/66: each arm independently checked 17 SAT models and 16
UNSAT proofs, with no invalidity, solve loss, malformed/missing result, cleanup
failure, or certificate residue.  Candidate/predecessor PAR-2 is
1,362.562/1,361.275 seconds (1.000945x), within the 1.10 guard.  Summary SHA-256
is `ac59145977a044af299d703c24ea5dc84bd25c25ded4ba85294891c5d6e4cae1`.

## Claude-owned fuzz authority

Claude Code exclusively prepared, submitted, monitored, inspected, and
classified `benchmarks/runs/20260827-0040-sweep-phase-word-fuzz/`; Codex did
not inspect its fuzz artifacts.  Claude's terminal verdict is PASS.  Prep job
`1651241`, generated legs `1651243`, `1651244`, and replacement random leg
`1651247`, plus reach job `1651246`, used immutable harness snapshot SHA
`f36a116d...6875`.  Each generated leg requested 50 physical cores (100 logical
CPUs), 400,000 MiB, and 50 workers.

Claude reports 81/81 corpus replays and 30,000/30,000 generated cases with zero
failures: 13,914 SAT models independently checked, 10,955 native LRAT proofs
accepted, and Kissat agreement on all 24,869 verdict cases.  The 5,131 bounded
five-second nonterminations carry no oracle claim.  Direct reachability saw the
changed little-endian word expansion in 12 oracle-clean gdb cases, the
`sweep_solve` call site in 24, and the unchanged scalar tail in 14.  There were
zero sanitizer, crash, invariant, model, proof, or differential failures and
zero certificate/scratch residue.

The campaign also repaired two general fuzz-harness defects without changing
solver behavior, seeds, cases, or oracles: snapshot-executed `fuzz.py` resolves
the live repository through `TACTIX_REPO_ROOT`, and `slurm.sh` restores owner
write permission before removing a successful read-only snapshot.  The latter
explains two post-oracle cleanup exits; replacement leg `1651247` validates the
repaired success path end to end.

## Limitations and next action

The end-to-end gain is intentionally small: this closes a source-backed equal-
work representation gap, not broad Rank-27 convergence.  Per project-owner
direction, Rank 27 is now parked.  After promotion, freeze the accepted Tactix
pin and the accepted current-Kissat comparator, evaluate ranks 1--40 without
rerunning immutable scoring references, and select the validated UNSAT instance
with the largest meaningful Tactix/Kissat slowdown for the next exact
worse-side investigation.

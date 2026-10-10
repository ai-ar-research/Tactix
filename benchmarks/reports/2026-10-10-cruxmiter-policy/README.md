# Cruxmiter public-policy experiment

## Result

The selected policy schedule improves the complete fixed size-32 cruxmiter
family, while using only the public Tactix interface. At a 600-second cutoff,
with PAR-2 for timeouts, the ten-seed screen produced:

| arm | solved | PAR-2 sum (s) | mean (s) |
|---|---:|---:|---:|
| DefaultPolicy | 5/10 | 7799.134 | 779.913 |
| final policy | 7/10 | 5507.825 | 550.783 |
| Kissat 4.0.4 | 3/10 | 9318.992 | 931.899 |

This is a 1.42x aggregate speedup over DefaultPolicy, 1.69x over Kissat, and
1.25x over the per-instance faster of those two baselines. Seed 0 and seed 1
are byte-identical. Counting that formula once gives 1.53x over DefaultPolicy,
1.88x over Kissat, and 1.32x over the faster baseline.

The strongest independently checked result remains
`cruxmiter32seed7.cnf`: 42.328 seconds, versus 176.731 seconds for
DefaultPolicy and 115.682 seconds for Kissat. All three UNSAT certificates
were independently verified. The policy is 4.18x faster than DefaultPolicy
and 2.73x faster than Kissat on that instance.

The aggregate gain is useful, but it is not the requested several-fold gain
over the whole family. It is concentrated on some seeds and leaves seeds 0,
1, and 4 unsolved at the fixed cutoff.

## Final policy

The selected order is:

1. propagate at the root;
2. run at least two bounded `Congruence(4) -> Substitute(24)` rounds;
3. allow rounds three and four only while the public congruence-merge or
   substituted-variable counters show new structural progress;
4. run one bounded startup `Backbone` dose, propagating every mutation;
5. enter `tactix_solve` in stable mode, retaining its target capture, mixed
   phases, Walk, rephasing, restarts, reductions, and heap decisions;
6. schedule recurrent Vivify at 200 per mille, while disabling Reorder, BVE,
   Factor, Sweep, conflict-clause reuse, and repeated Backbone.

This differs from DefaultPolicy in both startup order and recurrent
inprocessing. `Congruence` immediately feeds `Substitute`, the cheap pair is
given a guaranteed second pass, and further passes are conditional rather
than fixed. DefaultPolicy's general scheduler interleaves more rules and
continues several passes that were unproductive on these circuit miters.

The policy does not apply symmetry breaking. Public graph-symmetry profiles
were tested, but did not beat the smaller gate-closure schedule. There are no
filename, seed, or formula-hash branches in the selected path.

The implementation is policy composition only. It includes `tactix.h`, uses
registered public rules and public statistics, and changes neither the kernel
nor any proof rule.

## Why this inprocessing mix

The CNF and full seed-7 trace identify an acyclic signed-AND circuit with a
small comparison tail. The useful operations respect that construction:

- `Congruence -> Substitute` is a producer/consumer pair: equivalences found
  from gates are collapsed before another structural pass observes the CNF.
- Two rounds are a cheap family-wide baseline. A fixed third round sometimes
  helps but sometimes loses, so later rounds are earned by public progress
  counters and capped at four.
- One startup Backbone pass can expose constants after gate collapse.
- Vivify was productive in all 37 scheduled passes in the winning seed-7
  trace and dominates the useful recurrent inprocessing.
- Factor made progress in 0/29 observed calls, repeated Backbone in 1/75,
  Sweep in 2/11, and Strengthen in 1/14. Their recurrence cost more than the
  clauses they removed.
- A proof-valid greedy BVE prefix took 73.4 seconds where the chosen seed-7
  policy took 42.3 seconds. Reorder and BVE also discard useful authored gate
  and import order, so both are disabled in the final schedule.

The CNC+TACO `stable=2,target=0,phase=0` observation was useful as a search
direction, but the literal tuple did not transfer intact. On seed 7, Kissat
with `stable=2,target=0,phase=0` timed out at 120 seconds, while
`stable=2,target=1,phase=0` took 41.077 seconds. The final Tactix policy keeps
stable search but retains target capture and rephasing; a global phase-zero
policy lost because the successful trace used mixed decision phases.

## Fixed-family evidence

The 600-second screen timings are:

| seed | DefaultPolicy | final policy | Kissat |
|---:|---:|---:|---:|
| 0 | timeout | timeout | timeout |
| 1 (duplicate of 0) | timeout | timeout | timeout |
| 2 | 259.224 | 234.685 | timeout |
| 3 | timeout | 284.940 | 342.577 |
| 4 | timeout | timeout | timeout |
| 5 | 446.563 | 124.625 | timeout |
| 6 | 418.872 | 297.108 | 460.713 |
| 7 | 177.571 | 42.445 | 115.702 |
| 8 | 496.904 | 482.533 | timeout |
| 9 | timeout | 441.489 | timeout |

The screen wrote proofs for timing parity and then removed them; those screen
proofs are not called verified. Separate scored verification runs establish
the certificate claims below.

## Validation and artifacts

The ten-seed screen used an earlier adaptive implementation that could stop
after its first empty gate-closure round. The cleaned final implementation
guarantees the inexpensive first two rounds, then uses the same public-counter
stop rule for optional rounds three and four. The verified seed-7 run already
used the fixed two-round schedule. Thus the complete-family table is evidence
for the selected schedule, while the exact cleaned binary was rerun and
checked on seeds 5 and 9. Its frozen SHA-256 is
`7affc558d29b4f8eac69d128d1578ea6d7e23386326c1f64df6040d314f11edc`.
On seed 5 it solved UNSAT in 128.226 seconds and its 311,139,427-byte binary
DRAT certificate verified in 208.333 seconds. This is 3.48x faster than the
matched 446.563-second DefaultPolicy result. On seed 9 it solved UNSAT in
452.370 seconds and its 931,038,530-byte binary DRAT certificate verified in
858.615 seconds. DefaultPolicy and Kissat both timed out there, giving the
final policy a 2.65x PAR-2 advantage at the declared cutoff. The two-task
verification aggregate is correctness-clean, with no mismatch, invalid
certificate, checker exhaustion, or unverified scored result.

Primary artifacts are:

- policy source: `examples/cruxmiter_policy.c`;
- campaign harness: `tools/bench/cruxmiter_seed7_search.py`;
- fixed ten-seed screen:
  `benchmarks/runs/20261010-154212-crux-fixed-family-screen/`;
- cleaned-policy scored verification:
  `benchmarks/runs/20261010-163156-crux-fixed-family-verify/`;
- seed-7 checked comparison:
  `benchmarks/runs/20261010-140841-crux-seed7-verify/`;
- complete decision/inprocessing trace:
  `benchmarks/runs/20261010-crux-seed7-rbc-trace/`.

All final campaigns used the HWU partition with four requested CPUs and
16 GiB per task. Proofs and checker expansions live in task-local temporary
directories. Normal return, solver timeout, Python exceptions, SIGTERM, and
SIGINT remove those directories, so timed-out jobs do not retain proof files.

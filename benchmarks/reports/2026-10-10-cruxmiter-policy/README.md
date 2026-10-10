# Cruxmiter public-policy experiment

## Dedicated size-32 seed-7 follow-up

The later seed-7-only search produced a substantially faster public-API
policy.  On `cruxmiter32seed7.cnf` (SHA-256
`042bfac2783188c773ea8e2d26952a36f512ad3a7bd6821e92b128a0f40a0457`) it
solves UNSAT in **42.328 s** and its 101,573,080-byte binary DRAT certificate
is independently verified in 63.995 s.  The earlier checked references are
177.189 s for stock Tactix and 115.931 s for Kissat 4.0.4, making the policy
4.19x faster than stock Tactix and 2.74x faster than Kissat.  This is the best
checked crux result from the experiment, but it does not meet the requested 5x
speedup against the faster baseline.

The selected order and schedule are:

1. `Propagate`;
2. two bounded `Congruence(4) -> Substitute(24)` rounds;
3. one bounded `Backbone` closure, propagating every productive application;
4. DefaultPolicy in forced stable mode, with Reorder, BVE, and conflict-clause
   reuse disabled;
5. recurrent Vivify at 200 per mille, with Factor, Sweep, and repeated
   Backbone disabled after the dedicated startup pass.

This is policy composition only.  It includes `tactix.h`, uses registered rule
and support calls, and does not change a rule or the kernel.

The choices come from the seed-7 CNF and trace rather than a blind option
cross-product.  Variables 1--32 are primary inputs, variables 33--527 are 495
topologically ordered signed two-input AND gates, and variables 528--570 form
the comparison tail.  The winning trace had 3,156,364 conflicts.  Vivify was
productive in all 37 scheduled passes and dominated inprocessing work;
Substitute retired 51 variables, while later Sweep and Factor calls made no
progress and repeated Backbone was productive only once.  Reorder and BVE
destroyed useful gate/import structure, which explains why disabling them was
beneficial.  Stable decision phases were mixed (about 57% negative), so a
global phase-zero policy lost despite the broader CNC+TACO observation.

Direct controls confirmed that point.  Kissat with the exact
`stable=2,target=0,phase=0` tuple timed out at 120 s on seed 7, while
`stable=2,target=1,phase=0` took 41.077 s.  A bounded public
`stable=2,target=0,phase=0` learning prefix before DefaultPolicy took 74.953 s
at best.  Deep typed Sweep, exact BDD-guided input orders, random import
orders, and explicit BVE of the miter tail all lost to the selected policy.
An external exact BDD analysis did reduce the full miter to false in about
0.3 s and 219k nodes, but the public rule set has no proof-producing BDD
action; replaying its hundreds of pivots through separate public BVE sessions
cost more than CDCL.

The policy transferred across the smaller seed-7 ladder without tuning.  For
sizes 27--31 it solved in 34.050, 136.051, 113.718, 144.118, and 85.150 s.
Stock Tactix took 99.202, 97.065, 218.337, and timed out twice at 240 s;
Kissat took 35.969, 142.310, 177.581, timed out at size 30, and took 114.968 s
at size 31.  This establishes useful transfer, though the maximum advantage
over the faster available baseline is only 1.56x.

Primary follow-up artifacts are:

- policy source: `examples/cruxmiter_policy.c`;
- campaign harness: `tools/bench/cruxmiter_seed7_search.py`;
- selected-policy verification:
  `benchmarks/runs/20261010-140841-crux-seed7-verify/`;
- smaller seed-7 transfer:
  `benchmarks/runs/20261010-140425-crux-seed7-screen/`;
- complete decision/inprocessing trace:
  `benchmarks/runs/20261010-crux-seed7-rbc-trace/`.

All follow-up jobs used `cpu-dense-hwulab-q`, four logical CPUs, and 16 GiB.
Screen proofs were written for timing parity and removed without being called
verified.  The selected final candidate was rerun with independent proof
checking.  Proofs and checker expansions lived in task-local temporary
directories with normal, timeout, SIGTERM, and SIGINT cleanup.

## Outcome

This experiment produced a proof-checked public-API policy that improves the
chosen size-20 development instance modestly, but it did **not** establish the
requested large transfer improvement on `cruxmiter32seed4`.

The locked profile is:

1. `Propagate`;
2. two bounded rounds of `Congruence(4) -> Substitute(24)`;
3. up to 512 productive `Backbone` applications, propagating after every
   mutation;
4. hand the resulting state to `tactix_solve` for the mature default search
   schedule.

On `cruxmiter20seed4`, this `backbone` profile solved and verified UNSAT in
2.085 s versus 2.553 s for DefaultPolicy: a 1.22x speedup (18.3% less solver
time).  Its binary DRAT proof was 7,380,727 bytes; DefaultPolicy's native DSR
proof was 8,773,464 bytes.  The locked one-shot target comparison timed out
both arms at 1,200 s, so it supplies no evidence of a speedup at size 32.

## Design and development

The client is `examples/cruxmiter_policy.c`.  It uses only `tactix.h` and
retains explicit profiles for the hypotheses tested during development:

- always-stable EVSIDS with reluctant restarts and negative, positive, or
  saved phases, without target capture;
- gate closure (`Congruence -> Substitute`);
- gate closure with `HyperBinaryRes`;
- gate closure with bounded `Backbone`;
- gate closure with `Sweep` and follow-up substitution; and
- a broader structural sequence that keeps BVE after gate/binary mining.

The TACO-inspired stable-only profiles all lost to DefaultPolicy on the
development instance.  The best useful change was front-loading Backbone
after gate closure.  Backbone doses of 512 and 1024 produced the same proof
and essentially the same solver time, showing that the useful dose terminates
before the larger cap.  Running Backbone before gate closure was slower.

## Protocol and evidence

All real runs used full-width SLURM arrays on `cpu-dense-hwulab-q`, with four
physical cores/eight logical CPUs and 32 GiB per task.  Solver timeout was
1,200 s.  Candidate and baseline arms were proof-scored.  Inputs, solver
binaries, policy binaries, campaign wrapper, checker binaries, allocation,
and cleanup policy are frozen in each run manifest.

The size-20 screen used the nine unique seeds 0 and 2--9; seed 1 was omitted
because it is byte-identical to seed 0.  DefaultPolicy solved all nine UNSAT
with checked proofs.  Seed 4 was the slowest at 2.558 s and was selected for
development, consistent with the intended size-32 seed-4 transfer.

Primary artifacts:

- screen: [`20261010-001002-crux-screen`](../../runs/20261010-001002-crux-screen/);
- final dose selection: [`20261010-003019-crux-tune`](../../runs/20261010-003019-crux-tune/);
- locked target: [`20261010-003108-crux-target`](../../runs/20261010-003108-crux-target/).

The final source pin is `255f5ade0`; the campaign wrapper commit is
`e45bb972a`.  Binary SHA-256 values are:

- DefaultPolicy: `57bdd51cc50d70dd930262f15af5133cbca22e09c4a3f434967f9fa4fdfed536`;
- public policy: `b754130ff9e8be2a5aa5985ac9cd1d7bdec09cd6afdaf383a454356e65d089c3`.

Input SHA-256 values are:

- `cruxmiter20seed4.cnf`: `4630761c43baa5245dcab1593ca059e1c411a49f888a6d9ff53097159faac940`;
- `cruxmiter32seed4.cnf`: `160503b4d57369b7d789dd643e7f6257ed7ceb28a3e18380108c5f7c3554fbdb`.

The size-20 winner and baseline both have clean correctness records: no
verdict mismatch, invalid model, invalid certificate, checker exhaustion, or
unverified scored arm.  The target arms returned UNKNOWN only because both
hit the declared solver timeout; no proof was claimed or checked.

## Proof lifecycle and validation

`tools/bench/run.py` now creates proofs and checker expansions only inside an
exact task-local `mkdtemp`.  Normal return, Python exceptions, solver timeout,
SIGTERM, and SIGINT all remove that directory.  Generated arrays request
`SIGTERM` 60 seconds before the SLURM wall limit.  The target timeout left no
owned `tactix-bench-*` or `tactix-proof-*` directory in `/tmp`, and retained
campaign directories contain no proof, DRAT, DSR, LRAT, or LSR file.

The pinned DSR-to-Trestle chain passed its positive tests and malformed,
truncated, corrupt, overflow, and invalid-substitution negative controls.  A
separate tiny policy-generated binary DRAT proof was accepted by `drat-trim`,
while a proof with its required final lemma removed was rejected.  The
SLURM build-lock tests pass (2 tests).

## Limits and next action

The measured 1.22x development gain is useful but much smaller than the hoped
for 10x--100x improvement.  Because both target arms timed out, the experiment
does not show whether the locked policy is faster or slower at size 32 below
the censor.  The target must not be described as a win.

If this line is continued, collect non-target progress telemetry or use a
larger development size before changing the policy.  Keep `cruxmiter32seed4`
out of that tuning loop; otherwise the one-shot transfer interpretation is
lost.  A longer matched target run would answer a different question and
requires a newly declared timeout.

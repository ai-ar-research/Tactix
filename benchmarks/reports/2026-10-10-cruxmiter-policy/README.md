# Cruxmiter public-policy experiment

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

# Refactor/N1 gate checkpoint — 2026-09-12

The fuzz and performance executable is the immutable pin with SHA-256
`1c5b3c41416ce98db2e97cd4a2adc40a778fdd82793b9f59b7a19f542cc12320`.
It is shared by Claude fuzz job `1683746` and performance jobs `1683747`,
`1683748`, and `1683830`.

After those jobs started, the abandoned non-root Lucky experiment was restored
exactly to the committed root-only implementation. DefaultPolicy invokes Lucky
only from its quiescent level-zero startup, so this removes a public state
surface without changing the measured default path. The final local Release
binary has SHA-256
`9c5aa6bbcd45964875faa183f62bc5e3a2fd2c49f1850a310cf1575fff86c2fc`.

## Correctness

- Release: 146/146 tests pass.
- ASan+UBSan: 145/145 core tests pass; the invariant-heavy scripted demo is
  Release-only evidence.
- The final root boundary is pinned by a registry-wide test: every
  `RuleTier::Inprocess` action is applicable from a consistent root fixture and
  rejects the same fixture at a non-root level. Lucky retains its own root-guard
  regression.
- Claude fuzz `1683746`: 30/30 corpus cases pass. Of 10,000 generated
  random-policy/native-LRAT cases, 8,929 reached a verdict and passed the
  differential plus original-model or native-LRAT oracle; 1,071 hit the
  five-second case bound. No oracle failure occurred.
- Random CLI coverage fired every required broad rule counter. The five zero
  counters (SweepResume, BveBackward, BveBackwardUnit, VariableCompaction and
  VivifyFlush) are covered by focused transition tests; replay, interruption
  and online reconfiguration are also focused tests.

## Performance

Both sets use their historical `cpu-dense-hwulab-q` contract: 12 logical CPUs,
48,000 MiB, one solver core, 1,200 solver seconds, and 16,000 checker seconds.
Only Tactix was rerun. Formula and certificate scratch use anonymous node-local
`O_TMPFILE` objects.

| Set | Current solver total | Retained prior Tactix | Ratio | Peak RSS current/prior |
|---|---:|---:|---:|---:|
| B28+C5 | 962.588 s | 952.088 s | 1.011028x | 3,256,108 / 3,271,032 KiB |
| Rank 1–50 | 8,327.103 s | 8,126.049 s | 1.024742x | 16,016,628 / 16,016,620 KiB |

No solver timed out. Summed peak-RSS ratios are 0.991110x and 0.998745x.
The largest individual Rank ratios are rank 47 at 1.131x (+15.834 s), rank 20
at 1.110x (+13.317 s), and rank 38 at 1.065x (+23.377 s). Rank 47 follows a
different, stronger Congruence trajectory after the retained ITE/value repair;
the aggregate and peak-memory gates remain within their bounds.

Certificate hashes match the retained prior Tactix run on 32/33 B28+C5 and
44/50 Rank cases. The differences are the declared Congruence N1/N2 repair:
additional legal ITE gates and justified value-derived units. They require
independent checking rather than byte-identity acceptance.

## Validation and packaging

The first two manifests omitted `modelcheck.py`. Their SAT solver timing is
usable, but those SAT final records are invalid. Job `1683830` reruns only the
42 SAT cases with the dependency pinned and hashed; valid UNSAT cases and all
reference solvers remain untouched. `runtime_task.py` now rejects this incomplete
pin contract before starting a solver.

Job `1683830` completed all 42 original-input model checks successfully and
released every scratch object. B28+C5 is now 33/33 valid (17 corrected SAT
models plus 16 UNSAT proofs). Rank 1–50 is 47/50 valid (25 corrected SAT models
plus 22 UNSAT proofs); three independent proof checks continue in the
background and still gate final acceptance.

The committed refactor is `7a668e76`. Experimental validation job `1683875`
passed native SAT/model, UNSAT/DRAT, forced-crash and forced-timeout cases, with
all formula, proof, model and checker scratch held in anonymous node-local
`O_TMPFILE` objects. Tier-D array `1683876` therefore submitted all 1,166
committed-Tactix tasks to `cpu-q` with four CPUs, 32 GiB, a 1,200-second solver
timeout, a 16,000-second checker timeout and no concurrency throttle.

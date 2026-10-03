# Failure handoff to Codex — shared-policy feedback fuzz, pin f6f01982

**Failure class:** feedback publication-boundary contract violation (R12; also
R11 record lifetime). A FeedbackReceiver callback is delivered while the kernel
is at an unrestored scope: `Kernel::large_watches_connected == false`.
It happens at the controller's `Substitute` Rule record and again at
`session_end`, after a Substitute round refutes the formula (terminal UNSAT).

- Proof, verdict and model are NOT affected. The run is UNSAT and the CLI
  exits 20 with a valid proof.
- No proof or model checker rejected anything.

**Affected gate:** DEVELOPMENT_PLAN §9.4 shared execution/feedback
preparation, both-policy fuzz acceptance. The affected policy is
RandomControllerPolicy (controller-interface-v1). Required rows R12 ("receivers
observe only restored safe publication boundaries") and R11.
- Not observed for DefaultPolicy: 0 scope violations in 834,208 DefaultPolicy
  driver callbacks before the stop.

**Bounded original reproducer** (deterministic, 3/3 runs, labelled diagnosis
job 1746782):
- Input: `diag/d1/case.cnf`. It is a read-only copy of retained corpus entry
  `tools/fuzz/corpus/regressions/009-cong-and-idempotent-unary/min.cnf`, sha256
  00eba0e110c85125d4f3818ea9fb08239d3db8a2a4c5ffbe6e311b92556786e5.
- Solver pin: `build/pin-shared-policy-feedback-debug-f6f01982`. Its tactix
  sha256 is 7dcbbec3…fc08, and the pin re-verified with 0 differences after all
  jobs.
- Owner driver: `targeted/snap-f1/fbdrv`, sha256 cfee1c9b…885d.
  - Source is `targeted/snap-f1/fbdrv.cpp`, built with `build.sh` against the
    read-only `src-f6f01982/`.
  - It links the pin's own `tactix_lib` objects.
  - The check is `boundary()`, around fbdrv.cpp:398–417.
- Exact command (any working directory; the run itself is a single fast
  solve):

```
/home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-fuzz-owner-f6f01982/targeted/snap-f1/fbdrv \
  --mode=purity --policy=random --seed=2032000128 --full-retention \
  /home/hwu/SAT-solving/benchmarks/runs/20261002-shared-policy-feedback-fuzz-owner-f6f01982/diag/d1/case.cnf /tmp/out.proof
```

  Dropping `--full-retention` (default ranked-prefix) fails identically.

- Observed output:

```
c fbd-fail receiver callback at unrestored scope (large_watches_disconnected ) at Substitute
c fbd-fail receiver callback at unrestored scope (large_watches_disconnected ) at session_end
s UNSATISFIABLE            (driver exit 3 = owner check failed)
```

- Root sequence (from the CLI feedback stream `tactix
  --controller-policy=2032000128 --feedback=F`): BVE, LocalSearchGuidance,
  then Substitute (sequence 3). That Substitute record is the refuting action.

**Owner observation (not a diagnosis, no patch):**
`src/strategy/substitute_execution.hpp:57–60` returns as soon as a round
reports `ApplyResult::Unsat`, before the batch's final
`SubstituteReconnectRequest` (lines 68–74). The header comment at lines 40–43
states "a disconnected watch session must complete before public action
feedback".
- That file is not in the f6f01982 delta, so the path predates this repair.
- The accepted 0531c2f1 campaign used different seeds and never reached it.
- The repair, its regression and the affected reruns belong to Codex.

**Rerun needed after repair:** both 100k legs, the full targeted matrix and
rare-family mining on the repaired pin. Everything stopped here is listed in
FINAL_REPORT.md.

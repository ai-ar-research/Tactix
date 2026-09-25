# AM429: AM427 Tier B is checker-clean; held-array lock handoff

Date: 2026-09-25. Neither N4 nor a performance gate is accepted here.

Matched, unthrottled Tier B array `1723236` and aggregate `1723237` are
terminal. Both AM427 and parent solve 28/28 (14 SAT, 14 UNSAT); every SAT model
and all 14 DSRs per arm independently check with the frozen AM369 native-DSR
checker pins. Every paired verdict and proof size is identical. AM427 solver
time totals 415.978 s versus parent's 427.866 s, a +11.888 s advantage over
28 instances (PAR-2 14.86 versus 15.28 s). The largest single difference is
4.567 s, 38% of the total; the sign survives removing any three rows. This
small timing signal does not establish changed Sweep branch reach or a stable
competitive gain. See `benchmarks/runs/20260925-140130-B-am427-sweep-lifecycle/`
for the exact task records, manifest, solver hashes, checker pins, resources,
and aggregate log. Resource shape: `cpu-dense-hwulab-q`, four physical cores /
eight logical CPUs, 32 GiB per task, 300 s solve limit.

Tier C was first submitted as array `1723256` while B still owned the same
build directories. `buildlock.py` correctly refused the second acquisition,
but `run.py` incorrectly left that array queued and printed submission success.
No C task started; array `1723256` and dependent aggregate `1723257` were
cancelled exactly. B retained its lock and completed normally. Its aggregate
released both paths before any C resubmission.

The general runner repair submits an array initially held, acquires every
build-pin lock, submits its dependent aggregate, and only then releases the
SLURM hold. A refused lock cancels the still-held array and returns failure;
release failure cancels both jobs. This prevents a second array from using a
binary after the first aggregate releases the only lock. Two new protocol
tests cover refusal/cancellation and successful operation order; all 31
runner/proof-oracle unit tests pass. The native-DSR checker-pin manifest and
per-task hash validation from AM428 remain in effect.

Next: submit matched Tier C only after this repair is committed, with the same
immutable solver pins and frozen checker trio. Inspect per-instance
performance, proof/model correctness, and Sweep reach; continue affected
fuzz, reference scheduling traces, H2 and N1--N4. Do not submit the requested
5,000-second Tier D run yet.

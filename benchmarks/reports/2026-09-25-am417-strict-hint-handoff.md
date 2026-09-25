# AM416–AM417 native-LRAT hint hygiene checkpoint

Date: 2026-09-25. Goal status: progress only; N1–N4 are not promoted and the Tier D Tactix run is not authorized yet.

## Repairs

- `9f1c9d6e439e3822a99a6c8291b719fb44e68911`: Instantiate omits a named root unit from its LRAT reason-walk chain when that unit is already an assumption of the complete negated strengthened clause. The regression first proves the raw reason walk visits the root unit, then checks the emitted ADD omits it. The same commit deduplicates shared absorbed root-unit hints in Strengthen; its separate minimal regression checks exactly one citation. This Strengthen path was a real latent duplicate-hint mechanism, **not** the producer of the archived `c287-R2` lines.
- `9433f500642b4539f22b7aa114f7ba3814a93c88`: BVE deduplicates root-unit hints from both resolution parents, including literals that became root-false after antecedent capture, before citing the parent clauses. A minimal native-LRAT BVE regression checks one citation. The old `c287-R2` lines 150–154 were from this producer.

Both repairs retain first-occurrence hint order, all solver clauses, verdicts, and non-LRAT proof streams. Calculus entries for X4, C2, and E1 were updated with the exact hint obligation.

## Original-CNF check

SLURM job `1722120` on `cpu-dense-hwulab-q` completed (4 CPUs, 32 GiB, 10-minute limit; one second elapsed). The source commit was `9433f500`; the Debug public-API driver SHA-256 was `e58ffdd0bd5c2a8bf2a794582853e66e97f116e9017c0ff8e28240afd38ec598`. Commands and per-case logs/proofs are preserved in the ignored run directory `benchmarks/runs/20260925-am417-bve-hint-check/verify.sh`. The original AM358-v2 `case.cnf` files were used unchanged, and the pinned CakeLPR accepted every UNSAT proof.

| Archived case | Strict audit on current proof | Observation |
| --- | --- | --- |
| `c9291-R3` | clean | Today's trajectory no longer emits the old Instantiate witness line; the new minimal regression is the causal check. |
| `c287-R2` | clean, 41 RAT lines passed | Five chains changed from `105 105 ...` to `105 ...`, with the rest of each chain preserved. The pre-BVE repair run `1722111` reproduced all five strict duplicate-hint failures, and its CakeLPR check also passed. |
| `c88-R2` | line 34 `satisfied-by-cube` | Still open. Lemma 56 `13 3 10 1` reuses the chain `47 2 34 19` for earlier lemma 52 `13 3 10`; hint 34 contains `-1`, already true in the new lemma's negated cube. CakeLPR accepts it, but the strict audit correctly rejects it. |

The first `1722092` script attempt stopped on the driver's normal UNSAT exit status 20. The corrected pre-BVE script `1722111` ran the first two cells and failed exactly at the expected duplicate-hint assertion. The final job `1722120` completed all three. No result from the aborted script was counted as verification.

## Focused validation and remaining gate

After the BVE repair, Debug `ctest` selected `bve`, `instantiate`, `strengthen`, `online_api`, `micro_proofs`, and `sweep_proofs`: **6/6 passed**. The earlier broad custom-config Debug CTest was intentionally interrupted after 60/186 tests because it had three failures outside these edits (`scheduling`, `event_trace`, `event_trace_zero_overhead`) and included a long Lucky test; this is not a full-suite pass. Dedicated Release, sanitizer, differential, and affected-scope fuzz gates remain to run before acceptance.

The separate Sweep ternary-chain satisfied-hint handoff (`AM358-v2 H2`) and BVE definition-unit lifted-chain case `c88-R2` remain unresolved strict proof-contract gaps. H2 canonical no-option DSR failure also remains open. No checker route or acceptance condition was weakened; do not launch the requested 5,000-second Tier D comparison or declare online-learning readiness until those gates and the N1–N4 audit close.

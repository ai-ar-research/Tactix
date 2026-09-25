# AM388 — Sweep core-export resource boundary

The AM363 terminal fuzz report's S1 failure is a pre-existing, proofless public
Sweep allocation failure, not an API model-cache regression. The 179-variable,
760-clause original and the 182-variable, 637-clause online-action case are
preserved in `benchmarks/runs/20260924-am363-api-model-cache-claude-fuzz/triage/s1/reproducer/`.
This repair is general to embedded proof export: it does not recognize those
formulas, seeds, or schedules.

## Diagnosis and repair

Under the original 2,048,000,000-byte address-space cap, frozen Debug stacks
showed three successive copies of the same large proof obligation:

| Frozen diagnostic | Allocation at failure | Evidence |
|---|---|---|
| parent, job 1719326 | full `SweepGeneralStepPlan` vector during validation | `stack-1719326.log` |
| streamed plan, job 1719369 | `SubStep` vector during `SubSolver::derivation` | `stack-v2-1719369.log` |
| shared-prefix candidate, job 1719507 | `CoreClause` vector during clausal-core export | `stack-v3-1719507.log` |

Sweep now validates general proofs step by step, retains only the final
translation, re-translates intermediates during emission, and reconstructs
proof-only reverse deletions instead of keeping a second full prefix. Core
units share immutable derivation steps with individual prefix lengths; after
a unit is processed its identity remains for duplicate suppression but its
proof capture is released. The Debug refutation-target assertion reads the
subsolver's target directly, without re-exporting the whole derivation.

The optional derivation and clausal-core exporter lives in
`src/engine/subsolver_proof_export.cpp`, a narrow exception-enabled translation
unit. Its `noexcept` entry catches allocation exhaustion, clears the partial
capture and reports a resource decline. Sweep then stops that environment;
previous certified consequences remain, but it cannot publish a consequence
from the failed capture. The search kernel remains compiled without exceptions.
This is a resource-admission outcome, not an arbitrary fixed proof-step cap or
a special case for S1. The default typed proof-step setting remains unbounded.

## Checked evidence

The final Debug public-driver pin is SHA-256
`70f9e24cbcd1e61899afeb72c333903b6b88536a284bde965569f26f43c24fff`
(`mdrv-debug-v5`). Array 1719760 replayed both original AM363 S1 schedules
on `cpu-dense-hwulab-q`, 4 CPUs, 8 GiB Slurm memory and the exact 2,048,000,000-byte
process address-space cap. Both tasks exited 10 (SAT); Slurm's `FAILED 10:0`
is the driver's SAT exit code, not a crash. The original completed in 7:12
with 1,689,860 KiB reported peak RSS; the second in 2 seconds. Independent
`tools/fuzz/checkmodel` checks satisfied all 760/637 original clauses with
complete 179/182-variable assignments. Both v5 outputs are byte-identical to
the earlier successful v4 pin. `verify-final.sbatch` and the output logs are
in `benchmarks/runs/20260925-am388-sweep-s1-diagnosis/`.

The deterministic `subsolver_export_resource` regression forces `bad_alloc`
inside proof export, verifies `resource_exhausted`, empty partial buffers and
a successful retry after allocation is restored. Focused `subsolver`, `sweep`
and `sweep_proofs` tests pass. Fixed Sweep DRAT/DSR/LRAT output is byte-identical
to the earlier AM382 build; the pinned proof-checker suite passes. Documentation
size, proof-rule sync, progress and diff-whitespace checks pass. The proof-oracle
unit and binary suites pass 30/30, including their known-bad controls.

The broader `micro_proofs` suite is **not green**. With all helper binaries
built, its corpus-034 Substitute certificate is rejected by the extra
`--every-lemma` forward `srcheck` audit. A frozen AM382 Release solver and the
current AM388 Debug solver both exit UNSAT/20 and emit byte-identical DSR
(SHA-256 `aea599ca25f2f0e9cc248b023e87a6608301c7c6d371ad1759f0d5518c5f0570`);
the canonical no-option DSR→LSR→Trestle route verifies that proof. The two-pin
checker comparison is job 1720002 under
`benchmarks/runs/20260925-am388-micro-substitute-diagnostic/`. Thus this
failure is not caused by AM388, but the disagreement still needs its own
N2 analysis; it is not recorded as a passing broad proof gate.

This closes the reproduced S1 resource crash, not the full correctness gate.
Claude-owned affected-scope fuzz, the separate AM369 H2 native-DSR checker
failure, and complete N1--N4/performance acceptance remain open. No Tier D
comparison was submitted.

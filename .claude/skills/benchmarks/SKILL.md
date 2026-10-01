---
name: benchmarks
description: Run or interpret Tactix performance evaluations, select the smallest benchmark gate that answers a question, compare pinned solvers, and write evidence reports. Use for performance claims, regressions, ablations, or milestone gates; benchmark tier definitions remain owned by DEVELOPMENT_PLAN.md.
---

# Benchmark evaluation

Treat `DEVELOPMENT_PLAN.md` §8.1 as the tier and scoring authority. This skill
only defines how to obtain trustworthy evidence.

## Select the gate

- **A:** correctness-adjacent smoke and gross regression detection.
- **A + B:** every meaningful implementation change and development comparison.
- **A + B + C:** scheduling, heuristic, or performance-sensitive changes.
- **A + B + C + D:** tier-based performance milestones and external competitiveness claims.

Use the cheapest level that can decide the stated question. Never tune defaults
on A, and never promote an A/B result to a competitiveness claim.
When the plan names a stage-specific gate, use that contract: controller
foundations require the four fixed DefaultPolicy regression sets in §8.4,
in addition to the two-policy fuzz acceptance in §8.2. Pin the accepted
checkpoint and compare per-instance retention; retain declared exceptions.

## Protocol

1. State the hypothesis, comparison, metric, and pass/fail criterion first.
2. Use tier lists and pinned binaries already named by the plan or harness; do
   not rebuild tier composition during an evaluation.
3. Run solver measurements through `slurm-jobs`. Compare candidates and
   baselines in the same allocation shape and time window. Pin immutable builds
   and record commit SHA, binary hash, tier-list hash, timeout, memory, throttle,
   seeds, and solver arguments. Submit to the partition without selecting a
   node; task order is instance-major so paired arms are not node-correlated.
4. Validate every SAT model and UNSAT proof. Keep every frozen instance in the
   denominator. A harness failure or missing final result makes the affected
   task incomplete and unscorable; report it and repair the evidence rather
   than silently excluding the row or claiming a complete gate.
5. Report solved counts, SAT/UNSAT splits, PAR-2, paired runtime ratios, timeout
   transitions, and uncertainty or repeat spread where it could change the
   decision. A subset is a scoring view of the parent run, not a separate run.
6. Save durable decision-changing evidence under `benchmarks/reports/`; raw
   results remain in the run directory.

Tier files are versioned inputs. Changing one is a separate, approved benchmark
design change with provenance and validation, not part of an ordinary run.

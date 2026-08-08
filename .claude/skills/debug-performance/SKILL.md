---
name: debug-performance
description: Diagnose a decision-relevant performance gap against a pinned baseline using matched measurements, work-versus-unit-cost decomposition, and ablations. Use when a regression threatens the current goal or gate, not merely because an arbitrary runtime ratio is large.
---

# Diagnose performance

Begin with the decision the diagnosis must support. If the observed gap cannot
change that decision, record it and stop.

1. Reproduce candidate and pinned baseline in the same SLURM window and
   allocation shape. Preserve exact instance, SHA, binary hashes, arguments,
   timeout, and raw statistics.
2. Decompose elapsed time into **work × unit cost**. Compare:
   - propagations/second: BCP and low-level throughput;
   - conflicts/second: overall search-loop throughput;
   - propagations/conflict: search work performed per conflict;
   - decisions, conflicts, restarts, learned clauses, reductions, and rule
     applications: total work and search trajectory.
3. Form ranked hypotheses: missing or mistimed simplification, search quality,
   raw engineering cost, scheduling defect, or interaction.
4. Test the cheapest discriminating ablation first. Disable or isolate a
   mechanism in candidate and, where supported, baseline. An ablation is
   evidence about causality, not yet confirmation.
5. Confirm the winning hypothesis with a focused profile, counter, trace, or
   second discriminating experiment. Repeat noisy measurements when variance
   could reverse the conclusion.
6. Route the fix: `rule-dev`, `revamp`, a bounded policy experiment, or
   `revise-plan`. Re-run the benchmark gate appropriate to the change.

Write a report only when the evidence changes a design, gate, default, or next
action. Do not preserve exploratory narratives that did not affect a decision.

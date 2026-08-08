---
name: revamp
description: Design and execute an approved structural rewrite of Tactix internals when architecture, data layout, module boundaries, or contracts impede correctness or the current performance goal. Use for substantive restructuring, not behavior-preserving cleanup.
---

# Structural revamp

Before editing, present a concise mini-design: problem and evidence, proposed
structure, invariants, migration stages, expected metric, risks, rollback point,
and validation. Obtain explicit user approval. The approval may be conversational;
capture the final design and evidence in the commit message.

## Execute

1. Start from a green, scoped baseline and define the short gate that detects a
   failed premise quickly.
2. Land a coherent staged series: scaffolding, migration, cutover, then removal
   of the old path. Keep intermediate states buildable when practical.
3. During each stage, run risk-based focused checks for affected invariants,
   proofs, models, APIs, and performance. At the end, run the full validation bar
   required by the owning subsystems and benchmark gate.
4. Compare the predeclared metric and inspect complexity, not just runtime. If
   the metric is missed or a premise fails, stop and return the evidence and
   options to the user; do not silently broaden or tune the design.
5. Update owned documentation and PROGRESS only for facts that changed.

Style cleanup and dead-code removal belong to `revisit-codebase`. A change that
alters contracts or gates also requires `revise-plan`.

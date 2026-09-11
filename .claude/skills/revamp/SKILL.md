---
name: revamp
description: Design and execute a structural rewrite of Tactix internals within the user's authorized task when architecture, data layout, module boundaries, or contracts impede correctness or the current performance goal. Use for substantive restructuring, not behavior-preserving cleanup.
---

# Structural revamp

Before editing, communicate a concise mini-design: problem and evidence, proposed
structure, invariants, migration stages, expected metric, risks, rollback point,
and validation. Proceed without waiting for user approval when the rewrite is
within the authorized task; the mini-design is an update, not an approval gate.
Capture the final design and evidence in the commit message. Ask only when new
authority is genuinely needed, such as a material scope expansion, a change to
user-set acceptance criteria, or an otherwise unauthorized external action.
This skill does not override safety, execution permissions, or prior denials.

## Execute

1. Start from a green, scoped baseline and define the short gate that detects a
   failed premise quickly.
2. Land a coherent staged series: scaffolding, migration, cutover, then removal
   of the old path. Keep intermediate states buildable when practical.
3. During each stage, run risk-based focused checks for affected invariants,
   proofs, models, APIs, and performance. At the end, run the full validation bar
   required by the owning subsystems and benchmark gate.
4. Compare the predeclared metric and inspect complexity, not just runtime. If
   the metric is missed or a premise fails, stop that experiment, report the
   evidence, and reassess. Proceed with an evidence-backed revised design within
   the authorized task without an approval pause; communicate its new premise
   and short gate first. Do not silently broaden scope, lower acceptance gates,
   or tune a failed premise into an apparent success.
5. Update owned documentation and PROGRESS only for facts that changed.

Style cleanup and dead-code removal belong to `revisit-codebase`. A change that
alters contracts or gates also requires `revise-plan`.

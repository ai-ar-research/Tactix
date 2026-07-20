# Tactix — proof-rule-driven SAT solver

- **Plan:** DEVELOPMENT_PLAN.md is authoritative (scope/architecture/roadmap).
  **Status:** PROGRESS.md (live checklist + log — read for "where we are now").
- **Sync rule (mandatory):** whenever you complete or start a checklist item, or
  change DEVELOPMENT_PLAN.md, update PROGRESS.md in the same commit. Plan edits
  go through the revise-plan skill.
- **Non-negotiables (plan §1.1) — never trade these away:**
  - N1: any applicable proof rule can be applied via the public API at any
    time, including the choice of branching variable/phase (for future LLM/RL
    policies).
  - N2: proof certificates (DRAT/LRAT) on every path.
  - N3: DefaultPolicy performance comparable to Kissat (T4 gate).
- `kissat/` (and other reference solvers) are untracked reference clones —
  never commit them, never copy code verbatim (transliterate + attribute).
  References are evidence, not templates (plan §5): adopt an approach only
  with an explicit fits-our-architecture argument, and check recent
  competition winners (CaDiCaL-SC2025, AE-Kissat-MAB, SBVA-CaDiCaL), not
  just plain Kissat.
- Project skills: benchmarks, fuzz, proof-check, rule-dev, compete,
  revise-plan, revamp, simplify-codebase, calculus, stress-policy — use them for the matching
  workflows.
- **`docs/calculus.md`** is authoritative for the *abstract* proof rules (each
  rule's pure precondition, effect, proof delta, soundness). Its companion
  **`docs/implementation-notes.md`** holds the engineering layer — the
  *implemented* guards and the engineering-deviation ledger (every guard stronger
  than its precondition, with a removal path) + standalone design decisions. Keep
  both in sync via the calculus skill whenever a rule or guard changes (guards
  must be pure preconditions per N1).
- **Doc discipline (avoid sprawl):** `docs/README.md` is the documentation map —
  what exists and which docs are *living* (kept current: the plan, PROGRESS,
  calculus, implementation-notes) vs *frozen* (design snapshots, reports). One
  fact has one home; other docs link, never restate. Before creating a *new* doc,
  prefer extending a living one — a new doc is justified only for (a) a design
  pass before a large feature (→ `docs/design/`, frozen on landing) or (b) a
  dated experiment/gate report (→ `benchmarks/reports/`). Both are frozen (never
  edited) once written.

## Working discipline

- **Orient (session start):** state which phase (PROGRESS.md) and which item(s)
  you are taking on, then proceed — don't wait for confirmation. Begin with the
  first unchecked item of the current phase.
- **Work loop, per item:** implement the smallest complete increment → **test it
  per the matching skill** — correctness work is not done until the fuzz oracles
  pass (proof-checked on every UNSAT from the first commit; model-checked on every
  SAT), and performance claims come only from the benchmark runner on the right
  tier (≤ 8 parallel jobs, plan §8.1) → update PROGRESS.md (tick items + Log
  entry) in the **same commit** as the work; commit with a clear message, push a
  coherent unit.
- **Rule work** follows the rule-dev skill and §5.3: one directory + one registry
  line; guard / lazy instances / apply; proof delta from the closed five-primitive
  vocabulary or it doesn't merge. Sequential solving only; C++20, C-style kernel
  discipline per §5.
- **When blocked or the plan is wrong/vague:** underspecified point → plan §11 +
  the revise-plan skill's concretize mode (decide minimally, annotate the
  register, keep going). Plan contradicts reality → revise the plan via
  revise-plan (never silently diverge; never weaken N1–N3).
- **Phase transitions:** when a phase's exit criteria (§9) are met, run the gate
  evaluation, write the report to `benchmarks/reports/`, tick the gate in
  PROGRESS.md, and start the next phase.
- **End of session:** leave PROGRESS.md accurate, tests green, and work committed
  and pushed; summarize what changed, the current fuzz/benchmark status, and the
  next step.

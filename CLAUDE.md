# Tactix (working name) — proof-rule-driven SAT solver

- **Plan:** DEVELOPMENT_PLAN.md is authoritative. **Status:** PROGRESS.md.
  **Session bootstrap prompt:** Instruction.md.
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
  revise-plan, revamp, simplify-codebase — use them for the matching
  workflows.

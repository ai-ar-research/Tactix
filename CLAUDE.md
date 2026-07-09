# Tactix (working name) — proof-rule-driven SAT solver

- **Plan:** DEVELOPMENT_PLAN.md is authoritative. **Status:** PROGRESS.md.
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
- Project skills: benchmarks, fuzz, proof-check, rule-dev, compete, revise-plan
  — use them for the matching workflows.

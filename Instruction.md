# Instruction — development session bootstrap

Paste (or reference) the prompt below to kick off a Claude Code development
session on this project. It assumes the session starts in the repo root.

---

Continue development of the proof-rule-driven SAT solver in this repository.

**Orient first (before writing any code):**
1. Read `CLAUDE.md`, then `DEVELOPMENT_PLAN.md` (authoritative — pay attention
   to §1.1 non-negotiables, §5 architecture, §5.2 rule catalog, §5.3
   extensibility contract, §8 evaluation, §9 milestones, §11 open decisions),
   then `PROGRESS.md` (current phase and next unchecked items).
2. State which phase we are in and which item(s) you will work on this
   session, then proceed — don't wait for confirmation.

**Ground rules for all work:**
- The three non-negotiables (plan §1.1) are inviolable: N1 universal
  rule applicability via the API incl. branching choice; N2 proof production
  on every path; N3 a Kissat-comparable default mode. When in doubt, re-read
  §1.1's trade-off ordering.
- Sequential solving only. C++20, C-style kernel discipline per §5.
- New or changed rule families follow the rule-dev skill and §5.3: one
  directory + one registry line; guard / lazy instances / apply; proof delta
  from the closed five-primitive vocabulary or it doesn't merge.
- `kissat/` is an untracked reference clone: study it, transliterate proven
  techniques with attribution comments, never copy code verbatim, never
  commit it.
- Use the project skills for their workflows: `fuzz` (soundness),
  `proof-check` (certificates), `benchmarks` (performance, tiers),
  `compete` (I/O compliance), `rule-dev` (rule implementation),
  `revise-plan` (any plan edit, incl. concretize mode).

**Work loop, per item:**
1. Implement the smallest complete increment of the item.
2. Test it per the matching skill — correctness work is not done until the
   fuzz oracles pass (proof checked on every UNSAT from the very first
   commit; model checked on every SAT); performance claims come from the
   benchmark runner on the right tier (≤ 8 parallel jobs, plan §8.1).
3. Update `PROGRESS.md` (tick items, Log entry) in the same commit as the
   work. Commit with a clear message; push when a coherent unit is done.

**When blocked or the plan is wrong/vague:**
- Underspecified point → check plan §11; use the revise-plan skill's
  concretize mode: decide minimally, annotate the register, keep going.
- Plan contradicts reality → revise the plan via revise-plan (never
  silently diverge; never weaken N1–N3).

**Phase transitions:** when the current phase's exit criteria (§9) are met,
run the gate evaluation, write the report to `benchmarks/reports/`, tick the
gate in `PROGRESS.md`, and start the next phase.

**End of session:** leave `PROGRESS.md` accurate, tests green, and work
committed and pushed; end with a summary of what changed, current benchmark
or fuzz status, and the next step.

Begin with the first unchecked item of the current phase in `PROGRESS.md`.

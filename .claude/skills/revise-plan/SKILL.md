---
name: revise-plan
description: Modify DEVELOPMENT_PLAN.md for any scope, architecture, roadmap, gate, catalog, or open-decision change. Use for every plan edit so N1–N4, PROGRESS.md, cross-references, and the external revision history stay synchronized.
---

# Revise the development plan

Keep `DEVELOPMENT_PLAN.md` stable and concise. Current status and measurements
belong in PROGRESS or frozen reports, not the plan.

## Procedure

1. Read the affected sections, §1.1, §9, §11, and current PROGRESS.
2. Check N1–N4. Never weaken them without the user's explicit informed
   decision; record any such decision and its consequences.
3. Make the smallest stable change. Keep catalog phases (§5.2), benchmark/gate
   definitions (§8), milestones (§9), and PROGRESS consistent.
4. Search forward-looking section and phase references across the plan,
   PROGRESS, CLAUDE, skills, and living docs. Preserve historical numbering in
   frozen artifacts.
5. Increment the plan revision/date and append one line to
   `docs/log/plan-revisions.md` in ascending order. Section 13 is only its
   pointer. Put rationale in the commit message or an existing evidence report,
   not a narrative PROGRESS log.
6. Synchronize PROGRESS's revision, affected checklist/gate state, and resume
   block. Update a skill only if its workflow changed.
7. Run doc checks and tests. Commit the plan, PROGRESS, revision-history line,
   and related workflow edits together as `plan rev N: <summary>`.

## Concretize an open decision

Resolve only what blocks current work. Write the decision in its owning plan
section, annotate its §11 row with `decided rev N: <decision> — <reason>`, and
follow the procedure above. If a concrete commitment is removed without a
replacement, reopen it in §11 with a decide-by point.

## Consistency checklist

- Each catalog family has a proof delta, phase, and disposition.
- T1–T5 match between §§8–9 and PROGRESS.
- Phase deliverables match the active checklist.
- The one-directory/one-registration-line contract and five proof primitives
  remain intact.
- Scope remains sequential and the benchmark discipline remains unchanged.
- Every open decision has a decide-by point.

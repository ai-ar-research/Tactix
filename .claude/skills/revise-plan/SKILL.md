---
name: revise-plan
description: Modify DEVELOPMENT_PLAN.md — any edit, however small; scope changes, milestones, rule catalog, evaluation, architecture. Use for every plan revision to enforce the non-negotiable invariants and keep PROGRESS.md, section cross-references, and the other skills in sync.
---

# Revising the development plan

DEVELOPMENT_PLAN.md is the project's authoritative document; PROGRESS.md mirrors
it as a living checklist. Every revision follows this procedure — no direct
one-off edits.

## The non-negotiables — check FIRST, before editing

These three requirements (plan §1.1) can never be weakened, reordered below
other goals, deferred out of the plan, or made optional by a revision:

1. **N1 — Universal rule applicability.** Any proof rule whose guard holds is
   applicable through the public API at any solver state, **including the
   choice of branching variable and phase in Decide**. This API is the basis
   for externally driven (LLM/RL) solving and must exist in every build/mode.
2. **N2 — Proof production.** Every mode, policy, and rule path emits checkable
   DRAT/LRAT certificates and competition-format output.
3. **N3 — Kissat-comparable mode.** At least one shipped policy (DefaultPolicy)
   reaches performance comparable to Kissat (T4 gate on Tier C/D).

If a requested revision conflicts with N1–N3: do not silently comply. State the
conflict explicitly, propose an alternative that preserves the invariant, and
proceed only on the user's explicit, informed confirmation — and if they do
confirm, record the decision and its consequences in the revision history.

## Revision procedure

1. **Read before writing:** re-read the sections being changed plus §1.1, §9
   (milestones), and PROGRESS.md. Understand what the current text commits to.
2. **Make the edit.** Keep the plan self-consistent: tier definitions (§8.1),
   rule catalog phases (§5.2), milestone table (§9), and gates T1–T5 (§8.3)
   reference each other — a change to one usually obligates another.
3. **Fix cross-references.** Section numbers appear in the plan *and* in the
   skills: `grep -rn '§' DEVELOPMENT_PLAN.md .claude/skills/ PROGRESS.md CLAUDE.md`
   and verify every reference still points where intended (renumbering is the
   classic failure).
4. **Bump the revision:** increment "rev. N" and date in the plan header, and
   append a dated entry to §12 (Revision history) summarizing what changed and
   why.
5. **Sync PROGRESS.md:** update the tracked plan revision, add/remove/re-gate
   checklist items affected by the change, and note the revision in the Log.
   If the change alters a skill's workflow (tiers, checklist, catalog), update
   that skill in the same pass.
6. **One commit** containing plan + PROGRESS.md + any touched skills, message
   starting `plan rev N: <summary>`. Push.

## Consistency checklist (run mentally at step 2, verify at step 3)

- Every rule family mentioned anywhere appears in the §5.2 catalog with a
  proof-delta type and phase.
- Every phase deliverable in §9 has matching checklist items in PROGRESS.md.
- Gates T1–T5 appear identically in §8.3, §9, and PROGRESS.md.
- The §5.3 extensibility contract (new rule = one directory + one registry
  line) and the five-primitive proof-delta vocabulary are intact.
- Scope remains sequential-only; benchmark discipline (never tune on Tier A;
  full sets only at gates) remains stated.

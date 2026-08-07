---
name: revise-plan
description: Modify DEVELOPMENT_PLAN.md — any edit, however small; scope changes, milestones, rule catalog, evaluation, architecture. Use for every plan revision to enforce the non-negotiable invariants and keep PROGRESS.md, section cross-references, and the other skills in sync.
---

# Revising the development plan

DEVELOPMENT_PLAN.md is the project's authoritative document; PROGRESS.md mirrors
it as a living checklist. Every revision follows this procedure — no direct
one-off edits.

## The non-negotiables — check FIRST, before editing

These four requirements (plan §1.1) can never be weakened, reordered below
other goals, deferred out of the plan, or made optional by a revision:

1. **N1 — Universal rule applicability.** Any proof rule whose guard holds is
   applicable through the public API at any solver state, **including the
   choice of branching variable and phase in Decide**. This API is the basis
   for externally driven (LLM/RL) solving and must exist in every build/mode.
2. **N2 — Proof production.** Every mode, policy, and rule path emits checkable
   DRAT/LRAT certificates and competition-format output.
3. **N3 — Kissat-comparable mode.** At least one shipped policy (DefaultPolicy)
   reaches performance comparable to Kissat (T4 gate: parity on Tier D, §8.3).
4. **N4 — Scheduling sophistication (the strategy floor).** DefaultPolicy's
   *scheduling* of the proof rules — order, effort budgets, warm-ups,
   watermarks, acceptance bounds and their ramps, and the couplings between
   them — is at least as sophisticated as Kissat's default strategy. N3
   measures the outcome; N4 constrains the mechanism. A rule scheduled more
   crudely than Kissat schedules its counterpart is an unmet requirement, not
   a tuning question, and never a §5.6 arm. Checked via the
   **scheduling-parity ledger** (§1.1): per Kissat mechanism, either an
   at-least-as-strong analogue or a deviation-ledger row with a named closer.
   Watch for the two shapes that have shipped — *we own the mechanism but
   never wire it*, and *we substitute an instance-level gate for a
   per-application quality bound* (§5 obligation 5).

If a requested revision conflicts with N1–N4: do not silently comply. State the
conflict explicitly, propose an alternative that preserves the invariant, and
proceed only on the user's explicit, informed confirmation — and if they do
confirm, record the decision and its consequences in the revision history.

## Revision procedure

1. **Read before writing:** re-read the sections being changed plus §1.1, §9
   (milestones), and PROGRESS.md. Understand what the current text commits to.
2. **Make the edit.** Keep the plan self-consistent: tier definitions (§8.1),
   rule catalog phases (§5.2), milestone table (§9), and gates T1–T5 (§8.3)
   reference each other — a change to one usually obligates another.
3. **Fix cross-references.** Section numbers *and phase numbers* appear in the
   plan, in PROGRESS, in CLAUDE.md and in the skills:
   `grep -rniE '§|phase[ -][0-9]' DEVELOPMENT_PLAN.md .claude/skills/ PROGRESS.md CLAUDE.md docs/`
   and verify every reference still points where intended. **Renumbering is the
   classic failure** — rev 83 shifted phases 4+ by one and left "phase 6 learner"
   readings in three skills. When a phase moves, add a numbering note to §9 and
   fix the *forward-looking* references; historical parentheticals ("moved from
   phase 4, rev 47") keep their contemporary numbering and are read against their
   rev. Machine-checked docs matter too: `tools/docs/check-option-coverage.py`
   anchors on PROGRESS's phase heading, so a renamed heading fails `ctest`.
4. **Bump the revision:** increment "rev. N" and date in the plan header, and
   append a ONE-LINE dated entry **at the BOTTOM of §13** (Revision history is
   **ascending — oldest at top, newest last**; do NOT prepend — that is the
   PROGRESS.md Log's newest-first convention, the opposite of §13, and conflating
   them has scrambled the index before). rev 24 policy: §13 is an index that keeps
   "rev N" references resolvable; the full rationale goes in the PROGRESS.md Log
   entry (step 5) and the commit message (step 6), both written in the same change
   anyway.
5. **Sync PROGRESS.md:** update the tracked plan revision, add/remove/re-gate
   checklist items affected by the change, and note the revision in the Log.
   If the change alters a skill's workflow (tiers, checklist, catalog), update
   that skill in the same pass.
6. **One commit** containing plan + PROGRESS.md + any touched skills, message
   starting `plan rev N: <summary>`. Push.

## Concretize mode — resolving deliberate underspecification

The plan keeps a register of deliberately open decisions (§11), because
deciding everything up front would be guessing. Use concretize mode when an
open item starts blocking work (its decide-by point arrives, or implementation
can't proceed without it):

1. Find the item in §11 (if the blocker isn't listed there, it's a *new* open
   item — add it first, then decide it if it's actually due).
2. **Decide minimally.** Resolve only what unblocks the work at hand; leave
   neighboring questions open. If a fuller design writes itself naturally,
   still record only the part that is needed now — premature detail calcifies.
3. Write the concrete decision into the relevant plan section (it becomes
   normal plan text), and annotate the §11 row in place:
   `→ decided rev N: <what> — <one-line why>`.
4. Follow the standard revision procedure above (cross-refs, rev bump, §13
   history entry, PROGRESS.md sync, one commit).

The inverse also holds: when a revision would *remove* concrete commitments
without replacing them, don't delete silently — move the question into §11
with a decide-by point, so underspecification stays deliberate and tracked.

## Consistency checklist (run mentally at step 2, verify at step 3)

- Every rule family mentioned anywhere appears in the §5.2 catalog with a
  proof-delta type and phase.
- Every phase deliverable in §9 has matching checklist items in PROGRESS.md.
- Gates T1–T5 appear identically in §8.3, §9, and PROGRESS.md.
- The §5.3 extensibility contract (new rule = one directory + one registry
  line) and the five-primitive proof-delta vocabulary are intact.
- Scope remains sequential-only; benchmark discipline (never tune on Tier A;
  full sets only at gates) remains stated.
- Rule phases agree everywhere they appear: catalog §5.2 is authoritative;
  §9 milestone rows and PROGRESS.md phase checklists must match it.
- Demo policies (§8.4) only require rules from phases at or before the phase
  that reviews them.
- Every §11 open item has a decide-by point; decided items carry their
  `→ decided rev N` annotation instead of being deleted.

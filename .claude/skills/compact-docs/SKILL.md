---
name: compact-docs
description: Keep the living docs (DEVELOPMENT_PLAN.md, PROGRESS.md, docs/calculus.md, docs/implementation-notes.md) lean, current, and oriented so a fresh agent session onboards fast without losing the big picture or the undone steps. Compress completed-work detail to a result + pointer, remove superseded/irrelevant content, and dedupe facts to their one home — losslessly, since the detail lives on in commits, reports, and frozen design docs. Use at phase gates, after a big feature lands, when a living doc grows unwieldy, or when onboarding feels slow. Executes THROUGH revise-plan (plan) and calculus (calculus/implementation-notes) — it is not a bypass.
---

# Living-document compaction

The living docs are a **summary layer**, not an archive. The full detail of every
decision, experiment, and landed feature already lives in immutable places — git
history + commit messages, `benchmarks/reports/`, and the frozen `docs/design/`
snapshots. So the living docs can be compressed aggressively **as long as every
pruned detail leaves a pointer to where it's preserved.** This skill does that
compression while guaranteeing the one thing a fresh session must never lose: the
big picture and the undone work.

This is the doc analog of `revisit-codebase`'s hygiene pass, on a different axis.
That skill removes drift / dead code / duplication (including doc drift). This one
manages **length and orientation** of the four living docs. Different concern,
different
trigger.

## The invariant it protects — the onboarding test

After a compaction pass, a **fresh agent session** reading only `PROGRESS.md`
(plus the plan's top matter) must be able to state, in ~2 minutes:

1. **What the project is** and the non-negotiables (N1–N3).
2. **Current phase** and the next gate.
3. **What was last completed** (headline + pointer, not the full narrative).
4. **The next 1–3 undone steps.**

If it can't, the compaction *failed its purpose* — fix orientation FIRST (restore
an accurate, prominent status surface: current phase, last-done, next-undone,
non-negotiables) before trimming anything else. Length is the secondary goal;
orientation is the point.

## What to compress or remove

Scan each living doc for:

- **Completed-work narratives.** A landed feature's multi-paragraph log/plan entry
  compresses to a **one-line result + pointer** (commit SHA / report / §13 rev /
  the frozen design doc) once it is no longer *recent* context. The rev-24
  discipline (§13 one-liner, PROGRESS newest-first & concise) is this rule already
  — generalize it.
- **Checked checklist items → one-liners.** A ticked `[x]` item in PROGRESS is a
  **one-liner**: headline + result + a pointer (commit SHA / the Log entry / a
  report). It does **not** re-tell the session's back-and-forth — the diagnosis,
  the dead ends, the A/B tables, the play-by-play — because that already lives in
  the Log entry and the commit message (one-fact-one-home). A completed item that
  runs to a multi-sentence paragraph is the most common bloat source, and it
  defeats the checklist's job as a *scannable status surface*. The moment an item
  goes `[ ]` → `[x]`, compress it (an in-flight `[ ]` item may still carry working
  detail). The one thing that survives compression is the **lesson** (see "never
  prune") — as a single clause in the one-liner, *"…; lesson: X was really Y
  (commit)"*, never a paragraph.
- **Superseded / irrelevant content.** Plans that were replaced, dead option
  branches, stale numbers, resolved TODOs. Remove — with a pointer if any residual
  value remains.
- **Duplicated facts** (one-fact-one-home violations). Keep the fact in its owner
  doc; leave a link from the other. (Rule semantics → calculus; guards/deviations
  → implementation-notes; scope → plan; status → PROGRESS.)
- **Closed items whose detail no longer teaches.** A closed deviation / decided
  §11 item / ticked gate keeps a one-line record; its verbose rationale (already
  in the commit) goes.

## What to NEVER prune

- The **big picture**, the **non-negotiables**, the **current phase/status**, and
  the **undone / next steps**. These are the reason the doc exists.
- **Lessons.** A failed experiment or a rejected approach is not "irrelevant" — it
  stops the next session repeating it. Compress it to one line — *"tried X → failed
  because Y (commit/report)"* — never delete it.
- Any fact whose **only home is the living doc.** If detail looks prunable but
  isn't archived anywhere, it is not yet safe to cut — either keep it, or move it
  to its correct home (calculus / a design doc / a report) *first*, then link.

## Procedure

1. **Run the onboarding test** (above). If it fails, restore the status surface
   first.
2. **Per living doc**, collect compressions in the categories above. For each,
   confirm the detail is preserved elsewhere (git/commit/report/design doc) — that
   confirmation is what makes the cut lossless.
3. **Compress**, always leaving a pointer for anything with residual value.
4. **Re-run the onboarding test** — verify the big picture + undone steps are
   still intact and prominent.
5. **Execute through the owning protocol, never around it:** plan changes go
   through `revise-plan` (rev bump + a one-line §13 entry); `calculus.md` /
   `implementation-notes.md` through the `calculus` skill; `PROGRESS.md` synced in
   the same commit. A compaction commit is a normal doc commit under those rules.

## Signals it's time (soft — no hard line caps)

- PROGRESS's older log entries out-bulk the current-phase status a reader needs.
- A plan section restates detail that now lives in `calculus.md` or a design doc.
- A closed-deviation / done-item entry is longer than the lesson it carries.
- Onboarding a session (or your own re-orientation) takes real effort to find
  "where are we + what's next."
- A phase gate or a large feature just landed (compact as part of closing it out).

## Guardrails

- **Never lose information silently.** Pruned detail moves to a pointer, not
  oblivion.
- **Don't over-compress.** A fresh reader still needs enough *why* to act, not just
  a list of *whats*. Terse ≠ cryptic.
- **Not a substitute** for `revise-plan` / `calculus` / the sync rule — it composes
  with them. It decides *when and what* to trim; they are *how* the edit lands.
- **N1–N3 and the roadmap are never trimmed away** — only their redundant retelling.

---
name: compact-docs
description: Compact Tactix living documentation on explicit request while preserving current requirements, decisions, semantics, deviations, and next steps. Use only when the user invokes it or asks for documentation compaction; it is not an automatic wrap-up step.
---

# Compact living documentation

Living docs are current-state indexes, not history. Git and primary reports
retain provenance.

1. Read `CLAUDE.md`, `DEVELOPMENT_PLAN.md`, `PROGRESS.md`, `docs/README.md`,
   `docs/calculus.md`, `docs/implementation-notes.md`, and the entire
   `docs/lessons.md` before editing.
2. Preserve N1–N4, ownership boundaries, active requirements, exact defaults,
   rule semantics, guard deviations, gates, open decisions, and the next
   actionable step.
3. Remove repetition, superseded prose, and session narrative. Replace copied
   detail with a link to its authoritative owner. Keep at most five concise
   recent changes in PROGRESS.
4. Route plan changes through `revise-plan` and rule/guard changes through
   `calculus`; do not conceal a substantive change as compaction.
5. Run `tools/docs/check-progress.py`, `tools/docs/check-doc-size.py`, and any
   affected semantic tests. Run broader tests only when the content change can
   affect generated inputs, parsers, or code-facing contracts.

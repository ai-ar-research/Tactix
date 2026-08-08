---
name: codex-revisit-docs
description: Conduct an exhaustive, independent documentation audit with Codex and propose a patch for Claude and the user to review. Use when the full documentation system needs reconciliation, compaction, ownership cleanup, or obsolete-file review; this is broader than compact-docs.
---

# Independent documentation audit

Start only from a clean tracked tree so findings and proposed edits are
attributable. Codex drafts; Claude reviews; neither treats the draft as approval.

## Pass 1: findings

Read `CLAUDE.md`, the plan and PROGRESS, all living documentation, every tracked
`docs/design/*.md`, every documentation checker, and the documentation-related
skills. Inspect code, tests, Git, reports, and logs only as needed to verify
claims.

Produce a complete inventory of:

- contradictions, stale facts, duplication, verbosity, missing ownership, and
  machine-parser risks;
- each design document and checker classified as KEEP, COMPACT, or DELETE;
- the authoritative destination for every fact that would move; and
- proposed validations.

Preserve N1–N4, active requirements, exact rule semantics and obligations,
current tasks, gates, deviations, decisions, and a self-contained next step.

## Pass 2: proposed patch

After Claude and the user review the findings, draft an `apply_patch` patch only
for approved scope. Do not directly edit the repository. Each file deletion
requires separate explicit user approval; uncertainty defaults to KEEP.

Claude reviews the patch for semantic loss, parser compatibility, and ownership
before applying any part, then runs documentation checks and affected tests.

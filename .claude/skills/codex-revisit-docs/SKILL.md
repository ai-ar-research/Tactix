---
name: codex-revisit-docs
description: Ask Codex for a read-only audit and proposed patch to compact and reconcile Tactix documentation, including CLAUDE.md, DEVELOPMENT_PLAN.md, living docs, docs/design, and documentation checkers. Use when documentation is repetitive, inconsistent, oversized, superseded, or hard for a fresh session to follow.
---

# Ask Codex to revisit documentation

Codex proposes an `apply_patch` patch; it never edits the repository. Claude
reviews the proposal with the user before applying any part.

## Scope and authority

Require Codex to read `CLAUDE.md`, `DEVELOPMENT_PLAN.md`, `PROGRESS.md`,
`docs/README.md`, `docs/calculus.md`, `docs/implementation-notes.md`,
`docs/lessons.md`, every tracked `docs/design/*.md`, and every
`tools/docs/*.py`. Also read `compact-docs`, `calculus`, and `revise-plan` skill
instructions and any parser affected by a proposed change.

Codex may inspect code, tests, Git, reports, and logs to verify claims. It must
not propose edits to source, tests, reports, logs, or generated reports.

Preserve N1–N4; every proof rule and its precondition, effect, proof delta,
soundness, and reconstruction obligation; every active requirement, task,
deviation, invariant, exact default, ownership boundary, reopening trigger,
roadmap item, and self-contained next step. Preserve machine-parsed structures.
Keep one fact in one authoritative home and replace duplication with pointers.

## Classifications

Classify every `docs/design/*.md` and `tools/docs/*.py` as:

- **KEEP:** unique or active requirements, decisions, checks, or rationale;
  also the default when deletion cannot be proved safe.
- **COMPACT:** useful current material with removable duplication or history.
- **DELETE:** fully superseded or obsolete, with all current facts migrated and
  all references updated in the same proposed patch.

Before proposing DELETE, search living docs, source, tests, skills, checkers,
and `git grep` for references. List unique facts and their destination. Never
delete `benchmarks/reports/` or `docs/log/` artifacts.

Treat active checker scripts as reviewed executable specifications, not code an
agent should regenerate. A checker may be deleted only if its invariant is
obsolete; a narrow checker should normally be kept or compacted.

## Invoke Codex read-only

From the repository root:

```bash
proposal_path=$(mktemp /tmp/tactix-doc-review.XXXXXX.md)
codex exec --cd "$PWD" --sandbox read-only --ask-for-approval never \
  --ephemeral --output-last-message "$proposal_path" - <<'CODEX_PROMPT'
Audit Tactix documentation and propose a patch. Do not edit the repository,
commit, push, run wrap-up, or generate a report.

Read completely: CLAUDE.md, DEVELOPMENT_PLAN.md, PROGRESS.md, docs/README.md,
docs/calculus.md, docs/implementation-notes.md, docs/lessons.md, every tracked
docs/design/*.md, every tools/docs/*.py, and the compact-docs, calculus, and
revise-plan SKILL.md files. Inspect code, tests, Git, reports, and logs as needed.

Find redundancy, inconsistency, obsolete chronology, unexplained shorthand, and
fragile or obsolete checker logic. Preserve every requirement, rule, active
task, invariant, default, deviation, reopening condition, and next step.

Classify every docs/design/*.md and tools/docs/*.py as KEEP, COMPACT, or DELETE.
Insufficient evidence means KEEP. Before DELETE, find every reference, migrate
every unique current fact, and update all live references in the same patch.

Return: (A) findings and authoritative resolutions; (B) the complete
classification table; (C) one internally consistent `*** Begin Patch` /
`*** End Patch` proposal; (D) exact validation commands; and (E) questions that
require user judgment. Do not omit unchanged files from the classification.
CODEX_PROMPT
sed -n '1,260p' "$proposal_path"
```

Continue reading the proposal in chunks until EOF.

## Review

Confirm Codex changed no files. Review the actual evidence, not only its
summary. Present the classification, patch, and Claude's objections to the
user. Obtain approval before applying anything, and separate approval for each
DELETE. Apply accepted hunks with `apply_patch`, then run the existing
documentation checks and affected CTest cases.

---
name: revise-plan
description: Modify DEVELOPMENT_PLAN.md when stable scope, architecture, contracts, roadmap, gates, catalog, or open decisions change. Use for substantive plan edits; Git history is the only revision history.
---

# Revise the plan

1. Read the affected sections, N1–N4, gates, open decisions, and current
   PROGRESS. Search all forward references that the change could invalidate.
2. Explain the proposed contract, scope, or gate change. Existing explicit user
   directions authorize the corresponding edits; do not ask again. For an
   unsettled substantive change, obtain approval before editing. Never weaken
   N1–N4 without an informed decision.
3. Make the smallest stable edit. Keep operational details in skills, current
   status in PROGRESS, exact rule semantics in the calculus, and evidence in
   reports.
4. Update PROGRESS only when active status, gates, or the next step actually
   change. Do not add dates, revision numbers, or a manual revision log.
5. Run documentation checks and affected semantic tests. Commit the plan and
   coupled changes together with a message that records the decision and reason.

When resolving an open decision, replace it with the concrete contract and its
rationale. If a commitment is removed without replacement, restore an explicit
open decision and decide-by point.

The phase 4.5 roadmap must retain proof-event correlation—mapping proof ranges
to emitting rules/applications—as a fixed prerequisite until implemented.

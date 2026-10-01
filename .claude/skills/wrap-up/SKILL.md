---
name: wrap-up
description: Finish every Tactix development session by validating the completed scope, auditing benchmark and certificate hygiene, synchronizing current status, generating Andrew's report, committing and pushing coherent work, and leaving an exact handoff. Do not compact docs automatically.
---

# Wrap up

1. Finish a coherent increment or record an intentional checkpoint. Follow the
   user's active scope and constraints; do not launch prohibited experiments or
   pushes to satisfy this workflow. Inspect relevant jobs and preserve user work.
2. Audit `benchmarks/` before landing. First map every active/pending SLURM job
   to its campaign and inspect launch files, manifests, predecessors, and
   inbound references. Preserve baselines, cache/tier authorities, reports,
   active campaigns, decision-changing run evidence, anything cited by living
   documentation, and run-local pins/runners needed to interpret a frozen
   result. Never alter a frozen artifact merely to make the tree look tidy.
3. Remove benchmark material only after proving it is no longer needed:
   campaign-owned certificate/checker/decompressed-input residue; abandoned
   scratch; empty failed-before-solver scaffolds; superseded unsubmitted clones
   with no unique result, trace, log, or manifest authority; and temporary
   scripts or directories whose completed purpose is owned by a maintained
   tool or frozen successor. Before deletion, require no active job, no inbound
   reference, no unique evidence, and no role in reproducing or checking a
   retained result. Use exact paths, record what and how many bytes were
   removed, rerun the residue scan, and leave uncertain candidates in the
   handoff rather than guessing.
4. Run the aggregate validation required by every owning skill used in the
   session. Route documentation-check failures to the document's owner; do not
   weaken checks to make the wrap-up green.
5. Keep `PROGRESS.md` as a resume surface: current authorities, active gates,
   in-flight jobs, and the exact next action, plus at most five one-line recent
   changes. Per-instance data and detailed experiment narratives belong in
   frozen run manifests/results or `benchmarks/reports/`; durable engineering
   invariants belong in their owning docs; Git owns ordinary chronology. If
   older PROGRESS detail is unique, preserve it through the explicit
   `compact-docs` archive process and replace it with a link. Never invoke
   `compact-docs` automatically; when compaction was not requested, report the
   size debt rather than deleting history.
6. Invoke `generate-report`; both TeX and PDF are mandatory.
7. Review the diff and repository status. Require the session's owned paths to
   be clean after landing, not a globally clean worktree. Stage explicit paths,
   commit coherent completed work, and push when within the authorized scope.
   A user-directed no-push handoff ends with local commits.
8. If an authorized push fails, hand off the commit SHA and exact error; do not
   rewrite history or conceal the failure. Report intentional no-push separately.
9. Summarize outcome, validation, benchmark cleanup, report path, commit/push
   state, remaining jobs,
   and next action.

The private transcript archive command may be shown on request but is never run
by Claude.

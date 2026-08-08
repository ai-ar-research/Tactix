---
name: wrap-up
description: Finish every Tactix development session by validating the completed scope, synchronizing current status, generating Andrew's report, committing and pushing coherent work, and leaving an exact handoff. Do not compact docs automatically.
---

# Wrap up

1. Finish a coherent increment or record an intentional checkpoint. Inspect
   relevant jobs and preserve user work.
2. Run the aggregate validation required by every owning skill used in the
   session. Route documentation-check failures to the document's owner; do not
   weaken checks to make the wrap-up green.
3. Update PROGRESS only with changed current facts, unfinished material findings,
   in-flight jobs, and an exact next step. Invoke `compact-docs` only when the
   user explicitly requested compaction.
4. Invoke `generate-report`; both TeX and PDF are mandatory.
5. Review the diff and repository status. Require the session's owned paths to
   be clean after landing, not a globally clean worktree. Stage explicit paths,
   commit coherent completed work, and push.
6. If push fails, hand off the commit SHA and exact error; do not rewrite history
   or conceal the failure.
7. Summarize outcome, validation, report path, commit/push state, remaining jobs,
   and next action.

The private transcript archive command may be shown on request but is never run
by Claude.

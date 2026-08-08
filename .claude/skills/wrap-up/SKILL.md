---
name: wrap-up
description: Finish every development session with tests green, living docs compact and current, an Andrew-facing LaTeX/PDF report generated, work committed and pushed, and PROGRESS containing a self-sufficient next step. Use immediately before ending any session.
---

# Wrap up a session

A fresh session reading CLAUDE and PROGRESS must be able to continue without a
question. Perform these steps in order.

1. **Stabilize work.** Finish the increment or leave an intentional checkpoint.
   Inspect the queue/processes through `slurm-jobs`; preserve user work and name
   any still-running jobs in PROGRESS.
2. **Validate.** Run `ctest` and the task-specific correctness checks. If code
   changed, run the required model/proof fuzz legs. Fix failures or record an
   explicit known-broken resume point.
3. **Synchronize.** Update only current facts in PROGRESS. Use `revise-plan` for
   plan changes and `calculus` for rule/guard changes. Ensure tasks and the next
   1–3 steps reflect reality.
4. **Compact.** Invoke `compact-docs`; run `tools/docs/check-progress.py` and
   `tools/docs/check-doc-size.py`. PROGRESS keeps at most five one-line recent
   changes, not a session narrative.
5. **Report to Andrew.** Invoke `generate-report` after synchronization. Use the
   user's suggested topic, otherwise the session's main task, otherwise
   `Tactix development status`. Both TeX and PDF are mandatory; compilation
   failure blocks wrap-up.
6. **Revalidate and land.** Rerun affected checks, review the diff, commit all
   coherent changes, push, and confirm the tracked tree is clean. Do not absorb
   unrelated user files.
7. **Handoff.** Summarize the change, test/fuzz/benchmark state, report path,
   in-flight work, and exact next step.

Optionally show—but never run—the user's private transcript archive command:

```bash
dir="$HOME/.claude/projects/$(pwd | sed 's#[/.]#-#g')"
cp "$(ls -t "$dir"/*.jsonl | head -1)" "session-$(date +%F).txt"
```

Never read, write, glob, or otherwise touch `session-*.txt`.

---
name: wrap-up
description: End-of-session checklist, run right before quitting so the next fresh session can just say "continue" and resume building. Ensures work is committed + pushed, tests are green, the living docs are current (PROGRESS reflects what was done + the next undone step; plan/calculus synced if touched), compact-docs has run to condense this session's ticked items to one-liners (and fully re-compact if a doc grew unwieldy), tasks reflect reality, and PROGRESS's resume anchor is accurate and self-sufficient. Also surfaces the (user-run) command to archive the session transcript to session-YYYY-MM-DD.txt. Use at the end of every work session.
---

# Wrapping up a session

**North star:** a fresh session with *no memory of this one* reads `CLAUDE.md`
(auto-loaded) + `PROGRESS.md`, and can continue building **without asking a
question**. Everything below serves that one goal.

## The resumability test

Before ending, confirm: reading only `CLAUDE.md` + `PROGRESS.md`, a fresh session
can state the current phase, what was just done, and the **exact next step** — and
start it. If it can't, that gap is the first thing to fix (usually the PROGRESS
resume anchor, step 6).

## Checklist — in order

1. **Land the work.** Every change committed with a clear message and **pushed**.
   `git status` clean — no stray uncommitted edits. If work is intentionally
   mid-flight, it is a *committed* checkpoint and PROGRESS says where it stands.
   Never leave the tree dirty for the next session to discover.
   **Cluster leftovers (slurm-jobs skill):** `squeue -u hwu` shows only the
   user's own jobs — cancel session leftovers by explicit job id; delete
   uncited green `tools/fuzz/slurm-*.log` files (commit the ones PROGRESS
   cites as evidence); bench run dirs self-prune on clean aggregation.
2. **Green tests.** The unit suite (ctest) passes; if solver code changed, a quick
   fuzz (including a random-policy round, proofs checked) is green. If anything is
   red or broken, **stop** — either fix it, or record it explicitly in PROGRESS as
   the known-broken resume point. Never leave silent breakage.
3. **Sync the living docs** (the mandatory sync rule). PROGRESS ticks the items
   done + a concise newest-first Log entry, and its next-unchecked item is the
   accurate resume anchor. Plan changed → `revise-plan` (rev bump + §13). Rules or
   guards changed → `calculus` (`calculus.md` + `implementation-notes.md`).
   One-fact-one-home: link, don't restate.
4. **Compact** — **run `compact-docs` every wrap-up.** It condenses the items this
   session ticked to one-liners and re-compacts any doc that grew unwieldy.
   compact-docs owns the rules for what and how; wrap-up just guarantees it runs.
   **First, run the archive check — it is a script, not a judgement call:**

   ```
   tools/docs/archive-log.py --check     # exit 1 = archiving is due
   tools/docs/archive-log.py             # do it
   ```

   `PROGRESS.md`'s Log holds the **current ISO week only**; completed weeks move
   verbatim to `docs/log/<ISO-week>.md`. If it archived anything: **update
   `docs/log/README.md`'s index table**, and commit the archives together with
   PROGRESS in ONE commit (the entries must never exist in both places, or in
   neither). Entries move **verbatim** — the Log is the durable record, so
   compression happens *before* an entry is written, never on the way out.
   **If `--check` prints `⚠ OVER BUDGET`**, archiving cannot help (only one week
   is present) — that is a signal to *compress* in step 4 proper: ticked
   checklist items to one-liners, and any Log entry whose detail already lives in
   a frozen report down to result + pointer.

   *Why a script:* this project's own diagnosis (2026-08-04, cause E) is that
   prose triggers do not fire — five rev-77 closers with natural-language
   conditions went off silently, one recording in its own text that "the
   condition was met and nothing was revisited". Make it mechanical, not
   remembered.
5. **Prune persistent memory.** Memory is a small set of *currently true*
   durable facts, not an archive — it is loaded into every future session, so a
   stale entry actively misleads and an accreting one crowds out what matters.
   For each memory the session touched or contradicted:
   - **Delete what is no longer true.** Do NOT append "superseded" notes — that
     is how memory blows up, and it forces every future reader to parse a
     contradiction. Rewrite the entry to what is true now, or remove it.
   - **Delete what the repo now records.** Project state (which rules are on,
     current benchmark numbers, landed features) belongs in PROGRESS / reports /
     git, not memory. Memory keeps what those do *not* say: the user's
     preferences, recurring hazard shapes, and *why* an approach was rejected.
   - **Watch for the dangerous shape:** a confidently-worded CONCLUSION about a
     cause ("X is inherently bad, don't retry it"). Those age worst, because a
     later session may find the cause was an artifact — and the entry is then the
     reason a good direction stays closed. Prefer recording the measurement over
     the causal story, and re-check such entries whenever the session revisits
     that area.
   - Keep `MEMORY.md`'s one-line pointers in step with the files, and check
     `[[links]]` still resolve after any deletion or rename.
6. **Tasks reflect reality.** Open work is captured (as tasks and/or PROGRESS
   items); nothing in-flight lives only in this session's memory.
7. **Write the resume anchor.** Make PROGRESS's *next step* self-sufficient — a
   fresh session with zero context can act on it: what to do next, where the code
   is, and any in-flight nuance or gotcha. This is the single most important
   artifact for "continue."
8. **Summarize to the user:** what changed this session, current fuzz/benchmark
   status, and the next step.
9. **Offer the transcript-save command** (a **user**-run action — agents must
   never touch `session-*.txt`; a hook enforces this, and the real handoff is
   PROGRESS + commits, steps 3/6). Show the user this command to archive the
   session; do **not** run it yourself:
   ```bash
   dir="$HOME/.claude/projects/$(pwd | sed 's#[/.]#-#g')"
   cp "$(ls -t "$dir"/*.jsonl | head -1)" "session-$(date +%F).txt"
   ```
   (`~/.claude/projects/<cwd-with-/-and-.-as-dashes>/` holds the session JSONLs;
   the newest is the active session — target a specific UUID if several run at
   once. Swap the `cp` for a `jq` extract if a human-readable dump is wanted.) The
   transcript is a convenience archive only; PROGRESS is the resume anchor.

## Guardrails

- Never end with a dirty tree, red tests, or an unpushed commit, silently.
- The next step must live in `PROGRESS.md`, never only in chat/memory.
- Don't over-document the session's play-by-play — the resume anchor + a concise
  Log entry is enough (that is what compact-docs enforces).
- **Never touch `session-*.txt`** — do not read, write, cat, grep, or glob them.
  They are user-private (hook-enforced); only *surface* the save command (step 8)
  for the user to run.

---
name: wrap-up
description: End-of-session checklist, run right before quitting so the next fresh session can just say "continue" and resume building. Ensures work is committed + pushed, tests are green, the living docs are current (PROGRESS reflects what was done + the next undone step; plan/calculus synced if touched), compact-docs has run if a doc grew unwieldy, tasks reflect reality, and PROGRESS's resume anchor is accurate and self-sufficient. Also surfaces the (user-run) command to archive the session transcript to session-YYYY-MM-DD.txt. Use at the end of every work session.
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
2. **Green tests.** The unit suite (ctest) passes; if solver code changed, a quick
   fuzz (including a random-policy round, proofs checked) is green. If anything is
   red or broken, **stop** — either fix it, or record it explicitly in PROGRESS as
   the known-broken resume point. Never leave silent breakage.
3. **Sync the living docs** (the mandatory sync rule). PROGRESS ticks the items
   done + a concise newest-first Log entry, and its next-unchecked item is the
   accurate resume anchor. Plan changed → `revise-plan` (rev bump + §13). Rules or
   guards changed → `calculus` (`calculus.md` + `implementation-notes.md`).
   One-fact-one-home: link, don't restate.
4. **Compact if needed** (`compact-docs`). If a living doc grew unwieldy or
   re-orienting took real effort, run the compact-docs skill — condense completed
   detail to result + pointer, keep the big picture and undone steps prominent.
5. **Tasks reflect reality.** Open work is captured (as tasks and/or PROGRESS
   items); nothing in-flight lives only in this session's memory.
6. **Write the resume anchor.** Make PROGRESS's *next step* self-sufficient — a
   fresh session with zero context can act on it: what to do next, where the code
   is, and any in-flight nuance or gotcha. This is the single most important
   artifact for "continue."
7. **Summarize to the user:** what changed this session, current fuzz/benchmark
   status, and the next step.
8. **Offer the transcript-save command** (a **user**-run action — agents must
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

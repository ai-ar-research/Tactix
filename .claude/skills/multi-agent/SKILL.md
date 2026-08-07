---
name: multi-agent
description: Run several agents in parallel on this repo — scoping them so they cannot collide, and merging their branches without losing work. Use whenever launching more than one agent, or merging an agent's worktree branch. Encodes the failures this project has actually had: a merge that silently absorbed 264 lines of another agent's work, merges that dropped five tick marks, two agents contending for one build path, and a killed process that let a stale edit re-apply.
---

# Running several agents at once

Parallel agents are now routine here, and every failure this workflow has had
was a **scoping or merging** failure, not a reasoning failure. The rules below
are each the residue of one.

## Scoping — decide the file sets BEFORE launching

- **Give every agent a disjoint file set, and say so in the brief**, in the form
  "you own X; do NOT touch Y, Z — another agent holds them; if you find a
  problem there, REPORT it, don't fix it." Agents respect this when told and
  cannot infer it otherwise.
- **`isolation: "worktree"`** for anything that edits files. An agent in the
  main tree shares your working directory: its `git add`, its build, and its
  checkout all land on top of yours.
- **The shared files are `PROGRESS.md`, `DEVELOPMENT_PLAN.md`,
  `docs/calculus.md` and `docs/implementation-notes.md`.** Every agent wants to
  touch them. Either give exactly one agent the right to edit each, or accept
  that you will resolve those conflicts by hand — and expect them to be
  semantic, not textual.
- **Re-examine the premise if you widen an agent's scope mid-run.** One agent
  was placed in the main tree *because* it made no source edits; its scope was
  later widened and the premise was never re-checked, which is how a `git add -A`
  swept 264 lines of another agent's in-progress work into an unrelated commit.

## While they run

- **Never `git add -A`, `git add .`, or `git commit -a`.** Stage explicit paths.
  This is the single rule that would have prevented the worst incident.
- **Two agents that measure need two `build/pin-*` directories.** A submitted
  array owns its path until it finishes; `tools/bench/buildlock.py` enforces it
  and the second `--acquire` will correctly refuse. Build both from one commit
  and check the binary SHAs match.
- **Correctness probes may overlap; a window whose output is a TIME may not.**
  Authorising "small sbatch probes are fine" in several briefs at once put 336
  job records and 4 OOM kills on a pinned node and voided a tier window. If any
  agent is timing something, the others do not run solvers.
- **Do not kill a stuck process an agent owns without thinking about what
  resumes.** Killing a hung `cp -i` let the parent shell continue and re-apply a
  stale edit on top of another agent's committed tree.

## Merging — where the ticks get lost

- **Read the branch, not the tip commit.** A branch whose tip looks like a
  7-line doc edit held six commits including a 435-line rule implementation;
  dismissing it by its tip meant that work was later redone from scratch.
- **Checklist marks are the classic silent loss.** Resolving a `PROGRESS.md`
  conflict in favour of "your" side reverts the other side's `[x]` marks, and
  nothing fails — the file still parses, the tests still pass, and the item
  simply reads unfinished. After merging, diff the checkbox lines specifically:
  `git diff <before> -- PROGRESS.md | grep -E '^[-+].*\[[ x]\]'`.
- **Build before trusting ctest.** After a conflicted merge, ctest once reported
  44/44 while the build had 12 errors — it ran the previous binary.
- **A merge commit's message must describe what it merges.** One said "comments
  and docs only" while carrying 411 lines of tooling. If you discover this after
  pushing, add an errata commit; do not rewrite pushed history.
- **Adding a rule renumbers things other agents' tests pin** — the frozen C-ABI
  rule ids (`tests/test_api_c.c`) and the stat-id boundary. Two agents each
  adding a rule will both need to update the same golden list.

## Reporting

An agent's final message is its whole report — you see nothing else, and neither
does the user. Ask for: what changed and why, what it recommends but did NOT do
with the reason it stopped, and anything wrong it found outside its scope so you
can route it. The third of those has been the highest-value section every time.

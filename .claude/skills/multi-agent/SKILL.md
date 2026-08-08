---
name: multi-agent
description: Coordinate multiple Claude agents on Tactix using isolated worktrees, disjoint write ownership, scoped commits, and serialized measurements. Use only when work is explicitly delegated or parallelized.
---

# Parallel Tactix work

Split work only into independently verifiable tasks. Give each agent an isolated
worktree under `.claude/worktrees/`, an explicit file or subsystem boundary,
acceptance criteria, and a required scoped commit.

## Invariants

- Writes are disjoint. Shared files such as `PROGRESS.md`, the plan, calculus,
  implementation notes, registries, and build lists have one owner: the primary
  Claude agent unless ownership is explicitly transferred.
- Agents stage explicit paths; never use `git add -A`, `git add .`, or
  `git commit -a`.
- Each agent reports commit SHA, changed paths, validation, assumptions, and
  unresolved findings. The primary reviews and integrates commits deliberately.
- Timing measurements are serialized or use isolated pinned builds and matched
  allocation shapes. Parallel timing is not comparable evidence.
- User-owned untracked files and another agent's worktree are never cleaned or
  absorbed.

Convert incident-specific warnings into these reusable invariants. Do not keep
a narrative incident log in the skill.

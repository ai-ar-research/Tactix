---
name: multi-agent
description: Coordinate explicitly delegated Tactix work with disjoint write ownership, scoped commits, and pinned matched measurements; use isolated worktrees only when the user requests them.
---

# Parallel Tactix work

Split work only into independently verifiable tasks. Follow `CLAUDE.md`'s root
checkout rule; isolated worktrees require an explicit user request. Give each
agent an explicit file or subsystem boundary, acceptance criteria, and a
required scoped commit. Do not leave authoritative work under `.claude/worktrees/`.

## Invariants

- Writes are disjoint. Shared files such as `PROGRESS.md`, the plan, calculus,
  implementation notes, registries, and build lists have one owner: the primary
  Claude agent unless ownership is explicitly transferred.
- Agents stage explicit paths; never use `git add -A`, `git add .`, or
  `git commit -a`.
- Each agent reports commit SHA, changed paths, validation, assumptions, and
  unresolved findings. The primary reviews and integrates commits deliberately.
- Timing comparisons follow `DEVELOPMENT_PLAN.md` §8.1 and §8.3: separate
  immutable pins, matched allocation shapes and concurrent candidate/baseline
  arms in the same measurement window. Serialize edits to shared infrastructure,
  not comparison arms; unrelated concurrent runs are not matched evidence.
- User-owned untracked files and another agent's worktree are never cleaned or
  absorbed.

Convert incident-specific warnings into these reusable invariants. Do not keep
a narrative incident log in the skill.

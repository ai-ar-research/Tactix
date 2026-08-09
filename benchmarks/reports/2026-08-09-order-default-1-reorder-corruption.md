# ORDER-DEFAULT-1: focused Reorder queue-corruption diagnosis

**Date:** 2026-08-09

## Result

The catastrophic default-on Reorder regression was an implementation bug, not
a bad reference default or interval. Tactix's focused Reorder sorted all
permanent variable identities. DefaultPolicy had already disconnected two or
more retired variables from its VMTF queue, so passing those identities through
`heuristics_queue_bump` dequeued them using stale links and corrupted the queue.
Kissat's executable path sorts only `ACTIVE` variables (`reorder.c:132-138`).

The repair sorts only non-retired Tactix variables, explicitly clears links on
queue removal, rebuilds the stable heap only from the owning policy's connected
queue, and adds structural queue/heap invariants. This preserves the
policy-neutral API: a policy that intentionally keeps a retired identity
connected can still select it through N1, while DefaultPolicy does not
reactivate identities it retired.

## First-divergence evidence

Commit `6451a0d` added an environment-gated semantic TSV trace and exact
comparator. SLURM job `1605445` ran only `tr-n170-s053.cnf` and
`tr-n220-s013.cnf`, with Tactix Reorder 0/1/2 and every later parity increment
off. Reorder 0 and stable-only mode 1 solved both. Both-mode Reorder was killed.

For `tr-n170-s053`, off and on traces are identical through record 23,553. At
record 23,554, the off arm makes decision `-46`; the on arm enters focused
Reorder at 10,226 conflicts. Its pre-event snapshot has 168 queue entries, 166
unassigned-active variables, and two retired heap members. The old Reorder's
post-event snapshot has only 90 queue entries. That population loss is the
first abnormal state, before any changed search trajectory can confound the
diagnosis.

Kissat's `--log=2` build was also exercised, but logging every low-level action
produced about 125 MB per ten-second task and prevented these fixtures from
reaching a useful Reorder anchor. The decisive cross-reference is therefore
the executable `ACTIVE` candidate filter plus Tactix's lock-step off/on trace,
not a runtime comparison distorted by Kissat logging overhead.

## Focused verification

Repair commit `57642ef`, SLURM job `1605455`:

- `tr-n170-s053`: Reorder 0/1/2 all return UNSAT; the mode-2 DRAT proof verifies.
- `tr-n220-s013`: Reorder 0/1/2 all return SAT; the mode-2 model satisfies all
  937 clauses and assigns all 220 variables.
- Every repaired pre/post Reorder snapshot preserves queue population exactly.
- Both fixtures report zero retired re-bumps and zero substituted decisions.

The Release suite passes 61/61. Debug Reorder and random-policy tests pass with
ASan/UBSan (leak detection disabled because LSAN cannot operate in this
container).

## Tier A gate

Run `benchmarks/runs/20260809-160936-A-reorder-fixed-old/` compares the same
`57642ef` binary with Reorder off and repaired Reorder on, under
`slurm partition=cpu-dense-hwulab-q cpus/task=1 mem/cpu=8G` and the frozen
Tier A manifest. Correctness is clean in both arms.

| Arm | Solved | SAT / UNSAT | PAR-2 |
|---|---:|---:|---:|
| Reorder off | 199/200 | 82 / 117 | 0.65 |
| Reorder fixed | 199/200 | 82 / 117 | 0.65 |

The only unsolved instance is the same 30-second timeout in both arms
(`tr-n300-s055.cnf`). Reorder is 0.5 seconds faster in aggregate, but
`tr-n280-s057.cnf` alone can flip that timing sign, so this is a parity/bug-fix
acceptance rather than a speedup claim. Reorder reaches 32 instances, up to ten
applications. The previous 24–25 lost solves and widespread B/C collapse are
gone at the Tier A gate; B/C remains intentionally paused until every later
cumulative increment passes Tier A.

## Reopening condition

Reopen `ORDER-DEFAULT-1` on any queue/heap invariant failure, retired re-bump,
substituted DefaultPolicy decision, correctness failure, or Tier A solve-count
loss attributable to Reorder. Do not reopen solely because a noisy paired
timing sign changes while solved counts and trajectories remain comparable.

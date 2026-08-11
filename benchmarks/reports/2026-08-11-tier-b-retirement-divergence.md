# Tier B retirement divergence: idempotent heuristic deactivation

**Date:** 2026-08-11
**Decision:** ship the repair; it gains four Tier B solves with no measured
loss and closes the selected divergence block.  The three remaining timeouts
are a separate performance frontier.

## Result

Tactix treated every retirement event as the variable's first removal from the
VMTF queue.  Substitute and BVE can report the same permanent identity before
DefaultPolicy drains the event queue.  The second removal follows the explicit
disconnected-link sentinels, erases the queue endpoints, and eventually makes
focused branching read an invalid variable.  The repair makes heuristic
deactivation idempotent: an already disconnected variable is removed from the
stable heap if necessary, the focused search cursor is re-seated if necessary,
and the VMTF links are not touched again.

A production counter confirms that this path is real, not a timing
perturbation.  The repaired `circuit_48in64out...seed4` run absorbed **7 repeated
retirement reports**, returned SAT, and its model satisfied all 192,064 clauses.

## Causal localization

The preregistered cross-section used the early-failing SAT circuit, the
long-running SAT `201311...`, the early-failing UNSAT `b22_1`, and held
`FmlaEquivChain` out as a second rule-heavy family.  Jobs and artifacts are
under these run directories:

- `20260811-tierb-divergence-selected-v1`: production event traces;
- `20260811-tierb-circuit-gdb-v1`: focused crash backtrace;
- `20260811-tierb-circuit-asan-v2`: sanitizer confirmation;
- `20260811-tierb-circuit-queue-arms-v1`: one-variable-family ablations;
- `20260811-094828-B-sweepresume-default`: full Tier B Sweep-resumption cut.

GDB localized the circuit crash to
`heuristics_queue_next_unassigned → DefaultBranching::pick`; ASan identified
the invalid read at the focused queue walk.  The same binary crashed without
proof output, and the preceding P4 pin also crashed, excluding the proof writer
and the latest Sweep proof repair.

The one-sided circuit cuts were decisive:

| Arm | Result | Time | Validation |
|---|---:|---:|---|
| shipped default | signal 11 | 39 s | — |
| Reorder off | signal 11 | 26 s | — |
| fresh-Factor demotion off | signal 11 | 32 s | — |
| variable retirement off | SAT | 40 s | model clean |

Disabling Sweep resumption was also rejected.  In the full paired Tier B run it
lost `mp1-klieber`, `pyhala-braun`, and `square`, despite recovering
`PRP_30_38` and `b22_1`; solved count fell from 21 to 20 and PAR-2 rose from
186.22 s to 197.08 s.  Sweep remains enabled and resumable.

## Full Tier B gate

Run: `benchmarks/runs/20260811-100705-B-retire-idempotent/`
Manifest: `benchmarks/tiers/B.list`, SHA-256
`5130866032ae706e48f17f0db431bb66e08536f69640b5a62cb90fd3e9c6bcef`
Shape: `cpu-dense-hwulab-q`, 1 CPU/task, 8 GiB/CPU, 300 s/instance
Source base: `83238d1`

| Arm | Binary SHA-256 | Solved | SAT / UNSAT | PAR-2 |
|---|---|---:|---:|---:|
| idempotent retirement | `0cb0d4c3…25f1b` | **25/28** | 12 / 13 | **116.58 s** |
| pinned current | `188f304f…c9b2` | 21/28 | 9 / 12 | 187.33 s |

The candidate uniquely recovers four formerly unsolved instances:

| Instance | Candidate | Pinned current | PAR-2 gain |
|---|---:|---:|---:|
| `summle_X4044_steps7...` | SAT, 246.55 s | UNKNOWN | 353.45 s |
| `circuit_48in64out...seed4` | SAT, 43.61 s | UNKNOWN | 556.39 s |
| `2013113162201nw...` | SAT, 10.35 s | timeout | 589.65 s |
| `b22_1` | UNSAT, 118.95 s | UNKNOWN | 481.06 s |

On the other 24 instances the largest paired elapsed-time change is 0.42 s;
there is no loss or trajectory-scale regression.  Every scored SAT model and
UNSAT DRAT certificate in both arms passes its independent checker.  The final
counter-only rebuild has SHA-256 `1a0c6a4a…320a`; its focused circuit model
check passes, and its complete Release suite passes 69/69 under SLURM job
`1609646`.

For context, the same-day pinned reference window
`20260811-084039-B-p4cvg-fixed` scored Kissat 28/28 at 10.16 s PAR-2 and CaDiCaL
27/28 at 41.65 s.  The repair removes a correctness-adjacent implementation
failure and materially narrows Tactix's aggregate, but does not close the Tier B
performance gap.

## Remaining frontier

The only candidate timeouts are SAT `mod4block`, UNSAT `FmlaEquivChain`, and SAT
`PRP_30_38`.  They should form the next primary divergence set.  A secondary
trace on the now-solved `b22_1` found an earlier source-backed difference:
Kissat's first Congruence pass derives an equivalence and immediately
Substitutes external variable 14812 by `-10713`, while Tactix's Congruence pass
reports no change and enters Sweep on the unreduced formula.  That missed
gate/equivalence remains a plausible explanation for the residual 118.95 s
versus 9.59 s gap, but it was not needed to accept this retirement repair.

Reopen this defect on any duplicate-deactivation queue mutation, nonzero
retired-variable re-bump, focused branch outside the connected queue, or
proof/model failure attributable to retirement handling.

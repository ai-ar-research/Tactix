# Shared-blocker propagation cost experiment

## Decision and evidence

Keep 6448 as an isolated, unpromoted execution-cost experiment while the
activation-repair full-50 campaign 6439 proceeds. No runtime or memory gain is
claimed. Production, fixed references, cpu-q 5784, all submitted source trees
and prior reports remain unchanged.

The actual Rank 32 Release profile 6372/6378 attributes 34.38% of candidate
self samples to `kernel_propagate`. Its current predecessor had the established
three-million-conflict decision closure and equal full native conflict/decision
totals; these facts motivate cost diagnosis but do not prove identical physical
formula state or propagation work. The separately completed 6383 cache experiment
was essentially neutral and is not the basis for another cache optimization.

Reference inspection found a small representation-neutral control-flow
difference. Kissat `src/proplit.h` copies a watch's optional second word, then
shares the satisfied-blocker exit before processing the binary/large cases.
CaDiCaL `src/propagate.cpp` also checks its blocking literal before the watch
type, although its watch representation is different. Tactix had duplicated
that blocker exit inside the two type-specific bodies.

The new code consumes the same complete one- or two-word entry, tests the
same blocker once, and then enters the unchanged type-specific body. Copying
the reference word is not dereferencing a clause and charges no tick. Garbage
handling, assignment/reason order, ignored-candidate tests, watch relocation,
searched positions, delayed-buffer ownership, snapshots, conflict exits and
every effort counter retain their contracts. No guard, public action, proof
rule, scheduler admission/budget, layout, allocation strategy or constant changes.
This is a bounded hot-path experiment, not the unapproved FlatLists redesign.

## Source and actual predecessor controls

[6448 build manifest](../runs/20260909-6448-shared-blocker-build/manifest.json)
owns the fresh `.claude/worktrees/runtime-shared-blocker-6448` successor to
6424, its full file inventory, zero-context incremental/cumulative patches,
three independent builds and logs. Release and B/C SHA256:
`1676979b15aaa2c0ec7caaff82760c0b0b043c6973876fbb66ffb8357c288caa`;
trace SHA256:
`bcd8fafe10be1d7e5503f84b059c1fce0ac01d0db771138f3ced107ebc6b5405`.
Only `src/kernel/kernel.cpp` and new `tests/test_shared_blocker.cpp` differ
from the frozen parent. No source used by a running campaign was modified.

Test consideration: existing propagation tests cover broad rule behavior, but
the newly factored exit needs focused mixed-width entry/cursor coverage.
The new 180 fixed cases cover both binary/large orders, `q==p` and `q<p`, prior
watch relocation or garbage removal, satisfied entries followed by assignment
and conflict tails, ignored large candidates, recording and persistent probing,
and warm-up continuation beyond conflicts. They snapshot exact trail/reason,
value, arena/watch pose, cursor, tick, counter and rollback state. The fixture
deliberately starts at a selected trail cursor to isolate that mixed watch list;
it does not claim a solved model or independently checked final proof.

[Actual-object control 6450](../runs/20260909-6448-shared-blocker-build/control-6450.json)
links identical fixtures against actual old/new object sets, not a mirror:

| Fixture | Cases | Old/new result |
|---|---:|---|
| shared blocker | 180 | identical state digest `4396a8e9f5c97c5f` |
| propagation cursor | 224 | identical state digest `f82475e400a7f62b` |
| existing kernel suite | full executable | both pass |

Both independent Release suites pass 109/109, and trace passes 108/108.
The micro-proof and Sweep proof tests execute and pass, not skip. Actual
generated `kernel_propagate` symbol size changes from 4129 to 4105 bytes;
the candidate-aware `propagate_literal` specialization stays 3865 bytes.
The 24-byte reduction establishes changed code generation, not reduced cycles
or a runtime gain. Control artifacts retain source/object/log hashes.

## Gate, timing and interpretation

[6451 campaign](../runs/20260909-6451-shared-blocker-BC-gate/manifest.json)
retains the unchanged B28+C5 matrix, immutable production comparison and two
separate candidate builds. Job **1677969**, submitted
**2026-09-09T08:42:08 EDT**, uses the matched 12-logical-CPU/48000-MiB contract,
one bound solver, 1200 solver-only seconds, 16000 checker seconds and a five-hour
allocation, without hard-node constraint or array throttle.

Validator 6453 requires all 66 expected solver outcomes and all 33 candidate
certificates/non-timing-statistic vectors exactly matching 6424. Pending checks
allow continuation but are not accepted certificates. A failed identity is not
silently discarded or priced as a speedup. A rejected proof/model immediately
interrupts affected development under the existing correctness workflow.

Helpers 6454/6455/6456 predeclare three independent paired waves on ranks 32,
35 and 45 versus 6424. Rank 32 is the profile-primary cost witness; 35 and 45
protect the adverse search-work case and newly repaired trajectory. Both arms
bind the same full-run parent work/proof oracle for each rank. Require the
complete 6453 solver gate before preparation; use all three paired ratios and
all arm spreads, reversing middle-wave order. The existing 2% arm-spread rule
governs an absolute claim. No speedup assumed, selective extra waves, parameter
tuning or fixed reference refresh. A neutral/noisy outcome is not a convergence
improvement. Full50, memory and source-fuzz acceptance remain independent.

## Continuing activation measurement and limits

Activation campaign 6439/job 1677930 (submitted 08:32:43 EDT) remains live.
Do not run frozen-output helper 6441 until all 50 solver records are available;
do not score a convenient partial subset. Helper 6443 may prepare its separate
changed-work activation/6383 timing waves as soon as current ranks 32/35/45
finish, without waiting for their proofs. At 08:43 EDT those oracles were still
pending; the helper refused without creating a campaign or submitting anything.
Audit 6457 owns the later exact solver/checker accounting and completed cleanup.

Rank 45's 184,622-row/100K closure and Rank 32's 153,176,971-row/3M closure
remain frozen for 6424. They are not automatically inherited as a full-run
6448 trace identity from the micro-tests. The latest measured full-50 source
remains 6383 at 1.196328 times fixed Kissat; no new aggregate score is available.
The accepted fresh reevaluation remains 100 checked results and zero residue.

Memory convergence remains open. Prior memory diagnosis places peaks in dense
payload growth/compaction; the proposed private realloc-backed redesign still
requires approval. The separate Rank 50 material tracker is also unapproved.
No fuzz job/artifact inspection, denied transfer/push retry, manual deletion or
source promotion occurred. Claude-owned current-source fuzz is unavailable and
blocks promotion, not this isolated investigation. Documentation stays within
its caps. Andrew's local report is
`report-to-andrew/20260909-0845-shared-blocker/report.pdf`, not externally sent.
The first PDF had a 3.23338-point overfull line, caught by the landing audit
before it wrote a checkpoint. Its original log is retained; the paragraph was
reflowed and two clean compilations are required before the audit is rerun.

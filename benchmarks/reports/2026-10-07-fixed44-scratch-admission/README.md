# Fixed44 scratch admission: root cause and shared-floor profile

Status: runner change committed (`cf2440956`), mechanics validated on Slurm,
launch recipe prepared, and no candidate campaign submitted. Nothing here is
solver evidence.

## Root cause of the b26 `scratch_node_busy` refusals

`fixed44-cooperative-v1` (`tactix-anonymous-scratch-capacity-v2`) takes a
**non-blocking** exclusive `flock` on one stable per-node file before scratch
admission. A contender fails at once with retryable `scratch_node_busy`. The
profile then requires 400 GB free (6 handles x 60 GB plus a 40 GB reserve).

- Node `/tmp` is the root XFS filesystem: 462.6 GB total and 403-445 GB
  available in recorded admissions (node002: 408.85 GB in pilot 1755641).
  Slurm reports `TmpDisk=0` on all 16 cpu-q nodes, so `--tmp` cannot place
  tasks. Any positive request would be unschedulable.
- Slurm packs 4-CPU/32 GiB tasks (up to 16 per 64-CPU node). A refused task
  exits in 1-2 s and frees its slot. The next pending task lands on the same
  node and is refused too, so the whole array drains onto the lock holders'
  nodes. b26 original: node001 20, node015 10, node002 6, node016 2 refusals.
  retry01: 36 of 37 refusals on node015.
- Classification: (a) under the 360 GB worst-case charge, two tasks truly
  cannot share one node (2 x 400 > 445), so the mutex is consistent with the
  charge. (b) The charge and the fail-fast policy are over-conservative.
  Observed demand is far smaller: 0e full44 (no capacity contract) proof
  streams were at most 56.3 GB per task. Co-located sums were about 81-150 GB
  per node with 4-7 tasks. am546 UNSAT proofs were at most 20.3 GB. No route
  holds six maximal files at once. The available alternatives either
  serialize (about 15 x 5.4 h per node) or need whole-node-like allocations
  (an October 9 start, per the v2 report).

## Fix: `fixed44-shared-floor-v1` (`tactix-anonymous-scratch-capacity-v3`)

The owning code is `tools/bench/anonymous_scratch.py`
(`NodeScratchLedger`, `admit_shared_scratch`, `ScratchFloorMonitor`,
`AnonymousScratch.accounted_bytes`). It is wired up in
`tools/bench/runtime_task.py` (`shared_floor_capacity_contract`, `main`,
`measured` masks SIGUSR1 around `Popen`) and in `tools/bench/experimental_tierd.py`
(the profile, with the wall including the admission wait). v1 and v2 behavior is unchanged.

- **Ledger.** There are 64 stable private slot files in
  `/tmp/tactix-sat-solving-<uid>/` (the 0700 directory the lock already used).
  A participant owns one slot through an exclusive `flock` held until exit, so
  the kernel frees a dead owner's slot. The slot holds one fixed-width record
  (accounted bytes, state, job/task) and never certificate bytes.
- **Admission.** There is no mutex. A task needs 100 GB available, which is
  one maximal 60 GB file above the 40 GB reserve. It polls every 15 s for up
  to 300 s. A timeout gives a retryable `scratch_admission`. The 60 GB
  `RLIMIT_FSIZE` and the six-handle bound are unchanged. The wall
  `5000+14400+300+900` rounds to the same 20700 s (05:45:00).
- **Floor monitor.** It is a 1 s thread. Each poll publishes the task's
  allocated anonymous bytes (`st_blocks`) and reads `statvfs`. When available
  space is below 40 GB, exactly one participant stops itself: the live one
  with the most accounted bytes. The others wait while it is `terminating`.
  Below 20 GB, every participant stops, which covers foreign writers and
  solver-internal temp files that are not accounted. A monitor failure also
  stops the task (fail-closed). A stop is a thread-directed SIGUSR1 to the
  main thread. It raises `ScratchCapacityError('scratch_enospc', trigger=…)`
  through the existing interruption path: the child process group gets
  SIGKILL, completed solve/check stages are preserved, the O_TMPFILE handles
  close, and the storage returns. The record is incomplete with
  `retryable=true`. It is never a verdict, a rejection, a timeout, or a
  closure. The scorers need no change because the reasons already exist.
- **Justification.** No array throttle and no node constraint are used. The
  root filesystem is still protected by a hard 20 GB floor plus a 40 GB reserve. A task
  stops only when the node's real aggregate demand exceeds capacity. Under the
  mutex, any co-location stopped it.

## Validation

- Local: `test_anonymous_scratch` 23 tests (8 new), `test_runtime_task` 38
  (3 new: v3 contract exactness and wall, retryable outcomes, a real SIGUSR1
  stop of a `measured()` process group including a grandchild). With
  `test_score_fixed44`, `test_score_tierd_full`, `test_gf2_target_score` and
  `test_fixed_runtime`: 126 tests OK. `test_dsr_transport` fails in `setUp`
  with or without this change because it needs an unbuilt transport helper.
- Pilot 1755641 (node002) ran on real `/tmp`. Three participants were
  admitted together (slots 0-2). With the reserve set 3.5 GB below the
  measured free space, writers of 0.5, 1.0 and 2.5 GB ran. Only the largest
  stopped (`node_floor`, 405.055 GB available versus a 405.283 GB reserve,
  2.14 GB accounted). The other two completed, and free space recovered. PASS.
- Dry full44 `benchmarks/runs/20261007-fixed44-scratch-dry-b26`: b26 pin,
  60 s solve, 600 s checker, validation 1755648 PASS, array 1755656 `0-43` at full width.
  **44/44 result records and 0 scratch refusals.** Every task was admitted on its first attempt.
  Placement: node015 29 tasks (up to nine concurrent ledger slots), node001 9,
  node016 4, node002 2. The three expected UNSAT rows were checked (147, 263,
  630), as were two SAT rows (401, 991), plus 39 solver timeouts at 60 s. The
  `score_fixed44` classification functions accept every record. The full
  scorer rejects the campaign by design because of its 60/600 contract.

## Open risks

- A stopped (`scratch_enospc`, `trigger=node_floor`) row is still
  unmeasured. `score_fixed44` retry binding accepts only `scratch_node_busy`
  predecessors. Retrying a floor stop needs a scorer extension first.
- Accounting covers runner-owned handles only. Solver-internal anonymous
  temp files and v2/foreign writers are seen only through `statvfs`, so the
  victim choice can be imperfect. The hard floor still bounds the filesystem.
- If the stop signal arrives in the few bytecodes between the end of the task
  body and the monitor stop, the task can exit without a result. That is a
  missing record, never a false closure, and the same class as the existing
  SIGXFSZ handler.
- The native DSR transport defers only SIGALRM and SIGINT. It is not selected
  by this recipe. Before enabling it with this profile, add SIGUSR1 deferral.
- Packing still puts up to 16 tasks on one node and they share its memory
  bandwidth. That is the ordinary Tier-D condition, not new.

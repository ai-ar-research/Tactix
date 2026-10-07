# Fixed-44 cooperative launch and retry provenance

This report freezes the initial diagnostic fixed-44 launch and its first
immutable retry attempt.  It is development evidence, not candidate
acceptance.  The same candidate missed the cumulative B28 threshold; only an
independently verified SAT model or UNSAT proof closes a selected instance.
Missing, busy, resource-censored, or unchecked rows do not close anything.

## Candidate and runner

The solver is SHA-256
`10942fbedbb8a66712ffb439c93601d6a74639f4f2548e43c05a0f477598299e`,
built from source checkpoint
`3ecfa8dfedb6d0dc4aaeeebaddb1a674d390f12e`.  The immutable solver arguments
are `--auto-proof --default-graph-labeling=on`; noL is retired.  The campaign
uses the explicit long64 DSR checker profile and
`fixed44-cooperative-v1` scratch profile with 5,000 solver seconds and 14,400
checker seconds.

The runner files are byte-for-byte copies from committed checkpoint
`3fa3250872f504887c14befda46d23965d8c7efb`.  The optional native proof
transport was omitted because its signal-lifecycle repair had not completed
independent acceptance when the campaign was frozen.  The pinned pure-Python
route is therefore the only proof transport in these attempts.  Preparation
inputs and the two-line wrapper diff are retained under
`benchmarks/runs/20261007-fixed44-10942-cooperative-python-preparation`;
`PREPARED.json` has SHA-256
`ab927bc9a22ba4c0f3906eb0b17a6fa06aceb07533656d6949aeb64c5afd7735`.

## Initial attempt

The campaign is
`benchmarks/runs/20261007-fixed44-10942-cooperative-python`.

- Manifest:
  `6f63d7eefd12783532d4b0c2e65851680f37042f1ff576d9c8c3d7b1f87fa0be`.
- Full 44-row matrix:
  `10aff9085ba5fe2d58f15770411043035a2d0c04344934d4f7b4575571154c25`.
- Array script:
  `78290e8e45d9a05daf82ac6d131b3ac4aa77aa5348dbca6df7055b67ce0b5202`.
- Validation job `1754975` completed with exit zero.  Its 15 anonymous-scratch
  tests and native SAT/model, UNSAT/automatic-proof, crash, and timeout routes
  passed.  `validation.json` is
  `b0a782e8f91d5b1cc0391a69295d4f06e42262cbac8dc5c2ac4686609691c2d0`.
- Array job `1754978` is bound by `launch.json` SHA-256
  `367132e1d6d3f47c881c9610baaeeb24b9288b2b82c608abd57e49b166743620`.

The scheduler initially placed the 44 tasks on four nodes.  Tasks 1, 4, 16,
and 20 acquired the stable cooperative lock on nodes 016, 015, 002, and 001.
The other 40 tasks atomically published `completion.status=incomplete`,
`reason=scratch_node_busy`, and `retryable=true`; none published a logical
result.  At 2026-10-07 17:48:10 EDT, the four lock holders were still running.

Validation created four unlisted Python bytecode cache files before the parent
array was submitted.  Submitted parent inputs were not mutated afterward.  A
supplement under
`benchmarks/runs/20261007-fixed44-10942-cooperative-python-retry01-preparation`
hashes those bytecode files and recursively compares their Python 3.9 code
objects and constants with a fresh compile of the exact pinned sources.  The
supplement is
`parent-pyc-provenance.json`, SHA-256
`2ada4a61e3d4eab99106df507334ed97433b671b54003f3ab77a9184922dc37e`;
its verifier is
`e0aa7fd0b5466cc6f090577c0a33766ecfd7afd134500a61ef2bdaf4c87d6c8d`.
This is supplementary provenance because the parent manifest did not bind the
bytecode hashes before launch.

## Retry attempt 1

The retry is
`benchmarks/runs/20261007-fixed44-10942-cooperative-python-retry01`.
It retains the byte-identical full 44-row matrix and selection authority.  Its
typed retry metadata permits only original task IDs
`0,2-3,5-15,17-19,21-43`.  Every allowed task binds the corresponding parent
busy result by SHA-256.  No row is renumbered and no parent result, claim, or
pin is overwritten.

- Manifest:
  `e64f61ac8051a84f68175e883534c25ce683cee6cf48a5aa45356970dc4fdf69`.
- Array script:
  `660f8e58f5d2b38ed0af318ca8050d2a4b4402a8133fd47b0c9e97fc025f9c1c`.
- Exact holder snapshot:
  `34146ed6c90a6a30d63e590345908a719eba9a223f98e42fe7731c4fd45b82bf`.
- Validation job `1755048` completed with exit zero and the same focused
  routes passed.  `PYTHONDONTWRITEBYTECODE=1` was active and no cache directory
  appeared.  `validation.json` is
  `b52db9c36fbacd6e7580ac349d4f97d78c4174038e84fcd895e5e2ea6b4f8423`.
- Retry array job `1755053` is bound by `launch.json` SHA-256
  `7c9dfd2ec422d84390ee230d7eb4efb3018c7506ea57c94d3396dc7d0b0272ee`.

The resource exception excludes only nodes 001, 002, 015, and 016 because the
four live parent tasks held this profile's node-local lock there.  It does not
select a performance-favorable node.  At 17:48:10 EDT, retry task 0 was running
on new node005 and the other 39 retry tasks were pending for resources; the
retry had produced no additional busy result.

Read-only scheduler probes explain this choice.  On the 16-node cpu-q
partition, four-CPU whole-memory, 520-GB, and 600-GB requests estimated starts
on October 9.  Four- to twelve-node one-task-per-node allocations estimated
October 10--12.  A 400-GB request was immediately admissible but could still
place two tasks per node and would reserve far more memory than the task uses.
The holder-only exclusion preserves the ordinary four-CPU/32-GiB comparison
contract and avoids a whole-node memory monopoly.

At the report cutoff, read-only merged scoring reported 44 expected rows, 40
resource-incomplete rows, four missing live rows, and zero independently
verified closures.  Further attempts must be separate immutable campaigns,
retain this full matrix, bind the exact immediate evidence source per retried
task, and exclude only nodes with a documented live cooperative lock holder.

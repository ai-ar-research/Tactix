# Rank 50: current-source confirmation of the material-pair witness

## Decision

The native scheduling discrepancy is still present on current 6424 Tactix.
Do not spend another native diagnostic run reproducing this same boundary.
Use this current-source witness and the actual native 6547 material-contract
fixtures as the short gate for the pending general added-material repair.
Implementation approval remains unanswered; this diagnostic does not supply it.

Both solvers record the selected binary `(574 OR NOT 525)` being added at
conflict 780400 and search tick 1298029921. Native deletes it during elimination
at 823562. Current Tactix records just the addition through one million conflicts.
Its corresponding shared-mechanism boundary window is empty, while native runs
Strengthen/BVE twice at 823562. The full decision streams match for 1226946 rows;
their first differing row is at conflict 823562, decision counter 1226743:
Tactix chooses literal 91 at level 14, native -586 at level 1.

This independently confirms that the current source still misses the native
phase, rather than relying only on the older 6269 admission observer. It does
not establish that repairing that phase recovers the whole runtime loss.

## Authority and validation

Campaign `benchmarks/runs/20260909-6562-rank50-current-material-pair` uses two
distinct immutable copies of the unchanged 6424 trace binary, SHA-256
`c5ca763ce93f48507648c32cb55027274a79332ef56147814bb917d6dddc9e8c`.
The candidate enables only the existing selected-pair lifecycle observer in
addition to the ordinary streams; the control does not enable it. Source hashes,
current full-run Rank 50 result, fixed selection and native 6556 authority are
bound before submission. No native solver or timing reference is rerun.

Job 1678214, submitted 2026-09-09T13:21:17, has two full-width tasks on
`cpu-dense-hwulab-q`: 12 logical CPUs, 48000 MiB, one bound solver per task,
1200 solver-only seconds, 25-minute bounded-diagnostic allocation, no node
constraint or throttle. Both tasks are terminal COMPLETED/0.

Frozen `summary-6564.json` verifies byte-identical decisions, boundaries,
macro boundaries, prefix and partial certificate between the current arms.
The inherited 139633-row common hash is `236dcfa1a362af4d`. Both partial proofs
are 63963136 bytes, SHA-256
`e36968d42b49693ccd26198f1c07629073b91097fb3cc9c473c8f3921242f091`.
Each runner hash-deleted its partial proof and decompressed input and reports
scratch deletion. These are not independently accepted final UNSAT proofs.
The cached native streams and lifecycle are hash-verified against frozen 6558.

The current partial certificate also matches historical 6286 Tactix, but the
complete ordinary-stream hash map does not. That historical comparison is
reported, not used as a substitute for the new same-binary control. No claim
of whole-stream replay across different Tactix sources is made.

## Limits and test consideration

Native's lifecycle callback is before watch insertion and material marking;
Tactix's is after binary publication. Their value and copy fields differ:
native reports (-1,0), copies 0 at addition; Tactix (-1,-1), copies 1.
Do not interpret these differently placed observations as aligned assignments
or use native's zero-copy deletion metadata to infer semantic absence.
Native source tags remain immediate marking callers, not the complete upstream
call stack. The current binary has no Eliminate-admission operand observer.
Its unchanged source still gates elimination on removal marks and root-fixed
watermarks without the native added-material operand.

No production, candidate source, parser or observer logic changed. Existing
strict lifecycle and decision-prefix unit tests cover reused readers; the
boundary parser's fixed positive and malformed-input controls run in 6564.
The new one-shot validator fails closed on any candidate/control stream or
partial-proof mismatch before making comparisons. A policy repair must add
focused tests for real addition versus repeat, clear/rearm, fixed-variable
addition, watermark consumption and deadline behavior; those are not waived
by this successful diagnostic.

Preparation initially used an incorrect 6439 authority pathname and stopped
before manifest publication or submission. It resumed only after verifying the
exact four-file scaffold and identical pins; the corrected path is recorded in
the manifest. No solver, model or proof failure, result exclusion, successful
writer rerun, old campaign mutation or denied transfer/push retry occurred.

## Aggregate and next action

Full current 6439 remains 50/50 independently checked and clean:
8731.653 / fixed 7239.010 seconds = 1.206194355. Older 6169 is better; no
promotion is warranted. Added ranks 41--50 already have 20/20 checked matched
12-CPU/48000-MiB results in 6108. Do not repeat that baseline or the completed
fresh reevaluation. Rank 50 remains 427.879 versus 49.047 seconds, with
11797027 versus 1718121 conflicts. All fifty ranks and memory remain scored.

The next safe design step is to map a separate subsumption-material producer,
clear/rearm lifecycle and consumer watermark onto current Tactix, identifying
every shared touch consumer that must remain unchanged. Do not implement or
substitute the existing shared touch epoch for native's false-to-true counter
without the pending approval. Preserve SP-SLOT-5 removal semantics and N1--N4.
Other proof checks stay asynchronous. Claude-owned fuzz remains unavailable;
no fuzz inspection or promotion. Audit 6565 owns integrity, accounting and
aggregate tool/doc tests. Andrew's report is local, not sent.

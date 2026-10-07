# Native DSR transport handled-signal repair: developer disposition

The repair removes the rejected whole-lifecycle `pthread_sigmask`,
`sigpending` and `sigwait` protocol. On the main thread, callable SIGALRM and
SIGINT handlers are temporarily replaced by non-raising recorders before any
staging namespace or helper process exists. A recorded signal stops and reaps
the helper process group; staging cleanup completes before handler replay.
Worker-thread calls retain ordinary `try`/`finally` cleanup and never invoke a
main-thread handler.

Handler restoration explicitly closes the repeated-signal boundary found in
review after r8. While both recorders are active it stops `ITIMER_REAL`,
restores SIGALRM first and SIGINT last with bounded state-checked retries,
rearms the remaining timer, and only then invokes recorded original handlers.
Acquisition rollback restores only recording handlers observed as installed.
The first operation exception remains primary and a permanent cleanup or
restoration error is chained. Native helper SIGXFSZ exits become
`OSError(EFBIG)`, which the committed runtime classifies as the incomplete
resource outcome `scratch_file_limit` rather than proof rejection.

Production repair commit
`838325505df20334e393576689b977958ca15fbf` contains exactly
`tools/bench/proof_oracle.py` and `tools/bench/test_dsr_transport.py`. Frozen developer campaign
`benchmarks/runs/20261007-native-transport-interrupt-r9` uses committed base
`6408945785ceea10803b7b970aaf8260e9f07b83` plus the two-file owned patch.
The source SHA-256 values are recorded in `pins/SHA256SUMS`. Release helper
`3ee37b53eec0e303ccef64ac1ea1413718f401b9d27671347e0bf1d770f6b6c9`
and ASan/UBSan helper
`aabab23978b4665ce0d1662b985818c25e6712dd60c85a2f60e6fb93a3208ab6`
are byte-identical to the prior independently reviewed pins.

SLURM array `1754972` ran at four CPUs and 16 GiB per arm with no node
constraint or throttle. Both frozen arms passed 23/23 transport tests,
including real checker reach, anonymous scratch lifecycle, unchanged failed
destinations, process-group reap, acquisition and cleanup interruption,
permanent cleanup failure precedence, file-cap typing, partial handler-install
rollback, and exceptions injected after both handler restorations. The exact
r6 process-directed threaded-hang reproducer completed 5,000 calls per arm
without a hang. The exact single-thread mask-leak reproducer completed 40,000
calls per arm without a changed mask or a lost subsequent deadline. No
ResourceWarning, sanitizer diagnostic or staging residue was reported.

Production-identical supplement `1755045` passed 24/24 tests in both modes.
Its added test completes native conversion inside an active outer proof
deadline and then waits; that same deadline fires in under one second. The
supplement's `proof_oracle.py` SHA-256 is identical to the main r9 pin and the
committed file: `ab09d4c98f3a08eb1366877bac70a8730c1006cb630ccd816046b51f07800ede`.
The main pin manifest SHA-256 is
`f7a83f215eb12158a85b3cf1b1ebd98602ab9659290c3493efdea731c26c5097`.

This is developer qualification, not independent acceptance. The preserved
`CLAUDE_REQUALIFICATION_REQUEST.md` asks the b063 owner to repeat the r6, r5
and r3 lifecycle reproducers, native reach and proof/model checks, NFS legs,
repeated restoration signals, ResourceWarning audit, and the affected
cooperative-scratch/early-release lifecycle. Full44 preparation deliberately
uses the pure-Python fallback and omits the optional native helper while that
independent work is pending. No solver semantics, proof acceptance rule or
default checker route changes here.

r7 (`1754864`) stopped on two deterministic test-setup assumptions. r8
(`1754901`) corrected those tests and passed its developer gates, but review
found its per-handler restoration loop exposed a repeated-signal boundary.
Both rounds remain immutable and are superseded; neither is acceptance.

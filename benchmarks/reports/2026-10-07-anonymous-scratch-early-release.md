# Anonymous proof scratch early release

The fixed-runtime route previously kept every anonymous solver and checker
artifact open until the task exited.  A large unsuccessful certificate could
therefore occupy local disk together with its checker spelling and converted
proofs for the remainder of a multi-hour checker route.

This increment gives `AnonymousScratch` a logical-name release operation and
releases each artifact only after its last consumer has returned.  Released
names are tombstoned, so a stale `/proc/PID/fd/N` path cannot target a newer
artifact after descriptor reuse.  Release truncates before close, closes even
when truncation raises, and preserves the truncation failure as the primary
error.  Named scratch retains its prior lifetime.

The runtime records certificate size and SHA-256, solver diagnostics, stage
tails, peak memory, and produced size before release.  It retains the original
CNF and model through the independent SAT model check.  For UNSAT it
materializes route command generators before releasing the original CNF, then
releases the original proof and each derived DSR, DRAT, LRAT, or PBP artifact
after the next independently checked handoff.  Failed-proof retention keeps
the original proof open until its bounded atomic copy completes.

Validation:

- SLURM job `1754591`, `cpu-test-q`, four CPUs: exact six-file hash check and
  33/33 focused unit tests passed.  Coverage includes descriptor reuse,
  duplicate cleanup, injected truncation failure, diagnostic capture, retained
  failed-proof scope, and witnessed DSR, witness-free DSR, and PBP handoffs.
- SLURM job `1754594`, `cpu-test-q`, four CPUs: exact pinned cumulative Release
  binary `4fa9b118c831c8faa0c6a01fb370bf991260807f2edb4e1a10b798ec9f589ffb`
  passed a tiny SAT result with independent original-CNF `checkmodel`, and a
  tiny UNSAT result through the solver-declared `dsr-cake_lpr` route followed
  by `drat-trim` and CakeLPR.  Crash and timeout cleanup also passed, with no
  live scratch handles before final close.  The validation result SHA-256 is
  `f219d5165303a813dc89e1b150edcb39542aee337f31d3812e4350431273cf9d`.

The frozen job records are
`benchmarks/runs/20261007-anonymous-scratch-early-release-r2` and
`benchmarks/runs/20261007-anonymous-scratch-early-release-actual-r2`.
These are lifecycle checks, not performance measurements.  This change does
not add a scratch-capacity admission policy and does not change solver logic,
proof semantics, or the graph-labeling default.

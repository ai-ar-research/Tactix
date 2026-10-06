# Rank263 live affine-matrix causal gate

This five-second CPU-Q gate asks only whether the persistent GF(2) matrix can
be admitted and can publish trail-relevant certified reasons that ordinary
CDCL consumes.  It is not a solve or performance comparison.

Job `1750965` ran source `ade1d63e494dc8fe69fd4a209fac0c729a90e341`
on the exact Tier-D rank-263 input.  The controller requested and installed two
complete matrices, called the search service 504 times, and recorded 216
published reason transactions.  During the bounded run, ordinary search made
273 decisions and reached 61 conflicts.  This establishes the missing causal
chain: recognition, authenticated matrix admission, trail synchronization,
certified Boolean publication, and subsequent CDCL search are all live.

The run also identifies the next bottleneck.  Its 216 reason records occupy
294,007,470 provenance bytes.  The final complete provenance census attributes
1,615,954 proof additions, 1,615,738 deletions, and 95,498 reserved helper
identities to those transactions.  Initialization ran twice even though the
first session remained valid.  Longer target runs should wait for persistent
session selection and proof-support reuse; repeating this proof expansion would
measure bookkeeping rather than the intended Gaussian-search mechanism.

The submitted harness exited nonzero because its postcondition required the
word `partial`.  The solver produced the stronger complete admission twice.
Job `1750966` rescored the retained artifacts without rerunning the solver and
accepted either complete or partial sound admission followed by live service.
Thus the SLURM failure is a harness predicate defect, not a solver or proof
failure.  No SAT/UNSAT result or target recovery is claimed.

The source commit and both CNF hashes were checked before execution.  The
executed harness terminated before its final binary-hash postflight, so this
artifact does not claim an exact solver-binary hash.  `manifest.json` records
the retained artifact hashes, including the exact executed harness, corrected
future harness, profile, feedback, provenance, summary, and rescore log.

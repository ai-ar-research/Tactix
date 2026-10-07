# Native DSR transport whole-lifecycle interrupt repair

Developer qualification is green; independent owner requalification remains
required.  This increment replaces bytecode-sized cleanup guards with one
whole managed lifetime: SIGALRM/SIGINT are blocked before staging acquisition,
the converter is polled and killed/reaped on a pending signal, owned staging is
removed with bounded retries, reaped `Popen` references are released, and the
saved handler is invoked at a controlled post-cleanup boundary.  The separate
binary-route probe now rethrows errno-less deadline exceptions instead of
classifying them as file-system errors.

The immutable source hashes are `proof_oracle.py` 0c34ee00... and
`test_dsr_transport.py` 7f1c4337....  `pins/SHA256SUMS` hashes to 11a46d50...
and `harness/SHA256SUMS` to ee4967af....  The unchanged release/ASan helpers
hash to 3ee37b53.../aabab239....  Pins are read-only and Python bytecode writes
are disabled.

SLURM array 1754621 completed both full-width arms (release on node005, 57 s;
ASan on node016, 87 s).  Each arm passed 20/20 focused tests and 3,000 local
plus 3,000 shared-filesystem natural deadline conversions.  Across 12,000
conversions there were zero owned staging residues, zero partial outputs, zero
unexpected exceptions, zero foreign NFS entries, and zero sweep stderr bytes;
the sanitizer arm reported no sanitizer failure.  The release unittest stream
contained one `ResourceWarning` for a forced-test subprocess even though the
suite passed; both 3,000-call release sweeps and an isolated cleanup-entry test
were clean.  This warning is retained for independent-owner review.

Earlier attempts remain diagnostic evidence.  Job 1754606 failed before tests
because the isolated launcher omitted the frozen import path.  In 1754609 the
ASan arm passed, while the release harness counted transient top-level `.nfs*`
files as owned staging; the final harness records those separately and only the
unique `.out.drat.dsr-transport-*` namespace is owned by this transport.  In
1754615 the ASan arm passed while release pin verification caught concurrent
`__pycache__` mutation.  Job 1754621 uses per-job output directories, disables
bytecode writes, and makes the pin non-writable.

This report does not qualify SIGKILL of the managing Python process.  It also
does not promote any checker or solver configuration.  Independent owner
review must confirm the handled-signal contract and the disclosed warning
before this is acceptance evidence.

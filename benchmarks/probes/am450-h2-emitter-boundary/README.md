# AM450 H2 unit-emitter trace

This is an unchanged, hash-pinned AM423 public H2 replay under `gdb` on
`cpu-dense-hwulab-q`. It records native-DSR `Proof::add_rup` unit call stacks
to identify the rule boundary for a general proof-state repair. The output
proof must remain byte-identical to the historical 680-byte proof. No source,
checker contract or public action changes are made.

The first submitted job, `1723675`, failed at the driver hash assertion
before invoking `gdb`: the launch script accidentally copied the CNF's hash
suffix into the driver pin. The assertion was corrected from the independently
recomputed driver SHA-256 before rerun; no diagnostic result came from 1723675.
Job `1723678` reproduced the proof byte-identically but GDB could not reset
its symbolic `Proof::add_rup` breakpoint after exec, so the trace assertion
failed. The executable is non-PIE and its pinned symbol address is
`0x42da30`; the third run uses that address directly.

Job `1723680` completed successfully. Its `out/1723680/gdb.log` records two
unit-sized `Proof::add_rup` stacks, and `proof.dsr` is byte-identical to the
AM423 historical proof. The [AM450 report](../../reports/2026-09-25-am450-h2-emitter-boundary.md)
interprets this bounded trace.

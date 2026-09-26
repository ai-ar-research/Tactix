# AM450: H2's first root unit crosses a public rule boundary

Date: 2026-09-25. Immutable AM423 Release driver SHA-256 `8db25626...` was
replayed under GDB on `cpu-dense-hwulab-q` using the exact 42-clause original
CNF and public action program. Job `1723680` completed `0:0` and emitted the
historical 680-byte native DSR byte-for-byte (SHA-256 `1327bb08...`). It
changed no solver source, action, proof bytes or checker route. The pinned
launch, debugger commands, complete call-stack log and hashes are in
`benchmarks/probes/am450-h2-emitter-boundary/`.

At the first unit output, GDB stopped in `Proof::add_rup` with unit size one
and internal literal 32 (external `17`). Its immediate caller was
`probe_one` in `src/rules/support/failed_literal.cpp`, reached through the
public `Backbone` rule application (`tactix_apply_instance`). The replay then
applied public `CCE`; the proof's contiguous 29-clause deletion block follows
that unit. The second observed unit-sized `add_rup` call occurs later inside
`substitution_redundancy_apply_detailed_impl` during `tactix_solve`. Thus the
first H2 ordering conflict spans a unit producer and a later deletion rule;
it is not a local X7 witness-serialization choice.

The source boundary explains why AM449 is diagnostic rather than a production
fix. `probe_one` calls `kernel_conflict_reason_walk` and passes a RUP hint
chain only in LRAT mode (`failed_literal.cpp:85-111`); native DSR writes the
unit without that support. `Proof::add_rup` streams it immediately, and later
`Proof::del` streams each deletion. AM449 used a partial *checker* LSR after
the fact to identify the first unit's support and retime only non-support
deletions. To reproduce that generally in the native emitter, the support
must be owned across the public rule boundary, and proof publication must
order subsequent deletions while preserving each addition's derivation and
the legal X7 surface. A CCE-only reorder or X7 decline would not solve this
cross-rule dependency.

Jobs `1723675` and `1723678` were failed diagnostic launches, not solver
failures: the first had a mistyped driver hash and never invoked GDB; the
second reproduced the proof exactly but GDB lost a symbolic breakpoint at
exec. Job `1723680` used the pinned non-PIE symbol address and recorded both
unit stacks. No H2/N2 acceptance or production change is claimed.

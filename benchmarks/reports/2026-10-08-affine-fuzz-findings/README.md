# Affine fuzz findings: refresh after Substitute/BVE, and the afB consequence

This report covers two deterministic findings from the c4 counted fuzz run:
- **am-refresh-after-elim:** fixed in the product.
- **afB-consequence-not-derived:** a correct but incomplete engine; I recommend
  the driver change, not the product.

The fuzz owner's handoff is
`benchmarks/runs/20261007-firstopen-original-seed-fuzz-owner/handoff/`. It was
read only, never edited. My diagnostics are in
`benchmarks/runs/20261008-am-refresh-diag/`; all of them are SLURM Debug builds
of HEAD that run copies of the owner's drivers.

## 1. am-refresh-after-elim (am-rel-548, am-rel-206): product, fixed

### Where InvalidRequest comes from

The diagnostic traced every line of the affine search that produces an
`Invalid` status. Both seeds stop at the same place:
- `affine_matrix_search_initialize` maps the root-image plan status
  `UncertifiedRoot` to `Invalid` (the ternary at "recognized = planned == …").
  A refresh (op 2) runs this path.
- `prepare_affine_root_image` returns `UncertifiedRoot` because one level-0
  trail fact fails its RUP check against the captured sources:
  - am-rel-548: the fact is `-21`;
  - am-rel-206: the fact is `-2`.
- Each such literal has the following state:
  - `reason == UNIT_REASON`, an asserted unit;
  - `unit_ids[v] == 0`;
  - no entry in the DSR root table (`dsr_root_values`).
- So the capture (`affine_live.cpp`, full root view) skips it as a source. It
  stayed provable from other live clauses until a later root BVE (206) or
  Substitute round (548) removed those clauses. From then on it could no longer
  be certified.

### Root cause

`PendingUnits` asserts Substitute's failed-root units and other batched units:
- It writes each unit line to the proof (`add_rup`).
- It installs `unit_ids` only for LRAT.
- Under DSR and DRAT the asserted unit was therefore known neither by id nor
  through the proof-only root table.
- The table recorded only propagation-derived units (materialized when their
  reason is retired).

Sweep and BVE units do get `unit_ids` in every format, which is why
am-rel-23's Sweep path never hit this.

### Invariant and repair

**Invariant.** Every asserted root unit (`kernel_assign_unit`) is a clause in
the proof stream. If it has no LRAT id, the stream's root-unit table holds it.

**Repair.**
- `kernel_assign_unit` records the unit with `kernel_note_dsr_root_unit`
  whenever `unit_ids` is zero. Nothing is emitted; the unit line was already
  written by the asserting rule, or is an input unit.
- The table serves every clausal stream whose checker keeps written units
  without ids: native DSR and DRAT (`kernel_root_unit_table_stream`). The
  affine capture and source revalidation read it under the same predicate.
- `affine_matrix_search_refresh` asserts the invariant at entry in Debug
  builds: every asserted root unit without an id is present in the table.

**Effect on the solver.** None.
- The table is proof-only.
- ULC authentication reads it only for unassigned pivots, so it never sees
  these units.
- `kernel_emit_root_units` skips `UNIT_REASON` literals.
- The only side effect: materialization no longer re-emits an asserted unit's
  line.

This differs from 9ed61994c (am-rel-23, the op-3 path). That change gave an
unconditional contradiction the empty reason. The failure here is in root-fact
certification, which runs before any matrix event.

### Test

`tests/test_affine_matrix_persistent_lifecycle.cpp`,
`refresh_after_root_unit_premises_removed`, runs in DSR ASCII, DSR binary and
DRAT, with a BVE variant and a Substitute variant. The sequence:
- a Substitute round turns the failed root 6 into the unit `-6`, with no
  LRAT id;
- the test asserts the table holds `-6`;
- BVE on 7, or a second Substitute round;
- garbage collection;
- a refresh (op 2) and root maintenance (op 3), neither allowed to return
  Invalid.

On the base source, with the table assertion removed, the BVE variant's
refresh returns `Invalid` in all three formats (`basecheck`). The Substitute
variant returns NoStructure there: its second round does not remove the
premises. The handoff's own seeds cover that path.

### Validation

- **Handoff seeds.** Both replay with the fix: am-rel-548 SAT (exit 10) and
  am-rel-206 UNSAT (exit 20), with no failure (`fixed3-*.out`, Debug).
- **CTest.**

| Check | Result |
|---|---|
| Focused Debug CTest (91 tests: affine, substitute, sweep, bve, proof, kernel, online, sequence, structured, API, micro proofs) | 91/91 |
| Focused ASan/UBSan CTest, same list | 91/91 |
| Full Debug CTest | 355/355 |
| Structured-root DSR fixtures | verified |

- **Not rerun:**
  - the owner's counted fuzz rows (the fuzz owner's job);
  - the solver gates. No solver decision reads the table.

## 2. afB-consequence-not-derived (afB-rel-410): correct engine, incomplete in this corner

**Recommendation: amend the driver.** The engine misses a unit that its own
admitted rows imply, but its documented design is a forward-echelon basis, and
it never claimed to find such units. Whether it should reduce fully is a
design question for the plan; I made no product change.

A printing copy of `afdrv` (`tools/test_zz_afdiag.cpp`) dumps the session's
equations and matrix rows at the point of failure:
- **The relevant admitted equations.**
  - From the plan, `(a,b,c) = (2,10,33)` with rhs 0 and `(b,c,d) = (8,10,33)`
    with rhs 1.
  - After the unit `-2`, the matrix holds the rows `{10,33} = 0` (pivot 10)
    and `{8,10,33} = 1` (pivot 8).
- **The unit is in their span.** Their sum is `{8} = 1`, which is exactly the
  planned consequence 8, so the matrix rows themselves imply it.
- **What the service did instead.**
  - It returned Applied with the reason `(-17 -29 31)`, from another row.
  - BCP then derives 31, not 8.
  - Three further root service calls return NoEvent.
- **Why it is missed.**
  - `reduce` (`affine_matrix.cpp`) performs forward elimination on the leading
    variable only.
  - `find_event` reports a row only once a single unassigned variable remains.
  - Here the row with pivot 8 still carries 10 and 33, because those are not
    pivots below it. Back-substitution, as in Gauss-Jordan, would be needed to
    expose the unit, and the engine never does it.
- **This is the documented design.**
  - implementation-notes describes a "forward-echelon submatrix".
  - The calculus states the matrix is "sound but incomplete".
  - No contract promises that one root service plus BCP derives every unit in
    the span.

**For the driver.** The plan generator assumes the service plus BCP derives
`d`. That holds only when forward elimination happens to reduce `d`'s row by
`a`'s row, which depends on the variable order after random renaming. It held
in 430 of 431 rows; afB-rel-410 is the order where it does not.

Precise wording for the fuzz owner: assert `consequence-not-derived` only when
the service's published reason (or a fixpoint of service + BCP) contains the
consequence's variable; otherwise count it as a reach miss. Alternatively,
make the plan place `d` after `b` and `c` in the post-renaming variable order.

If unit completeness is wanted, the repair is Gauss-Jordan back-substitution.
It would have to go into every event path: full synchronize, sliced
synchronize, the singleton index and restored predecessors. That is a planned
algorithm change, not a defect repair, so it should go through
`revise-plan`/`calculus` first.

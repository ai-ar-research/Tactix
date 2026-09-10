# Fixed Vivify ordering guard for the pending structural pilot

The new `tests/test_vivify_order_contract.cpp` passes96 fixed cases twice on
each of five existing build variants, all with digest `c67723e59482dad7`.
No solver implementation, guard, policy, proof vocabulary or performance
contract changed.7135's structural rewrite remains unapproved/unimplemented;
the failed7116 BVE pilot remains unpromoted. This increment is test coverage,
not a claimed runtime or memory improvement.

## Coverage beyond the separate existing fixtures

Existing Vivify tests cover ordinary maximum-count ordering and logical
replacement positions separately. The new fixture composes those obligations
at the exact ordinary/skip-sort boundary:

- Redundant populations0..3, with irredundant counts immediately below and at
  `10 * (redundant + 1)`. This tests the integer-floor threshold, not an
  approximate floating-point10:1 condition.
- No, all and mixed priority bits; both import/literal orders; and unchanged
  versus appended physical replacement records retaining old logical positions.
- Actual public candidate order: priority first; ordinary maximum count,
  inverse size and descending logical position; or reverse logical position
  in the native skip-sort regime. Duplicate references are rejected.
- Bounded registry enumeration at caps0,1,half,exact and beyond the surface,
  with an untouched sentinel after the returned entries. No action is hidden
  by replacing the generic public surface with a canonical-only fast path.
- Repeated const queries preserve arena, values, trail and ticks. Canonical
  entry uses an explicitly exhausted zero budget, so scheduling/rearming and
  watch reconstruction execute but no clause is probed.
- The schedule remains equal by clause identity after canonical entry and
  garbage collection. Appended replacements retain their logical-position
  ties before collection. Full kernel invariants run in the Debug arm.
- All256 assignments of each eight-variable formula are enumerated to confirm
  unchanged models across replacement setup, canonical entry and collection.

These are actual API/state checks, not a new radix sorter or a performance
microbenchmark. The fixture intentionally performs no proof-emitting
transformation. Existing proof-bearing Vivify tests remain mandatory for any
future implementation change, as do the root-false/cross-band/pose cases not
claimed by this fixture. The existing144-case max fixture and broader
count-cache/transaction fixtures are retained, not replaced.

## Reproducible linking without rebuilding a pin

[7141](../runs/20260910-7141-vivify-order-contract/summary.json) compiles the new
fixture with the exact recorded flags/includes of7030 Release,7030 trace and
7116 Release, and links it against their existing solver-library objects in
a new output directory. It first reproduces each build's existing
`test_vivify_max` executable byte-for-byte from the same object inventory.
Source, compiler/link commands, objects, old/new binaries and logs are
hash-bound. Every input hash is rechecked after both fixture executions.

[7142](../runs/20260910-7142-vivify-order-extra-controls/summary.json) adds the
production-source main Release build and fully instrumented7116 Debug objects.
The current production `src`/`include` tree must equalc2a95a6. It reproduces
existing `test_vivify` and Debug `test_bve` executables exactly before linking
the new fixture. ASan/UBSan and default leak detection run outside ptrace;
both runs pass with the same7141 digest. No main build configuration, frozen
pin object, existing executable or submitted job is modified.

The root test uses existing CMake test discovery; it requires no build-list
edit. No complete Release/trace suite rerun is claimed here: the changed scope
is one fixed test source, checked across five unchanged implementations with
independent model and invariant assertions. Owning rule contracts were read;
calculus/guard documentation is unchanged because semantics are unchanged.

## Continuation and hygiene

Keep this regression with the pending7135/7140 compact canonical schedule
design. If approved, the ordinary irredundant path must preserve these exact
semantics while moving to native-equivalent radix ranking, and the remaining
root/count/pose/fallback obligations still need their focused regressions and
full gates. Proposed15% whole-Vivify and1% whole-runtime improvements remain
unproven. Do not lower thresholds or substitute a small cached-boolean edit.

Audit7143 binds unchanged solver/campaign authority, the two fixture summaries,
published proof/cleanup states, and aggregate tooling/documentation checks.
Its first attempt stopped before writing because its expected-output literal
contained an escaped backslash instead of a newline; the exact byte comparison
is corrected without changing fixture outputs or acceptance. Draft PDF line
overflow was also corrected and recompiled. Neither was a solver failure.
Continue old proof checks asynchronously; no new benchmark or solver job was
submitted. No material deletion. All prior raw and compressed evidence remains,
including7140's hash-bound local disassemblies. Claude debugging was not used;
Claude-owned fuzz remains unavailable and uninspected, with no transfer or
promotion waiver. No denied push retry. The goal is still unmet.
Andrew TeX/PDF is local/unsent in
`report-to-andrew/20260910-1603-vivify-order-contract/`.

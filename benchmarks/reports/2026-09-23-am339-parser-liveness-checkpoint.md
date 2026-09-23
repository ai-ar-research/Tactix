# AM339 — advisory-header binary-liveness scope

Intentional unaccepted repair checkpoint, 2026-09-23. No rule, policy,
proof vocabulary, parser tolerance or public API boundary changed. AM333 remains
the gated authority; AM337's submitted arrays retain their original pins.

## Diagnosis and bounded evidence

AM336's published owner status reports pre-existing Debug assertions for
above-header variables and repeated headers. Codex did not inspect its private
campaign. Instead, the two smallest independently constructed stdin cases were
run through AM337's pinned Release and Debug binaries using `/dev/stdin`:

```
p cnf 2 1
1 3 0
```

```
p cnf 2 1
p cnf 2 1
1 2 0
```

Release `40b0de22...` returns 10 and a satisfying model on each; Debug
`52ee866b...` aborts with SIGABRT in `BinaryLivenessIndex::exact_position` and
`ScopedInputBinaryLiveness::arm`, respectively. These are the same assertion
sites reported by the owner, but these are not its exact fixtures: its published
Release parse-error observation is not contradicted or reclassified here.

`src/frontend/parse.hpp` explicitly promises advisory header counts and growth
beyond a lying header. The parser's temporary exact pair bitmap instead assumes
a fixed literal universe; an above-header binary addresses it after the kernel
has grown. A second header tries to arm an already owned index. A related late
first header would create an empty index despite earlier live binaries, losing
the no-false-negative premise needed for exact duplicate handling.

The added `test_advisory_headers_preserve_binary_liveness` regression was built
and run before changing solver code. It aborted in `exact_position` (exit 134).
After the repair it passes in both Debug and Release. The test embeds the exact
clause streams, so there is no separate private or transient reproducer dependency.

## Repair and scope

Before generic input ingestion grows the variable population beyond an attached
exact bitmap's universe, the parser uses its existing filter-growth helper to
switch to the ordinary filter and rebuild it from every live mirrored watch.
Later duplicates still consume their raw input IDs without creating extra
kernel copies. Repeated advisory headers retain the same scoped owner and
rebuild the filter. A late first header likewise seeds it from existing pairs.
The scoped destructor still detaches the same owned index.

Valid single-header DIMACS keeps its original exact-bitmap admission and clause
path. No parameter, budget, startup detector, rule guard, scheduling decision or
benchmark identity participates. Headerless C API input retains its existing
growth capability. This is a representation-lifetime repair, not stricter
parsing or permission to certify malformed DIMACS. AM338 still rejects malformed
original headers/bounds/counts before checking a certificate; no checker or
acceptance condition was relaxed.

The fixed test compares the full imported-state fingerprint with an equivalent
canonical-header stream, including variable/root state, ordered binary and
large clauses, activation order and normalization count. Four variants cover
above-header growth, a growing repeated header, an unchanged repeated header
and a late first header, each at block sizes 1--40 and the production size.
It additionally checks exactly three unique binaries, one copy of each, all
kernel invariants, and index release. New CLI Debug and Release agree on the
two smallest cases, including the original Release model.

## Immutable authority

`benchmarks/runs/20260923-am339-parser-liveness/manifest.json` records:

- Base `182a535efc0719b66bb4d8bade086ec87548b68f`.
- Source patch `8a7e7101703f7103d135d8cbbbd096a8becdcf5400c149a6a0fae95f98f532b0`.
- Source/harness archive `30401182583fe7458242fd31b1811a5e3d5a1cd5c2e0e4e698223397efaabc50`.
- Release `cbe999105947efd3782bf4abc7ec469136ec0b0551322df1039bd5e250e596c2`,
  `build/pin-parser-liveness-release-20260923`.
- Debug `c0320acb91dddadcf60b2816c42b549f49e1975c5e2f5b8e449247624b395435`,
  `build/pin-parser-liveness-debug-20260923`.

All seven proof-checker binaries are unchanged AM337 copies with verified hashes.
The bundle includes current AM338 proof-oracle/runtime/model-check tooling and
an immutable combined `claude-fuzz-request.md`. New mutable builds are separate
from every submitted job and live earlier CTest tree.

## Validation and outstanding work

- New parser regression and existing parser tests pass in Debug and Release.
- New Release passes the startup-parity and count-preservation API/default
  VeriPB/CakePB checks, normalization and SAT controls, no-proof agreement,
  the random/replay congruence handoff in automatic/strict-DSR/ASCII modes
  through backward and forward Trestle, and all CLI output-failure controls.
- Fresh full Debug CTest has **177/178 passed** with explicit immutable DSR/DRAT
  checker environment, including micro_proofs, sweep_proofs, the corrected
  proof-output-failure fixture and all three focused proof pipelines. Only the
  long scripted-default demo remains active (CTest PID 2944685, demo 2946772);
  this is not a full-suite pass.
- Documentation and whitespace checks pass; implementation notes remain
  560/560 lines. No compaction was requested or performed.
- The new Claude invocation exited 1 before preparing a campaign, again with
  `You've reached your Fable limit.` This confirms the operational blocker on
  the new request, not a solver or fuzz result. No model switch or quota purchase
  was assumed. Combined default/random/sanitizer and focused tooling/parser
  acceptance remain mandatory and pending.

Earlier CTests now have terminal evidence: AM336 is 175/175 passing, including
its demo at 3,686.94 seconds. AM337's original full invocation ended with 177/178
passing (demo 3,367.30 seconds) and the already documented old API fixture
failure; the corrected fixture passed separately and passes in this new suite.
Do not relabel that original nonzero full-suite exit as a clean pass. AM335's
older O0 demo remains live.

AM337 jobs 1715989--1715992 remain active/queued. A pre-wrap-up sample has 28
valid Rank results and no recorded pending cleanup; no full gate is closed.
No old result is silently transferred to AM339. Required successor performance
gates remain due before promoting a new binary. No material benchmark artifact
was deleted; active campaigns, pins, earlier source authorities and failures
remain retained. The successful fixed tests clean only their own scratch.

Next: restore Claude-owned execution/reporting, resume the preserved campaigns
without repeating completed unchanged legs, finish the new combined request and
remaining tests, and inspect every terminal routed gate result. Only then
advance to hcp/6g matched-work owner comparison; never dose or reopen the user's
excluded rows. The complete WV2-89/600-second objective remains unachieved.

Andrew deliverables:
`report-to-andrew/20260923T082000Z-parser-liveness/report.tex` and `report.pdf`.

# AM337 — proof lifetime and publication checkpoint

## Outcome and authority

Intentional **ungated checkpoint**. The four requested startup-certificate
diagnostics are independently verified. Two pre-existing N2 failures handed off
by Claude now have causal repairs and passing fixed regressions. Combined fuzz
and all four full routed gates remain due; AM333 remains the last gated binary,
with its newly discovered correctness debt explicitly acknowledged.

Immutable combined candidate:
`benchmarks/runs/20260923-am337-proof-lifetime-v2/manifest.json`.

- Base: `26229c890ec387d735d12c89ae7ce4f753099545`.
- Source patch: `4f2a8b3b4a6fd03e065cc45e7992faa329c2780390de5aa96a02b6acad5cc9d2`.
- Release: `40b0de228744fb878d6e9cb478babd3fc589eb944b01b52f93e59475516df40c`.
- Debug: `52ee866b031235a57a7d61cceae35c5ac358f8fb873c3cd1216acd2a2c9af3b4`,
  `build/pin-proof-repair-debug-20260923/tactix` (`-g -O1`, assertions enabled).
- The manifest hashes the source archive, all DSR/DRAT/PB checkers and the
  maintained routed runner. V1 remains frozen; V2 refreshes test-source
  provenance only, with identical solver/checker binaries. Neither V1 nor V2
  claims benchmark acceptance.

## Causal N2 repairs

### Congruence proof-only clauses

Claude's bounded AM335 reproducer has 17 variables, 41 clauses, random-policy
seed 335110095 and an exact trace. The owner reproduced its failure on AM333,
AM335 automatic and explicit clausal modes. Startup PB does not fire there.
The handoff is preserved as
`tools/fuzz/corpus/regressions/032-congruence-transient-sr/`.

Three Congruence helpers emitted XOR-degeneracy, XOR-matching and ITE-matching
ladder clauses in non-LRAT formats without registering their later deletion.
Subsequent SR was validated against the kernel, but its checker database still
contained those proof-only clauses. The retained failing proof's later SR line
therefore had extra obligations absent from the kernel snapshot.

Each emitted ladder is now registered for exact retirement in DSR/DRAT as well
as LRAT. Two failed-chain exits also retire their already-emitted prefixes.
This follows the owning Kissat proof-chain lifecycle (`delete_proof_chain` in
`winners/kissat-sc2024/src/congruence.c`); no formula effect, guard, scheduling
choice or proofless trajectory was changed. Calculus V6 and the engineering
ledger now state the complete lifetime obligation.

The AM336 Debug pin reproduced `dsr-trim [line 49] No UP contradiction for RAT
clause 79`. The repaired Release and Debug pins each pass all six combinations
of random seed/replay and automatic/strict DSR/ASCII, with both ordinary backward
authority and forward every-lemma DSR-to-LSR-to-Trestle checking. A separate
six-fixture test covers XOR matching, ITE matching and XOR degeneration in
DSR/DRAT ASCII, replaying clause multiplicities and checking that no nonunit
proof-only clause remains after the inference.

### Failed certificate publication

Text serializers previously ignored sticky stdio errors and failed flushes.
`Proof::flush` now aborts on either; destruction checks flush and close. The CLI
already flushes before its verdict. All public API UNSAT-return paths now also
flush before publication: solve, status, direct rule, explicit instance and
online strategy. A failed write cannot become a successful UNSAT return merely
because the caller has not released the solver yet.

Tests exercise explicit flush and destruction in all six formats; all six CLI
format combinations reject `/dev/full` without printing UNSAT. The C API test
covers six formats times five verdict paths (30 controls), with setup failures
using distinct normal exit codes so they cannot masquerade as the expected
abort. Its first full-suite run exposed a fixture mistake: online Probe needed
post-propagation, and direct Probe needed a second application to close the
root contradiction. Correcting the fixture, not the production fix, made every
control pass. The V2 source freeze includes that correction.

## Completed four-row diagnostic evidence

| row | solver pin | solve seconds | PBP bytes | check seconds | result |
|---|---|---:|---:|---:|---|
| FmlaEquivChain_4_8_8 | AM336 `8aa84a97` | 3.322 | 388,840,230 | 526.734 | verified |
| tseitin_grid_n250_m250 | AM335 v1 `94cd4710` | 0.817 | 103,104,441 | 152.775 | verified |
| tseitin_d3_n100000 | AM335 v1 `94cd4710` | 0.617 | 73,298,895 | 366.783 | verified |
| tseitin_grid_n400_m400 | AM335 v1 `94cd4710` | 2.070 | 273,035,686 | 1,117.364 | verified |

Fmla primary result:
`benchmarks/runs/20260923-am336-startup-count/results/000.json`, job `1715952`.
VeriPB took 465.075 s and 6,653,440 KiB peak RSS, producing a 471,379,645-byte
kernel; CakePB took 61.657 s and 25,255,540 KiB peak RSS. Both returned their
exact verification token and exit 0 against original CNF. Solver peak RSS was
384,800 KiB. The rule used 229,376 stars, 20,036,944 work and 10,641,137 PB
additions. All stages fit the 32 GiB allocation. The runner deleted the checked
certificate and derived artifacts. The retained owner certificate was
436,975,142 bytes; AM333 clausal DSR was 4,503,640,567 bytes. These are size
comparisons, not matched-window runtime evidence.

Tseitin authority is
`benchmarks/runs/20260923-am335-startup-parity-retry`, job `1715866`, as reported
in the frozen AM336 checkpoint. These different diagnostic pins do **not**
substitute for combined-candidate Rank 1--50, B28+C5, WV100 or WV2-89 gates.

## Validation and in-flight ownership

- Six focused CTests pass on AM337 Debug: proof, startup_parity_check,
  count_preservation_check, congruence_sr_check, congruence_ladder_lifetime and
  proof_output_failure (6.35 s total). Release independently passed the fixed
  proof/replay and CLI-output-failure checker script.
- Full AM337 Debug suite has 178 entries: 176 passed, the initial API fixture
  failed and passed after correction, and the scripted-default demo remains
  running. This is not a completed full-suite pass. Earlier O0 AM335 and O1
  AM336 demos also remain active; their owning build directories are preserved.
- All documentation checks passed; `git diff --check` passed. Plan/calculus/
  ledger are at their budgets and implementation notes have one line spare.
  No documentation compaction was authorized or performed.
- AM335 and AM336 Claude owners are preserved. AM336 submitted focused count
  jobs `1715955`--`1715968`; live owner status, not submission, determines their
  results. Codex has not inspected either campaign's internals.
- New combined owner PID `2484169` was launched from
  `benchmarks/runs/20260923-am337-proof-lifetime-v2/claude-fuzz-request.md`.
  It owns `benchmarks/runs/20260923-am337-proof-lifetime-claude-fuzz/` through a
  terminal report or bounded handoff. Requested coverage includes broad
  Release default/random nightly, Debug default/random, focused parity/count,
  sanitizer, failure seeds, API, strict formats and negative oracles. The new
  owner is not asked to review or repair the implementation.

## Hygiene and next action

Queue audit maps AM335/AM336 fuzz jobs to their owner status tables; unrelated
`t4-*` arrays belong to `/home/hwu/sdsl`, and interactive shells are outside
this change. They are untouched. No material was manually deleted. Exact scans
of the AM335 retry, AM336 diagnostic and AM337 freeze directories found no
named DSR/PBP/LRAT/LSR/DRAT or decompressed CNF residue; anonymous checked Fmla
artifacts were removed by the runner. Frozen manifests, pins, reproducer,
predecessors and active owners are retained.

Consume Claude's terminal reports and debug any bounded handoff. Once the
combined correctness evidence is ready, run all four full routed gates with
immutable DSR/DRAT/PB pins and full-width SLURM arrays. Broad startup snapshot
allocation/discovery overhead has not yet been measured across those gates.
Then compare hcp_CP18_18 and 6g_6color at matched work against the owner; repair
deterministic mechanisms, never dosing. Excluded rows were not reopened.

Andrew deliverables:
`report-to-andrew/20260923T073500Z-proof-lifetime/report.tex` and `report.pdf`.

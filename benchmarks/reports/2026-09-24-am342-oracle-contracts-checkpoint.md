# AM342: conversion eligibility and strict checker contracts

Unaccepted tooling-only checkpoint, 2026-09-24 UTC. Solver/checker binaries,
N1–N4, startup routing, proof vocabulary, performance gates and user exclusions
are unchanged. This repairs AM340's bounded F1/O3 handoffs; it is not full
tooling or solver acceptance.

## Cause and repairs

F1: the optional DRAT-to-LRAT cross-check predicate compared raw literal strings
on individual lines. It missed `81 0081` and repeated literals split across
lines, and could mistake a repeat in separate clauses for an intra-clause
duplicate. It now compares numeric values within zero-terminated clauses and
is called on validated `checker.cnf`, just like the actual checkers. Existing
duplicate-literal and empty-input conversion exclusions are unchanged; native
LRAT remains mandatory. A skipped conversion is not reported as CakeLPR success.

O3: the tier runner matched a success substring without checking checker
identity or exit code. It now delegates acceptance to `proof_check_status`
with the actual stage name. Exact lines, exits and rejection precedence apply
to DRAT, LRAT and DSR stages; `splitlines` handles drat-trim's carriage return
without accepting an embedded or wrong-checker token.

The owner's related trivial-UNSAT stand-in observation is also covered.
Validated DIMACS metadata records the first real empty input clause, without
changing any clause/literal/ID/multiplicity. Runtime explicit/routed DRAT,
`check_routed` and the tier runner pass this semantic condition into status
checking. drat-trim's exceptional exit 1 with exact trivial-UNSAT success is
rejected when the input lacks an empty clause. Fuzz already enforces that
condition independently. Generic nonzero/signal exits remain unaccepted.

AM339 also reported a model-reader failure for a comment inside a clause.
Direct tests here establish that **separate comment lines** interrupting a
clause already work in both `modelcheck.check_files` and
`generators.parse_dimacs`, including CRLF, whitespace and decimal leading zeros.
Correct models pass and falsifying models fail; inline `c` tokens remain
malformed. No model/parser implementation changed. These tested cases do not
resolve a different, as-yet-unsupplied bounded legal failing layout; the new
owner is asked to provide it if found. The old report remains frozen.

## Reproduction and validation

The new two focused tests fail before repair with 14 subtest failures and pass
afterward. Final suites on the frozen tooling:

- `tests/test_fuzz_proof_oracles.py`: 28/28, including actual pinned checkers,
  semantic empty-input stand-ins, numeric/layout cases and strict exits.
- `tests/test_modelcheck.py`: pass, including new legal-comment cases.
- bench proof-oracle suites: 18/18; bench runtime: 12/12; runtime integration:
  7/7. Commands and results: authority `logs/fixed.log`.
- Final complete `tools/fuzz/selftest.sh`: PASS, with known-bad model/proof
  wrappers flagged for the intended reasons and planted-core shrinking.
  Kissat reaches 145/150 verdicts, CaDiCaL 96/100; nine timeouts are lost
  coverage, not certified cases. Conversion skips are 24 and 18, respectively.
  `logs/selftest.log` is the final unchanged-snapshot run. An earlier exploratory
  selftest is not the frozen acceptance evidence.

Direct bounded SLURM job **1716501** (2 CPUs / 16,000 MB, sequential saved-proof
diagnostic, not a performance measurement) completed exit 0 in two seconds.
It extracts the immutable AM340 and AM342 tooling archives into separate roots,
replays the exact F1 saved proof with unchanged pinned checkers, and exercises
the three O3 stand-ins. Old tooling rejects the genuine F1 proof and accepts
all three O3 outputs; new tooling accepts F1 with its explicit duplicate-class
conversion skip and rejects all three stand-ins. Results and full checker logs
are in `validation-1716501/results.json`. No solver invocation or private
campaign inspection was needed; only the explicitly bounded handoff was used.

## Authority and independent validation

`benchmarks/runs/20260924-am342-oracle-contracts/manifest.json` pins:

- base `f7d5f01289adfb942bd1b8190833e7730718bc29`;
- patch `ab061e1f09900317a5e4bfdfbfe1b9190cb96b98b79412c4871fa79dd8b769ae`;
- overlay archive `f429b32993fa39b22e2babae3228f03508a303a6b5af141f773a1e268692a0df`;
- unchanged AM341 Debug `722f1eb5` / AM339 Release `cbe99910`, and all seven
  AM341 proof checker pins through the candidate manifest.

All 293 tooling-file hashes and archive/patch hashes match after the final tests.
The explicit Opus owner (PID 2015745, session 78542) is PREPARING under
`...-am342-oracle-contracts-claude-fuzz`. Its immutable request requires old
controls, strict statuses, real-empty semantics, original input preservation,
anonymous scratch, full selftest, and 30k affected legal-layout default/random
cases with independent models/proofs and replay. This is tooling-focused
validation, not repetition of unchanged broad solver legs. No Fable is used.

The independent AM341 owner exited 0 before terminal reporting while its jobs
remained live. Recovery Opus PID 2078249 / session 81011 continues the SAME
campaign from `...-am341-unwatch/claude-owner-terminal-recovery.md`; no job was
restarted. Published owner status records the exact handoff matrix and
nonvacuous count-PB/tamper controls passed. Debug random 1716500, sanitizer
1716503, and Lucky floor arrays 1716504/1716512 remain live. The full AM341
Debug CTest still has 178/179 passed, demo pending, session 3825. Preserve it.

## Remaining acceptance work and hygiene

Next: genuine-empty-input routed LRAT bridge; routed duplicate-literal mismatch;
malformed DSR route classification; DSR reader layout limits; and propagation
of the requested checker window past drat-trim's internal 40,000-second default.
None is waived by this checkpoint. Then finish combined fuzz and successor full
routed gates before promotion, followed by the requested AM333 matched-work
owner comparison for hcp_CP18_18 and 6g_6color. All four startup targets already
verify on AM337, but its terminal coverage remains 50/50, 33/33, 97/100 and
82/89 (78 within 600 seconds), not full success and not transferred to AM341.

Queue audit maps task-owned jobs to the AM341 campaign and preserves unrelated
sdsl t4-ws500/sweep-repeat and interactive allocations. The 272 terminal AM337
results retain their complete cleanup status. New bounded validation artifacts
are evidence; all authorities, baselines, caches and active/private campaigns
are preserved. No new benchmark files or jobs were deleted/cancelled.
Successful selftest scratch is removed by the maintained selftest's own trap.
Documentation checks pass with pre-existing size debt; no compaction occurred.

Andrew deliverables: `report-to-andrew/20260924T013300Z-oracle-contracts/report.tex`
and `report.pdf`, compiled twice. PROGRESS owns live continuation status.

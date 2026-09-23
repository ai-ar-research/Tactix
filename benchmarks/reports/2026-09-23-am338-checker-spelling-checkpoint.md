# AM338 — original-clause-preserving checker spelling

Intentional tooling-only checkpoint, 2026-09-23 approximately 08:00 UTC.
No solver or checker binary changed. No combined candidate is promoted.

## Evidence and causal diagnosis

AM336's published owner status isolated three verified-reader restrictions:
CakePB rejects split clauses and multiple clauses per line; Trestle rejects
interleaved comments and a zero-variable header. The owner reports identical
PBP bytes and successful VeriPB checks on the layout variants, followed by
CakePB's own DIMACS parse failure. The pre-AM336 pins exhibit the same issues.
Malformed-input Debug assertions are separate, pre-existing solver failures;
this change does not repair or waive them.

Codex independently reproduced the fully specified smallest case, without
reading campaign internals: `p cnf 0 1` followed by `0`, using AM337's pinned
Release solver. It returns 20 and the two-byte DSR `01 00`. Pinned Trestle rejects
with `Invalid header: 0, 1, or iter out of bounds`. The local source at
`trestle/Experiments/SR/Parsing/SRParser.lean` explicitly rejects zero variables
or zero clauses in `parseHeader`; its clause loop does not skip comments.
This is an input-reader failure, not a rejected contradiction inference.
The independent bounded reproducer remains at
`benchmarks/probes/scratch/am338-empty-input-NoPU27/` and is now covered by tests.

## Repair contract and soundness

`canonical_checker_cnf` validates the original DIMACS and writes one clause per
line for the final proof checker. The complete ordered literal sequence, clause
boundaries, repetitions, tautologies and clause IDs are preserved. Only comments,
whitespace and integer spelling change. A header variable bound of zero becomes
one unused identity because of Trestle's restriction. No literal using that
identity is added; the CNF clause sequence is exactly unchanged. Extending a
valuation by an unused Boolean preserves all clauses, so UNSAT of the spelled
formula implies UNSAT of the original, including the zero-variable case.

The adapter rejects missing/repeated/malformed headers, nonnumeric literals,
out-of-bound variables, incorrect clause counts and missing terminators. It
refuses to overwrite the source inode. It never repairs malformed input by
dropping or adding clauses. Tests cover leading-zero integers without changing
their values.

The solver and elaborator (VeriPB, dsr-trim or drat-trim) continue reading the
exact original file; only CakePB, Trestle or CakeLPR receive the validated
spelling. Both proof stages still must verify under the unchanged exact
success-token and exit-status contracts. Native DSR fuzz retains the forward
every-lemma audit in addition to backward authority. The routed runner records
both input hashes, counts and bounds, and uses anonymous node-local scratch.
The helper reads one physical line at a time, not a whole formula image;
additional checker-stage parsing/I/O cost is not yet benchmarked.

The SAT model oracle now returns an explicit failed check on malformed tokens
or I/O errors instead of raising an uncaught exception. Its assignment and
clause semantics are unchanged. In particular, an inline `c` token cannot
silently skip the model oracle. This does not make malformed solver input legal.

## Provenance and validation

Tooling authority:
`benchmarks/runs/20260923-am338-checker-spelling/manifest.json`.
Base commit `2a377f7c37e8c6c301f2a2da8e1fa3b5e2473e25`;
patch `95497870d0b67d51a32e4414b945d78ce17c4d211ffa1ddb29a4c33ebf5f3489`;
tooling archive `6bc6d112f5aa662f230a768364b599de21ae5a0868632966daf4300244f869da`.
The manifest records every frozen harness/test file hash and refers to AM337's
unchanged binary/checker authority. Release remains `40b0de22...`, Debug
`52ee866b...`; all seven proof-checker pins remain unchanged.

- `test_fuzz_proof_oracles.py`: **22/22 pass**, including real pinned PB and
  DSR layout variants, zero-variable empty input, backward/forward audit,
  preserved source hash and clause sequence, refusal to overwrite original,
  malformed-input rejection before any checker call, invalid empty lemmas on
  SAT and zero-clause inputs, wrong CNF, truncated and unused-invalid PBP,
  strict success statuses and final-checker dispatch.
- `test_runtime_task.py`: **7/7 pass**, including the real two-checker PB route
  with split/multi-line input on anonymous scratch, input hashes, unchanged raw
  file, failed proof on a SAT input and cleanup.
- `test_modelcheck.py`: passes, including malformed CNF/header/model tokens.
- Five affected CTests pass: modelcheck, runtime_task, startup_parity_check,
  count_preservation_check and congruence_sr_check.
- `tools/fuzz/selftest.sh`: **SELFTEST OK**. Fixed Kissat stand-in: 145/150
  reached verdicts, 5 timeouts; CaDiCaL: 96/100, 4 timeouts. Neither timeout
  population ran an oracle. Known model lies, negated models and truncated
  proofs were detected; the planted-core shrink and native-LRAT controls pass.
  These are harness controls, not Tactix combined-candidate fuzz acceptance.
- Documentation and whitespace checks pass. Implementation notes now use
  560/560 lines; plan, calculus and parity ledger were already at their budgets.
  No documentation compaction was authorized or performed.

## Owner interruption and in-flight gates

The new AM338 Claude-owned request covers focused layout/negative-oracle
validation, all changed callers, hash/anonymous-scratch contracts and unchanged
solver/checker pins. Its CLI invocation exited **1** before preparation. All
three earlier owner handles also ended with exit 1 and their PIDs disappeared.
The four launch logs contain the same terminal operational error:
`You've reached your Fable limit.` No observation timeout was treated as exit,
and no campaign was restarted blindly. The user was asked to restore quota or
select another available Claude model; no paid capacity or model switch was
assumed. Codex did not inspect private campaign internals.

AM335/AM336 submitted SLURM jobs are still live. AM337's last owner status is
PREPARING, not a terminal fuzz report; AM338 has no prepared campaign. All
requests, immutable snapshots and evidence are preserved. Pending ownership
cannot satisfy the required fuzz gates.

The four AM337 benchmark arrays (`1715989`--`1715992`) also remain live. At the
latest result inspection, Rank has 25 valid results (19 SAT, 6 UNSAT); the other
three arrays are queued. They use their original frozen pre-AM338 tooling;
neither their pins nor manifests were modified. These partial results do not
close a gate. The original Debug demos remain running. No material benchmark
evidence was deleted; the self-test removed only its disposable scratch on
success. The bounded reproducer is retained deliberately as failure evidence.

## Next action

Restore Claude-owned preparation/reporting on the existing campaigns and run
the AM338 frozen tooling request; do not repeat passing unchanged legs. Repair
the remaining malformed-input solver assertions through bounded regressions.
Continue inspecting every terminal gate result and cleanup before acceptance.
The hcp/6g owner comparison and all original exclusions remain unchanged.

Andrew deliverables:
`report-to-andrew/20260923T080000Z-checker-spelling/report.tex` and `report.pdf`.

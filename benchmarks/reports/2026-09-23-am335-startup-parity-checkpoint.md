# AM335 startup parity PBP: implementation checkpoint

This is an ungated checkpoint, not promotion over AM333. The user approved
the startup-certificate proposal. The active goal still includes Fmla count
preservation, Claude-owned fuzz acceptance, all four routed gates, and the
matched-work hcp/6g repairs. No excluded row was reopened.

## Implementation

Automatic CLI/API output selects one terminal original-input PBP refutation or
binary DSR. Only input normalization is privately staged; any ordinary action
closes the startup boundary and publishes DSR. No published prefix is replaced.
Explicit DSR/DRAT/LRAT and ASCII diagnostic mode retain their meanings.

Shared Lucky instance 19 recognizes complete XOR groups of arity 2--4, even
variable incidence, and odd total right-hand side. It uses existing typed
recognition/proof bounds. Threshold carry definitions, local contradiction
subproofs, summation, and division yield a PB contradiction. Recognition and
count-only preflight precede publication; no-proof execution uses the same
logical action. The last variable of each local assignment is propagated from
the complete XOR encoding rather than split again. This is a structural rule,
not a generator/filename/numbering recognizer. Fmla's separate counting rule
has not yet been implemented.

Acceptance requires pinned VeriPB 3.0.2 to elaborate the original CNF and
CakePB to check that elaboration. Both exact success tokens and exit zero are
required. Automatic routing was added to both the benchmark and fuzz harness;
the fuzz SLURM wrapper freezes the PB checkers with its harness snapshot.

## Immutable provenance and diagnostic results

Base commit: `fce7c249bbadc8ab6b0aad3dd04ffbe1877b4ef4`.
The first snapshot is `benchmarks/runs/20260923-am335-startup-parity`:
Release `94cd471005e5bbab427dd3bc83f066c0347e34c4865fd4ba9fe9ed4a71f53253`,
Debug `4404101fcc268766abccbc9017ea6fa5c148f7d6dfe9bee73297398c2e3a2466`,
source patch `bdf5d222461f0dea0bc6699c3557afd51d8db157c192cf17967f60f83f6a9e7d`.

Job `1715851` solved all three diagnostic rows but failed before checking: the
runner attempted to unlink an anonymous `/proc/PID/fd/N` kernel-proof file.
The maintained runner now truncates that handle. A real two-checker fixed-proof
test covers this exact scratch implementation. The original failed campaign
is retained unchanged. Retry `1715866`, in the sibling `...-retry` campaign,
uses the same solver binary, inputs, checkers, and 32-GiB allocation, with only
the runner repair. Arrays have full width and no node constraint.

| Instance | Solve (s) | PBP bytes | Complete check (s) | Checkpoint acceptance |
|---|---:|---:|---:|---|
| tseitin_grid_n250_m250 | 0.817 | 103,104,441 | 152.775 | VeriPB + CakePB verified |
| tseitin_d3_n100000 | 0.617 | 73,298,895 | 366.783 | VeriPB + CakePB verified |
| tseitin_grid_n400_m400 | 2.070 (first attempt) | 273,035,686 | pending | not yet accepted |

Accepted rows are from `...-retry/results/{000,001}.json`; the pending row's
size and solve time above are from the first campaign's `provisional/002.json`.
The grid250 kernel elaboration is 162,432,993 bytes; its VeriPB and CakePB
stages take 39.280 and 113.494 seconds. All completed runs deleted their
certificate and decompressed-input scratch; active scratch is anonymous.
Old sizes and owner measurements are preserved in the approved proposal,
`2026-09-22-startup-certificate-proposal.md`. These are cross-campaign retained
measurements, not a matched-window performance gate.

Review found a separate I/O regression: automatic DSR fallback had inherited
buffered stdio on its destination, so a small write to `/dev/full` could appear
successful. A forked fixed test reproduced that behavior. Five backend lines
restore the ordinary binary serializer's unbuffered publication before any
destination bytes; the regression now passes. PB emission is unchanged.

The repaired v2 snapshot is `benchmarks/runs/20260923-am335-startup-parity-v2`:
Release `9cf4bd0855e30d22ebffa3c60d54244271e85e31bbb72c9f2d163f807e7afdc2`,
Debug `0b00296108ad92f7ada65c1a3dc7af5f0fb3def933baa605b5ea926f8f989aaf`,
source patch `cdc110d6b0e442a8287a3f34837b3368d0336edbe93624baef9c1878d5705fc9`.
Its manifest and source archive permit independent rebuild. The diagnostic
table is explicitly v1 evidence, not a claim that the v2 full gates ran.

## Validation and remaining gates

- Fixed degree-2/3/4 XOR graphs, sign-flipped/renamed/permuted copies, even-RHS
  SAT controls, API and registry reachability, closed action boundary, proof
  budget, and interruption checks pass. Six emitted micro-proofs and the
  default CLI path pass VeriPB and CakePB. A raw-input normalization control
  with duplicate literals/clauses and a tautology also passes both checkers.
- Format locking and DSR fallback byte tests pass; `/dev/full` PB and DSR
  controls detect failed publication. Eighteen fixed oracle tests and twelve
  runtime acceptance tests pass, including missing/failed PB checker stages,
  false UNSAT, truncated proof, invalid unused lemma, and explicit-format
  rejection. These are fixed tests, not substitutes for fuzzing.
- The 173-entry Debug CTest run has two long-running entries at this checkpoint
  (Lucky and scripted-default demo). The other entries have passing results
  after correcting the strict-DSR micro harness and using immutable AM333 DSR
  checker pins. The initial Sweep check failed with mutable convenience tools;
  `tools/checkers/bin/{dsr-trim,dsr-compress}` differ from the required hashes.
  Explicit pinned micro/Sweep reruns pass. All six v2 focused tests pass,
  including the micro/Sweep suites (135.54 s combined wall time).
- Documentation checks pass with five reserve warnings; no compaction was
  performed. Plan, calculus and scheduling-ledger budgets have no line reserve.
- Claude's first coordinator was interrupted to refresh the candidate after
  the I/O repair. The v2 owner request resumes its prepared campaign at
  `...-am335-startup-parity-claude-fuzz`. No terminal owner report exists at
  this checkpoint; fuzz remains unaccepted. Codex does not inspect campaign
  internals or reinterpret the owner's results.
- Rank 1--50, B28+C5, WV100, and WV2-89 have not run on this candidate. Their
  staging must copy the maintained runner and proof oracle and pin DSR, DRAT,
  and PB checkers. The combined candidate still needs Fmla's general
  structural count-preservation rule. No full-coverage or performance claim
  is made for AM335.

The explicitly requested AM334 terminal table amendment records the ncc and
three Tseitin timeouts. Retired `build/am333-release-1714363` was removed
(513,033,589 bytes); byte-identical binaries remain in the AM333 gate pins.
No active campaign, reference baseline, or retained proof-size evidence was
removed. Next actions are owned by PROGRESS, not this frozen checkpoint.

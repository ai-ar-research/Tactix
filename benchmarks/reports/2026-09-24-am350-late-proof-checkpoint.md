# AM350: legal late-proof attachment and exact literal-addressed input population

## Outcome and scope

This is an **unaccepted correctness checkpoint**, not an N2 or combined-gate
closure. It repairs the public C API contract that allows proof attachment after
input clauses but before the first reasoning call, and the F3 population drift
caused by normalized DRAT input. No rule, instance, guard, schedule, or
benchmark-specific path was removed. Proofless import and the existing solver
state remain in place. N3/N4 performance gates and post-goal full Tier D were
not run.

## Mechanism

The C API now delays proof publication when a writer is attached after any
completed clause. At the first reasoning boundary, it replays the complete
zero-delimited raw input in a temporary proof-bearing kernel. This reserves all
original clause IDs before native LRAT-derived IDs, reconstructs root-unit
proofs and DSR/DRAT import transitions, then binds the proof back to the
existing session. Native LRAT proves each live clause image that eager
proofless import normalized beyond the replay, with root-unit hints followed
by its original raw clause ID, and remaps its binary/large proof identity.
Assignments, watches, reasons, heuristics, configuration, and action
availability stay with the original session. The raw snapshot is released
for explicit clausal formats after reconciliation; automatic startup PBP keeps
its pre-action snapshot. A writer still cannot be attached after reasoning.

Literal-addressed DSR **and DRAT** input now publish each normalized clause
and retire the exact raw predecessor immediately. Duplicate binary copies,
tautologies, and root-satisfied clauses that were not stored also retire their
raw copy. Previously DRAT kept the raw predecessor, so a later deletion of
the live normalized clause had no checker-held counterpart. Native LRAT's
existing deferred input normalization remains distinct; its extra raw-clause
population is a separate audit question, not claimed closed here.

## Evidence

- Before repair, F2 late-attached native LRAT rejected while early attachment
  verified (`20260924-am349-congruence-diagnostic`, job 1717164). With the
  AM350 Debug candidate, job **1717184** independently verifies F2 with both
  timings against the original CNF via pinned CakeLPR. Job **1717204** also
  verifies the two separately handed-off cases 005791 and 009769.
- Job **1717215** verifies F2 late attachment in binary and ASCII DSR through
  backward and every-lemma dsr-trim/LSR/Trestle.
- A fixed midstream C API input attaches after two clauses, then adds four
  more. Job **1717207** independently verifies its LRAT proof; drat-trim
  returned exit 0 and `s VERIFIED` for DRAT, but the wrapper's exact-line
  check missed drat-trim's carriage return and marked that task failed.
  Checker-only job **1717209**, with CR normalization, completed 0 and
  reconfirmed `s VERIFIED`. The script incident did not change a solver or
  checker artifact.
- In the AM346 F3 corpus, the original proof for case 000111 fails the
  independently maintained exact clause-multiset/every-addition-RUP audit at
  step 49: deletion of absent `(4,6,9)`. AM350 job **1717192** runs the same
  public canonical action on all 11 retained F3 cases; every proof passes
  that audit, with 19–29 verified additions and no absent deletion.
- `test_parse` checks final exact checker population in both ASCII DSR and
  DRAT, including root-false narrowing, duplicate literals/binaries,
  root-satisfied clauses, and tautologies. `test_api_c` checks all six public
  formats with late attachment before further clauses and with already
  contradictory input. Debug and Release targeted `parse`/`api_n1` pass.
  Full AM350 Debug CTest is live in
  `benchmarks/runs/20260924-am350-late-proof/ctest-debug-all.log`; it is not
  counted as complete here.
- That CTest's `sweep_proofs` case 033 failed under the mutable
  `tools/checkers/bin/dsr-trim` symlink: its forward elaboration omitted two
  final empty-clause hints, and Trestle rejected the resulting LSR. The AM349
  and AM350 solver proofs for 033 are **byte-identical** (SHA-256
  `f0cf8f2edebb4491dd0b0fd1194081269c4a7ea4c22d69784eed12fedc707796`).
  Both pass the pinned AM349 checker, including every-lemma Trestle (job
  **1717229**). The live checker's dirty forward-dependency optimization
  rejects the unchanged proof (job **1717232**); its emitted LSR differs only
  in the final hint list. Full pinned `sweep_proofs.sh`, job **1717233**,
  verifies all 47 derivations and all five DSR cases. The mutable checker
  source is left untouched. This is not an AM350 solver regression, but the
  default local CTest outcome is not being reported as an all-pass.

## Provenance and open work

The original CNFs, exact source/driver/checker pins, proof files, checker
outputs and SLURM logs are retained under
`benchmarks/runs/20260924-am350-late-proof/` and the cited AM346/AM349
campaigns. Debug CLI SHA-256 `bff417f1561fe2d16ea604da67a3dd38366c5fa38414e49607619df446b7283b`;
Release CLI SHA-256 `bbadff02664d317766d805c90c5af890e252459257ad95d12ab28cc5323e6701`.
The immutable source snapshot and full pin manifest are recorded in the
AM350 run directory after the source commit; the executable source changes
are `src/api/api.cpp` and `src/frontend/parse.cpp` only.

Claude Code's Opus AM349 campaign is still active; it does not validate this
new source. AM350 needs its own Claude-owned fuzz report, sanitizer coverage,
and full Debug CTest before acceptance. F4 (Debug target-phase population
assertion), AM347's checker descendant-pipe deadline, remaining bounded
handoffs, the all-baseline proof-rule inventory, and N1–N4 audits stay open.
The first winner baseline's 4 CPUs/32 GiB/5,000 s and 45,000 s checker
allowance are preserved for a **post-goal** Tier-D run; no Tier-D submission
is implied by this checkpoint.

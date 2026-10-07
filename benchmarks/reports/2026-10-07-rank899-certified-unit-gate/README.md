# Rank899 certified-unit gate on d576abaeb

This report executes the bounded gate in
`../2026-10-07-rank899-unit-admission-handoff/README.md` for commit
`d576abaeb` ("Always admit checked symmetry units"). Both steps pass. On the
original `mp1-Nb7T42` CNF, the d576 Release solver is SAT in 55.921 s with a
verified model. The unchanged b26 Release solver, run on the same CPU in the
same allocation, is UNKNOWN at 300 s. This is one instance and one draw per
arm. It is a causal closure of the certified-unit effect on Rank899. It is not
a default-policy verdict, and it does not replace the regression gates.
`evidence.json` records every number below together with its artifact hash.

## Pins

- The source is `git archive d576abaeb`, stored at
  `benchmarks/runs/20261007-rank899-d576-source/source.tar` with SHA256
  `b5041e19…767d`. It contains tracked files only. The Release `tactix` and
  `dsr-transport` targets read no ignored reference clone. SLURM job
  **1755552** built both pins from that one archive with the b26 recipe
  (gcc 13.1, Release, PROOF_SHADOW and RULE_JOURNAL off).
  - `build/pin-rank899-d576-rel` uses EVENT_TRACE=OFF. Its `tactix` SHA256 is
    `5d48dbaccedadf457d78f16211b397122d1bbf7c7d8d667f741598395aac0a37`.
  - `build/pin-rank899-d576-trace` uses EVENT_TRACE=ON. Its `tactix` SHA256 is
    `384c337b…7ab3`.
  - Both pins build a `dsr-transport` that is byte-identical to b26's.
- Focused gate **1755545** passed with Debug and ASan both exiting 0. The
  singleton DSR is `s VERIFIED UNSAT` under every-lemma checking. The invalid
  witnessed-unit control is rejected. Its `src/`, `tests/`, `third_party/` and
  `CMakeLists.txt` are byte-identical to d576.

## Step 1: boundary (PASS)

The frozen 1754883 extractor is reused byte-identically: `run.py` is
`275565df…`, `compare.py` is `311d7a8e…` and the Satsuma pin is `0680c895…`.
It ran in job **1755557**, on node015, with graph labeling off and the snapshot
taken at `preprocess_begin`. A supplement repeats the integrated arm with
`--default-graph-labeling=on`, the causal-pair option, and moves the trace
endpoint to `preprocess_end`. That supplement is job **1755837**. Its
predecessor in job 1755557 was cancelled by me before it produced a result: its
comparator recomputed the residual roots once per clause. It was never scored.

| Check | Result | Evidence |
|---|---|---|
| Root unit `+40310` comes from the concrete large-domain producer | PASS | The SR trace shows `symmetry_generator_fixing_result negation_units 1` (the `large_domain_generator_fixing` path; 5 generators, all validated). Event `root_assign 40310` appears at seq 22. Feedback reports `sr_applied` 1 and `sr_singleton_forcing_rows_declined` 0. The d576 end-of-run counters read `c sr 1 applications, 1 applied, 105461 witness literals` and `0 singleton forcing batches declined`; b26 reads `1 … declined, 1 rows`. |
| The proof prefix verifies independently | PASS | See the next table. |
| Canonical live-clause equality with the actual Satsuma residual | PASS | Satsuma was rerun and reproduced the residual byte-identically (`792bb4ab…`). Frozen `compare.py` reports `equal_reference_multiset: true` for integrated with 208,933 canonical clauses, root set `{40310}`, and `formula_multiset_equal` and `root_set_equal` both true against the fresh residual. The graph-on arm is identical. |
| Activation queue and presentation | PASS | The queue is identical to the fresh-residual queue: 52,730 entries and no differing index, with graph labeling either off or on. The ordered clause storage differs in exactly one known way. Integrated keeps the 8 ternaries containing 40310 in place, with a root-false literal; this is how a reader that adds the clauses before the residual's trailing unit line holds them. That Kissat stores them this way is inferred from that file order; it was not traced. A fresh Tactix parse instead rewrites them to 4 binaries and drops 4 satisfied clauses. The canonical multisets are equal. |
| Structured root grows the trail | PASS | The macro trail is 0 through `exact_one_refutation_end` and 1 from `direct_csp_model_begin` onward. It is 1 at `preprocess_begin` and at `preprocess_end`. |
| `fastel_root_suppressed` increments | PASS | The d576 counter line in the causal pair reads `c fastel 0 checks, … 1 root-suppressed, 0 eliminated`. b26 reads `2 checks, … 0 root-suppressed, 28741 eliminated`. |
| Startup FastEliminate rounds stay at zero | PASS | `fastel_rounds` is 0 (the same line). The graph-on macro trace has no `initial_fast_eliminate_*` events between `preprocess_begin` and `preprocess_end`. |

To check the prefix, the d576 Release solver ran with
`--auto-proof --default-graph-labeling=on` on the original CNF. It was stopped
by the 64 MiB diagnostic file cap at 13.5 s (SIGXFSZ). Only the proof prefix is
used from that run, not its counters. The proof was cut after the first
addition of unit `+40310` and the deletions that follow it. The cut proof is
299,918 bytes, SHA256 `039a8160…`. It has five lines:

1. `E = (-52732 40310)`
2. `K = (52732 40310)`, witnessed: z and 40310 true, plus 52,730 literal mappings
3. `C = (40310)`
4. delete K
5. delete E

Every checker below is the canonical bundle that the runner pins (hashes in
`fixed_runtime.CANONICAL_CHECKERS`). Checker-only job **1755853** ran the
diagnostic rows.

| Checker | Tactix prefix | Satsuma's own prefix (control) |
|---|---|---|
| `dsr-trim -f` (forward, every line) | `s VALID` | `s VALID` |
| Trestle `srcheck <cnf> <lsr> c` on binary LSR | `s VALID` | `s VALID` |
| Trestle on text LSR | `s INVALID`, already at line 1 | `s INVALID`, already at line 1 |
| Witness removed from K (negative control) | dsr-trim: `No UP contradiction for RAT clause 208937` | — |

The text-mode INVALID is a checker artifact on non-refutation proofs, not a
defect in the emitted proof. The likely cause, read from the source but not
separately tested, is that Trestle's text driver folds over
`lsrContents.splitOn "\n"`, so the trailing empty element fails to parse as
`.error false`. In a refutation, the empty-clause line returns `.error true`
first, so the problem never shows. Satsuma's prefix fails identically even
though the reference pipeline accepts it. Trestle's binary driver ends at the
byte bound and accepts both prefixes. Declaring the extension variable in the
CNF header changes nothing. Tactix and Satsuma prove the same unit with
different but valid symmetries; for example, Tactix maps 41→26713 where
Satsuma maps 41→26714.

## Step 2: causal pair (closure)

Job **1755843** is the campaign `benchmarks/runs/20261007-rank899-d576-causal-pair`.
It ran through the repository runner `tools/bench/runtime_task.py`, with
anonymous O_TMPFILE scratch for the CNF and proof. The allocation was
`cpu-dense-hwulab-q`, 12 CPUs and 48,000 MiB on node018. The two tasks ran in
sequence in that one allocation, with both pinned to the same CPU (affinity[0]).
Both used `--auto-proof --default-graph-labeling=on` with a 300 s solve limit.

| Arm | Binary | Result | Wall | Conflicts | Decisions | Restarts / switches / rephases / walks |
|---|---|---|---:|---:|---:|---|
| b26 | `b26efc0f…` | UNKNOWN (timeout) | 300.005 s | 4,294,533 | 74,804,723 | 152,211 / 59 / 42 / 14 |
| d576 | `5d48dbac…` | **SAT**, exit 10, `checkmodel: OK — 208936 clauses satisfied, 52731 vars assigned` | **55.921 s** | 638,214 | 15,456,796 | 32,819 / 29 / 21 / 7 |

For context only, b26 was also UNKNOWN at 600 s in its WV2-89 regression row
(`20261007-firstopen-b26ef-regression-wv2-89/results/035.json`). The
historical Satsuma+Kissat 107.102 s result comes from a different window. It is
reported separately and is not compared with this pair.

## Interpretation and limits

The certified-unit change is the controlled difference, and it changes
startup in two ways. It publishes `+40310`. That trail growth then suppresses
startup FastEliminate: 28,741 eliminations in b26 versus 0 in d576. d576's
startup therefore equals the reference boundary: Satsuma's residual followed by
Kissat 4.0.4, which has no fastel. The earlier exact 50-conflict match was
measured at that same boundary (job 1755460). The pair does not separate the
unit itself from the suppression it triggers; the two are one mechanism under
the current `default_should_run_fast_elimination_after_structured` guard.
This is one Rank899 draw per arm (see
`chaotic-instance-veto`). No search-policy change is proposed. Step 4, the
diagnosis of residual Boolean-search divergence, was not needed.

## Cleanup and storage

The formula and proof copies made by the boundary campaign have been deleted.
Their hashes are in `benchmarks/runs/20261007-rank899-d576-boundary/CLEANUP.json`.
The causal pair used the runner's anonymous scratch. The boundary trace
snapshots (`*.large.tsv`, `*.binary.tsv`, `*.queue.tsv`) remain in the run
directory as the extracted trace. They are not committed.

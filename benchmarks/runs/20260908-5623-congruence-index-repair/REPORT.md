# 5623 — Congruence index cleanup unit-test repair (escalation under plan §8.2)

Verdict: **PASS**. Bad test expectation, not an invalid proof delta or solver
failure. The solver source is unchanged; one test file changed.

## Scope

- Prototype under review (unsubmitted, preserved): `.claude/worktrees/runtime-congruence-index-5619`
- Parent (immutable, untouched): `.claude/worktrees/runtime-factor-root-support-5592`
- Repair copy: `.claude/worktrees/runtime-congruence-index-repair-5623`
- Original failing builds/logs (preserved): `build/pin-{5619,5620,5621}-congruence-index-{release,trace,bc}`,
  `benchmarks/runs/20260908-5619-congruence-index/logs/ctest-{release,bc,trace}.log`
- Repair builds: `build/pin-congruence-index-repair-5623-{release,bc,trace}`
- Repair logs: `benchmarks/runs/20260908-5623-congruence-index-repair/logs/`

## Reproducer

`build/pin-5619-congruence-index-release/tests/test_congruence_index_cleanup`
aborts at `tests/test_congruence_index_cleanup.cpp:81` (`actual == expected`).
The progress lines it prints per case were fully buffered and lost at abort,
so "no case printed" did not identify the failing case. A diagnostic copy of
the fixture that reports instead of aborting
(`benchmarks/probes/scratch/5623-congruence-index-repair/index_cleanup_diag.cpp`,
linked against the 5619 release objects, log `logs/diag-5619-release.log`)
shows exactly 20 of 1200 cases mismatch, all with the same shape:

| dimension | failing cases |
|---|---|
| format | DRAT ascii only (every LRAT case passes) |
| value | +1 (satisfied) |
| position | 0 only |
| width / negative / copies | all 5 widths x both polarities x 1 and 2 copies = 20 |

Example (width 3): expected `d 1 2 3 0`, actual `d 2 1 3 0`. Every mismatch
is the same clause with its first two literals exchanged; the LRAT line ids,
the deletion count, the garbage flags, the live-clause count and both NoOp
results are all as expected.

## Root cause

`congruence_apply` enters through Tactix's dense-to-sparse boundary
`kernel_canonicalize_binary_watches` (src/kernel/kernel.cpp, existing parent
code, unchanged by the prototype). That rebuild calls
`sort_clause_watches_like_kissat` on every live large clause, which is the
transliteration of Kissat's `kissat_sort_literals` at sparse resume: a
satisfied `lits[0]` is exchanged with the first open literal, and a false
`lits[1]` with the first non-false literal after it. With the fixture's unit
at position 0 and every other literal open, the record becomes
`(lits[1], lits[0], ...)` before the XOR index visits it. The prototype's
cleanup then calls `kernel_delete_clause`, whose DRAT line writes the
record's current literal sequence. The test built its DRAT expectation from
the arena snapshot taken before apply, so it encoded the pre-rebuild order.
Positions 1..5 never move (an open `lits[0]`/`lits[1]` is kept), and LRAT
names the clause by id, which is why only DRAT position-0 cases failed.

The emitted delta is valid: a DRAT deletion names a clause as a literal set.
Confirmed with the project checker on a fixed 3-clause microfixture
(`checker/`): `drat-trim` verifies the swapped-order deletion with no warning,
verifies the original order identically, and warns
"deleted clause ... does not occur" only for a genuinely absent clause.
No real CNF was solved; no SLURM job was needed.

Native comparison (static, no runtime claim): Kissat's
`init_xor_gate_extraction` marks a satisfied clause garbage, emitting the
deletion immediately (`mark_clause_as_garbage` -> `DELETE_CLAUSE_FROM_PROOF`),
and stops at the sixth open literal, which the prototype's
`ls.size() <= CONG_XOR_ARITY + 1` cut reproduces. Kissat indexes in dense
mode before its resume sort while Tactix indexes after the rebuild, but the
rebuild is at most one exchange of position 0 (or 1) with the first
non-false literal after it, which preserves the multiset of literals before
every later position and changes the count of open literals preceding the
first true literal by at most +1 from 0. It therefore cannot move a true
literal across the five-open-literal cap; the fixture exercises positions
0..6 at width 7 and confirms the cap in every configuration.

## Fix (test only)

Changed path: `.claude/worktrees/runtime-congruence-index-repair-5623/tests/test_congruence_index_cleanup.cpp`
(diff: `test_congruence_index_cleanup.diff` in this directory).

- The expectation is built after apply from the retired record: exact bytes
  for both formats are still required, so the emitter contract (DRAT writes
  the record verbatim, LRAT writes `<next id> d <clause id> 0`) is unchanged.
- Each retired record is additionally pinned to a permutation of the original
  clause and to the original width, so a deletion can only ever name the
  fixture clause.
- `k.next_clause_id == proof_line` is asserted after both applications, so
  the deletions are provably the only proof events (no lemma was added).
- A mismatch now prints the case and both byte strings to stderr before the
  assertion, and stdout is line-buffered so progress survives an abort.

Retained coverage: pure public extraction (arena byte-equal, no gates),
guard at level 1/0, single public instance, redundant records never deleted,
duplicate copies each deleted once with no second deletion on re-apply,
both polarities, value -1/0/+1, position-cap boundary (5 deleted, 6 kept),
DRAT and native LRAT bytes, retained-index build flag. Nothing in the
deletion contract was weakened.

## Validation

| suite | result | log |
|---|---|---|
| Release (`pin-congruence-index-repair-5623-release`) | 88/88 passed | `logs/ctest-release.log` |
| Release BC (`pin-congruence-index-repair-5623-bc`) | 88/88 passed | `logs/ctest-bc.log` |
| Trace (`pin-congruence-index-repair-5623-trace`, EVENT_TRACE=ON) | 87/87 passed | `logs/ctest-trace.log` |

Binary hashes (`logs/binary-hashes.txt`):

| binary | sha256 |
|---|---|
| repair release `tactix` | `351bef360ee6427cf40ea3450f3777a615779c3377cb26721a4435ad2b79073e` |
| repair bc `tactix` | `351bef360ee6427cf40ea3450f3777a615779c3377cb26721a4435ad2b79073e` (matches release) |
| original 5619 release `tactix` | `351bef360ee6427cf40ea3450f3777a615779c3377cb26721a4435ad2b79073e` (identical: solver unchanged) |
| repair trace `tactix` | `794f28b8a6a4730cdfd9d02ecae76bbe8fc3b992f327eb59bf7be155765bf130` |
| repair release/bc `test_congruence_index_cleanup` | `818e0fb920dd674a1e42d6310ed940dd902e8c6265d5cdc6b3fcfacd8b23a899` (both) |

Source hashes (`logs/source-hashes.txt`):

| file | sha256 |
|---|---|
| 5623 `src/rules/congruence/congruence.cpp` | `01004745a067bc0f4eb88bd0b2455324c1d2039870dc65e16b56957a17170398` (byte-identical to 5619) |
| 5619 `tests/test_congruence_index_cleanup.cpp` | `1cf1f73bc3dc193e5cc517363b1c47c068a379e7cd7b218428b7ffbb453120eb` |
| 5623 `tests/test_congruence_index_cleanup.cpp` | `e2004d074334e22fed8d590c741abdbfedcf2bf0affc4cd99b2df02c337746b3` |

`diff -rq` of `src/` between 5619 and 5623 is empty; `tests/` differs only in
the file above. Build configuration matches the original pins
(Release, GCC 13.1.0, trace = `-DTACTIX_ENABLE_EVENT_TRACE=ON`).

## Residual correctness risk

None from this defect: the emitted deletion names the same clause and the
checker accepts it. The residual observation is about fidelity, not
soundness: Tactix visits clause literals in post-rebuild order while Kissat
visits them in pre-resume order. The analysis above shows the visited
cleanup decision is invariant under that rebuild, so no runtime effect is
expected or claimed; the Rank33 decision divergence remains open and is not
addressed here.

The defect is local to the new test's expectation. It is not present in the
parent 5592 tree (the test does not exist there) and does not implicate the
running 5592 jobs or the 5610 fuzz campaign.

## Cleanup

- Removed the compiled diagnostic binary from this directory (source kept in
  `benchmarks/probes/scratch/5623-congruence-index-repair/`).
- Removed four stale `/tmp/tactix-congruence-index-*` files left by the
  aborted 5619 runs (each contained only `d 2 1 3 0`); the test removes its
  temp file itself on success.
- Original 5619 source, pins and logs are untouched. No commit, push, or
  edit to the parent, production, docs, reports or frozen artifacts.

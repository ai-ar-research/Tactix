# AM320--AM325: reference proof pipeline, proof-production repairs, gates and fuzz

Checkpoint 20 September 2026 UTC. Base commit `fb1cc3bc`. This report freezes the
work that moved Tactix's acceptance checking onto the Satsuma+Kissat reference DSR
pipeline and repaired every proof-production issue that move exposed. Authorities
are the campaign directories named below; all solver work ran on
`cpu-dense-hwulab-q` with 12 CPUs, 32 GiB, a 1,200 s solve limit and a 57,600 s
checker window, certificates in anonymous node-local scratch.

## Pins

| Pin | Release SHA-256 | Debug SHA-256 | Build job |
|---|---|---|---|
| AM322 | `e56994e7...` | `70eaee42...` | `1708868` |
| AM324 | `34bcac0b...` | `80dfb4a5...` | `1709322` |
| AM325 | `34bcac0b...` (byte-identical to AM324) | `32f0c5d1...` | `1709754` |

Frozen checkers throughout: dsr-trim `3b5c7737`, Trestle `19bf79f4`, reference
dsr-compress `a44102ce`. The working tree's `tools/checkers/bin` holds a local
dsr-trim/dsr-compress build that is not an authority; AM325's verification and every
campaign use the frozen copies.

## Changes

1. **Checking pipeline.** `dsr-trim <cnf> <dsr> <lsr>` (backward), `dsr-compress -d`,
   Trestle on the text LSR; one definition in `tools/bench/proof_oracle.py`. The
   former forward `-f -c --delete-implied-units` pipeline was 50--160x slower on gate
   certificates. Because backward elaboration never examines lemmas outside the
   refutation's core, the fuzz harness and both proof suites additionally run
   forward elaboration (`every_lemma_dsr_stages`).
2. **No witnessed line may propagate when read.** Backward dsr-trim derives the
   empty clause at the line whose propagation closes the refutation, stamps the
   clauses it used with that line, and then skips them as RAT candidates
   (`can_skip_clause`, `lui <= current_line`): a valid witnessed unit elaborates to
   an LSR line without hint groups, which Trestle rejects, and an invalid one is
   reported VERIFIED by dsr-trim alone. A 13-clause reproduction and both
   directions are pinned in `tests/micro_proofs.sh`. X7 clauses that are unit at the
   root take the serializer's general extension encoding and enter by RUP. This
   replaced a per-unit consequence probe that rebuilt a whole-formula snapshot per
   application (`oski15a01b01s`: SR 449 ms -> 1,129,025 ms, six Rank timeouts; now
   443 ms).
3. **Sweep.** Lifted prefix lemmas are written conditioned on their target; the
   proof-only shadow propagator is gone; runs with and without a proof are
   identical by construction. Two avoidable declines of proved consequences are
   closed: the unbacked 4,096-step capture bound (every `DivS_862_11` refusal was a
   4,392--6,256 step derivation) and the demand for original proof ids outside LRAT.
4. **BVE.** Candidate contract asserted at entry; a canonical round ends at the
   first candidate that changes state; "changed state" is read from
   `kernel_change_mark`, not from per-site flags.
5. **Debug invariant.** "The DSR proof holds no root unit the kernel lacks" is a
   statement about a consistent kernel; a Backbone replay that closes the refutation
   legitimately leaves retracted literals unassigned (`test_backbone` regression).

## Gates (AM324 Release pin; AM322 is the same search except the Sweep change)

| Gate | Records | Valid | Solve sum | AM322 on the same records | Conflict-count changes |
|---|---|---|---|---|---|
| B28+C5 | 33/33 | 33 | 869.0 s (bar 952.263 s) | 877.7 s | `DivS_862_11` 178,468 -> 155,740 |
| Rank 1--50 | 49/50 (task 021 `oisc` in the checker) | 49 | 7,431.8 s (bar 8,115.735 s for 50) | 7,514.3 s | `b19_1` 6,753,950 -> 6,775,981 |
| WV100 | 96/100 (tasks 079, 082, 085, 089 in the checker) | 96 | 2,114.3 s | 2,112.4 s | none |

Every instance solves within 1,200 s and no solve is more than 25% slower than its
AM322 run. AM322 equals AM311/AM320 conflict counts on every instance.

## Large certificates

At freeze time five certificates of the AM324 gates were still in the checker (all five instances
solved, UNSAT, in 11--237 s): `oisc-subrv-sll-nested-8` (710 MiB) and `tseitin_grid_n400_m400` (2.44 GiB) in
`dsr-trim` after more than 4 h; `tseitin_grid_n250_m250` (832 MiB, `dsr-trim` about 52 min), `tseitin_d3_n100000`
(1.62 GiB, `dsr-trim` about 4 h) and `ncc_none_21015_5_3_3_0_0_11` (378 MiB, `dsr-trim` about 2.4 h) in Trestle.
`linked_list_swap_contents_safety_unwind73` (1.56 GiB) verified: `dsr-trim` 4,957 s, Trestle 8,407 s. Their final
records land in the campaign directories; a checker timeout there is a not-valid record. The superseded AM322 checks
of the same five instances were cancelled.

Reference context (`benchmarks/baselines/sc24-26-32g-5000s`, 45,000 s checker
limit): satsuma-kissat's own DSR proofs of `oisc-subrv-sll-nested-8` (495 MiB) and
`ncc_none_21015_5_3_3_0_0_11` (281 MiB) did not verify in 45,000 s; no clausal
reference solver solves the three Tseitin instances in 5,000 s; on
`linked_list_swap_contents_safety_unwind73` satsuma-kissat verifies in 17,198 s and
Tactix in 13,327 s with an equally sized proof. AM311 recorded all six of these as
not valid at its 16,000 s limit.

## Fuzz

`20260920-am324-proof-production-claude-fuzz` (Release legs) and
`20260920-am325-invariant-scope-claude-fuzz` (Debug legs), frozen checkers,
reference pipeline plus the every-lemma audit.

| Legs | Pin | Cases | Failures |
|---|---|---|---|
| Release nightly, default and random policy, 4 x 50k each | AM324 | 400,000 | 0 |
| Release focused default / random (replay-checked) | AM324 | 42,000 / 28,000 | 0 |
| AVX2 lookup, Release / Debug | AM324 / AM325 | 9,000 / 9,000 | 0 |
| Debug default / random | AM325 | 50,000 / 50,000 (49,095 / 44,464 verdicts; 1.8% / 11.1% non-terminating at 10 s) | 0 |
| Debug focused default / random (replay-checked) | AM325 | 21,000 / 14,000 | 0 |
| failure-seeds (109 preserved cases, both pins) and controls | AM324, AM325 | 109 + controls | 0, controls rejected |

Every leg met its minimum-verdict and non-termination thresholds and re-verified all 130 pinned files after the
run.

The one failure of the campaign, five Debug aborts in `dbg-random-focused` on
AM324, was the over-strict invariant of change 5; the solver and its proofs were
correct, and the rerun on AM325 passes the same seeds.

## Open

Tseitin/parity certificates are 8--33x larger than LymphoSAT's pseudo-Boolean ones.
A witnessed clause that is already false at the root cannot avoid closing the
refutation at its own line; no occurrence is known.

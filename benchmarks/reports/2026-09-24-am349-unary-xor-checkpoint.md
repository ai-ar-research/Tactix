# AM349: general unary-XOR and LRAT replay repair — unaccepted checkpoint

## Scope and outcome

The user requires general repairs, all baseline proof-rule coverage (competition
winners, CaDiCaL and Kissat), verified N1--N4, and codebase readiness for online
learning. This checkpoint repairs shared Congruence proof construction, not a
benchmark-specific path. It is **not combined correctness/performance acceptance**.

The user additionally authorizes full Tier D only after those obligations are
complete: 5,000-second solver limit, matching the first winner-run resources.
The retained baseline `benchmarks/baselines/sc24-26-32g-5000s/{manifest.json,array.sbatch}`
records 4 allocated CPUs, 32 GiB, a 45,000-second checker allowance and partition
`cpu-dense-hwulab-q`. Its historical throttle is not carried forward: current
instructions require full-width arrays. An asynchronous question asks whether
the user's “hwulab partition” means that exact partition. No Tier-D run submitted.

## General mechanisms

1. A root-fixed XOR input justifies removing that input, not an independent
   cancelled representative pair. All four canonical/semantic unary-equivalence
   and contradiction branches now retain the existing cancellation ladder when
   the cancellation set is nonempty. No rule, budget, dose or legal action is
   removed. No filename, seed, formula census or problem-family dispatch exists.
2. Shared `congruence_rup_chain` installs the complete negated conclusion before
   propagating. Previously propagation between assumptions could assign a later
   assumption by an implication; its reason then appeared in LRAT's hints even
   though that clause was already satisfied under the checker's complete initial
   cube. The corrected reason walk sees all assumptions as decisions and emits
   only the subsequent propagation refutation. Root closure still precedes the
   scope, and existing probing rollback restores the trail/watch pose.

Reference inspection: Kissat SC2024 `add_xor_shrinking_proof_chain` certifies the
smaller XOR before using it; CaDiCaL's homonymous routine supplies explicit
parent clauses for LRAT. Tactix retains immutable source gates, so its shared
ladder discharges the same cancellation obligation without rewriting gate data.
This is a mechanism comparison, not evidence of performance parity.

## Immutable provenance

Authority: `benchmarks/runs/20260924-am349-unary-xor/manifest.json`.

- Base: `b7b758db2c9eada1dc0d069e4b2d3edb7dfb49e6`.
- Patch: `8d9be604c3229e83199b6ab0837553281941d8777e5dda185ad6c5589764d0ba`.
- Archive: `4bfd84277781f5e1051a76c4f6aa4cfada5d366d9e66f5abaeba042212a36924`.
- Debug: `33ac93740e642240eee59f4585ad90ec0e3126e01a05c337a281ecbc0e238dc6`.
- Release: `e9389c965b4eb108353356005f51fc0fc01c99ad2f978fa78c7ca12981eb738c`.
- Seven independent checkers are hash-matched to AM348; all source hashes and
  explicit read-only build paths are in the manifest. Later status/report edits
  do not modify this authority.

## Evidence

Main-owned diagnostics are in `benchmarks/runs/20260924-am349-congruence-diagnostic`.
The initial `candidate-v1` differs from the final solver source only in comments;
the final fixed test also passes on both Debug and Release.

| Evidence | Observed result |
|---|---|
| AM348 old default CLI, SLURM 1717165 | Both owner F1-CLI inputs return UNSAT with rejected native DSR: idx9195 line12, idx9406 line9 |
| Old C API driver, 1717164 | F1 rejects DSR/DRAT with early or late proof attachment; F2 rejects LRAT only with late attachment |
| Repaired Debug exact cases, 1717167 | F1 verifies DSR/DRAT/LRAT early and late; both F1-CLI cases verify all three formats |
| New fixed regression | 192 cases per build: signs, root values, equivalence/literal-clash/representative-clash, canonical/semantic, proofless/DSR/DRAT/LRAT; exhaustive six-variable semantics and every addition/deletion/hint checked |
| Pinned independent fixed-certificate checks, 1717167 | 96/96 generated contradiction certificates verify; DSR backward plus every-lemma Trestle, native LRAT CakeLPR |
| New test linked against AM348 objects | Aborts on the already-satisfied LRAT hint, establishing a causal control (`old-causal.log`) |
| Focused Debug CTest | 20/20 pass, including Congruence, API, startup and output-failure checks |
| Full final Debug CTest | Live session32404/PID1323297 at checkpoint; not claimed all-pass |
| Prior AM348 full Debug CTest | Terminal 180/180 PASS, 3,181.33 seconds; not transferred to AM349 |

Job1717167 has 114 total certificate checks, **113 verified and one rejected**:
the remaining rejection is the deliberately rechecked legal late-attachment
F2 case. This is an open defect, not an expected pass or an acceptance exception.
New corpus entries040--042 preserve the exact bounded handoff bytes; 040 needs
the public canonical API action to reach its original defect, whereas041/042
reach it under default CLI. No original artifacts were overwritten.

## Independent execution and remaining obligations

Opus owner PID1325714/session95543 owns the immutable AM349 campaign. Its request
requires exact handoffs, focused adversarial properties, Debug/Release/ASan,
negative controls, native/checker-population validation, proofless/replay
agreement and the standard Release200k+200k / Debug50k+50k broad matrix. Only
OWNER_STATUS, terminal REPORT and explicit bounded handoffs are read by main.
Pending fuzz never authorizes promotion. AM348 owner recovery PID1320361/session87977
resumes the same campaign; completed passing unchanged legs are preserved.

N1-focused static checks and API fixtures passed in the preceding audit, but do
not prove universal action reachability. N2 is **not verified**: late attachment
can lose normalization proof history; F3 clause-population and F4 Debug target-phase
findings remain. AM347's descendant-pipe deadline finding also remains open.
No N3/N4 performance claim is made and no benchmark gate is waived.

The earlier rule comparison covered active winner pipelines, not the expanded
all-baseline scope. In particular, CaDiCaL implements optional `Condition`
(globally blocked clause elimination, default disabled); Tactix's inventory
explicitly declines it because cube reconstruction is missing. That disposition
must be revisited under the user's broader goal, rather than counted as full
coverage. Generic definitional extension and any other non-default baseline
capabilities require an explicit source-backed disposition and N1/N2 design.

Next: repair late proof attachment generally while preserving the public API,
consume bounded owner findings, repair F4 and deadline handling, complete the
broader rule/N1--N4 audit and cleanup, then run the authorized final comparison.
Active/preserved campaigns and pins were inspected; no benchmark evidence or
user data was deleted. The bounded CLI reproduction script removed only its
own ephemeral checker outputs; original handoffs and result log remain.

# AM330--AM331: the owner's large-domain symmetry program, and what failed alone

Checkpoint 21 September 2026 UTC. Base commit `7304ebbf`. Second Phase 3 (WV2-89)
increment. All solver work ran on `cpu-dense-hwulab-q` with 12 CPUs, 32 GiB, a 1,200 s
solve limit and a 57,600 s checker window; frozen checkers dsr-trim `3b5c7737`, Trestle
`19bf79f4`, dsr-compress `a44102ce`. Exploratory screens are solve-only and are named as
such in their run directories; only the four gate campaigns are gates.

## Pins

| Pin | Release SHA-256 | Debug SHA-256 | Build job | Status |
|---|---|---|---|---|
| AM330 | `a08c9a2f...` | `f00f8e5e...` | `1712865` | gated, **rejected** (regressions) |
| AM331 | `c024f183...` (two independent builds byte-identical) | `6ca994ad...` | `1713577` | candidate |

## Method note

Every piece below was inert or harmful when tried alone and only works combined with the
others. Earlier in the day the first of them had been parked as "no gain"; the user's
direction was to look for the coupled change instead of shelving a change that fails.

## Changes (AM331 relative to `7304ebbf`)

Above 50,000 variables the owner (Satsuma) replaces its Schreier chain by generator scans
but otherwise repeats its whole pass. Tactix's program there was the affine-model stage
plus one repeated graph step.

1. **Binary fixing is unconditional.** Tactix ran generator binary fixing only after
   negation fixing had produced a unit; the owner runs it always (`satsuma.h`). On
   `hcp_CP18_18` the owner fixes 0 literals by negation and 1,729 by binaries.
2. **A work-capped generator prefix is kept** in generator-fixing mode: each unit comes
   from one exactly validated generator. The discard rule remains for chain planning.
3. **The pass is repeated**: pure closure, generator-fixing graph step, compaction.
4. **Owner iteration bound.** New `RepeatMode::UntilSizeScaledFixpoint`: one pass plus
   `clamp(ceil((2^23 / (literals + variables))^1.5), 1, limit)` repetitions, read from the
   live formula each round (`iteration_max`). Public
   (`TACTIX_ONLINE_REPEAT_SIZE_SCALED_FIXPOINT`), serialized (`size-scaled-fixpoint`),
   drawn for random policies from a derived stream. Every structured program and the
   reference profile use it with limit 128.
5. **Pure closure from one occurrence image.** New `BveCandidateScope::PureClosure`
   selects the whole pure-literal closure layer by layer, each layer ordered as a
   `PureOnly` pass over the reduced formula would order it; a unit test checks equal
   applications, retired variables and reconstruction stack against repeated `PureOnly`.
   The former per-layer pass rebuilt the occurrence image and stable-sorted every variable
   each time (profile on `pj2009_k80`, 3.1M variables: 28% image build, 15% dense rebuild,
   ~30% merge sort; 72 s of structured startup, now 0.5 s).
6. **Compound-progress admission of a generator pass.** Under the existing
   `RequireCompoundProgress` setting a pass yielding a single unit is withheld before any
   mutation, as a one-row chain batch already is.
7. Typed, default-off `first_literal_fix_anchor` (`sr30`, public mode): the owner's
   `orbit_overlaps_clause` anchors a fixable clause on its first stored literal. With the
   owner's cut point it solves `ramsey_4_4_19` (timeout -> 290 s; Kissat solves original +
   those rows in 463 s and times out on Tactix's default rows) and `bphp_p23_h22`
   (489 s -> 1.8 s), but flips `ntil-90d-37/40`; DefaultPolicy keeps both off by user
   decision (`benchmarks/runs/20260921-wv2-anchor-cut-screen`, `...-wv2-owner-anchor`).
   The negation-first scan keeps the owner's restart at every level and is tick-charged.

Accepted deviations from the owner in the large-domain scope (parity ledger): at-most-one
completion and row/product refinement are not run there, and single-unit generator passes
are withheld. Both were measured on Kissat-owned rows where the owner's preprocessing is
pure perturbation.

## Evidence that the pieces are coupled

`benchmarks/runs/20260921-wv2-large-domain-dig{,2,3,4}` on `hcp_CP18_18`: base, keep-prefix
alone, ungated alone and full program alone all apply 0 rows and time out; keep-prefix +
ungated applies 5,566 units and times out; all together apply 8,639 units (owner 8,626) and
solve (SAT).

`benchmarks/runs/20260921-wv2-6g-ablation`: on `6g_6color` Satsuma's 3 units alone time out,
its 193,315 at-most-one binaries alone time out, both give 232 conflicts (17 s).
`...-wv2-6g-units-landscape` then shows the row is luck-dominated: equally valid
colour-symmetry unit sets range from 17 s to a timeout with no monotone pattern.

## AM330: gated and rejected

AM330 ran the owner's full sequence (with at-most-one completion, rows and product) and no
pass admission. Gates `benchmarks/runs/20260921-am330-owner-fix-anchor-*`: WV100 lost
`snw_16_9_CCSpre` and `Break_20_72`, Rank 1--50 +9.9%, WV2-89 gained `hcp` and lost `6g`.
Every moved row has more than 50,000 variables, where the committed binary applied no SR
row. Row-level causes (`...-wv2-large-domain-admission{,2}`): at-most-one completion
perturbs Kissat-owned SAT rows (`cabp-V-nos6` 222 s -> 1,129 s; `Break_20_72` lost on 31
binaries) and `SC25_Timetable_C_496` solves only without it; a single generator unit flips
`snw_16_9_CCSpre`, `6g_6color` and `mp1-Nb7T42`; the pure step cost 56--110 s.

## AM331 evidence

Build/verify job `1713577`: corpus 43/43 in Release and Debug under recorded arguments and
random policy; 324 AM327 failure cases replayed, 0 failing; CTest 169/169 Release and
168/168 Debug (job `1713574`, frozen-checker overlay). Fuzz matrix
`benchmarks/runs/20260921-am331-large-domain-lean-claude-fuzz` (jobs `1713597`--`1713614`):
failure-seeds PASS, controls OK, 0 failures in every finished leg (eight nightly legs,
`dbg-default`, four focused legs 42,000 / 27,500 / 21,000 / 14,000 cases, both AVX2 legs);
`dbg-random` 50,000 cases, 0 failures; all 18 jobs exited 0. Proof vs no-proof search identical on 97 of 97 comparable runs; `xor_op_n40_d3` UNSAT in 133 s, proof verified with the every-lemma audit in 1,519 s; `hcp_CP18_18` model checked. The Debug
cross-check of certified X7 instances is active in every Debug leg, and random policies draw
the new repeat mode, pure scope and anchor.

Gates `benchmarks/runs/20260921-am331-large-domain-lean-{rank1-50,b28c5,wv100,wv2-89}` (jobs
`1713654`--`1713657`) against the committed AM329 gates:

| Gate | Solved | Time on rows solved by both | Lost | Notes |
|---|---|---|---|---|
| Rank 1--50 | 50/50 (49 within 600 s) | 7,241.2 s vs 7,659.6 s (-5.5%) | none | `baseballcover` 218 -> 25 s, `linked_list` 153 -> 61 s, `pj2008` 252 -> 164 s, `pj2009` 95 -> 128 s |
| B28+C5 | 33/33, all valid | 909.0 s vs 873.9 s (+4.0%) | none | `EDP3-11000` 13 -> 50 s |
| WV100 | 100/100 (99 within 600 s) | 2,370.8 s vs 2,321.5 s (+2.1%) | none | `Break_20_72` 91 -> 160 s |
| WV2-89 | **82** (was 79), **76 within 600 s** (was 74) | 7,355.8 s vs 7,152.7 s (+2.8%) | none | gained `mp1-Nb7T42` 179 s, `SC25_Timetable_C_496` 467 s, `hcp_CP18_18` 804 s |

No solved row is invalid. Large certificates still in the checker do not gate the solve
comparison; the five that never verify inside the window are unchanged from AM329
(`NOTE-cancelled-checks.txt` in the AM329 gate directories).

## Open

`FmlaEquivChain_4_8_8` is slower (751 s -> 1,112 s; 12 generator units with 3.5M witness
literals, 43 s of structured startup; LymphoSAT-owned, above 600 s before and after).
`hcp_CP18_18` is solved but luck-sensitive (107--804 s across equivalent configurations).
`ramsey_4_4_19` needs the declined anchor + cut point. Untouched: LymphoSAT structural
families (`sgen1-sat`, `sted5`, `1-ET-512`, `FmlaEquivChain`, `noL-11-4`, `puzzle32`) and
plain CDCL parity rows (`snw_16_9`, `Circuit_multiplier24`, `ntil-90d-39`, `lockchart...L19`).
Large-domain at-most-one completion remains an owner mechanism Tactix does not run; its
value is a "when to break symmetry" decision (PROGRESS, learned-scheduling candidate).

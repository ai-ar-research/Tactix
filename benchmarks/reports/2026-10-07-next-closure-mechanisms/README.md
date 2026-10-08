# Fixed-44: mechanisms for the rows outside the likely and blocked sets

This is a read-only planning audit of saved evidence. No solver, checker,
build, or Slurm job was run. It covers the 28 rows of the frozen
[`selection.tsv`](../2026-10-07-fixed44-successor-campaign-preparation/selection.tsv)
that are neither in c1's likely set (147, 263, 401, 540, 630, 744, 899, 991)
nor in the blocked set (291, 541, 569, 627, 654, 1004, 1075, 1129).
c1's full44 job 1755927 is still running and supersedes every Tactix entry
below once it is terminal.

Sources:
- Owner times come from `benchmarks/baselines/sc24-26-32g-5000s/results.json`.
  That baseline allows a 45,000 s checker, so this report also asks whether
  each UNSAT owner's check fits the fixed-44 limit of 14,400 s.
- Tactix outcomes come from the d170 Tier-D records (`2fc7421f`,
  `benchmarks/runs/20260928-am546-no-mab-tactix-hybrid-tierd-cpuq`) and from
  the 0e6b95c91 full44 records.
- Owner techniques come from the
  [attribution](../2026-10-03-winner-integration-attribution.md),
  [action map](../2026-10-04-target-to-boolean-action-map.md),
  [LymphoSAT overfit audit](../2026-10-07-lymphosat-overfit-audit.md),
  [Rank899 gate](../2026-10-07-rank899-certified-unit-gate/README.md), and
  [six-domain cost screen](../2026-10-07-six-domain-symmetry-cost/README.md).

## Rows

Owner times are solve seconds; for UNSAT rows the check time follows a `+`.
"V" means independently verified. "≤14.4k" means the check fits the fixed-44
contract.

| Rank | Instance | Owner(s), sc24-26 | Tactix best and failure | Owner technique (evidence) | Group |
|---:|---|---|---|---|---|
| 82 | cfi-rigid UNSAT | sc2024 658+6,872 V ≤14.4k; LymphoSAT 1,024+18,348 | UNSAT in 1,302 s; 2.06 GB DRAT route timed out at 14,400 s | Plain CDCL. Kissat 4.0.4's 2.06 GB and AE's 2.72 GB proofs did not verify in 40,000 s | M6 |
| 97 | Break_triple SAT | AE 999 | Timeout (d170 and 0e6b) | MAB. The FastEliminate port caused the same family's Break_20_72 loss | out |
| 190 | ITC2021_Late SAT | 4.0.4 1,137; sc2024 307; AE 476; LymphoSAT fallback 1,962 | Timeout; 37.1M conflicts; startup fastel eliminated 57,024 | Plain Kissat: 4 of 5 Kissat-family arms solve it, Satsuma does not | M2 |
| 226 | REGRandom UNSAT | No owner within 14,400 s (4.0.4 26.8k, AE 31.3k) | UNSAT in 1,326 s; 20.3 GB; checker timeout | CDCL | out |
| 323 | bvsub UNSAT | sc2024 1,713+4,536 V; AE 3,373+5,378 V | Timeout; 83.9M conflicts | Kissat 4.0.2-family trajectory. Kissat 4.0.4 also times out | out |
| 343 | 1x1x14 pair SAT | 4.0.4 3,119; sc2024 2,622; LymphoSAT 3,595 | Timeout | Plain Kissat, 1.6x margin | M2 (weak) |
| 367 | cabp-X-can SAT | AE 1,856 | Timeout; 9.2M conflicts | MAB. No structural signal | out |
| 410 | pcmax m24 SAT | AE 1,600 | Timeout | MAB | out |
| 453 | lockchart g2 SAT | AE 1,705; Satsuma 3,511 | Timeout. Applied 254,451 SR steps (254,448 AMO binaries); fastel suppressed by root growth | Satsuma small-domain symmetry | M1 |
| 479 | par32-4-c SAT | sc2024 3,410; LymphoSAT fallback 3,831 | Timeout; 92.4M conflicts | CDCL. Parity family by name, but no saved XOR recognition | M3 (weak) |
| 489 | oisc-11 UNSAT | sc2024 1,105+8,907 V; AE 896+8,181 V | UNSAT in 836 s; 2.52 GB DRAT route timed out at 14,400 s | Plain CDCL. Kissat 4.0.4's 2.70 GB proof did not verify in 40,000 s | M6 |
| 497 | snw_16_9 SAT | AE 1,163; sc2024 2,056; LymphoSAT 260 | Timeout | LymphoSAT's fixed `snw16` phase lists fail the generality gate. Otherwise Kissat 4.0.2-family CDCL | out |
| 499 | 55-140792 SAT | AE 916 | Timeout; 195M conflicts | MAB. The station-repacking matcher accepted, but the handler is unknown | out |
| 511 | sokoban UNSAT | LymphoSAT PBP 1,200+13,619 is the only check within 14,400 s; DRAT owners need 17.5k to 31k | UNSAT in 737 s; 5.65 GB; checker timeout | CDCL | out |
| 557 | pcmax m43 SAT | AE 2,857 | Timeout | MAB. The affine census found one binary XOR and no events | out |
| 754 | Timetable E48 SAT | Satsuma 424; AE 553; 4.0.4 1,508 | Timeout; 38.2M conflicts. The six-row screen applied 5 SR steps | Satsuma large-domain symmetry with Kissat 4.0.4 | M1+M2 |
| 762 | xorshift SAT | sc2024 4,564; LymphoSAT fallback 4,432 | Timeout | CDCL near the 5,000 s limit | out |
| 781 | SDP SAT | 4.0.4 950; Satsuma 3,784 | Timeout; 23.8M conflicts | Plain Kissat 4.0.4, 5.3x margin | M2 |
| 887 | ITC2021_Early SAT | AE 449; 4.0.4 2,395; Satsuma 3,718 | Timeout; 0 SR applied | Plain Kissat | M2 |
| 896 | pcmax m19 UNSAT | Not verified in sc24-26. Satsuma 2,378 with an 8.06 GB DSR hit dsr-trim `xrealloc`; 4.0.4 3,682 and AE 3,821 hit DRAT MEMOUT. The frozen ledger has Satsuma checked at 2,708 | Timeout; 86.3M conflicts. **SR 8 applications, 0 applied, 0 validation ticks, 0 declines** | Satsuma symmetry. Closure also needs a fixed64-sized check | M1 |
| 941 | as-p75 SAT | LymphoSAT fallback 4,485 | Timeout | CDCL near the limit | out |
| 1009 | cabp-M UNSAT | AE 2,757+6,074 | Timeout | MAB | out |
| 1040 | 59-134241 SAT | AE 3,801 | Timeout; 177M conflicts | MAB. The station-repacking matcher accepted | out |
| 1085 | oisc-13 UNSAT | AE 2,079+12,004 V ≤14.4k; sc2024 1,771+17,276 | UNSAT in 1,447 s; 2.69 GB DRAT route timed out at 14,400 s | Plain CDCL. Kissat 4.0.4's 2.63 GB proof did not verify in 40,000 s | M6 |
| 1118 | TT7F SAT | AE 882 | Timeout | MAB | out |
| 1144 | Timetable E50 SAT | AE 1,279; sc2024 3,412; Satsuma 4,067; 4.0.4 4,264 | Timeout; 39.9M conflicts. 8 SR applied (27,016 witness literals); fastel suppressed | Kissat CDCL. Satsuma is slow here | M1/M2 (weak) |
| 1161 | lockchart g1 SAT | AE 1,092; sc2024 4,012; LymphoSAT 307 (output protocol rejected; frozen ledger 459 checked) | Timeout; 3.1M conflicts | LymphoSAT handler unknown. Guarded-cover RUP tuples have been consumed, but nothing solved | M4 |
| 1163 | ntil-90d-41 SAT | sc2024 2,793; AE 4,168 | Timeout | Kissat CDCL. Satsuma cuts hurt the ntil family | out |

Kissat 4.0.4 and AE-Kissat-MAB 2025 differ from kissat-sc2024 in their
defaults:
- Kissat 4.0.4 (`kissat/NEWS.md`) disables startup `fastel`; it is
  `OPTION (fastel, 0, …)` in `kissat/src/options.h:62`.
- kissat-sc2024 and AE (Kissat 4.0.2) enable it.
- Tactix enables SC2024 startup FastEliminate by default
  (`initial_fast_elimination = true`, `src/policies/default_strategy_config.hpp:383`).

So on the rows owned by Kissat 4.0.4 or Satsuma+4.0.4, Tactix's startup
departs from the owner's. Rank899's closure was the case where
structured-root growth suppressed fastel and so restored the owner boundary.

## Mechanisms, ranked

The score is rows × evidence ÷ risk. Evidence and risk are each rated 1 to 3.
"Rows" counts plausible closures among the 28.

| # | Mechanism | Rows | Evidence | Risk | Score | Generality gate |
|---:|---|---|---:|---:|---:|---|
| 1 | **M1: owner-boundary fidelity for Satsuma-solved rows.** Generalizes the Rank899 method: match the actual Satsuma residual (clause multiset, root set, activation queue), then port the owner mechanism at the first divergence | 754, 896, 453 (stretch: 1144); ≈2 | 3 | 1.5 | **4.0** | Passes: a faithful Satsuma port with no dispatch. 899 closed causally once the residuals were equal (UNKNOWN at 300 s became SAT in 55.9 s) |
| 2 | **M2: Kissat 4.0.4 search parity** on rows the reference itself solves | 190, 781, 754, 887 (stretch: 343, 1144); ≈2 | 2 | 2.5 | 1.6 | Passes only as fidelity repairs. It fails if it turns into choosing fastel or structured-root variants per row |
| 3 | **M6: checkability of witness-free DRAT proofs** | 82, 489, 1085; ≈1 | 1.5 | 3 | 0.5 | Passes. Owners fit within 14,400 s, but drat-trim time is not monotone in proof size, and every certificate is deleted |
| 4 | M3: persistent GF(2) (CryptoMiniSat reference) | 479 (and 762); ≈0.3 | 1 | 3 | 0.1 | Passes only with clause-backed XOR recognition. None is saved for these rows |
| 5 | M4: guarded-cover / exact-domain consequences | 1161; ≈0.3 | 1 | 3 | 0.1 | Conditional: the schema is general, but there is one target and no solve |
| — | Independent-set cover (499, 1040); affine composition (410, 557) | 0 | 0.5 | 3 | 0 | No yield on the saved evidence: AE owns the rows and the affine census is negative |
| — | MAB/CHB (97, 367, 410, 499, 557, 1009, 1040, 1118); LymphoSAT `snw` phase lists (497); a hand gate on symmetry for ntil (1163); selecting kissat-sc2024 variants (323, 479, 762, 1163); a new PB route (511, 226) | — | — | — | — | **Rejected**: retired by the user, fails the generality gate, is speculative variant hunting, or is a PB path barred by plan §9.5 |

These rows are realistically out of reach:
- The eight AE-only rows.
- 226 and 511: no DRAT owner fits 14,400 s.
- 323, 762, 941, 1163: Kissat 4.0.2-family or near-limit CDCL trajectories that Kissat 4.0.4 also misses.
- 497.
- 479, unless a clause-backed XOR image appears.

The realistic yield from these 28 rows is about 2 to 4, mostly through M1
and M2. Rows 82, 489 and 1085 belong in the route-cost owner's queue; c1's
full44 records their check times under the 14,400 s contract.

## Recommended next increment

**Satsuma-boundary parity on Rank896, then 754, 453 and 1144**, using the
Rank899 procedure unchanged.

1. Read c1's full44 rows first. d576 changed SR unit admission, so 453, 754,
   896 and 1144 may already have moved.
2. For each row still open, run the frozen boundary extractor on c1's pin:
   `benchmarks/runs/20261007-rank899-actual-handoff-boundary-r3/run.py`
   (`275565df…`) and `compare.py` (`311d7a8e…`). Compare the actual
   `satsuma fix --full-skip-limit 100000000 --add-reduced-as-unit --bsr`
   residual against Tactix's `preprocess_begin` and `preprocess_end`
   snapshots, on canonical clauses, roots and queue.
3. Start with Rank896, for three reasons:
   - It has 10,198 variables, so it falls in the owner's small-domain
     iterate regime (`satsuma.h:809-810`).
   - Tactix's eight SR applications returned before validation. With 0
     validation ticks, they took the NoChange path at
     `substitution_redundancy.cpp:633`: the candidate was already present or
     already true.
   - Satsuma solves the row.
4. Port the owner mechanism at the first divergence. The candidates already
   named in [the review](../../runs/20261007-restart-symmetry-claude-review/REVIEW.md)
   and in the parity ledger §13.8 are:
   - SP-SR-REPEAT-OUTPUT: repeat only on SBP output;
   - SP-SR-REPEAT-LIMIT: stop on `reached_limits`;
   - the large-domain `negation_fixing`/`binary_fixing` path;
   - the accepted large-domain AMO/row/product deviation.
5. Gate the repair with a same-CPU 300–600 s causal pair, as for 899. Then
   run the 57 Satsuma-owned rows, B28+C5, Rank1–50, WV100, WV2–89, effective
   fuzz and full Debug.

A cheap discriminator for M2 can run in parallel: a same-CPU pair of
Kissat 4.0.4 and Tactix on 190 and 781, recording conflicts per second and
conflicts to solution, plus the shuffle probe. It is diagnostic only and
changes no default.

Owning source files:
- **Reference (Satsuma 1.4):**
  - `winners/satsuma-v1.4/src/markusa4-satsuma-d47721e/src/satsuma.h`: special-action detection 160-176; negation/Schreier fixing and `binary_fixing`/`fix_and_cut` 208-229; `reached_limits` 49/175/194/267; the iterate gate 800-810.
  - `satsuma.cpp:102-111`: the `fix` profile.
  - `symmetries.h:3060`: `fix_and_cut`.
- **Reference (pipeline and Kissat):**
  - `winners/satsuma-iter-kissat-sc2026/anders/satsuma-iter-kissat/src/run_base.sh`.
  - Kissat 4.0.4 `kissat/src/options.h:62`.
- **Tactix:**
  - `src/policies/default_policy.cpp`: `default_should_run_fast_elimination_after_structured` at 640, the call site at 5032, and the large-domain window at 6670.
  - `src/policies/structured_root_strategy.cpp`.
  - `src/strategy/online_inprocess.cpp`: the repeat and progress gate.
  - `src/policies/default_strategy_config.hpp`: 383 and 433.
  - `src/rules/substitution_redundancy/{substitution_redundancy.cpp,symmetry.cpp,orbitopal*.cpp}`.
  - `src/policies/heuristics.cpp`: residual activation order.
  - `docs/scheduling-parity-ledger.md` §13.8.

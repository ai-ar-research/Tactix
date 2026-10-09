# Reports to Andrew: curated index

This folder holds the Andrew-facing LaTeX/PDF reports for Tactix, the
proof-rule-driven SAT solver developed with AI agents over September and October
2026. On 2026-10-09 it was curated from 340 report directories down to the ones
listed here. The ones removed were routine hourly checkpoints, session handoffs,
near-duplicates and progress notes that later reports supersede. Git history
keeps the removed tracked reports.

Each entry gives the directory, the report title or topic, and why the report
matters. Reports marked *(linked)* are cited from PROGRESS, the plan, docs,
skills, agent memory or a frozen `benchmarks/reports/` record, and are kept for
that reason even when a later report supersedes them.

## Phase 1: N1 refactor and Satsuma symmetry on WV100 (12–18 Sep)

| Report | Topic | Why it matters |
|---|---|---|
| `20260913-0912-online-refactor-n1-closure` | Online-strategy and N1 refactor closure | Milestone: every proof rule became selectable through one typed executor (N1). |
| `20260913-0933-tierd-all-runtime-ranking` | Tier D all-configuration runtime ranking (CSV) | *(linked)* Source ranking that defines the WV100 and WV2-89 target sets. |
| `20260914-1600-sr-rules-explained` | What the substitution-redundancy mechanisms actually do | Explains that there is one SR rule (X7). ULC–AMO, orbitopal and graph symmetry are only ways of discovering X7 applications. |
| `20260915-0855-complete-sr-proof` | Complete SR proof encoding after a symmetry crash | Pitfall: repeated binary clauses became parallel edges in DejaVu's graph and caused a SIGFPE. |
| `20260915-0925-sr-scheduling-audit` | Measured SR scheduling mismatches | Lesson: a "ten-million-tick" budget was really 20 billion. Unbacked constants mislead. |
| `20260915-1845-root-stabilizer-sr-repair` | Symmetry tracing and a general SR soundness repair | Soundness repair found by tracing generator retention. |
| `20260915-2047-detector-order-checkpoint` | Checked residual comparison and detector ordering | *(linked)* Satsuma-preprocessed rank 91 verified; detector-order evidence. |
| `20260915-2112-structured-generator-handoff` | Structured-to-generic handoff | *(linked)* The native and Satsuma graphs match exactly. |
| `20260915-2146-native-row-handoff` | Native row-generator handoff closes WV100 rank 91 | *(linked)* Rank 91 proved UNSAT in 1.7 s natively. |
| `20260915-2325-structured-first` | Structured startup and the remaining Satsuma gap | *(linked)* Scheduling decision for structured-first startup. |
| `20260915-2344-product-discovery-next` | Scheduling controls point back to product discovery | *(linked)* Negative scheduling result; the remaining gap is in discovery. |
| `20260916-0247-production-composition-checkpoint` | Production composition and proof repair | Milestone: the production CLI returns UNSAT on all 57 Satsuma-owned rows. |
| `20260916-0455-dsr-copy-population` | DSR duplicate-copy repair | Pitfall found by fuzzing: retiring one physical copy of a duplicate clause dropped its proof deletion (82 dsr-trim rejections). |
| `20260916-1610-rank17-followups` | Rank 17 diagnosis | *(linked)* |
| `20260916-1718-am9-resume-startup-cost` | AM9 regression audit and startup-cost diagnosis | *(linked)* |
| `20260916-202935-am10-bve-cache` | AM10 scoped BVE occurrence cache | *(linked)* Recovered Rank 25 by removing per-call linear scratch (a recurring hazard). |
| `20260917-024626-preprocessing-progress-feedback` | Progress feedback inside preprocessing | *(linked)* |
| `20260917-043134-orbitopal-binary-stamps-and-bce-lifetime` | Orbitopal preprocessing cost and helper-clause lifetime | *(linked)* |
| `20260917-051240-within-pass-feasibility-and-amo-cleanup` | Feedback inside preprocessing passes | *(linked)* |
| `20260917-054043-touched-row-cells-and-early-strengthening` | Row extraction cost and early inference timing | *(linked)* Phase 1 gate status at the end of the preprocessing-cost push. |
| `20260917-1505-native-affine-model-constructor` | How the native affine model constructor works | Explains the first LymphoSAT-derived (GF(2) affine) integration. |
| `20260918-1530-solver-performance-table` | Retained performance on B28+C5, Rank 1–50 and WV100 | Consolidated benchmark table at the end of Phase 1. |
| `20260918-1953-lymphosat-satsuma-integration` | LymphoSAT and Satsuma integration overview | Design statement: owner techniques are ported as rules, not run as external stages. |

## Phase 2: WV2-89 target and the certificate pipeline (20–23 Sep)

| Report | Topic | Why it matters |
|---|---|---|
| `20260920-2300-reference-pipeline-proof-production` | Reference-pipeline proof production | Pitfall: forward dsr-trim elaboration was 50–160x slower than the owners' backward pipeline. |
| `20260921-1045-wv2-89-first-increment` | WV2-89 target and first increment | Defines the 89-instance goal (each solved within 600 s). |
| `20260922-1500-certificate-routing` | Certificate routing | Five certificates never verified within a 16 h window. This motivated per-technique certificate formats. |
| `20260923T003859Z-startup-certificates` | Startup refutation certificates: decision proposal | N2 design decision: complete startup refutations may emit PBP checked by VeriPB and CakePB. |
| `20260923T073500Z-proof-lifetime` | Certificate diagnostics; proof-lifetime repair | *(linked)* All four startup PBP certificates pass VeriPB and CakePB. |
| `20260923T074200Z-routed-gates` | AM337 full routed gates | *(linked)* First gates with both native DSR and DRAT/LRAT routes. |
| `20260923T080000Z-checker-spelling` | AM338 clause-preserving checker input | *(linked)* Pitfall: pinned checkers cannot read some valid DIMACS layouts. |
| `20260923T082000Z-parser-liveness` | AM339 parser binary-index lifetime | *(linked)* |
| `20260923T111231Z-opus-recovery` | Fuzz ownership resumed without Fable | Process incident: a model-account quota blocked the fuzz owner, so fuzzing switched to Opus. |
| `20260923T200000Z-drat-input` | AM340 repairing DRAT input verification | Pitfall: pinned drat-trim misreads CNF comments with binary proofs, which let false proofs through. |

## Phase 3: fuzz-driven hardening, the proof-route contract, and the MAB decision (24–28 Sep)

| Report | Topic | Why it matters |
|---|---|---|
| `20260924T012000Z-unwatch` | AM341 root-fixed watch assertion | *(linked)* |
| `20260924T013300Z-oracle-contracts` | AM342 strict checker contracts | *(linked)* Tooling-side checker pitfalls. |
| `20260924T014600Z-empty-lrat` | AM343 input-empty LRAT gap | *(linked)* Pitfall: drat-trim writes no LRAT when the input has an empty clause. |
| `20260924T020100Z-dsr-routing` | AM344 reliable DSR routing | *(linked)* |
| `20260924T021500Z-lrat-restatement` | AM345 LRAT input restatements | *(linked)* Pitfall: drat-trim deduplicates input literals, so CakeLPR rejects its LRAT. |
| `20260924T025100Z-xor-cancellation` | AM346 XOR matching certificate repair | *(linked)* Soundness bug found by fuzzing: an invalid Congruence XOR lemma. |
| `20260924T030700Z-checker-boundaries` | AM347 checker boundary repairs | *(linked)* Pitfall: Python poll timeouts overflow above about 2.1M seconds. |
| `20260924T032700Z-walk-score-bound` | AM348 Walk score storage bound | *(linked)* Memory runaway found by fuzzing. |
| `20260924T181338Z-am359-h3` | H3 DRAT proof-audit classification | Lesson: strict audits that ignore DRAT's implicit root units raise false alarms. |
| `20260924T215500Z-am369-condition-explicit` | Explicit general Condition rule (X5) | Closed a proof-rule gap against CaDiCaL (N1 coverage). |
| `20260924T234537Z-am378-ncc-terminal` | ncc proof checks through LSR and CakeLPR | A never-verifying certificate verified by an alternative route. |
| `20260925T002543Z-online-learning-readiness` | Readiness for online learning | Architecture verdict: ready for bounded order/dose experiments, not production learning. |
| `20260925T013858Z-am386-tier-routing` | Automatic-certificate routing in the tier runner | Pitfall: the runner chose the checker from the solver command, not from the emitted proof. |
| `20260925T034000Z-am389-checker-pin-split` | Proof-checker pin split | Pitfall: the workspace dsr-trim was an untracked build that differed from the frozen checker. |
| `20260925T144643Z-am411-or-randxor-paired` | PB versus DSR on a winner family | LymphoSAT's PBP proof is 157 KB; Tactix's DSR proof is 7.8 MB. Shows a proof-representation gap, not an N2 failure. |
| `20260925T154429Z-am414-api-boundary` | Safe input boundary | Wrong-SAT path through late clause addition, found by fuzzing, now fails closed. |
| `20260925T194436Z-am434-tierc-checker-timeout` | Tier C checker-timeout gate failure | Lesson: a checker timeout makes a gate unclean even when both arms are correct. |
| `20260925T200000Z-am435-bounded-dsr-routing` | Bounded-memory certificate routing | Pitfall: routers loaded whole proofs into memory, an OOM risk on Tier D. |
| `20260925T205000Z-am440-n4-ledger-audit` | N4 audit: implementation versus ledger | Lesson: the parity ledger lagged the shipped code (documentation drift). |
| `20260926T013000Z-am452-h2-transaction-checkpoint` | H2 proof transaction | Closes the long-running H2 native-DSR rejection. |
| `20260926T202947Z-am464-split-unit` | Canonical DSR root-unit transaction | *(linked)* General repair for the D1/H2 DSR failures. |
| `20260927T020003Z-am479-proof-spool` | Rank 12 proof-production recovery | Performance lesson: a bounded deletion spool halved proof-enabled time. |
| `20260927T223225Z-am512-goal-and-wv100-checkpoint` | Current checker route | N2 contract made operational: no-option dsr-trim, then CakeLPR (witness-free) or Trestle (witnessed). |
| `20260927T235314Z-am518-mab-purge` | Removal of the MAB integration | *(linked)* Retired mechanism and the reason. |
| `20260928T0120Z-am524-immediate-mab-gates` | Always-on MAB fails Phase 1 | Negative result: the faithful AE-Kissat-MAB controller regressed the fixed sets. |
| `20260928T1510Z-no-mab-milestone` | No-MAB milestone and learning readiness | *(linked)* User-accepted stabilization milestone. |
| `20260928T2103Z-no-mab-hybrid-milestone` | No-MAB milestone and remaining correctness work | Latest statement of that milestone and its disclosed exceptions. |

## Phase 4: Tier D comparison, controller foundations, shared policy (1–3 Oct)

| Report | Topic | Why it matters |
|---|---|---|
| `20261001T125514Z-tierd-five-arm-results` | Tactix and four baselines: Tier D results | *(linked)* Headline: on 1,166 instances Tactix verifies 986, against 941 for Satsuma+Kissat and 860 for Kissat-sc2024, with the lowest PAR-2. |
| `20261001T153649Z-controller-foundations-plan` | Controller foundations before learning | Design of the controller and observation interface with a random policy. |
| `20261001T234955Z-controller-instance-timings` | Controller per-instance timing | *(linked)* |
| `20261002T010526Z-controller-foundations-accepted` | Controller foundations accepted | *(linked)* Milestone: the random controller over 20 named configurations is accepted. |
| `20261002T052830Z-shared-startup-substitute` … `20261002T232951Z-shared-policy-contract-review` (18 reports) | Shared execution and feedback preparation series | *(linked)* Step-by-step evidence for unifying DefaultPolicy and controller execution. Each is cited by its own frozen benchmark record. |
| `20261003T005230Z-terminal-substitute-feedback-repair` | Terminal Substitute boundary violation | *(linked)* Defect found by controller fuzzing. |
| `20261003T030624Z-terminal-substitute-repair-applied` | Terminal Substitute repair applied | *(linked)* |
| `20261003T033045Z-terminal-substitute-revalidation-started` | Repaired-source acceptance runs | *(linked)* |
| `20261003T044611Z-terminal-substitute-regression-loss` | A lost WV100 solve | *(linked)* Regression after a correctness repair. |
| `20261003T050344Z-snw16-matched-retention-diagnosis` | Matched diagnosis of the lost solve | *(linked)* Lesson: a same-node matched diagnosis is needed before blaming a change. |
| `20261003T130219Z-shared-policy-terminal-closeout-review` | Shared-policy closeout review | *(linked)* |
| `20261003T131637Z-shared-policy-engineering-audit` | Independent engineering audit | *(linked)* |
| `20261003T163643Z-shared-policy-preparation-accepted` | Shared execution and feedback preparation accepted | *(linked)* Milestone: engineering preparation for online learning is accepted. |

The shared-policy series covers `shared-startup-substitute`, `shared-work-vivify`,
`shared-preparation-publication`, `native-policy-feedback`,
`native-inprocess-feedback`, `feedback-frontend-publication`,
`startup-support-feedback`, `shared-heuristic-lifecycle`,
`shared-parameter-phase-lifetimes`, `native-support-audit`,
`broad-default-correspondence`, `shared-policy-validation-started`,
`shared-policy-regression-feedback`, `shared-policy-debug-failures`,
`shared-policy-staged-repair-validated`, `shared-policy-repair-revalidation`,
`shared-policy-repaired-regressions` and `shared-policy-contract-review`.

## Phase 5: organic winner integration and theory-to-Boolean reasoning (3–6 Oct)

| Report | Topic | Why it matters |
|---|---|---|
| `20261003T164617Z-tierd-portfolio-misses` | Tier D: portfolio coverage missing from Tactix | *(linked)* Defines the fixed 44-instance portfolio gap. |
| `20261003T173008Z-organic-winner-integration-plan` | Organic winner-technique integration plan | The agreed plan for the rest of the project. |
| `20261003T195500Z-graph-effort-calibration` | Graph-constructor effort calibration | *(linked)* |
| `20261003T200600Z-graph-budget-review` | Source-derived graph budgets | *(linked)* Budget derived from the source, not a tuned constant. |
| `20261003T201500Z-ramsey-dsr-feasibility` | Native DSR feasibility for Ramsey | *(linked)* |
| `20261003T202500Z-ramsey-compact-dsr-checked` | Compact native DSR for the Ramsey target | *(linked)* |
| `20261003T204556Z-structural-boolean-inprocessing` | Theory-to-Boolean inprocessing direction | Key decision: translate structure into certified Boolean constraints for CDCL instead of running dedicated local search. |
| `20261003T222632Z-recognized-ramsey-certificates` | Checked Ramsey proofs from recovered structure | *(linked)* |
| `20261004T133025Z-restored-lymphosat-sat-constructors` | Restoring the LymphoSAT SAT-constructor surface | Reverses the retirement of the SAT constructors. |
| `20261004T151533Z-restored-lymphosat-sat-regression` | Restored constructors: regression result | Negative result: the restoration lost seven SAT solves and was not accepted. |
| `20261004T172259Z-certified-unsat-verticals` | Certified UNSAT verticals: PHP, GF(3), Tseitin | Integration of pigeonhole, GF(3) Gaussian and Tseitin reasoning. |
| `20261004T225147Z-current-n1-n4-audit` | Current N1–N4 audit | Full audit of the controller and proof/model surface. |
| `20261005T144552Z-certified-affine-and-regression-checkpoint` | Affine rows, noL discovery, regressions | Accepted on all four standing sets; WV2-89 gains ramsey_4_4_19. |
| `20261005T184909Z-certified-gf2-organic-integration` | Certified GF(2) organic integration | Gaussian elimination as a learner-selectable certified action. |
| `20261005T192906Z-oddball-unsat-tactix-vs-lymphosat` | Why Tactix beats LymphoSAT on Oddball UNSAT | 23% faster with 5.7x less proof data on LymphoSAT's own family. |
| `20261006T190404Z-integrated-recovery-storage` | Integrated recovery and storage policy | Recovery after a crash and the home-quota storage incident; sets the storage policy. |

## Phase 6: closing the fixed-44 gap, candidates c2 and c2.1 (7–9 Oct)

| Report | Topic | Why it matters |
|---|---|---|
| `20261007T115402Z-persistent-xor-coherent-repairs` | Persistent XOR: repairs and effectiveness boundary | Final state of persistent mixed-CNF XOR reasoning. |
| `20261007T134930Z-fixed44-closure-requirements` | What Tactix needs to close the fixed-44 instances | Requirements analysis per open row. |
| `20261007T172422Z-twelve-target-milestone` | Twelve-target milestone and bottlenecks | Milestone definition and the measured bottlenecks. |
| `20261007T191055Z-ordinary-search-and-retirement` | Removal of noL search | Retired mechanism: specialised noL local search replaced by ordinary search. |
| `20261007T224148Z-session-handoff` | Development checkpoint and handoff | *(linked)* |
| `20261007T225802Z-documentation-cleanup` | Documentation cleanup | *(linked)* |
| `20261008T065417Z-candidate-c2-findings-and-decisions` | Candidate c2: findings and decisions | Decisions record for the first qualification candidate. |
| `20261008T070515Z-dsr-checker-width-and-trust-boundary` | DSR checker width defects and the trust boundary | Pitfall: valid proofs went uncounted because of checker bugs (32-bit overflow, deletion bookkeeping). |
| `20261009T005602Z-candidate-c2.1-qualification` | Candidate c2.1 qualified | Milestone: 13 verified fixed-44 closures on one coherent candidate, with all gates passed. |
| `20261009T063610Z-c2.1-novelty-beyond-references` | Contributions beyond Satsuma, LymphoSAT and Kissat | What c2.1 adds beyond the reference solvers (being edited). |
| `20261009T072332Z-tierd-five-arm-results-extended` | Extended Tier D five-arm report | Extended analysis of the Tier D comparison, with tables, cactus and scatter plots. |

## Pitfalls and lessons

- **Proof checkers have bugs too.** Before counting a rejection as a solver bug,
  rule out the checker: `20260923T080000Z-checker-spelling`,
  `20260923T200000Z-drat-input`, `20260924T014600Z-empty-lrat`,
  `20260924T021500Z-lrat-restatement`,
  `20260924T030700Z-checker-boundaries`,
  `20261008T070515Z-dsr-checker-width-and-trust-boundary`.
- **Pin and route checkers explicitly.** Choose the checker from the emitted
  proof, not from the solver command, and never use an untracked workspace
  build: `20260925T013858Z-am386-tier-routing`,
  `20260925T034000Z-am389-checker-pin-split`,
  `20260927T223225Z-am512-goal-and-wv100-checkpoint`.
- **Checker cost is part of the gate.** Forward elaboration was far slower than
  the owners' backward route, some certificates never verified, and timeouts
  make gates unclean: `20260920-2300-reference-pipeline-proof-production`,
  `20260922-1500-certificate-routing`,
  `20260925T194436Z-am434-tierc-checker-timeout`,
  `20260924T234537Z-am378-ncc-terminal`.
- **Soundness bugs found by fuzzing.** Duplicate-copy DSR deletions, the XOR
  matching lemma, wrong SAT after late input, the DSR root-unit transaction, and
  the terminal Substitute boundary: `20260916-0455-dsr-copy-population`,
  `20260924T025100Z-xor-cancellation`,
  `20260925T154429Z-am414-api-boundary`, `20260926T202947Z-am464-split-unit`,
  `20261003T005230Z-terminal-substitute-feedback-repair`.
- **Audit tools must match the proof semantics.** Strict audits that ignore
  DRAT's implicit root units raise false alarms: `20260924T181338Z-am359-h3`.
- **Resource hazards.** Watch for whole-proof loading, per-call linear scratch
  and unbounded score storage: `20260925T200000Z-am435-bounded-dsr-routing`,
  `20260916-202935-am10-bve-cache`, `20260924T032700Z-walk-score-bound`,
  `20260927T020003Z-am479-proof-spool`.
- **Constants and ledgers drift.** A named budget held a different value from
  the one documented, and the parity ledger lagged the shipped code:
  `20260915-0925-sr-scheduling-audit`, `20260925T205000Z-am440-n4-ledger-audit`.
- **Faithful ports can still regress.** The MAB controller was tried, failed and
  was retired; the LymphoSAT constructors were retired, restored and regressed;
  noL search was removed: `20260928T0120Z-am524-immediate-mab-gates`,
  `20260927T235314Z-am518-mab-purge`,
  `20261004T151533Z-restored-lymphosat-sat-regression`,
  `20261007T191055Z-ordinary-search-and-retirement`.
- **Process and infrastructure incidents.** Model-account quota blocked
  fuzzing, the home quota filled during a crash, and a correctness repair cost a
  solve until a matched diagnosis explained it:
  `20260923T111231Z-opus-recovery`, `20261006T190404Z-integrated-recovery-storage`,
  `20261003T050344Z-snw16-matched-retention-diagnosis`.

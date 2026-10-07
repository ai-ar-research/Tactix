# Progress and resume guide

## Current stage

The accepted shared execution/feedback preparation remains `375b621b`; its
[acceptance report](benchmarks/reports/2026-10-03-shared-policy-preparation-accepted.md)
owns the controller audit, replay exception, full Debug, selected regression
gates, fuzz, and Andrew handoff. Reward design and training remain deferred.
Current work is the unaccepted organic winner-integration stage in
[plan §9.5](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration) and the
[detailed subplan](docs/winner-integration-plan.md). N1--N4, learner-selectable
shared actions, deterministic replay, pure observation, after-action raw
feedback, both-policy fuzz, and independent proof/model checking remain active.

The immediate checkpoint is the original fixed 36 misses from LymphoSAT,
Satsuma+Kissat and Kissat-sc2024; after that, close the eight MAB-only additions
for all 44 four-arm misses. LymphoSAT integration means
authenticated theory information translated into selective certified Boolean
consequences, definitions or deductions for continuing CDCL, on SAT as well as
UNSAT techniques. Constructors/local search remain reference evidence and do
not fulfill this objective. Use the saved `d170b4a5` comparison; do not rerun it.
LymphoSAT-owned WV100/WV2 rows are recovery obligations, other rows are
preservation controls, and B28+C5 plus Rank1--50 are mandatory gates. DSR is the
default; add PB support only after a rule-specific derivation and measured
clausal proof/check cost establish need.

## Current evidence and disposition

| Scope | Authority | Disposition |
|---|---|---|
| Restored LymphoSAT SAT constructors | [terminal regression](benchmarks/reports/2026-10-04-restored-lymphosat-sat-regression-terminal.md) | Rejected `156afef6b`: B28+C5 passed, Rank aggregate failed, and seven WV/WV2 solves were lost; no wrong verdict or checker rejection |
| Affine and guidance focus | [terminal focused report](benchmarks/reports/2026-10-04-affine-guidance-recovery.md) | Six unique direct MDP owners recovered with checked models; `cabp`, Break, and Timetable trajectories not recovered; guidance never offered or consumed |
| Current scheduler repair | `f6d7453f6` | Lucky/8 restored as structured-root stage 0; combined checkpoint suite passes 10/10 in normal and ASan/UBSan builds; no benchmark result yet |
| Ramsey startup recovery | `ef4d1f1f1`, jobs `1749573`, `1749575`, `1749610` | DefaultPolicy's bounded original-root Lucky/22 solves fixed rank-630/WV2-155 `ramsey_4_4_19` UNSAT in 0.167 s with zero decisions/conflicts; its 735,125-byte DSR checks through the canonical pipeline in 7.968 s. B28+C5 and Rank are terminal accepted at 33/33 and 50/50 with zero failures |
| noL hypergraph diagnostic | `5eb58e52c`, job `1749663` | Terminal 0/2: rank-401 `noL-11-10` and WV2-221 `noL-11-4` both timed out at 600 s. Exact recognition fired 372/395 times and charged 743.5M/789.4M moves; Lucky consumed 92.9%/94.1% of runtime, found no model, and no offered partial guidance was consumed. This is a constructor/search-publication miss after recognition, not a guard miss. DefaultPolicy remains disabled; compliant noL work belongs in relation-indexed X17 deductions |
| Certified target coverage | [current all-44 audit](benchmarks/reports/2026-10-05-fixed44-current-evidence-audit/README.md), [exact-certificate audit](benchmarks/reports/20261005-gap44-certificate-identity-audit/README.md) | Current independently checked coverage is 4/36 and 4/44: Ramsey ranks 147 and 630 plus ranks 263 and 744. Rank147 solves in 0.167 s and its DSR checks in 7.705 s; its former miss was stale. Rank744 retains its fixed-target 14,400-second acceptance but exceeds the newer 1,800-second regression cap. The other 40 have no exact accepted current certificate or defensible likely-stale short-preflight classification |
| Partial-model guidance | [terminal ablation](benchmarks/reports/2026-10-05-partial-model-guidance-ablation.md) | Walk-only repair is 21/21 independently valid with zero raw CDCL installs, 148 seed imports and 139 exports. It is 12.8% faster than the preceding run but 12.0% slower than saved `d170`. No-seed is 70.0% slower than `d170`, so removing guidance is rejected. The split-budget prototype is deferred: review found an unmetered duplicate scan and feedback inconsistent with its implicit fallback |
| LymphoSAT SAT mechanism cohort | [six-row causal report](benchmarks/reports/2026-10-05-lymphosat-sat-cohort.md) | The retained graph constructor checks rank 991 SAT in 12.136 s while current default times out; the noL constructor checks WV2 rank 223 in 18.327 s versus 67.838 s default, but regresses rank 222 and misses the other three noL rows. Across 19,525 guided CDCL decisions every installed target already matched the native phase and zero overrode it; Walk imported none. The gains are terminal-constructor evidence, not support for expanding partial-model guidance or certified Boolean recovery |
| `a9f1e14d4` no-seed gates | jobs `1749006`--`1749009` | All accepted: B28+C5 33/33 at 959.055 s, Rank 50/50 at 7,669.874 s, WV100 100/100 with 98 checked/two proof censors, and WV2 89/89 with 77 checked/two proof censors/ten unchanged solver censors. No wrong verdict, checker rejection, or scorer failure |
| Certified theory actions v5 | [technical report](benchmarks/reports/2026-10-04-certified-theory-actions-v5.md) | Public GF(3)/DirectCSP consequences and random offerers implemented but unaccepted; ordinary fuzz had incomplete verdict coverage and no effective v5 reach |
| Public N-ary CSP prefix X17 | [public checkpoint](benchmarks/reports/2026-10-04-public-nary-csp-prefix-action.md), [planner](benchmarks/reports/2026-10-04-nary-csp-prefix-planner.md), [live/action](benchmarks/reports/2026-10-04-nary-csp-prefix-live-action.md), [native offerer](benchmarks/reports/2026-10-04-nary-csp-prefix-native-offerer.md) | Registry ID 39, disabled StepId 28 and C v8 tag 42 expose exact deep-copied domain/residual/prefix payloads through typed replay and a Kernel-owned session. Opt-in `nary-csp` production, raw feedback and post-followup `NaryCsp` semantic-variable publication are wired; focused tests pass. DefaultPolicy remains disabled, and fuzz, checked noL recovery, targets and gates remain open |
| noL certified discovery hardening | `8d13ca87a` | ULC reauthenticates the live checker population and emits pivot-first RAT through DSR/DRAT; X17 uses bounded canonical relation indexing and durable paging. Six focused Debug tests and Release fixtures pass, including a 363-domain/2,486-relation/22,374-prefix noL census and independently checked ULC microproof. Real noL targets remain unsolved |
| Scalable repeated GF(2) X9 | `3787d6724`--`3267251d9`/`9ea8261c4`, [live causal gate](benchmarks/reports/2026-10-05-rank263-affine-matrix-live-causal/README.md), [retained-row A/B](benchmarks/reports/2026-10-06-rank263-retained-row-ab.md), [checked root closure](benchmarks/reports/2026-10-06-rank263-root-affine-closure/README.md), [DefaultPolicy integration](benchmarks/reports/2026-10-06-rank263-default-affine-integration/README.md), jobs `1750992`/`1751022`/`1751024`/`1751026`/`1751028`/`1751029`/`1751030`/`1751032`/`1751378`/`1751436` | The authenticated search matrix, proof cache, replay and current-main focused checks remain accepted. Rank263 is entirely affine: 1,899 ternary XOR equations plus 576 units account for all 8,172 clauses. Exact root reduction finds a 670-source contradiction, while the old 160-source cap discards 24 rows. A bounded 2,048-source/100M-work profile refutes at root in 0.75 s with zero decisions, a 4.99MB DSR and 43.20MB provenance; the every-lemma witnessed-DSR/Trestle pipeline verifies it in 45.80 s. This is a checked target recovery faster than the 7.07 s no-proof CryptoMiniSat reference. Retaining conditioned rows across monotone trail extensions is implemented and sound, but the old partial cap forced all 677 later services to rebuild. DefaultPolicy now schedules that same certified root closure only when affine recognition and matrix construction cover the complete startup snapshot, with limits derived from the live formula population and raw init/service feedback; focused Debug, sanitizer and microproof tests pass. The immutable default DSR/no-proof pair refutes Rank263 at root in 0.72/0.76 seconds with zero decisions; its byte-identical 4.99MB DSR independently verifies in 46.01 seconds. The exact-support follow-up preserves the identical proof and refutes in 0.70/0.67 seconds; its independent every-lemma check verifies in 47.21 seconds. All four bounded regression campaigns for the enabled checkpoint accept. Current full Debug and Claude-owned fuzz remain open. General integration still needs component-derived admission/proof budgets, useful-work scheduling, watched touched rows, local pivot repair and efficient backtrack state |
| GF(2) mixed-support census | [census report](benchmarks/reports/2026-10-06-affine-mixed-support-census/README.md), jobs `1751231`/`1751290`/`1751377` | Raising only the affine recognition group cap admits ranks 540 and 557, confirming the cap diagnosis, but their matrices contain only one binary XOR each; rank 896 is likewise one row. All three produce zero events. Exact admitted-support capture and unchanged-support zero-work reuse are implemented without weakening full-trail publication revalidation. The matched follow-up probes exactly two support variables per service: capture falls from 82--86M assignments to 107--284K (292x--805x), matrix work also falls, and service throughput rises 2x--10x. Component usefulness admission, touched-row wakeups and local pivot repair remain open |
| Public graph interval equivalence X18 | `c6aec42a1`--`cf5ef96d6`, [rank-991 preflight](benchmarks/reports/2026-10-05-rank991-x18-duplicate-preflight/README.md) | Registry ID 40 exposes a disabled-by-default generic scalar action through registry/C/online/replay. It recovers indexed primaries, independently authenticates unique live NOR interval definitions, final-RUP-checks both equivalence binaries against the unchanged formula, adds only existing-variable clauses, records identity projection and leaves propagation separate. Focused action, online/C/replay, feedback, DSR emission, root-fixed propagation and documentation/inventory tests pass. On rank 991 the replay-fingerprinted first action recognized 116 groups but all 232 directions were already-live binaries; across 97 calls it published nothing and consumed 17.505 of 29.953 seconds. The paired ablation was stopped at its preflight gate. X18 does not explain the graph constructor's solve; all target and acceptance gates remain open |
| Current full Debug | jobs `1750117`, `1750129` | Source `14e09a2a4` was rejected at 286/292 by four diagnosed causes: binary-source ULC discovery, an all-zero generated ExactCover profile, feedback-inspector v6 drift and a missing archived DSR checker. Repaired source `d7e48970d` is accepted at 292/292 in 820.39 s; postflight verified 244 executables and 340 immutable bindings |
| Accepted four regression references | jobs `1749575`, `1749610`, `1749666`, `1749709` | All accepted with zero failures. B28+C5 and Rank use `ef4d1f1f1`: 33/33 at 956.316 s and 50/50 at 7,559.461 s. WV100 and WV2 use `5eb58e52c`: 100/100 with 98 checked/two clean proof censors at 2,718.740 s, and 89/89 with 78 checked/two clean proof censors/nine allowed solver timeouts at 3,990.643 s. WV2 checks `ramsey_4_4_19` instead of timing out |
| Fresh current-source gates | `d7e48970d`, jobs `1750135`, `1750168`, `1750241`, `1750302` | All four accept with zero failures on Debug-qualified immutable Release SHA-256 `b670b8de…`. B28+C5 is 33/33, 32 independently verified plus one clean proof censor, at 959.350 s below its 999.87615 s ceiling. Rank1--50 is 50/50, 42 independently verified plus eight clean proof censors, at 7,620.029 s below its 8,521.52175 s ceiling. WV100 is 100/100, 98 independently verified plus two clean proof censors, at 2,806.871 s. WV2--89 has 89/89 dispositions, 78 independently verified plus two clean proof censors and nine allowed solver timeouts, at 4,029.436 counted solver seconds |
| Rank263-enabled regression gates | `3676cff1c`/`f0b67fb0c`, jobs `1751033`, `1751066`, `1751069`, `1751073` | All four bounded scorers accept with zero failures. B28+C5 is 33/33, 32 independently verified plus one clean proof censor, at 940.395 s below its ceiling. Rank1--50 is 50/50, 42 independently verified plus eight clean proof censors, at 7,554.430 s below its ceiling. WV100 is 100/100, 98 independently verified plus two proof censors, at 2,748.336 s. WV2--89 has all 89 dispositions, 78 independently verified, two proof censors and nine allowed solver timeouts, at 3,936.277 counted solver seconds |
| `7dcd36101` default regressions | [terminal report](benchmarks/reports/2026-10-04-nary-offerer-default-regressions.md) | B28+C5, WV100 and WV2--89 pass the bounded scorer; WV2 retains 79 solves, two unchanged proof censors and ten solver timeouts. Rank solves 50/50 below its aggregate ceiling but fails on rank 45 `oddball_19_4_ttf`: the changed Walk-import effort trajectory emits a larger proof whose 319.565-second check exceeds the 304.5289-second allowance. No wrong verdict or checker rejection |
| Certified UNSAT verticals | [technical checkpoint](benchmarks/reports/2026-10-04-public-tseitin-cycle-breaking-action.md), [Andrew report](report-to-andrew/20261004T210026Z-public-tseitin-cycle-action/report.pdf) | Direct PHP is public and terminal; GF(3) has a public authenticated Gaussian batch through C v6; Tseitin X16 is public at registry 38/disabled StepId 27/C v7 tag 41 with exact `tcx2:` component replay, repeated propagated-root discovery, DSR/no-proof, random production and raw feedback. Independent fresh Debug is 10/10; ASan/UBSan action/discovery plus ASCII, binary and repeated-root DSR checks pass. Fuzz, targets and gates remain open |
| Focused controller configuration | [initial target report](benchmarks/reports/2026-10-05-certified-affine-target-probe-initial.md) | The frozen WV2/WV100 probes passed anonymous runtime validation, but all three completed rows timed out with zero offers: width/source caps block WV2-132 and WV100-7, while the group cap blocks WV100-51. Feedback v2 retains exact observation patches and actions in a bounded streaming gzip sidecar; later prototypes use provenance-only telemetry because decoding the full rank-263 stream dominates the short experimental loop. The search subset is learner-owned and replay-fingerprinted; the GF(2) profile can request nonzero semantic advice while `Propagate` remains explicit with no fallback. Rank 263 installed one authenticated wide consequence and propagated six literals, but the attempted 8/64-row startup profiles committed only that one row because of a post-`Propagate` lifecycle defect; both timed out and are not batch evidence. No new target recovery is claimed |
| Rank-263 CryptoMiniSat mechanism controls | pinned CryptoMiniSat 5.14.7 jobs `1750457`, `1750463` | Default CryptoMiniSat solves UNSAT without proof in 7.07 s at 56,003 conflicts after recognizing 776 XORs and building a 736-by-745 matrix. Disabling XOR handling times out at 180.21 s; retaining discovery and native XOR attachment while disabling matrices times out at 180.16 s. These controls isolate the persistent Gaussian matrix as the dominant reference mechanism; the X9 row owns the subsequent checked Tactix recovery |
| Fixed-44 work partition | frozen four-arm comparison plus [current evidence audit](benchmarks/reports/2026-10-05-fixed44-current-evidence-audit/README.md) | The 44 rows partition into 14 certificate-only acceptance failures, 17 structure-led search gaps and 13 unattributed policy/search gaps. Ranks147/263/630/744 are checked current recoveries; 40 remain true open. Rank263's static X9 failure is closed by the checked DefaultPolicy affine root service. On Rank401, certified ULC completion creates 347 exact domains and eight X17 offers; 3/3 X17 applications are effective, but [the target result](benchmarks/runs/20261005-rank401-ulc-x17-753574f34-r2/results/000.json) remains UNKNOWN at 300 seconds, moving the boundary from recognition to deduction strength/scheduling. Rank991's 232 interval directions are already-live duplicates. None is a new recovery |
| GF(3) Gaussian general rows | `b94cf1c48`/`84f7b1a32`, [WV9 measured report](benchmarks/reports/2026-10-05-wv9-x14-no-supported-outcome/README.md), jobs `1750867`/`1750876`/`1750969`/`1750985`/`1750988`/`1750993`/`1750995`/`1750997`/`1750999`/`1751001`/`1751004`/`1751005`/`1751006` | Recognition, bridge, Gaussian planning and certificate work all complete within measured limits. The former boundary was 360 multi-variable rows with no supported X14 outcome. Registry ID 43/StepId 32/C v11 now exposes one forbidden tuple of one authenticated non-unit row as a bounded learner-selectable Boolean clause with exact `g3r1` identity, replay, provenance, explicit propagation/advice, native pagination and raw feedback; DefaultPolicy remains disabled. CPU-Q sanitizer, exhaustive model, exact-current-main CLI/API/replay/producer and independent forward/backward DSR/Trestle checks pass. The short WV9 probe authenticates one width-23 candidate but offers none before mutation. Exact diagnostics reject the tuple-width hypothesis. An exact occurrence index cuts premise planning from at least 50M to 643,415 work, then exposes 274 authenticated branch domains. Raising only measured width/work bounds confirms the old certificate explores a global `3^251` case tree and emits nothing. Replace it with a memoized authenticated Gaussian-DAG proof that eliminates only each Add node's local canceled domains before another public checked action; effective fuzz and gates remain open |
| RB-SAT DirectCSP boundary | `066c11f2a`, [causal report](benchmarks/reports/2026-10-05-rbsat-direct-csp-causal/README.md), jobs `1750888`/`1750890`/`1750896`/`1750964` | WV100 rank93 reaches 16 exact offers. One selected action publishes a certified 17-literal support row, produces no root propagation, and then returns the same independently checked SAT model as the plain control. A valid opt-in replay-fingerprinted profile now schedules a bounded sequence of distinct `DirectCspSupport -> Propagate -> SemanticVariableAdvice` actions without fallback; its 21 focused sanitizer, proof/model, CLI, API, replay and scheduling checks pass. DefaultPolicy is unchanged. A short repeated-action target probe and effective fuzz remain open; no solve gain or target closure is claimed |
| Compact n-ary equality consequence | `d50361cdb`/`143211d3b`/`9ba6d06a7`, jobs `1750928`/`1750971`/`1750974`/`1750977`/`1750979`/`1750981`/`1750990`/`1750994`/`1750996`/`1751000`/`1751002` | A freshly recaptured exact ternary X17 owner defines one branchable aligned-domain equality bit and publishes `3k` certified ternary clauses from complete diagonal nogoods. Registry ID 42, disabled StepId 31, C v10 tag 43, typed replay, raw telemetry, explicit propagation/advice and bounded native pagination expose it to the shared controller; DefaultPolicy is unchanged. Current-main CPU-Q integration passes action/API/replay/producer tests, the native-producer CLI contract, DSR and model oracles, and feedback inventory. Two 30-second Rank401 probes are expected admission negatives: the saved effective X17 run used an unreported wrapper-injected ordered startup program with CompleteCurrentPass SR, a 2,048 allowance and ULC completion, while current random selection requested one canonical SR with ULC disabled. The generic explicit startup-prefix capability now serializes that exact once-only sequence, repeats CompleteCurrentPass until completion, resolves a controller-only native offer and fingerprints replay; exact-main job `1750990` passes policy, online and native-producer tests. Job `1750994` reproduces the hidden sequence explicitly: CompleteCurrentPass SR finishes after 1,089 applications (1,067 effective), records 1,047 ULC binaries, finds 347 exact domains/eight X17 offers and applies one effectively before ResumeSearch; CDCL then reaches its first Restart after 241 decisions/72 conflicts. The public equality rung offers three choices, adds one variable/nine certified clauses, advises it, and makes it CDCL's first decision. In a fixed 30-second pair, X17-only and X17+equality both remain UNKNOWN; equality performs 4.68% more conflicts, 10.17% more propagations and writes larger proof/provenance. Under the simpler-wins rule it is not promoted on Rank401; stronger relation consequences, fuzz, checked recovery and gates remain open |
| N-ary equality AMO X20 | `581e817f4`/`ed30d5f91`, [technical report](benchmarks/reports/2026-10-06-nary-equality-amo-action.md), jobs `1751013`/`1751018`/`1751020` | The shared controller can now compose all three complete aligned-equality definitions of one freshly recaptured ternary X17 owner. For each pair it derives value-specific ternary helpers, uses the shared exact-one cover to publish one binary exclusion, deletes the helpers, and adds no variable. Registry ID 44, disabled StepId 33, C v12 tag 45, deep-copied typed replay, native discovery and raw feedback are wired; exact `3k` live definition clauses per equality are required fail closed. The immutable CPU-Q sanitizer gate passes 8/8, forward/backward DSR plus both Trestle checks verify, and the corrected CLI producer test passes 1/1 with explicit X20 and `all`. In the 30-second Rank401 pair both arms remain UNKNOWN; two X20 applications cut conflicts from 141,860 to 75,697 and DSR bytes from 9.87M to 4.75M, but also alter later random X17 selections, so this is promising integrated evidence rather than an isolated causal estimate. DefaultPolicy is unchanged; effective fuzz and acceptance gates remain open |
| Current both-policy fuzz defects | [F1/F3 repair](benchmarks/reports/2026-10-05-both-policy-fuzz-f1-f3-repair.md) | The Claude-owned superseded candidate found a repeated Direct-PHP snapshot `InvalidRequest` and an X9 wide-row helper-polarity proof rejection. Both minimized cases are repaired. CPU-Q job `1750809` passes six focused Debug+ASan/UBSan tests, the 51-clause controller reproducer completes SAT without error, and the 12-clause X9 DSR independently verifies UNSAT. A fresh immutable both-policy fuzz campaign remains required; the old candidate's continuing census is diagnostic only |
| Rank291 certificate route | [binary-LSR causal gate](benchmarks/reports/2026-10-05-rank291-binary-lsr-causal/README.md), job `1750934` | The retained 424,530,578-byte DSR independently verifies in `dsr-trim` after 723.776 seconds and produces a 656,051,430-byte binary LSR. Feeding that binary stream directly to pinned Trestle removes the 1.46-GB text expansion, but Trestle still times out after the remaining 1,075.928 seconds. The conversion is not the decisive 1,800-second acceptance bottleneck, so no checker-route patch or target recovery is claimed. Only 20 additions carry witnesses; reducing or replacing that witnessed core remains an unproved solver-side hypothesis |
| Rank541 certificate route | [binary-LSR negative](benchmarks/reports/2026-10-05-rank541-proof-route-negative/README.md), job `1750998` | The exact 247,511,403-byte DSR independently verifies in 363.628 seconds, retaining 3,476,435 additions and emitting a 342,910,304-byte binary LSR. Direct pinned Trestle then times out on the 11,133,828-clause formula after the remaining 1,436.299 seconds. This matches Rank1129's semantic-core boundary within 0.83%; conversion is not decisive, no general compaction is yet justified, and the row remains open |
| Remaining certificate evidence gaps | [Rank1004/1075 audit](benchmarks/reports/2026-10-05-certificate-gap-evidence-boundaries/README.md) | Rank1004's deleted 4.75GB DSR had one SR event/five witness literals and hit eager `dsr-trim` allocation; without the event bytes, RUP-first publication is speculative. Rank1075's current certificate grew 12.51% despite fewer learned clauses and unchanged 32-event/160-witness SR shape. Forward every-lemma versus backward core trimming is a falsifiable hypothesis, but both exact certificates were deleted; retain future ordinary proofs and record bytes by producing rule before changing solver or checker behavior |
| Rank1129 certificate route | [frozen negative diagnostic and dispatch attribution](benchmarks/reports/2026-10-05-rank1129-proof-route-negative/README.md) | Direct binary LSR input removes only the 6.074-second text expansion; Trestle still times out on the 3,447,844-addition retained core, so there is no checker-route change or target closure. Detector-only job `1750802` proves none of the 126 LymphoSAT matchers accepts this row: its verified 1,340.631-second PBP came from the ordinary Kissat-PBP fallback, not the clique-color theory solver. This is a proof-representation/certificate target; no organic theory action or general CDCL PBP integration is justified in this increment |
| Public controller full Debug | [conformance report](benchmarks/reports/2026-10-04-public-controller-full-debug.md) | `ec7e0ac86` passes 285/285 Debug tests in 957.60 s. Terminal Direct-PHP random programs cannot acquire a bundled follow-up; append-only Tseitin registry contracts and restored Lucky/8 lifecycle tests are synchronized. Fresh immutable ASan+UBSan pin `build/pin-public-theory-ec7e0ac86-sanitize-01` passes its focused checks; effective both-policy fuzz, targets and gates remain open |
| Current N1--N4 audit | [scoped audit](benchmarks/reports/2026-10-04-current-n1-n4-audit.md) | Current code passes 46/46 focused public-action/proof/controller/wiring checks and 69/69 proof-oracle tests. N1/N2 universal coverage, current N3 gates, and N4 ledger acceptance remain pending |
| Naive target row-cover proof | [checker-censor report](benchmarks/reports/2026-10-04-row-cover-target-checker-censor.md), [Rank991 admission report](benchmarks/reports/2026-10-06-rank991-balanced-row-cover-admission.md) | The original 6,783,269-addition, 187,733,915-byte proof reached the 1,800-second checker cap. A public-wrapper prototype remains isolated: its 300-second Rank991 probe made 2,646 zero-recognition calls, published nothing and spent 55% of runtime in discovery. Any retry needs cheaper admission plus durable session-level no-structure suppression; no cap increase or action integration is justified |

The corrected guidance fuzz jobs are terminal. Default job `1748417` found zero
failures in 51 corpus cases and 10,000 generated cases, with 9,718 terminal
verdicts and 282 timeouts. Random job `1748418` found zero failures with 8,464
terminal verdicts and 1,536 timeouts. `LocalSearchGuidance` fired zero times in
both, so these are compatibility/correctness evidence for terminal cases, not
full oracle coverage or effective guidance fuzz acceptance. Current commit
`7dcd36101` is pinned as Release binary SHA-256 `c652bed6a98e07d4362146240ccd161c4082065def7c50ccbcc8472d542ae05c`.
Its full-width controller regressions are terminal: B28+C5 `1748622`, WV100
`1748669`, and WV2--89 `1748673` pass; Rank1--50 `1748655` fails only the
rank-45 proof-cost comparison despite 50/50 solves and an accepted aggregate.
The [terminal report](benchmarks/reports/2026-10-04-nary-offerer-default-regressions.md)
owns exact counts, the unchanged WV2 censors and the Walk-effort diagnosis.
The focused oddball campaign `1748917` has 21/21 matching solver verdicts and
no solve timeout. Its solve aggregate is 4,315.167 seconds versus 3,359.788
seconds saved (+28.4%); guidance-active TTF accounts for the change, and proof
size tracks solve regression. `oddball_26_5_ttf` emitted a 5.312-GB proof and
its checker exhausted allocation rather than rejecting it. Repair-isolation job
`1748977` removes direct raw-candidate CDCL handoff while retaining seeded Walk:
21/21 are checked valid, solver time is 3,761.701 s, and `26_5` is valid at
544.356 solver / 1,139.677 checker seconds with a 2.287-GB proof. No-seed job
`1749005` is 21/21 valid but takes 5,711.837 s; `29_4` alone takes 3,171.919
solver / 10,808.933 checker seconds with a 9.052-GB proof. The four standing
gates all accept. Archived Debug job `1749241` is invalid infrastructure
evidence: its snapshot omitted checker binaries and tests mutated the hashed
source tree, causing 11 harness failures. Corrected job `1749472` proves the
long `lucky` test passes, but is also invalid: its frozen build contains seven
non-executable test files, six zero length, causing `BAD_COMMAND` rather than
source assertion failures. The reusable v3 harness now requires a clean build
and validates every CTest executable before freezing; replacement job `1749659`
runs the same `64de46fe9` source archive with 289 tests and 295 bindings.
The unrelated interactive allocation `1747502` remains outside this workstream.
The current forced-TTF repair makes balanced-signature Lucky fix row `b+1`
and search only signs when live clauses require those rows. Its focused test
passes. Immutable Release commit `64de46fe9` solves and validates cached
`112_5_ttf` in 2.570 seconds versus saved LymphoSAT's 1.970 seconds. The
11-control campaign retains 10/11 expected verdicts: `29_4_ttf` reaches its
1,800-second cap versus 942.814 seconds on saved `d170b4a5`; the full 5,000-second
oddball campaign remains authoritative for solve retention.
On immutable forced-TTF pin `64de46fe9`, B28+C5 job `1749473` is accepted:
33/33 verdicts, 32 independently verified plus one clean proof censor, and
961.313 solver seconds under the 999.87615 ceiling. Rank job `1749474` is also
accepted: 50/50 verdicts, 42 independently verified plus eight clean proof
censors, and 7,647.905 seconds under the 8,521.52175 ceiling; every completed
proof comparison meets its row bound. The Ramsey-enabled four-gate checkpoint
is terminal accepted. B28+C5 `1749575` has 33/33 expected verdicts, 32
independently checked and one clean proof censor at 956.316 solver seconds.
Rank `1749610` has 50/50, 42 independently checked and eight clean proof
censors at 7,559.461 seconds. WV100 `1749666` has 100/100 dispositions, 98
independently checked and two clean proof censors at 2,718.740 seconds.
WV2--89 `1749709` has 89/89 dispositions, 78 independently checked, two clean
proof censors and nine allowed solver timeouts at 3,990.643 seconds; checked
`ramsey_4_4_19` is the one-row gain over `a9f1e14d4`. B28+C5 and Rank bind
`ef4d1f1f1`; WV100 and WV2 bind `5eb58e52c`. All use the shared 1,800-second
proof deadline and standing row gates; none has a wrong verdict, checker
rejection or scorer failure.
Replacement Debug job `1749659` is accepted: 289/289 pass in 814.11 seconds,
with 242 executables and 295 frozen bindings reverified after the run. These
results predate the ULC/X17 and scalable GF(2) commits, so current source still
requires a fresh Debug and four-gate checkpoint. Full oddball job `1749458` has
20/21 final valid rows.
`26_4_ttf` checks after 1,076.174 solver and 3,944.219 proof-pipeline seconds;
`29_4_ttf` solved UNSAT in 2,524.876 seconds and is proof-checking. Its verdict is
recovered but remains much slower than saved `d170b4a5` at 942.814 seconds.

## Next action

Final runtime source `76e1f68b8` passes focused Debug and sanitizer/proof/model
checks 13/13 each. The [parallel acceptance handoff](benchmarks/reports/2026-10-07-final-parallel-acceptance-handoff/README.md)
owns all active pin/job bindings and provisional dispositions. [Final Debug](benchmarks/reports/2026-10-07-prefix-meter-full-debug-terminal/README.md)
`1751523` finishes314/315; only doc-size debt fails, and post-run pin verification passes.
[Claude smoke](benchmarks/reports/2026-10-07-final-fuzz-resource-priority-handoff/README.md) `1751519` has zero owner-reported failures; native broad `1751520` runs
and full target `1751604` is queued. Owner marks cancelled `1751515`/`1751516` superseded, with no verdict.
B28+C5 `1751524`, Rank1–50 `1751525`, WV100 `1751526`, WV2–89 `1751527`
are submitted concurrently. Grade each gate; pending checks never promote.

Crash recovery committed the affine publication/session repair at `95b0fad13`;
its [focused checkpoint](benchmarks/reports/2026-10-06-resume-affine-repair-cleanup/README.md) passes Debug and ASan/UBSan 6/6 with every-lemma proofs.
Claude accepted its [fresh fuzz campaign](benchmarks/reports/2026-10-06-affine-repair-fuzz-owner-verdict/README.md): both broad legs and 12,998 targeted tasks pass. A disclosed width-one conflict publication decline remains a nonfatal follow-up; service-time interruption was not reached. This does not accept the later cumulative pin.
[The preceding owner report](benchmarks/runs/20261006-r263-default-affine-fuzz-owner/FINAL_REPORT.md) rejected root-unit, empty-conflict and invalidated-session handling.
Parallel increments add a [bounded GF(2) predecessor](benchmarks/reports/2026-10-06-affine-predecessor-microgate.md) and X20 decline/retry feedback (`2ddfcbedf`); cumulative Debug and ASan/UBSan with leak detection and proof shadow each pass 10/10, including independent proofs and restore/rebuild telemetry.
The [test-only replay microgate](benchmarks/reports/2026-10-06-affine-controller-replay-microgate/README.md)
adds failed-nonmatching predecessor atomicity and independently reexecuted public
controller extension/restore/deeper-rebuild transcripts; affected Debug and
sanitizer checks pass with identical proof/provenance and a checked SAT model.
The [WV93 short probe](benchmarks/reports/2026-10-06-directcsp-repeated-native/README.md) reaches one/four support publications but yields no root deduction or solve; no longer target is justified.
The preceding source pin `4096d41b3` has a Claude-owned cumulative fuzz request, [prepared and invoked](benchmarks/reports/2026-10-06-integrated-4096d41b3-fuzz-owner/request.txt).
Claude's [owner leg verdict](benchmarks/reports/2026-10-06-integrated-default-fuzz-owner-verdict/README.md)
closes DefaultPolicy fuzz job `1751467` as passed after same-pin
graded corpus rerun `1751474` clears entry 026's 60-second sanitizer timeout.
The report owns checked counts and timeout coverage limits. Claude's
[random-controller verdict](benchmarks/reports/2026-10-06-integrated-controller-fuzz-owner-verdict/README.md)
closes `1751468` as passed: 100,010 generated cases and 51 corpus entries,
zero failures and zero UNKNOWN; effective affine initialization and controller
transitions are covered. Calibration `1751473` completed without a source
failure, exposing two private harness expectations that Claude repaired.
Both broad legs pass, but Claude rejects cumulative acceptance after the
13,155-task targeted matrix `1751475` found a native X18 stale-owner decline
misclassified as InvalidRequest, terminating search with UNKNOWN. The
[owner failure handoff](benchmarks/reports/2026-10-06-integrated-targeted-failure-handoff/README.md)
owns the disposition and bounded reproducer. Repair equality recapture/admission,
then rerun affected targeted kinds and reproducers through Claude. The
[terminal owner verdict](benchmarks/reports/2026-10-06-integrated-terminal-fuzz-owner-verdict/README.md)
closes restore coverage at 100 restores in 69 checked CLI runs. No cumulative
fuzz jobs remain live; the sole source blocker is the native equality defect.
Claude reports default census `1751469` has
zero UNKNOWN but zero admitted affine startup actions, so targeted reach is
mandatory. Claude reports
the completed controller census `1751470` has zero UNKNOWN exits across
100,000 seeds (timeouts lack oracle coverage). Both matrix differential
runs passed, but terminal cumulative acceptance is rejected. Resume its existing
session `4109784f-d61a-4bd4-8f29-8f842447ddc7` with `claude --resume ID -p`
after job transitions; Claude alone inspects campaign output and owns the final
report. The initial repair campaign is terminal accepted; its evidence does
not accept the cumulative X18 failure.
[Storage verification](benchmarks/reports/2026-10-06-final-storage-verification/README.md) records 219.93 GB after reclaiming about 528 GB.
The [allocated scan](benchmarks/reports/2026-10-07-final-allocated-storage-recheck/README.md)
completes without errors at 259.23 GB; the earlier 452.94-GB scan measured apparent size.
The final coherent pin still needs effective both-policy fuzz, full Debug,
B28+C5, Rank1--50, WV100 and WV2--89 acceptance. Follow plan §8.1
for the standing 700-GB storage check and aggressive cleanup gate.

Rank263 is now a checked DefaultPolicy recovery at the measured root boundary.
The next GF(2) work must test generality rather than repeat this target: run full
Debug, both-policy structure-targeted fuzz and the four regression campaigns;
then use another affine owner that requires reasoning during CDCL to drive
component scheduling, watched/touched rows, local pivot repair and efficient
backtrack state. Keep complete-image admission and DSR checking as hard gates.

Use the same promotion ladder for every gap technique: focused semantic and
proof/model tests, one immutable sanitizer gate, one 5--30-second causal target
probe with detection/offer/application/deduction/CDCL-yield/proof-cost counters,
then a longer target only after the mechanism produces useful nonduplicate
Boolean information. Batch full Debug, both-policy fuzz and the four regression
campaigns at coherent integration checkpoints rather than rebuilding them for
each internal primitive.

Two disjoint certified SAT lanes proceed in parallel. DirectCSP already has its
source-derived stock profile, metered native enumeration and repeated
learner-visible `DirectCspSupport -> Propagate -> SemanticVariableAdvice`
scheduling with replay, raw feedback, relevant-change admission and duplicate
suppression at `066c11f2a`. The one/four-row WV93 short probe confirms repeated
publication but no root deduction; next obtain useful Boolean deduction or an
isolated independently checked search benefit before extending target runtime. Rank401/noL already reaches
three effective X17 actions after certified ULC completion; its next increment
is a stronger generic relation consequence or conservative definition with exact
premises, DSR/RAT derivation, mapping/reconstruction and bounded repeated
inprocessing. Neither lane may promote to DefaultPolicy from a single target.

After these shared increments are integrated, run their focused proof/model, API,
replay, feedback and sanitizer checks as one cumulative candidate. Then invoke
Claude-owned effective DefaultPolicy/random-controller fuzz on that exact pin.
Only a behavior-changing DefaultPolicy candidate that survives fuzz proceeds to
full Debug and the B28+C5, Rank1--50, WV100 and WV2--89 gates under the standing
1,800-second proof-check and slowdown rules.

The certified structured verticals now include public X17 NaryCspPrefix.
Direct PHP is append-only ID 36, GF(3) Gaussian ID 37, Tseitin cycle breaking
ID 38, and NaryCspPrefix ID 39 with disabled StepId 28 and C v8 tag 42.
X16 offers exact connected-component identities without a private component
choice, publishes one authenticated X7 unit, leaves Propagate separate, and
then permits a fresh root-aware census. Every constraint still retains Boolean
form, theory meaning, exact premises, parameters, mapping, proof link,
projection/deletion state and exact effort/yield/proof cost. Native-producer and
root-family CLI selection now makes focused reach observable without a static
payload suppressing discovery. Fresh immutable sanitizer pin
`build/pin-public-theory-ec7e0ac86-sanitize-01` binds source
`ec7e0ac86c7afde5f7c9288d45272e2b465f1581` to binary SHA-256
`76090ca862ba7ed07a20140289e398638966ebed8a177cdf7c059d4bcc35b6c2`.
Its controller CLI check and Direct-PHP/random/Tseitin ASan+UBSan matrix pass.
The complete fixed fuzz-harness self-test also passes against the frozen
checker collection, including known-bad model/proof/truncation controls,
every-lemma audits, shrink, checker-collection validation and native LRAT.
Require Claude-owned both-policy fuzz: disabled DefaultPolicy is the zero-reach
compatibility control, while controller-random must show producer calls/offers
and effective applications for every enabled structured family. The Tier-D
preparer now records repeatable immutable solver arguments without permitting a
certificate-mode override, so a passing focused fuzz report can feed exact
controller configurations into saved-target diagnostics. Four validated
diagnostic projections now freeze 12 GF(3) WV100 rows, three Tseitin WV100
rows, six Tseitin WV2 rows and four plausible direct-PHP WV2 rows while keeping
the 1,200/600-second contracts separate. Their scorer fails closed unless every
row is independently checked and shows an application plus effective action;
native campaigns additionally require producer calls and offers. Then evaluate
the saved target set and regression gates only when benchmark execution is
authorized. DefaultPolicy keeps all four actions disabled meanwhile.

Keep intended DefaultPolicy structural seeding disabled. Preserve the rejected
split-budget patch only as design input; redesign it only if later evidence
justifies separate allowances without duplicate unmetered work or implicit
fallback. Raw candidates never replace Walk-refined targets. Complete the
active oddball, Debug, B28+C5 and Rank campaigns, then run WV/WV2. Forced-TTF
fuzz must distinguish exact forced recognition from guarded-clause near misses.
The Ramsey slice has passed its checked rank-630/WV2-155 target probe. Finish
its active Rank, oddball, WV and WV2 campaigns. Lucky/25 is the noL reference
constructor. Keep it out of DefaultPolicy; use its rank-401 `noL-11-10` and WV2
rank-221/original-484 `noL-11-4` probes only to measure recoverability and
recognition/search effort. The
[certified noL design](benchmarks/reports/2026-10-05-nol-certified-boolean-design.md)
uses authenticated ULC exact-one completion, relation-clause-indexed X17
discovery and repeated certified prefix consequences. Commit `8d13ca87a`
implements live-population-authenticated ULC RAT and replaces combination
enumeration with bounded canonical relation indexing; its 363-domain synthetic
census passes. Retain constructor and seeded/no-seed campaigns as causal
evidence, and require checked real-target recovery before claiming effect.
The earlier `cabp`, Break and Timetable trajectory controls remain open evidence
obligations if those rows regress again. The n-ary CSP vertical
is now public through exact typed C/replay payloads, a Kernel-owned session,
controller execution/raw feedback, native `nary-csp` production and
semantic-variable publication. Its focused tests pass. Obtain effective
both-policy fuzz reach next, and do not infer noL recovery before checked target
evidence.

## Established authorities and open gates

- Accepted solver baseline: no-MAB `d170b4a5`; the [AM551 milestone](benchmarks/reports/2026-09-28-am551-no-mab-hybrid-milestone-accepted.md) and [AM552 WV evidence](benchmarks/reports/2026-09-29-am552-wv100-terminal-tierd-progress.md) own stabilization.
- The [five-arm Tier-D report](benchmarks/reports/2026-10-01-tierd-five-arm-terminal-results.md) owns 986 checked Tactix solves, the 36 checker failures/146 checker timeouts, and the saved portfolio comparison.
- The newest retained Tier-B window, [AM466 split paired](benchmarks/runs/20260926-163543-B-am466-split-paired/), predates the no-MAB checkpoint and is historical diagnostic evidence only.
- Accepted regression references: Rank1--50 50/50 and B28+C5 33/33, with aggregate solver ceilings 8,521.52175 and 999.87615 seconds. The newest accepted `5eb58e52c` checkpoint has WV100 98 checked plus two clean proof censors, and WV2--89 78 checked plus two clean proof censors and nine solver timeouts.
- Full Debug passes 289/289 at archived source `64de46fe9` in 814.11 seconds; its archive reverified 242 executables and 295 bindings. The scoped N1--N4 audit passes, while universal N1/N2, current N3, and N4 verification remain pending. Full acceptance still requires a fresh post-`405b56724` Debug/gate run, both-policy fuzz with effective new-action reach, and all 44 independently checked targets.
- Formal T4-K/T4-VBS targets and universal N1--N4 verification remain unmet. The broader objective is to substantially exceed LymphoSAT, Satsuma+Kissat, Kissat-sc2024, and AE-Kissat-MAB combined.

## Recent changes

- Forced-TTF recognition now removes unnecessary row selection from balanced-
  signature Lucky and searches only signs under the existing typed limits;
  focused and cached Debug model validation pass, while Release evidence is pending.
- Ramsey startup now runs once by default at the original-input boundary and
  the normal CLI supplies anonymous structural provenance when no retained log
  is requested. Its fixed Tier-D target and all four standing gates are checked.
- Public Lucky/25 adds exact noL three-color recognition and a bounded generic
  coloring search. It validates every SAT model, publishes only active signed
  phases after exhaustion, meters terminal publication, and records feedback.
  Its two target probes timed out despite repeated exact recognition, so it
  remains a disabled diagnostic and the certified X17 path takes priority.
- The current N1--N4 audit passes 46/46 focused public-action/proof/controller
  checks and 69/69 proof-oracle tests; it keeps universal and performance
  verification explicitly pending.
- Public X17 NaryCspPrefix is append-only ID 39/StepId 28/C v8 tag 42. Exact
  nested payload copying, typed replay, Kernel session, `nary-csp` production,
  raw feedback and `NaryCsp` semantic-variable publication pass focused tests.
  ULC now has authenticated pivot-first RAT and discovery is relation-indexed;
  DefaultPolicy and target acceptance remain unchanged.
- Current source repairs terminal-action random generation and synchronizes the
  append-only public registry tests; the latest archived Debug suite passes
  289/289, before the current GF(2)/ULC checkpoint.
  The scripted-policy fixture retains all schedule transitions while reducing
  its externally stepped conflict path from about 75,000 to about 4,000.
- The replacement `ec7e0ac86` ASan+UBSan pin is immutable and hash-verified;
  the fixed harness self-test passes and the fuzz handoff rejects the
  superseded pre-repair pin.
- The current controller checkpoint exposes replay-fingerprinted native-producer and root-family selection for non-vacuous focused fuzz.
- Public X9 now exposes exact direct and extended rows through C v9, replay and
  the `gf2-affine` producer. Wide selected rows publish certified bounded XOR
  networks with exact projection and helper accounting; focused and external
  proof checks pass. A bounded-count producer profile can now request a finite
  startup dose: successful row settlement synchronizes durable keys, preserves
  the remaining exact page across a separately selected root Propagate action,
  and records the dose in the controller fingerprint. This repairs the observed
  one-row lifecycle defect. Rank263 now applies all 1,328 requested rows, but
  repeated export grows 37,748 variables and remains UNKNOWN at 300 seconds.
  A pure owner-bound affine matrix now derives exact-source implication/conflict
  reasons across backtrack. Its authenticated transaction bridge meters source
  validation, complete-premise checking, proof events, helper extent and arena
  cost, then marks only one compact RUP reason for installation. The integrated
  live publisher revalidates the current trail, namespaces, source occurrences
  and proof writer, emits the preflighted proof, installs the ordinary reason,
  and leaves consumption to BCP. Debug+ASan and forward DSR/Trestle pass in jobs
  `1750746` and `1750800`. DefaultPolicy now owns complete-image root initialization and the deterministic
  service; Rank263 is independently checked at 0.72 seconds. Full Debug,
  both-policy fuzz and the four regression gates remain open.
- Public X18 `GraphIntervalEquivalence` is a generic bounded continuing-CDCL
  action with focused merged-main proof/interface coverage. Its rank-991
  preflight selected scalar 1024 before search and authenticated 116 groups,
  but every one of the 232 directions was already live. Repeated unchanged
  censuses added nothing and consumed 58.4% of the 30-second interval, so the
  paired ablation was stopped. Rank 991 needs a different certified consequence
  or an explanation of the constructor's coordinated choices.
- Rank401 one-call census job `1750735` completes discovery with 7,473 ALO
  candidates, zero exact domains, and `insufficient-exact-domains`: the exact
  CNF has 363 ternary ALOs, 7,458 negative relation clauses and no binaries.
  AMO is not implied, so X17 must remain strict. Next run the existing certified
  ULC completion as an explicit public action, Propagate, then take exactly one
  bounded X17 census/offer page before any 300-second target campaign.

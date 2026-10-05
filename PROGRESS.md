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

The primary objective is all 44 misses from the saved four-winner portfolio;
the original fixed 36 remain a diagnostic subgroup. Use the saved `d170b4a5`
comparison; do not rerun it. LymphoSAT-owned WV100/WV2 rows are recovery
obligations, other rows are preservation controls, and B28+C5 plus Rank1--50
are mandatory gates. DSR is the default; add PB support only after a
rule-specific derivation and measured clausal proof/check cost establish need.

## Current evidence and disposition

| Scope | Authority | Disposition |
|---|---|---|
| Restored LymphoSAT SAT constructors | [terminal regression](benchmarks/reports/2026-10-04-restored-lymphosat-sat-regression-terminal.md) | Rejected `156afef6b`: B28+C5 passed, Rank aggregate failed, and seven WV/WV2 solves were lost; no wrong verdict or checker rejection |
| Affine and guidance focus | [terminal focused report](benchmarks/reports/2026-10-04-affine-guidance-recovery.md) | Six unique direct MDP owners recovered with checked models; `cabp`, Break, and Timetable trajectories not recovered; guidance never offered or consumed |
| Current scheduler repair | `f6d7453f6` | Lucky/8 restored as structured-root stage 0; combined checkpoint suite passes 10/10 in normal and ASan/UBSan builds; no benchmark result yet |
| Partial-model guidance | oddball jobs `1748917`/`1748977`; repair after `27e0088ed` | All 21 Tier-D oddballs solve, but guidance-active TTF rows are 29.1% slower while inactive TTO rows are 3.5% slower. The first repair removes the raw-S8 overwrite after seeded Walk. A separate audit shows seed preparation consumes search allowance and admits worse starts; DefaultPolicy therefore disables structural seeding pending split budgets/admission, while explicit controller Walk/S8 actions remain. Focused validation and the isolation campaign are active |
| Certified theory actions v5 | [technical report](benchmarks/reports/2026-10-04-certified-theory-actions-v5.md) | Public GF(3)/DirectCSP consequences and random offerers implemented but unaccepted; ordinary fuzz had incomplete verdict coverage and no effective v5 reach |
| Public N-ary CSP prefix X17 | [public checkpoint](benchmarks/reports/2026-10-04-public-nary-csp-prefix-action.md), [planner](benchmarks/reports/2026-10-04-nary-csp-prefix-planner.md), [live/action](benchmarks/reports/2026-10-04-nary-csp-prefix-live-action.md), [native offerer](benchmarks/reports/2026-10-04-nary-csp-prefix-native-offerer.md) | Registry ID 39, disabled StepId 28 and C v8 tag 42 expose exact deep-copied domain/residual/prefix payloads through typed replay and a Kernel-owned session. Opt-in `nary-csp` production, raw feedback and post-followup `NaryCsp` semantic-variable publication are wired; focused tests pass. DefaultPolicy remains disabled, and fuzz, checked noL recovery, targets and gates remain open |
| `7dcd36101` default regressions | [terminal report](benchmarks/reports/2026-10-04-nary-offerer-default-regressions.md) | B28+C5, WV100 and WV2--89 pass the bounded scorer; WV2 retains 79 solves, two unchanged proof censors and ten solver timeouts. Rank solves 50/50 below its aggregate ceiling but fails on rank 45 `oddball_19_4_ttf`: the changed Walk-import effort trajectory emits a larger proof whose 319.565-second check exceeds the 304.5289-second allowance. No wrong verdict or checker rejection |
| Certified UNSAT verticals | [technical checkpoint](benchmarks/reports/2026-10-04-public-tseitin-cycle-breaking-action.md), [Andrew report](report-to-andrew/20261004T210026Z-public-tseitin-cycle-action/report.pdf) | Direct PHP is public and terminal; GF(3) has a public authenticated Gaussian batch through C v6; Tseitin X16 is public at registry 38/disabled StepId 27/C v7 tag 41 with exact `tcx2:` component replay, repeated propagated-root discovery, DSR/no-proof, random production and raw feedback. Independent fresh Debug is 10/10; ASan/UBSan action/discovery plus ASCII, binary and repeated-root DSR checks pass. Fuzz, targets and gates remain open |
| Focused controller configuration | current checkpoint | CLI selection can enable any subset of the five native structured producers and independently restrict canonical root-action families. Both choices are replay-fingerprinted; malformed, excluded and ambiguous action collections fail closed. Focused frontend/action/API validation passes; effective campaign evidence remains open |
| Public controller full Debug | [conformance report](benchmarks/reports/2026-10-04-public-controller-full-debug.md) | `ec7e0ac86` passes 285/285 Debug tests in 957.60 s. Terminal Direct-PHP random programs cannot acquire a bundled follow-up; append-only Tseitin registry contracts and restored Lucky/8 lifecycle tests are synchronized. Fresh immutable ASan+UBSan pin `build/pin-public-theory-ec7e0ac86-sanitize-01` passes its focused checks; effective both-policy fuzz, targets and gates remain open |
| Current N1--N4 audit | [scoped audit](benchmarks/reports/2026-10-04-current-n1-n4-audit.md) | Current code passes 46/46 focused public-action/proof/controller/wiring checks and 69/69 proof-oracle tests. N1/N2 universal coverage, current N3 gates, and N4 ledger acceptance remain pending |
| Naive target row-cover proof | [checker-censor report](benchmarks/reports/2026-10-04-row-cover-target-checker-censor.md) | Rejected: 6,783,269 additions and 187,733,915 bytes reached the 1,800-second checker cap |

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
size tracks solve regression. `oddball_26_5_ttf` emits a 5.312-GB proof and its
checker exhausts allocation rather than rejecting it; the last proof check is
still active. The raw-after-refined target overwrite is independently confirmed
and repaired locally. Full Debug job `1748974` validates the preceding X17
checkpoint while this repair receives focused and regression validation.
The unrelated interactive allocation `1747502` remains outside this workstream.

## Next action

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

Finish the seeded-Walk isolation campaign, but gate the intended DefaultPolicy
with structural seeding disabled: raw candidates neither replace Walk-refined
targets nor consume search allowance without a reviewed admission rule.
Random/future policies retain exact seeded Walk and S8 actions. Next split seed
preparation from search allowance and define admission without silently
enlarging the action dose; only then reconsider default enablement. Retain both
proof-shape diagnostics as causal evidence rather than production profiles.
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
- Accepted regression references: Rank1--50 50/50 and B28+C5 33/33, with aggregate solver ceilings 8,521.52175 and 999.87615 seconds; WV100 is 99 checked plus one checker timeout, WV2--89 is 79 checked plus ten solver timeouts.
- Current-source full Debug passes 288/288 at `7dcd36101` in 866.85 seconds; the current scoped N1--N4 audit passes, while universal N1/N2, current N3, and N4 verification remain pending. Full acceptance still requires B28+C5, Rank1--50, WV100, and WV2--89; both-policy fuzz with effective new-action reach; and all 44 independently checked targets.
- Formal T4-K/T4-VBS targets and universal N1--N4 verification remain unmet. The broader objective is to substantially exceed LymphoSAT, Satsuma+Kissat, Kissat-sc2024, and AE-Kissat-MAB combined.

## Recent changes

- The current N1--N4 audit passes 46/46 focused public-action/proof/controller
  checks and 69/69 proof-oracle tests; it keeps universal and performance
  verification explicitly pending.
- Public X17 NaryCspPrefix is append-only ID 39/StepId 28/C v8 tag 42. Exact
  nested payload copying, typed replay, Kernel session, `nary-csp` production,
  raw feedback and `NaryCsp` semantic-variable publication pass focused tests;
  DefaultPolicy and every acceptance gate remain unchanged.
- Current source repairs terminal-action random generation and synchronizes the
  append-only public registry tests; the complete Debug suite passes 288/288.
  The scripted-policy fixture retains all schedule transitions while reducing
  its externally stepped conflict path from about 75,000 to about 4,000.
- The replacement `ec7e0ac86` ASan+UBSan pin is immutable and hash-verified;
  the fixed harness self-test passes and the fuzz handoff rejects the
  superseded pre-repair pin.
- The current controller checkpoint exposes replay-fingerprinted native-producer and root-family selection for non-vacuous focused fuzz.

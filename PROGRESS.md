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
| Ramsey startup recovery | `ef4d1f1f1`, job `1749573` | DefaultPolicy's bounded original-root Lucky/22 solves fixed rank-630/WV2-155 `ramsey_4_4_19` UNSAT in 0.167 s with zero decisions/conflicts; its 735,125-byte DSR checks through the canonical pipeline in 7.968 s. The current B28+C5 and Rank reruns are active with no failure so far |
| noL hypergraph recovery | `5eb58e52c`, job `1749663` | Lucky/25 exposes an exact three-color hypergraph constructor with one 2,000,000-move owner-scale attempt per action, replay-stable distinct repeat seeds, whole-CNF SAT validation, partial phase guidance, native effort feedback and CLI telemetry. The exact guard requires all three color clauses per hyperedge; terminal publication is reserved inside the same allowance. Immutable rank-401/WV2-221 probes are active; DefaultPolicy remains disabled |
| Partial-model guidance | [terminal ablation](benchmarks/reports/2026-10-05-partial-model-guidance-ablation.md) | Walk-only repair is 21/21 independently valid with zero raw CDCL installs, 148 seed imports and 139 exports. It is 12.8% faster than the preceding run but 12.0% slower than saved `d170`. No-seed is 70.0% slower than `d170`, so removing guidance is rejected. The split-budget prototype is deferred: review found an unmetered duplicate scan and feedback inconsistent with its implicit fallback |
| `a9f1e14d4` no-seed gates | jobs `1749006`--`1749009` | All accepted: B28+C5 33/33 at 959.055 s, Rank 50/50 at 7,669.874 s, WV100 100/100 with 98 checked/two proof censors, and WV2 89/89 with 77 checked/two proof censors/ten unchanged solver censors. No wrong verdict, checker rejection, or scorer failure |
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
proof comparison meets its row bound. Current Ramsey-enabled reruns `1749575`
and `1749610` have no wrong verdict, checker rejection or scorer failure so
far. At the last complete snapshot B28+C5 had 32/33 final rows with task 32
checking, while Rank had 38/50 final, all valid and independently checked, with
the other 12 solving or checking. Replacement Debug job `1749659` is accepted:
289/289 pass in 814.11 seconds, with 242 executables and 295 frozen bindings
reverified after the run. Full oddball job `1749458` had 19/21 final
valid rows, with `26_4_ttf` solved and checking and `29_4_ttf` still solving.

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

Keep intended DefaultPolicy structural seeding disabled. Preserve the rejected
split-budget patch only as design input; redesign it only if later evidence
justifies separate allowances without duplicate unmetered work or implicit
fallback. Raw candidates never replace Walk-refined targets. Complete the
active oddball, Debug, B28+C5 and Rank campaigns, then run WV/WV2. Forced-TTF
fuzz must distinguish exact forced recognition from guarded-clause near misses.
The Ramsey slice has passed its checked rank-630/WV2-155 target probe. Finish
its active Debug/B28+C5/Rank and oddball campaigns, then run WV/WV2. The next
minimal LymphoSAT slice is generic noL hypergraph coloring through public
Lucky/25. Keep it out of DefaultPolicy until immutable Release probes recover
fixed rank-401 `noL-11-10` and WV2 rank-221/original-484 `noL-11-4` with
independently checked models. Use probe telemetry to tune the general search,
phase publication and effort allowance rather than adding instance dispatch.
Retain the seeded/no-seed campaigns as causal evidence.
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
- Current-source full Debug passes 288/288 at `7dcd36101` in 866.85 seconds. Archived job `1749241` is unscorable because its campaign omitted checker binaries and allowed source-snapshot mutation; a corrected current-source rerun is pending. The scoped N1--N4 audit passes, while universal N1/N2, current N3, and N4 verification remain pending. Full acceptance still requires both-policy fuzz with effective new-action reach and all 44 independently checked targets; `a9f1e14d4` passes all four standing regression gates.
- Formal T4-K/T4-VBS targets and universal N1--N4 verification remain unmet. The broader objective is to substantially exceed LymphoSAT, Satsuma+Kissat, Kissat-sc2024, and AE-Kissat-MAB combined.

## Recent changes

- Forced-TTF recognition now removes unnecessary row selection from balanced-
  signature Lucky and searches only signs under the existing typed limits;
  focused and cached Debug model validation pass, while Release evidence is pending.
- Ramsey startup now runs once by default at the original-input boundary and
  the normal CLI supplies anonymous structural provenance when no retained log
  is requested. Its fixed Tier-D target is checked; current gates are active.
- Public Lucky/25 adds exact noL three-color recognition and a bounded generic
  coloring search. It validates every SAT model, publishes only active signed
  phases after exhaustion, meters terminal publication, records raw feedback
  and telemetry, and remains disabled in DefaultPolicy pending target probes.
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

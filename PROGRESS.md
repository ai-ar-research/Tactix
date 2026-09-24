# Progress and resume guide

## Active ordered goal

Fix pending fuzz bugs, audit all winner/CaDiCaL/Kissat rules and N1--N4, and clean up for online learning; report to Andrew periodically. Preserve these gates: Phase 1 requires independently
checked Rank 1--50 at 50/50 below 8,115.735 solver seconds and B28+C5 at 33/33
below 952.263 solver seconds, with every case within 1,200 seconds and 32 GiB.
The general Rank 1/6 preprocessing repair must remain effective. Phase 2 then
requires independently checked WV100 100/100 under the same per-case limits,
while preserving Phase 1. Phase 3 (user, 2026-09-20): solve each of the 89 fixed
WV2-89 instances (`benchmarks/tiers/WV2-89.tsv`, plan gate T4-WV2-89) within 600
seconds and 32 GiB with checked models or proofs, by studying each row's owning
solver and integrating its technique as a shared rule or strategy; rows that
several Kissat configurations solve are a DefaultPolicy parity question, never
per-instance tuning. After the goal is verified, submit full Tier D at 5,000 s with the original winner resources (4 CPUs, 32 GiB) on `cpu-dense-hwulab-q`; Phases 1 and 2 stay preserved.

All mechanisms must live in Tactix's shared typed rule and strategy surfaces.
No sequential portfolio, external solving stage, benchmark/rank/filename
dispatch, or arbitrary benchmark tuning qualifies. Preserve N1--N4, native
DSR-to-LSR-to-Trestle verification, original-CNF SAT model checking, immutable
authorities, and Claude-owned default/random-policy fuzzing.

## Current checkpoint

AM337 is an **ungated repair checkpoint**, not a replacement for the
AM333 authority below. Automatic startup PBP now has shared Lucky 19 (parity)
and 20 (general signed count preservation); explicit clausal formats stay strict.
AM337 Release `40b0de22...`, Debug `52ee866b...`, source `26229c89` + patch
`4f2a8b3b...` are frozen in `benchmarks/runs/20260923-am337-proof-lifetime-v2`.
The AM336 Fmla diagnostic is now independently verified: UNSAT in 3.322 s,
388,840,230-byte PBP, VeriPB + CakePB checking in 526.734 s.
All three Tseitin diagnostics now pass VeriPB and CakePB on AM335 v1
(`94cd4710...`); these are not combined-candidate gates. Both N2 failures from
Claude's AM335 handoff have causal repairs and passing focused regressions:
retire non-LRAT Congruence ladders before SR; detect all-format publication
failure before CLI/API UNSAT. Combined Claude fuzz resumed on Opus; AM337 Rank
and B/C are fully checked; WV2-89 and WV100 are now terminal with coverage gaps.
The full gate set is not accepted; no promotion. Repair evidence:
`benchmarks/reports/2026-09-23-am337-proof-lifetime-checkpoint.md`.

AM338 is a **tooling-only, unaccepted checkpoint** on the same solver/checker
pins. A validated final-checker DIMACS spelling preserves the exact original
clause/literal sequence, IDs and multiplicities while removing reader-layout
limitations; zero declared variables become one unused identity. Solvers and
elaborators still read the original file. Fixed positive/negative checks and
the harness self-test pass. Frozen tooling: `...-am338-checker-spelling`, patch
`95497870...`, archive `6bc6d112...`. Evidence:
`benchmarks/reports/2026-09-23-am338-checker-spelling-checkpoint.md`.
Claude Code's Fable quota exhausted the earlier owners, including AM338 before
preparation. The user requested non-Fable operation: explicit `--model opus`
now resolves to `claude-opus-5-5` and works. Opus owners recover AM335/AM336 and
prepare combined AM339 (including AM337 repairs and AM338 tooling). Preserve
existing jobs and completed unchanged legs; no promotion and no waiver.

AM339 is an **unaccepted parser repair checkpoint**, atop AM338. The temporary
binary-liveness bitmap now falls back before advisory-header variable growth;
late/repeated headers preserve the live-pair population and scoped owner.
Parser tolerance and every rule/API boundary are unchanged. Debug/Release fixed
regressions pass; the failed Fable invocation is replaced by an Opus owner.
New immutable authority/request: `benchmarks/runs/20260923-am339-parser-liveness`
(Release `cbe99910...`, Debug `c0320acb...`, patch `8a7e7101...`). Fresh Debug
CTest ended **178/178 passed** in `build/parser-liveness-dev` (demo 3,165.24 s);
fuzz acceptance and successor gates remain due. Evidence:
`benchmarks/reports/2026-09-23-am339-parser-liveness-checkpoint.md`.
Opus recovery requests/logs are in the AM339 authority directory; its terminal
`...-am339-parser-liveness-claude-fuzz/REPORT.md` is FAILED, with bounded handoffs.
All task-owned Claude invocations must explicitly select Opus, never Fable.

AM340 is an **unaccepted tooling-only repair** on AM339's unchanged binaries.
drat-trim now receives validated clause-preserving DIMACS in runtime, fuzz,
routed elaboration and the tier runner, closing the false acceptance caused by
binary-proof comments and octal reading of decimal leading-zero literals.
Fixed tests and full harness self-test pass; Opus validation ended FAILED in
`...-am340-drat-input-claude-fuzz/REPORT.md`. Tooling authority: `...-am340-drat-input`,
patch `005bf541...`, archive `eacd7ed7...`. Evidence and gate snapshot:
`benchmarks/reports/2026-09-23-am340-drat-input-checkpoint.md`.
AM340 rejects both original false proofs on all seven surfaces, but handed off
F1 (optional conversion predicate reads original layout) and O3 (tier runner
accepts success-looking checker output without strict status). AM342 repairs
both; independent affected validation remains open.
AM335/AM336 terminal reports are both
NOT clean acceptance; their coverage gaps remain unwaived. No new solver
failure was handed off by AM336; its Propagate closure check was vacuous.

AM341 is an **unaccepted Debug-assertion repair** for AM339 seed 340726072.
Sparse collection legally drops root-fixed watches on unswept prefix clauses;
unwatch now matches that existing invariant while missing non-root entries still
assert. No rule, guard, schedule or Release behavior changes. Authority:
`benchmarks/runs/20260923-am341-unwatch/manifest.json`, Debug `722f1eb5...`,
Release byte-identical `cbe99910...`, patch `e5473fd3...`, archive `165e3608...`.
Regression 037 and six-variable boundary/death tests preserve the cause; the
owner-reported four-mode bounds defect in `test_random_online` is also repaired.
Job `1716498`: 48 independently checked random/replay runs across six formats,
Debug/Release proof-byte equality, two old-Debug abort controls. Focused tests
pass. Full Debug CTest is terminal: **178/179 passed**, `sweep_proofs` failed
every-lemma Trestle on corpus 033. Earlier live summaries misread the completion
counter as passing tests. Exact log: `...-am341-unwatch/ctest-debug-all.log`.
Original Opus owner `1934171` exited before reporting; recovery `2078249`
finished the SAME campaign. Terminal `...-am341-unwatch-claude-fuzz/REPORT.md`
is FAILED: assertion-focused coverage passes, but six native DSR proofs reject.
Lucky floors are met (Release 68, Debug 16); Propagate boundary reach remains
unproved. Evidence: `benchmarks/reports/2026-09-24-am341-unwatch-checkpoint.md`.

AM346 repairs the bounded XOR-matching failure: quantify cancelled inputs from
both gates in the shared proof ladder. No guard/schedule change. Debug/Release
pass 192 fixed cases; 72 fixed certificates and 30 exact handoff runs verify.
Authority `...-am346-xor-cancellation`, patch `4de87c93...`, archive `c6623309...`,
Debug `7e2abefb...`, Release `4e8a5306...`; terminal fuzz FAIL (F1/F1-CLI/F2/F3/F4); full CTest 179/180, with the corrected micro-proof invocation passing separately.
AM349 repairs general unary cancellation/full-cube LRAT replay; fixed tests and F1/F1-CLI verify. Authority `...-am349-unary-xor`, Debug `33ac9374...`, Release `e9389c96...`; Opus fuzz and full CTest remain live. Canonical late-LRAT H1/H4 idx17 and H4 idx142 verify on current AM355 Debug; the H4 old-pin controls reject (AM356/AM357 reports). H4 semantic idx142 also verifies under a reconstructed public action, but the old AM349 repaired pin verifies that command too, so its separately reported rejection still needs the exact failing artifact (`benchmarks/reports/2026-09-24-am357-h4-semantic-command-audit.md`); broad coverage remains due.
AM350 F2/F3 have a terminal scoped-clean Opus report: 50,500 checked structured cases and bounded H1, a pre-existing Debug native-LRAT assertion after public Propagate conflict. AM351 F4 retires discarded-only identities; its 12 DSR/LRAT proof runs and four original-CNF SAT models pass. Its full Debug CTest exposed a tautology-only online-API fixture (now corrected) and a dirty live-checker Sweep failure; Opus full-matrix fuzz is queued. AM352 fixes H1 generally at DefaultPolicy solve entry with Restart's conflict-aware root transition; the new C-API regression fails the old Debug build and passes AM352, and exact 45-action early/late LRAT H1 models check. The online-API fixture uses a live clause and passes. AM352 full Debug CTest remains live on demo3; absolute-pinned standalone micro/Sweep suites and the corrected style check pass. AM352 Opus F1 exposes a pre-existing Debug-only LRAT invariant after a completed late replay refutation; AM355's live-derivation scope passes the exact early/late public-online regression, both 432-byte CakeLPR proofs, 16 focused CTests, pinned micro/Sweep and full harness selftest. AM355 Opus remains live. AM358 removes already-assumed root-unit hints in shared Congruence LRAT replay; the causal exact-chain regression and 17 focused tests pass. The current Debug CTest excluding micro/Sweep/demo3 is terminal 178/178 PASS (Lucky 1,538.77 s); micro proofs and absolute-pinned Sweep separately pass. A live `dsr-trim` rejects the same fixed Sweep DSR proof that the frozen checker verifies. The first AM358 archive was the unmodified parent and its Opus preparation was stopped without SLURM jobs; corrected source-v2 is byte-checked and its new Opus owner is in uncounted smoke. N1/N2 and combined acceptance remain open. Evidence: `benchmarks/reports/2026-09-24-am352-conflict-entry-checkpoint.md`, `2026-09-24-am355-terminal-lrat-invariant-checkpoint.md`, and `2026-09-24-am358-root-cube-lrat-checkpoint.md`; no Tier D submitted.
AM359 closes bounded H3 as an audit-contract mismatch: parent/repaired DRAT action proofs are byte-identical and pinned `drat-trim -f -U` verifies every addition as a derivation; an invalid unit control rejects. This is not an UNSAT proof or N2 acceptance (`benchmarks/reports/2026-09-24-am359-h3-drat-derivation-audit.md`). AM360 repairs AM352 F2 generally in shared Lucky model publication: a validated model now settles both propagation cursors. The exact seven-clause public-API regression fails against the pre-fix Debug objects and passes repaired; 20 affected model/API tests, seven documentation checks, broader Debug CTest 177/177 (excluding Lucky, micro/Sweep proofs, demo3), separately frozen-checker micro/Sweep suites, and Lucky (1,606.58 s) pass. AM360 Opus has counted legs running; its terminal fuzz verdict remains due (`benchmarks/reports/2026-09-24-am360-lucky-model-status-checkpoint.md`, `2026-09-24-am361-lucky-and-ncc-terminal-audit.md`); no Tier D submitted.
AM342 is an **unaccepted tooling-only repair** of AM340 F1/O3. Conversion
eligibility uses numeric zero-terminated clauses on validated spelling; the
tier runner uses exact checker-specific status/exit contracts. Exceptional
DRAT trivial-UNSAT exit 1 now requires a real input empty clause in runtime,
routed and tier-runner callers. No solver/checker binary changes. Fixed suites
and final full selftest pass; bounded job `1716501` reproduces both old defects
and passes on the repair. Authority `...-am342-oracle-contracts`, patch
`ab061e1f...`, archive `f429b329...`. Opus terminal report in
`...-am342-oracle-contracts-claude-fuzz` is **scoped PASS**: 34,000 cases,
zero scope discrepancies, 13,377 replay matches; known certificate debt unwaived.
Legal separate comment lines inside clauses pass both maintained readers and
new positive/negative tests; no reader implementation was changed. The earlier
reported reader failure needs an exact legal bounded case if it persists.
Evidence: `benchmarks/reports/2026-09-24-am342-oracle-contracts-checkpoint.md`.

AM343 is an **unaccepted tooling-only repair** of the genuine-empty-input LRAT
gap and nested drat-trim timeout. Only exact parse-time trivial-UNSAT success
with an existing zero-byte LRAT gets an original empty-clause hint; CakeLPR
still independently verifies it. DRAT invocation layers pass the requested
allowance to `-t`, without extending their outer wall timeout. Authority:
`...-am343-empty-lrat`, patch `71a231a3...`, archive `dbf7d663...`; solver/checker
pins unchanged. Fixed suites and full selftest pass, including corrupted-hint
rejection and anonymous scratch. Original Opus owner `2249935` exited 0 without
terminal reporting. Recovery `2470638` finished the SAME campaign: terminal
report records 8,320 cases without a new solver failure, 226 timeouts, and H1
outer-wait overflow above 2,147,483.647 s; AM347 repairs it (`...-am347-checker-boundaries`, patch `039885d2...`, archive `c5c8ae78...`).
Four-row diagnostic `...-am343-routed-diagnostic`, array `1716522`, is terminal:
WV100 ranks 68/74 independently verified, solve 0.568/1.521 s, check 38.769/108.918 s;
SCPC also verified (65.392 s solve, 112.357 s check). ncc is expected UNSAT in
242.680 s but NOT-VERIFIED: pinned drat-trim exhausted 57,459 s, and the 57,600 s
checker window elapsed; all four rows have complete cleanup. The identical ncc proof also missed the AM324 native-DSR/Trestle window. This is not a full successor
gate (`benchmarks/reports/2026-09-24-am361-lucky-and-ncc-terminal-audit.md`). A bounded same-proof dsr-trim/LSR/CakeLPR diagnostic is PENDING as hwulab job `1717619` (12 CPUs/32 GiB, 57,600 s checker window); a small original-CNF witness-free control passes CakeLPR and an invalid control rejects, but no ncc result or production-route approval exists.

AM344 is an **unaccepted tooling-only repair** of malformed DSR classification
and native DSR reader layouts. Bounded varint/ASCII diagnostics become proof
routing rejections, not infrastructure exceptions; missing files stay errors.
Both dsr-trim and Trestle now receive validated clause-preserving spelling,
including the tier runner. No solver/checker binary changes. Fixed suites and
full selftest pass. Authority `...-am344-dsr-routing`, patch `ec421484...`,
archive `6ce9470d...`; terminal affected report records 8,000 cases without a scope failure, plus F1 PBP layout rejection, now repaired by AM347.
AM347 terminal owner report validates PBP spelling (641 fixed positives, 2,844 default PBPs) and 8,000 focused cases; its A1 descendant-pipe timeout is now repaired by AM353: bounded capture reaps the direct child without reopening inherited pipes. The exact .5-second handoff returns at .52 seconds versus 6.03 before; 27 bench-oracle tests, 19 binary/runtime/anonymous tests, 7 runtime integration tests and full harness selftest pass. Opus terminal report validates AM353 and closes A1 with zero failures across the affected timeout/proof surfaces; solver/checker pins are unchanged. AM354 corrects the future Tier D launcher's 8-CPU drift to the first winner run's 4 CPUs/32 GiB; a mocked-submit dry run checks the full-width hwulab script. No Tier D task is submitted. Evidence:
`benchmarks/reports/2026-09-24-am347-checker-boundaries-checkpoint.md` (AM344's earlier checkpoint report remains frozen).

AM345 is an **unaccepted tooling-only LRAT repair**: explicit checked duplicate
input restatements and ID remapping preserve original CNF multiplicities.
Fixed suites/selftest pass. Authority `...-am345-lrat-restatement`, patch
`fd429a27...`, archive `294251d9...`; Opus PID `3377189`/session `99280` owns
affected fuzz; its bounded OOM is repaired by unaccepted AM348: only Walk's reachable score prefix is stored. Authority `...-am348-walk-score-bound`, patch `bfbf2c58...`, archive `a8e33f23...`, Debug `f2d93302...`, Release `870b0314...`; the recovery Opus terminal report gives scoped PASS with zero solver/proof/model failures and independently verifies all six checker-window timeout rows. No combined acceptance follows.
AM348 fixed Debug/Release tests pass; job `1716796`: 24 verified certificates / 28 executions, 1.55--2.90 s, peak 12,808 KiB. Full CTest is terminal 180/180 PASS (3,181.33 s). Earlier sweep diagnostic `1716608` verifies 36/36; AM345 pinned full CTest is terminal 179/179 PASS (unpinned dsr-trim caused the old failure).
Evidence: `benchmarks/reports/2026-09-24-am348-walk-score-bound-checkpoint.md`; the AM345 LRAT-restatement checkpoint report remains frozen.

Solver/tooling authority: `10cd18c9` (certificate routing) on top of the
AM333 solver source (`75487222`). The AM333 pin is the last gated binary:
Release `0ec66e12...` (build job `1714363`, retained in AM333 campaign pins),
checker pins unchanged (`dsr-trim 3b5c7737...`, `dsr-compress a44102ce...`,
Trestle `19bf79f4...`), plus the frozen baseline's `drat-trim f2cb7d45...` and
`cake_lpr` for the DRAT route. Its gates
(`benchmarks/runs/20260921-am333-lymphosat-models-*`, jobs `1714420`--`1714423`)
against AM331: Rank 1--50 50/50 (+0.1%), B28+C5 33/33 (-0.4%), WV100 100/100
(+1.8%), zero conflict-count changes; WV2-89 83 solved, 78 within 600 s, no lost
row. Fuzz matrix `...-am333-lymphosat-models-claude-fuzz` (`1714364`--`1714381`):
0 failures, failure-seeds PASS, controls OK. Storage guard 700 GiB. Evidence:
`benchmarks/reports/2026-09-21-am333-lymphosat-model-constructors.md`; the
narrative this replaces is archived verbatim in
`docs/log/2026-09-22-pre-am334-progress.md`.

Certificate acceptance routes by content (plan R1/§5,
`tools/bench/proof_oracle.py`): a witnessed line selects the Satsuma+Kissat DSR
pipeline with Trestle as authority; a witness-free certificate is DRAT content
and takes `drat-trim` then `cake_lpr` under the baseline amendment's
`CML_HEAP_SIZE=24576`. No solver change. `oisc-subrv-sll-nested-8` now verifies
in 7,182 s (was a 57,600 s timeout); the three Tseitin parity refutations
(0.87--2.6 GB, 34--95 M additions) are a proof-size problem that no checker
choice fixes (`tseitin_grid_n250_m250` exceeds 30,000 s in drat-trim as DRAT).
Evidence: `benchmarks/reports/2026-09-22-am334-certificate-routing.md`.

## Phase 3 status (WV2-89)

Baseline 72 within 600 s (`benchmarks/runs/20260920-am325-wv2-89-baseline`);
now 83 solved / 78 within 600 s on the AM333 pin. Landed owner techniques:
negation fixing before FixAndCut and the generic-entry generator floor (AM329),
the large-domain symmetry program with `PureClosure` BVE scope and size-scaled
repeat (AM331), Lucky 17 signed exact cover and Lucky 18 independent set
(AM333). Remaining rows and their disposition:

| rows | disposition |
|---|---|
| `FmlaEquivChain_4_8_8` | combined AM337 PBP independently verified by both checkers; successor fuzz/full gates remain due |
| `tseitin_grid_n250_m250`, `tseitin_d3_n100000`, `tseitin_grid_n400_m400` | all three combined AM337 PBP certificates independently verified; successor fuzz/full gates remain due |
| `ncc_none_21015...` | AM337 solved in 162.181 s; drat-trim reached its internal 40,000 s default despite the 57,600 s outer window; not verified |
| `ramsey_4_4_19` | owner cut point + first-literal fix anchor solves it (290 s) but loses `ntil` SAT rows; kept declined by the user, typed for the learner (next section) |
| `hcp_CP18_18`, `6g_6color` | solved over 600 s on the large-domain program; per-rule time at matching work is the next measurement |
| `snw_16_9`, `Circuit_multiplier24`, `ntil-90d-39`, `lockchart...L19` | plain CDCL; the shuffle probe (five renamed/flipped/shuffled copies per solver) shows trajectory luck, Tactix medians not worse |
| `sted5`, `noL-11-4`, `puzzle32_sat` | owner solves by stored witness/template lookup or a one-generator decoder: declined as not a technique |

## Learned scheduling candidate: when to break symmetry

User direction: *when* (and how strongly) to apply symmetry breaking is a decision to
learn (plan §8.5), not a hand-tuned gate. Evidence,
`benchmarks/runs/20260921-wv2-anchor-cut-screen` (WV100+WV2-89, three same-window arms):
the owner's cut point with its first-literal fix anchor gains UNSAT rows
(`ramsey_4_4_19` timeout -> 290s, `bphp_p23_h22` 489s -> 1.8s) and loses or slows SAT
rows (`ntil-90d-37/40` lost, `noL-11-0/8` 2-3x slower). On `ntil` the whole difference
is one cut binary, `(-323 v 306)` on early-decided variables against `(-2 v 19)`.
Satsuma+Kissat itself loses SAT rows plain Kissat solves (`ntil-90d-37` 5,000s vs 223s,
`noL-11-*` 1.5-2.6ks vs 0.1-0.5ks). SAT/UNSAT is unknown up front, so no static rule
separates the two groups. Consequences: every symmetry choice stays a typed,
API-selectable action (`negation_fixing_first`, `cut_point`, `first_literal_fix_anchor`,
nonforcing-cut admission, structured handoff) so a learner has the action space; no
gate is tuned on the deciding rows and nothing dispatches on benchmark identity. Late
(search-time) symmetry breaking is not free under N2: an SR line must also repair every
learned clause in the checker's database.


## In flight

AM335 three-Tseitin diagnostic: initial job `1715851` solved all rows but
failed before checking because a new runner path tried to unlink a `/proc/fd`
anonymous file. Maintained runner now truncates it; a real two-checker fixed
test covers this. Retry `1715866` in `...-am335-startup-parity-retry` ended:
all three rows verified by both stages (checks 152.775/366.783/1117.364 s).
Claude's initial owner process was
interrupted before visible fuzz job submission to refresh the I/O-fix candidate;
a replacement owner was launched with `claude-fuzz-v2-request.md` in the base
campaign (log in `...-v2/logs/claude-owner.log`). Read only `OWNER_STATUS.md`,
terminal reports and the explicitly handed-off bounded reproducer, not campaign
internals. Its interim `REPORT.md` hands off `triage/reproducer/min.cnf` and
`min.trace` (random seed 335110095; 17 variables/41 clauses), plus `/dev/full`
text-writer failure. Its terminal Opus report now confirms both pre-existing
failure classes, no new solver class, and unwaived coverage gaps. The old O0 Debug CTest had
the scripted-default demo running; its other tests pass after documented reruns.
AM336 Fmla job `1715952` ended verified; full Debug-O1 CTest in `build/count-pb-dev`
ended **175/175 passed** (demo 3,686.94 s), with immutable AM333 checker pins.
Five focused tests pass, including
both-checker API/default proofs, SAT controls, strict declines, budget/interrupt
and startup-boundary controls. A separate Claude owner submitted focused
count fuzz jobs `1715955`--`1715968` from `...-am336-startup-count/claude-fuzz-request.md`; monitor only
`...-am336-startup-count-claude-fuzz/OWNER_STATUS.md` and its terminal report. It does not
replace broad combined acceptance after repairs.
The owner revised its generator after some intended firing fixtures closed by
root propagation; replacement legs are `1715973`--`1715986`. Its interim status
classifies layout/zero-header failures as checker-side, and malformed-input Debug
assertions as pre-existing. AM338 repairs the checker-reader path, not those
solver assertions. Further owner jobs include `1716000`, `1716004`, and
replacement focused legs `1716006`--`1716008`; consume owner reports only.

AM337 combined Claude owner was launched from
`...-am337-proof-lifetime-v2/claude-fuzz-request.md` (PID `2484169`, log in that
campaign's `logs/claude-owner.log`); read only
`...-am337-proof-lifetime-claude-fuzz/OWNER_STATUS.md` and terminal `REPORT.md`.
Its process has now exited 1 at the Fable usage limit while the last status
still says PREPARING. AM335/AM336 owner processes also exited 1 at that limit.
Do not infer completion or read campaign internals; preserve submitted jobs.
Six focused CTests pass, including the exact handoff in both DSR checker modes
and all 30 C API output-failure controls. Full 178-entry Debug-O1 suite in
`build/proof-repair-dev` ended: 177 passed, including the demo (3,367.30 s), and
the initial API fixture failed. That fixture separately passed after correcting
Probe closure (no solver change). V2 preserves corrected test provenance;
V1 pins remain frozen, no jobs submitted from V1. AM339's fresh full suite used
the corrected fixture and immutable checker pins and has now passed all 178 tests.

AM337 full routed gates `...-am337-proof-lifetime-{rank1-50,b28c5,wv100,wv2-89}`:
jobs `1715989`, `1715990`, `1715991`, `1715992` respectively, 272 tasks total.
Full width, no throttle/node constraint, 12 CPUs/32 GiB, 1,200-second observation
window; WV2-89 scores at 600 seconds by exact censoring. All checker windows are
57,600 seconds (old 600-second exceptions removed). Independent Release and B/C
builds match `40b0de22...`. Both DSR/Trestle and DRAT/LRAT routes have completed
valid rows. All tasks are terminal (2026-09-24): Rank 50/50 checked, 7,266.692 s;
B28+C5 33/33, 916.196 s. WV100 has 97 checked, two missing-LRAT failures
on genuine empty-clause inputs, and ncc task `1715991_89` NOT-VERIFIED after
drat-trim's internal 40,000-second timeout (40,116.714 s total check).
WV2-89 is terminal: 82 checked, 78 within 600 s, six solver timeouts and one
CakeLPR DIMACS-reader failure (SCPC-500-1). All 272 final rows report complete
certificate/scratch cleanup. All four target startup certificates verify on
the combined AM337 pin. Fuzz, full coverage and successor gates remain due;
do not transfer this evidence silently to AM339. Watchers use `tools/slurm/wait.py`.
Launch authority: `...-am337-proof-lifetime-v2/gate-launch.json`; evidence:
`benchmarks/reports/2026-09-23-am337-routed-gates-launch.md`.

Routed re-check in `benchmarks/runs/20260922-am334-certificate-routing`:
all checks ended; ncc and three Tseitin rows are NOT-VERIFIED after roughly
40 ks each. The AM334 report table records terminal verdicts as explicitly
requested. `1714785_4` was the superseded first ncc pass. The temporary
`build/am333-release-1714363` was deleted as requested (513,033,589 bytes);
byte-identical solver pins remain in the AM333 gate campaigns.

## Next steps

1. **FmlaEquivChain / Tseitin certificate (chosen by the user, 2026-09-22).**
   User approved `benchmarks/reports/2026-09-22-startup-certificate-proposal.md`:
   implement automatic startup PBP, checked by pinned VeriPB then CakePB,
   with DSR for all search paths and strict explicit format modes. The proposal
   compares this with retaining clausal DSR for every refutation and specifies
   the original-input boundary, general structural recognition, N1/API scope,
   format locking, failure behavior, and validation. Plan §5 now owns the
   approved contract. The four target diagnostics are independently verified;
   AM337 repairs both bounded AM335 failures with focused regressions. Preserve
   all active Claude owners and immutable pins. Consume their terminal reports,
   debug any bounded handoff, and complete the repaired combined candidate's
   fuzz acceptance and all four routed gates before promotion. A newly established
   solver defect stops dependent work; preserve exact failures and pins.
   Continue Claude-owned execution/reporting with explicit Opus (user requested
   no Fable). AM341 repairs the bounded unwatch assertion; preserve its failed
   full CTest evidence and Opus owner, without waiving random-policy reachability
   floors. AM342's owner reports scoped PASS, not combined-candidate acceptance.
   AM343 repairs genuine-empty-input missing LRAT and the inner DRAT timeout;
   preserve its affected owner and live four-row diagnostic. AM344 repairs
   malformed DSR routing classification and DSR reader-layout limitations;
   preserve its owner. AM346 repairs the bounded XOR-matching DSR failure;
   preserve its owner and finish the remaining five-case check, fuzz and CTest.
   AM347's terminal report validates H1/F1; AM353's terminal Opus report closes A1. AM348's affected Walk owner is terminal scoped PASS. AM351 repairs the F4 phase assertion; finish its full CTest/Opus matrix and investigate any bounded handoffs before successor gates. Legal comment lines
   already pass both readers; obtain an exact bounded failing legal layout if
   the earlier owner finding persists rather than changing parser tolerance.
   Complete these oracle obligations before successor gates; no affected
   surface or full-tier coverage requirement is waived.
   SCPC's trailing blank line requires the AM338 final-checker spelling.
   AM339's immutable `claude-fuzz-request.md` covers
   the new combined candidate, AM338 tooling and the parser repair. Earlier
   campaigns must be resumed by their owner without repeating passing unchanged
   legs; no old gate is silently transferred to the new pin.
2. **Preserve the terminal first routed gate campaign.** Jobs
   `1715989`--`1715992` have 272 final results with complete cleanup; checked
   coverage is 50/50, 33/33, 97/100 and 82/89 (78 within 600 s). These are not
   full acceptance. AM337 staging pins baseline `drat-trim`/`cake_lpr`,
   DSR and PB checkers and the maintained runner. Do not re-run unchanged AM333
   gates or substitute partial coverage for acceptance.
3. **`hcp_CP18_18` / `6g_6color` under 600 s.** Per-rule time at matching work
   counters against the owner on the large-domain program; a deterministic gap
   is repaired against the reference, never dosed.
4. **Symmetry-breaking timing as a learned decision** (previous section): the
   action space is typed; the learner is plan §8.5 work, not a hand gate.

## Recent changes

- AM334: certificate acceptance routed by content; `oisc` verified in 7,182 s; Tseitin is proof size.
- AM333: Lucky 17 (signed exact cover, sgen owner) and 18 (independent set under exact-count encoding); per-instance checker-window overrides.
- AM331: owner large-domain symmetry program (four coupled pieces), `PureClosure` scope, size-scaled repeat, generator-pass admission; AM330 gated and rejected.
- AM329: owner negation fixing before FixAndCut, generic-entry generator floor, Debug cross-check of certified X7 rows; AM327 withdrawn as unsound.
- Fixed the WV2-89 tranche (`benchmarks/tiers/WV2-89.tsv`, plan gate T4-WV2-89) as Phase 3.

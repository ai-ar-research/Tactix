# Progress and resume guide

## Active ordered goal

Complete the two acceptance phases in order. Phase 1 requires independently
checked Rank 1--50 at 50/50 below 8,115.735 solver seconds and B28+C5 at 33/33
below 952.263 solver seconds, with every case within 1,200 seconds and 32 GiB.
The general Rank 1/6 preprocessing repair must remain effective. Phase 2 then
requires independently checked WV100 100/100 under the same per-case limits,
while preserving Phase 1. Phase 3 (user, 2026-09-20): solve each of the 89 fixed
WV2-89 instances (`benchmarks/tiers/WV2-89.tsv`, plan gate T4-WV2-89) within 600
seconds and 32 GiB with checked models or proofs, by studying each row's owning
solver and integrating its technique as a shared rule or strategy; rows that
several Kissat configurations solve are a DefaultPolicy parity question, never
per-instance tuning. Phases 1 and 2 stay preserved.

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
failure before CLI/API UNSAT. Combined Claude fuzz and four full gates remain
due; no promotion. Evidence:
`benchmarks/reports/2026-09-23-am337-proof-lifetime-checkpoint.md`.

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
| `FmlaEquivChain_4_8_8` | AM336 shared count-preservation diagnostic independently verified (3.322 s solve, 526.734 s check); combined-candidate full gates due |
| `tseitin_grid_n250_m250`, `tseitin_d3_n100000`, `tseitin_grid_n400_m400` | AM335 v1 startup PB diagnostics independently verified; combined-candidate full gates due |
| `ncc_none_21015...` | solved; routed drat-trim timed out after 40,119.2 s; not verified |
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
a new owner process is resuming with `claude-fuzz-v2-request.md` in the base
campaign (log in `...-v2/logs/claude-owner.log`). Read only `OWNER_STATUS.md`,
terminal reports and the explicitly handed-off bounded reproducer, not campaign
internals. Its interim `REPORT.md` hands off `triage/reproducer/min.cnf` and
`min.trace` (random seed 335110095; 17 variables/41 clauses), plus `/dev/full`
text-writer failure. Other legs remain active. The old O0 Debug CTest still has
the scripted-default demo running; its other tests pass after documented reruns.
AM336 Fmla job `1715952` ended verified; full Debug-O1 CTest in `build/count-pb-dev`
still has its demo active (174/175 passed), with immutable AM333 checker pins.
Five focused tests pass, including
both-checker API/default proofs, SAT controls, strict declines, budget/interrupt
and startup-boundary controls. A separate Claude owner submitted focused
count fuzz jobs `1715955`--`1715968` from `...-am336-startup-count/claude-fuzz-request.md`; monitor only
`...-am336-startup-count-claude-fuzz/OWNER_STATUS.md` and its report. It does not
replace broad combined acceptance after repairs.

AM337 combined Claude owner launched from
`...-am337-proof-lifetime-v2/claude-fuzz-request.md` (PID `2484169`, log in that
campaign's `logs/claude-owner.log`); read only
`...-am337-proof-lifetime-claude-fuzz/OWNER_STATUS.md` and terminal `REPORT.md`.
Six focused CTests pass, including the exact handoff in both DSR checker modes
and all 30 C API output-failure controls. Full 178-entry Debug-O1 suite in
`build/proof-repair-dev` has the demo active; 176 other entries passed and the
initial API test fixture failed, then passed after correcting Probe closure in
the fixture (no solver change). V2 refresh preserves corrected test provenance;
V1 pins remain frozen, no jobs submitted from V1. All four full gates remain due.

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
   fuzz acceptance and all four routed gates before promotion.
2. **First gate campaign on the routed runner.** Whatever candidate lands next
   is the end-to-end test of `runtime_task.py`'s DRAT route; its staging must
   copy `pins/drat-trim` and `pins/cake_lpr` from
   `benchmarks/baselines/sc24-26-32g-5000s/pins` and the maintained runner
   (template `benchmarks/runs/20260921-am333-lymphosat-models/stage_gates.py`).
   Do not re-run the AM333 gates for it: same binary, same config, a replicate.
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

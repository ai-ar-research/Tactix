# Shared policy preparation: terminal closeout and acceptance decision

Whole-goal acceptance remains **unproven** on production source `375b621ba6677600f3e77bf8383af25d256d7b67`. All owner jobs are terminal. The owner's scoped replay-replacement acceptance is narrower than the strict original requirement: lost original proofs/models cannot be linked to independently checked artifacts. No solver, proof, model or checker failure has been observed. A user decision is required before treating the replay scope as sufficient; this checkpoint does not relax a gate.

## Delivered evidence and ownership

The same exclusive Claude owner delivered a [closeout](2026-10-03-shared-policy-terminal-closeout-review/owner-closeout.md), then a [reporting-only clarification](2026-10-03-shared-policy-terminal-closeout-review/owner-clarification.md). The latter explicitly makes strict disposition **NOT ACCEPTED** and corrects timeout attribution. Codex read only delivered reports/responses and public status/scheduler metadata. No private campaign plans, accounting, CNFs, runners or logs were inspected. No solver runs were launched for the clarification.

The original report, correcting addendum and scoped closeout remain frozen byte-for-byte. Original report hashes and complete solver/checker/runner/snapshot hashes are published in the clarification. Its full hash is `94bcc3960e06c384f6f0daef9394f6f4db639e24ddaeadaa2d8cf35ef49e0c45`; closeout is `1eb5b3aab3b40b0a27cfa6c145b74c963299a3f99ec531ed45df44f5f0e4ed71`. Earlier correction evidence remains in the [prior checkpoint](2026-10-03-shared-policy-fuzz-closeout-required.md).

## C1–C3 outcome and exact limitation

**C1:** the original 20,000-case mining scan had 19,084 completions (11,382 SAT, 7,702 UNSAT) and 916 solver timeouts. Original artifacts were not retained or checked. The explicitly permitted full-schedule graded replay used the same immutable pin, generated CNFs, seeds and invocation, with two independently checked executions per case. All 19,084 original completions now have matching replay verdicts and hit counts plus byte-identical new twin proofs/models/deterministic streams. Four unresolved 30-second cases passed both twins under the preregistered 600-second stage. Those facts establish replay behavior and retained-field correspondence; they do not independently check the lost bytes.

**C3:** all six exact graded d2 reruns are UNSAT and independently verified, with new off/on proofs byte-identical and every retained original trajectory field matching. No original proof hash, bytes or full feedback stream survives. Coarse counters, line counts, equality flags and proof sizes cannot establish equality to an independently checked artifact. The owner's clarification therefore explicitly leaves the requested original-artifact correspondence **UNRESOLVED**. Further reruns cannot recover lost historical fingerprints.

**C2:** owner terminal postflight at 08:42:32 EDT followed all eleven owner jobs and passed solver/source, both checker locations, three snapshots, leg harness and frozen script checks. New closeout jobs had in-job pre/post verification. Original d1/d2 had no in-job pin/checker/snapshot checks; that historical gap remains, and the terminal check does not create them.

| Closeout job | Role | Terminal scheduler state | Oracle result |
|---|---|---|---|
| 1747298 | Labelled smoke, 24 twin cases | COMPLETED 0:0, 1:55 | 48 checked completions; no failures |
| 1747300 | Full 20,000-case twin replay | COMPLETED 0:0, 1:10:41 | 38,196 checked completions; 1,804 solver timeouts |
| 1747314 | Four unresolved cases, twin replay | COMPLETED 0:0, 2:20 | Eight checked completions; no failures |
| 1747299 | Three configurations, off/on reruns | COMPLETED 0:0, 2:04:10 | Six checked UNSAT completions; no failures |

Across 40,062 new executions: 22,806 SAT models and 15,452 UNSAT proofs independently checked, 1,804 solver timeouts, zero checker timeouts/rejections/errors and zero solver/model/differential/crash failures. These counts use executions, not case rows. All replay, smoke and diagnosis runs earn **zero fresh-seed credit**.

## Unchanged qualifying evidence

The two fresh 100,000-seed legs remain separate: DefaultPolicy checked 57,423 SAT and 39,901 UNSAT with 2,676 solver timeouts; random checked 56,018 SAT and 38,302 UNSAT with 5,680 timeouts. Checker failure categories are zero. Retained corpus, R1–R13 effective coverage and terminal-watch controls remain applicable to the same unchanged pin/checkers/snapshot, within their reported scopes. Default corpus 022 remains crash-oracle-only and is not a checked semantic pass.

Current-pin full Debug remains [accepted exact 212/212](2026-10-03-shared-policy-full-debug-accepted.md). The [four selected default gates](2026-10-03-shared-policy-regressions-recovered.md) retain 272 rows: 123 checked SAT models, 125 checked UNSAT proofs, 14 checker censors and ten retained solver timeouts. The 1,800-second complete-pipeline deadline and baseline plus max(30%, 20 seconds) comparison remain unchanged. Censored proofs are never called verified. The original failed WV100 campaign and the single preregistered recovery are both preserved; no further retry is queued.

After the last owner job, Codex rehashed all 984 parent manifest bindings, 30 mapped sources, 12 reviewed contracts, 17 project skills, 14 frozen Debug companion files and 31 regression companion files: no mismatches. The [parent binding review](2026-10-03-shared-policy-terminal-closeout-review/parent-binding-review.json) records the scope. Source has not changed since 375b621b; this is integrity evidence, not universal execution equivalence.

## Original thirteen-requirement checkpoint

The [full audit snapshot](2026-10-03-shared-policy-terminal-closeout-review/goal-audit.json) preserves all original groups; no objective is replaced by the passing subset.

| Requirement | Evidence / current disposition |
|---|---|
| Shared stateful policy execution | 28-group/49-site map, shared mechanics source review, native support controls and current effective coverage; final campaign disposition pending |
| Default faithfulness | Preserved-source productive correspondence, current source review, work/certificate comparisons and native lifecycle tests; scoped evidence, not universal equivalence |
| Partial/full restart and state | Current controller guards/lifecycles plus both-policy targeted retention, transition and interruption coverage |
| All 20 configurations, repeated actions, fixed doses | Current complete catalog/loop plus effective configuration, repeated action, allowance and continuation coverage |
| Replaceable selection and completion/interval feedback | Current interfaces, safe publication/source review and targeted lifecycle records |
| Pure observation, inspection and deterministic replay | Current pure snapshot/replay implementations, targeted purity, replay and intended negative controls |
| Raw result/effort without reward or learning | Native currencies/progress exposed; O6 scalar, common currency, zero/terminal and attribution choices remain deferred |
| Full Debug | Accepted current-pin 212/212 with frozen registry/JUnit and pre/post bindings |
| Fresh both-policy fuzz, corpus, effective coverage and independent oracles | Fresh legs/coverage/new graded runs pass; strict lost-original requirement unproven; replay-scope decision required |
| Feedback overhead | Measured paired synthetic frontend cost and large Debug diagnosis cost disclosed |
| Four default regressions and bounded checks | Selected four gates accepted with explicit censors/timeouts and original negative evidence |
| Docs/skills and preserved authorities | Current contracts/skills match reviewed bindings; status reconciled; final acceptance remains pending |
| Local commits and Andrew TeX/PDF | Coherent source/evidence commits and reviewed checkpoints; final whole-goal deliverable awaits decision/audit |

## Cost, reach and remaining decisions

Paired recording on four small inputs adds median 17.597–63.084 ms (2.63–8.01 times). Large Debug CLI diagnoses show 68–88 times overhead and one roughly 50-GB stream. These are material limitations; no cheap-feedback or Release-performance claim is made. Four CLI feedback-variant timeouts have diagnosis evidence; two TF variants remain **undiagnosed**. All six remain non-evidence.

Condition requires an offered witness; native SR never became effective, with effects supplied by structured payloads. LocalSearchGuidance/Propagate record-level effects came from checked mining follow-ups. Factor's terminal arm is unreached and source-dispositioned. Exact coverage qualifications remain in owner reports. Broader formal N1–N4/Tier-D targets and historical checker issues remain separate obligations.

Next obtain the user's decision whether scoped replay replacement suffices for the lost original artifacts, retaining the explicit limitation. If approved, finish every original group at its required scope and publish final acceptance/handoff; if not, leave strict acceptance unproven. No scalar reward, learning, fallback, rollback, forced resumption, global loop budget, new experiment or push is introduced.

Andrew-facing checkpoint: [TeX](../../report-to-andrew/20261003T130219Z-shared-policy-terminal-closeout-review/report.tex) and [PDF](../../report-to-andrew/20261003T130219Z-shared-policy-terminal-closeout-review/report.pdf). Both are compiled and reviewed before delivery. This is a checkpoint, not whole-goal acceptance.

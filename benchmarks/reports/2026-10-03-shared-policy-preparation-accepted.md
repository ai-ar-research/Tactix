# Shared policy execution and feedback preparation accepted

Engineering preparation for online learning is **accepted** on production source `375b621ba6677600f3e77bf8383af25d256d7b67`, with the user's explicit approval of scoped graded replay replacement for lost historical mining/diagnosis artifacts. This accepts the preparation stage; no scalar reward, feedback-informed policy or learning algorithm is implemented. Broader formal benchmark and historical proof-checker obligations remain separate.

## Acceptance basis and user decision

The [original thirteen-group audit](2026-10-03-shared-policy-preparation-accepted/goal-audit.json) preserves the full objective. [Independent engineering review](2026-10-03-shared-policy-engineering-readiness-audit.md), accepted current-source validation and the reviewed final handoff establish the required engineering/validation scopes. Frozen source/evidence bindings and the [approval receipt](2026-10-03-shared-policy-preparation-accepted/replay-approval.json) accompany this report.

The user answered **“of course.”** to accepting independently checked reruns as replacements for original historical mining and diagnosis outputs that were not saved. This resolves the final acceptance decision only within that scope. The original 19,084 mining completions and six raw d2 completions were never independently checked or retained; no original content fingerprint proves equality to the reruns. They are accepted by replay replacement, **not relabeled independently verified originals**. The owner's frozen strict NOT ACCEPTED statements and original negative evidence remain unchanged. Future completed-run oracle requirements are not weakened.

## Delivered implementation

DefaultPolicy and the seeded random controller are concrete stateful policies over shared typed execution, preparation, publication, continuation, accounting and cleanup. Policies own scheduling, instances, parameters, allowances and strategy updates. Default retains native order, adaptive doses, startup/preprocessing, heuristic state and static hot dispatch. Native support and preserved-source productive/search comparisons demonstrate action/state correspondence at their declared scopes rather than inferring architecture from performance passes.

The random controller repeats eligible root actions until explicit ResumeSearch or termination. ResumeSearch chooses from all twenty VMTF/EVSIDS x adaptive-glue/reluctant x saved/target/positive/negative/focused-rotation configurations. Rephasing remains separate. Fixed configurable per-action doses, native progress/currency, overshoot, continuation and stale-request handling remain explicit. No global root-loop budget, forced resumption, automatic policy fallback or solver-state rollback is introduced.

Ordinary same-configuration restarts may retain valid search prefixes in both policies. Root inprocessing and controller-selected strategy changes require explicit full restart to level zero. Inactive branch history, shared phases, restart clocks and native continuation are retained under the interface lifecycle. Default's level-independent native Propagate/Reduce/Reorder remain search services; the controller root loop does not narrow the public API.

Read-only observation, replaceable root selection and immutable feedback expose raw formula/root/search state and results. Versioned after-action records bind identity/request, before/after observations, progress, actual effort/currency, overshoot, duration, outcome and stop reason. Search intervals are separately linked through restart, termination or interruption. Delivery waits for required native restoration/publication. Offline inspection and seeded replay compare deterministic fields, excluding timing. Recording/extra observations preserve random choices and checked deterministic effects.

## Correctness and performance evidence

Full Debug [passes exact 212/212](2026-10-03-shared-policy-full-debug-accepted.md), with zero failures/errors/skips/disabled cases and verified 984 binary/source/support bindings. Solver source has not changed since the accepted production pin.

| Fresh policy leg | Independently checked SAT | Independently checked UNSAT | Solver timeout | Checker timeout/rejection/error |
|---|---:|---:|---:|---:|
| DefaultPolicy, 100,000 fresh seeds | 57,423 | 39,901 | 2,676 | 0 |
| Random controller, 100,000 fresh seeds | 56,018 | 38,302 | 5,680 | 0 |

The [terminal owner review](2026-10-03-shared-policy-terminal-closeout-review.md) owns complete corpus, R1–R13/terminal-watch coverage, replay/purity, known-bad controls, checker routes and terminal provenance. Original corpus 022 remains crash-oracle-only for DefaultPolicy, never a checked semantic pass. All new completed closeout outputs were checked: 22,806 SAT models and 15,452 UNSAT proofs across 40,062 executions, with 1,804 solver timeouts and no checker/model/solver failures. Replay, smoke and diagnosis runs earn zero fresh credit. Terminal postflight followed all owner jobs; original d1/d2 historical in-job verification remains absent and is not invented by that check.

| Selected default set | Solver seconds | Checked SAT | Checked UNSAT | Checker censor | Retained solver timeout |
|---|---:|---:|---:|---:|---:|
| B28+C5 | 956.934 | 17 | 15 | 1 | 0 |
| Rank 1–50 | 7,580.783 | 25 | 16 | 9 | 0 |
| WV100 recovery | 2,783.045 | 33 | 65 | 2 | 0 |
| WV2–89 | 4,092.871 | 48 | 29 | 2 | 10 |

All [four selected gates pass](2026-10-03-shared-policy-regressions-recovered.md) with unchanged resource/per-row/aggregate contracts. Of 272 rows, 248 are independently checked; 14 clean checker censors and ten retained WV2 timeouts remain explicit. One complete proof pipeline has a 1,800-second deadline. All 125 completed comparable checks meet baseline plus max(30% of baseline, 20 seconds); censors remain incomplete verification. The original WV100 loss and single preregistered full recovery/confirmation remain frozen, with no further retry. Matched work/certificate evidence does not establish the original timeout's cause or universal architectural equivalence.

## Costs and limitations retained

Paired end-to-end frontend recording on four small inputs adds median 17.597–63.084 ms (2.63–8.01x). Large Debug CLI recording shows 68–88x cost and one roughly 50 GB stream. Four CLI timeout variants have diagnosis evidence; two TF variants remain undiagnosed. All six remain non-evidence. These observations establish measured overhead, not cheap feedback or Release competitiveness; optimization is not claimed.

Condition needs an offered witness; native SR was not effective and supplied payloads provide its targeted effects. LocalSearchGuidance/Propagate record-level effects came from checked mining follow-ups. Factor's terminal arm is unreached and source-dispositioned. Native durations/work were not independently remeasured for every fuzz record; source boundaries, focused meter/overflow tests and per-request/lifecycle checks provide complementary scoped evidence. Broader N1–N4/Tier-D coverage, historical checker issues and prior negative-control/reach gaps remain in PROGRESS and their primary reports.

All frozen pins/baselines/reports and unrelated work are preserved. Parent-only hygiene evidence records complete selected-run cleanup with no named certificate/input residue; no private campaign files were inspected or deleted. No new experiment, solver change, global checker change or push was introduced at final closeout. Current documentation and seventeen skills remain consistent with owning architecture/interface/gates; PROGRESS owns current status.

## Next stage

The engineering preparation goal ends here. Next discuss O6: immediate result divided by actual effort after an action, with an explicit numerator, common currency (if any), zero/terminal treatment and ResumeSearch reward boundary. Validate chosen proxies offline before feedback-informed selection. Selection remains random until that subsequent stage is authorized. The current reward choices are intentionally unresolved, not incomplete work in this goal.

Final Andrew handoff: [TeX](../../report-to-andrew/20261003T163643Z-shared-policy-preparation-accepted/report.tex) and [PDF](../../report-to-andrew/20261003T163643Z-shared-policy-preparation-accepted/report.pdf), compiled twice and reviewed before delivery. Closing commit is local only.

# AM337 — four full routed gates submitted

Checkpoint 2026-09-23, approximately 07:42 UTC. This is a launch and early
pipeline-validation record, **not** a performance or correctness acceptance.
It supplements the frozen AM337 repair checkpoint without changing it.

## Hypothesis and acceptance

The combined shared startup parity/count implementation and two certificate
repairs should retain full Rank 1--50, B28+C5 and WV100 coverage while making
the four requested WV2 certificates independently checkable. No assumption is
made that the remaining WV2 rows now meet 600 seconds. Retained gates require
50/50 below 8,115.735 solver seconds, 33/33 below 952.263 solver seconds, and
WV100 100/100, under the plan's memory and regression obligations. WV2's final
target remains every one of its 89 rows independently checked within 600 s.
No excluded row is removed from scoring and no solver identity dispatch is used.

## Immutable authority and resources

Source `68d55f4c` implements the AM337 candidate frozen as base `26229c89` plus
patch `4f2a8b3b4a6fd03e065cc45e7992faa329c2780390de5aa96a02b6acad5cc9d2`.
The independent Release and B/C builds both hash to
`40b0de228744fb878d6e9cb478babd3fc589eb944b01b52f93e59475516df40c`.
They are read-only. The latter build completed this checkpoint, with only the
existing Vivify unused-parameter and SubSolver missing-initializer warnings.
No production source changed after the repair checkpoint.

Staging: `benchmarks/runs/20260923-am337-proof-lifetime-v2/stage_gates.py`.
Launch record: that directory's `gate-launch.json` (07:38:43 UTC).
Each new campaign has its own immutable pins, hashed matrix and manifest:

| campaign suffix under `20260923-am337-proof-lifetime-` | tasks | job |
|---|---:|---:|
| rank1-50 | 50 | 1715989 |
| b28c5 | 33 | 1715990 |
| wv100 | 100 | 1715991 |
| wv2-89 | 89 | 1715992 |

All 272 tasks are eligible at full width, with no throttle or node constraint.
Each has one solver, 12 CPUs, 32 GiB on `cpu-dense-hwulab-q`. The observation
window remains 1,200 solver seconds for comparison with AM333; WV2 scoring is
exactly censored at 600 seconds, not loosened to the observation window.
The batch envelope is 18 hours and the checker window 57,600 seconds. Old
600-second checker exceptions were removed: they could not verify the now
tractable PB or routed `oisc` certificates. All rows remain scored.

Matrices and input-hash authorities are inherited byte-for-byte from AM333;
applicable tier-list hashes are checked. No unchanged reference is rerun. The
historical fixed aggregate controls remain in force; close unit-cost claims
will require matched measurements, not these cross-window observations.

## Routed certificate setup

Automatic output is routed by content. Each bundle includes pinned
`dsr-trim`, `dsr-compress`, Trestle `srcheck`, `drat-trim`, `cake_lpr`, VeriPB,
CakePB and model checkers. `drat-trim` and `cake_lpr` hashes match the frozen
baseline pins; the maintained `runtime_task.py` and `proof_oracle.py` match
AM337's frozen tooling authority. Staging invokes the runner's pin-contract
validator before freezing the manifest. The `runtime_task` CTest passes.

Early Rank results `000`--`003` demonstrate actual invocation rather than only
file presence: three witness-free DSR proofs pass drat-trim plus cake_lpr,
and one witnessed DSR proof passes dsr-trim, dsr-compress and Trestle. Each
stage returned the required successful status/exit, and the result is marked
valid with scratch cleanup. Row `005` has an independently checked SAT model.
This proves these pipeline paths run on the staged bundle, not full coverage.
The live result directories, not this snapshot count, own subsequent outcomes.

## Correctness remains pending

The AM337 Claude owner (PID 2484169) is confirmed live and reports PREPARING.
AM335 and AM336 owners remain live; no restart was inferred from observation
delay. AM336 replaced fixture-dependent legs after its initial generator
sometimes triggered parse-time unit propagation instead of Lucky 20. Its
interim status reports DIMACS-layout checker failures and malformed-input Debug
assertions under classification, with no bounded handoff yet. Codex consumes
only owner status/reports and bounded handoffs, not campaign internals. A newly
established solver defect must stop dependent work. None of these pending
campaigns or early benchmark results permits promotion.

The full Debug demo remains live. Two read-only debugger samples on PID 2202907
showed conflicts 33,732 -> 35,743, decisions 56,828 -> 59,267 and propagations
1,109,423 -> 1,171,291. Both sampled the per-API invariant check. This establishes
progress, not completion or a performance diagnosis. Older Debug demos remain
untouched; no build owned by a live suite was rebuilt.

## Hygiene and next action

All active gate jobs map to their explicit campaign and frozen launch file.
Existing fuzz remains with its owners; unrelated `t4-*` jobs belong to the sdsl
checkout and interactive jobs are untouched. No material was deleted. New
gates use anonymous node-local solver/checker scratch; terminal result cleanup
will be audited before scoring. Existing baselines, evidence and predecessor
manifests are preserved. No documentation compaction was performed.

Monitor these exact jobs with `tools/slurm/wait.py`, inspect every terminal
result and proof/model outcome, consume Claude's handoffs and repair any failure.
Then assess full regression coverage, solver/checker memory and runtime before
promotion. The hcp/6g matched-work owner comparison remains subsequent work;
no schedule or dosing change was made here.

Andrew deliverables:
`report-to-andrew/20260923T074200Z-routed-gates/report.tex` and `report.pdf`.

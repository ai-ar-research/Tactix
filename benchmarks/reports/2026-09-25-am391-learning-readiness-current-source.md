# AM391: current-source online-learning readiness audit

Date: 2026-09-25. Source: HEAD `56e9b45ff5123fe9f4a44e0f550f447506a9cd12`; `src/`, `include/`, `tests/`, and `CMakeLists.txt` are byte-identical to AM388 commit `cba6c8f4`. This is a bounded architecture and fixed-test review, not N1--N4 acceptance. Earlier architecture findings: [AM376](2026-09-24-am376-online-learning-readiness.md) and [AM385](2026-09-25-am385-learning-readiness-proof-gate.md).

## Verdict

Opt-in experiments can already change the order and dose of explicit root-boundary inprocessing actions. Production online learning over the mature default schedule and live branching/clause-deletion sub-strategies is **not ready**. Two major, staged **policy-layer** refactors are needed: (1) a shared, default-exact inprocessing slot controller; (2) owned, boundary-safe live sub-strategy handles, including a complete reduction strategy. No evidence from this review calls for a kernel or proof-rule rewrite for learning itself. The independent native-DSR H2 rejection still blocks N2 and full-gate promotion.

## Source findings

* `src/strategy/online_inprocess.hpp/.cpp`, `src/strategy/online_sequence.hpp`, and `include/tactix.h` provide ordered programs, typed X5 v3 payloads, work limits, repeats, atomic advancement, retained budgets, and reconfiguration between advances. The public interface is a workable opt-in experimental action plane.
* `DefaultInprocessScheduler::SlotEntry` and `SlotClock` in `src/policies/default_inprocess.hpp` separately own default admission, dose, delay, cursor, follow-up, and telemetry. `DefaultPolicy::solve` calls that scheduler directly. The online executor does not yet reconfigure this exact default state machine. Extract a shared policy-layer controller incrementally, with the current DefaultPolicy as an exact arm; compare action/work/proof traces and runtime before moving each slot.
* `src/policies/subpolicy.hpp` defines branching, candidate-order, arena-order, reduction-victim-count, BVE-bound, and Factor-match interfaces. `DefaultPolicy` still holds borrowed `BranchingPolicy*` and `ReductionPolicy*`; `DefaultInprocessScheduler` borrows two more. The public C API has no mid-solve sub-strategy switch, and `solve` is synchronous. A pointer reassignment during solving would lack ownership, safe-point, and state-migration semantics.
* In `src/policies/default_policy.cpp`, reduction-candidate collection and glue-tier handling precede `ReductionPolicy::victims`; replacing that interface switches only the victim count. Restart/mode/rephase clocks and inprocessing slots also remain outside the swappable interface set. A complete deletion arm must own candidate/tier/threshold choices while retaining the rule's soundness guard.
* Do not put learned admission or budgets in a rule. Add a typed strategy request/observation boundary, explicit reset/preserve/migrate rules for each arm, and replayable low-cost telemetry. Preserve N1 reachability and N2 proof/model correctness under every switch. The plan requires default dynamic overhead <=5%, L-safe within +2% Tier-C PAR-2, and learner decisions <=0.1% of ticks before L-win claims.

## Bounded current-source checks

Built 14 focused test targets in `build/am388-sweep-stream-debug`; the solver SHA-256 is `5f8172c9575f925a0590414f97770b4fc1d89ffb48f9be6334ff5e587f1ce615`. Fifteen focused public-action, rule, proof, and model CTests passed 15/15 using frozen AM369 DSR/Trestle checker pins. Five documentation/inventory CTests passed 5/5. A further `online_inprocess`, `online_all_actions`, `condition` run passed 3/3. The public API fixture reached all 29 registered entries; the catalogue accounts for 30 families because M4 is folded into M3. Option accounting covers 418 reference options without unaccounted rows. These are reachability/accounting checks, not exhaustive proof of legal-instance universality, N2 over arbitrary schedules, N3, or N4 quality. The previously run current-source proof-oracle unit suite passed 66/66 and pinned `micro_proofs` passed 1/1; source remained unchanged since those runs.

H2 remains rejected by frozen no-option `dsr-trim`; `--delete-implied-units` validates that proof but changes a negative-control outcome. The canonical checker contract was not changed; see [AM390](2026-09-25-am390-h2-checker-contract-controls.md). AM388 Sweep S1 affected fuzz remains live, so the bounded S1 repair and these fixed tests do not close the combined N1/N2 gate.

## Sequencing for Andrew

Close H2 and outstanding Claude-owned fuzz/combined correctness gates. Then implement one default-exact slot adapter and verify trace/proof/runtime parity. Add owned live branching and complete deletion arms at deterministic boundaries, with switch/replay/interruption/lifetime tests. Only then implement rewards and the plan's offline proxy, L-safe, cost, and L-win gates. The requested full 5,000-second Tier-D comparison follows the existing goal gates, not this architecture audit.

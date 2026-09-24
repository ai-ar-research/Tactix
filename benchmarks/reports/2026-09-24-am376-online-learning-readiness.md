# AM376: online-learning architecture readiness review (provisional)

Date: 2026-09-24. Scope: source-level review of the two requested learning controls, plus the additive X5 online action repaired in this checkpoint. This is not a Phase 6/7 acceptance result; AM351/363/369/372/373 fuzz and the combined N1--N4/performance gates remain open.

## Verdict

Tactix is ready for *bounded prototype experiments* in choosing the order and dose of explicit root-boundary inprocessing actions. It is **not yet production-ready for online learning that can freely change both requested dimensions while retaining DefaultPolicy fidelity**. The necessary refactoring is primarily in policy/composition and strategy ownership, not in the kernel or the proof rules. Do not replace the default scheduler wholesale: preserve its action/work/proof trace and measured runtime, and keep learning opt-in.

## 1. Inprocessing dose and order

`OnlineInvocation` / `OnlineStrategyExecutor` (`src/strategy/online_inprocess.hpp/.cpp`) already provide ordered steps, exact/complete/canonical granularity, selected batches, repeat limits, application/propagation budgets, per-rule effort, material settings, atomic follow-up propagation, state-version checks, retained budgets, and reconfiguration. `OnlineStrategySequence` (`src/strategy/online_sequence.hpp`) adds stage order and exhaustion/follow-up policy. RandomPolicy and its serializer provide a replayable source of varied programs. The public `tactix_online_configure[_v2/_v3]` and `tactix_online_advance` expose this control at the root action boundary.

The missing integration is substantial: `DefaultInprocessScheduler` (`src/policies/default_inprocess.hpp`, `src/policies/default_policy.cpp`) separately owns slot entries, admission delays, effort clocks, candidate cursors, follow-up behavior, and telemetry. The online executor does not inherit all those reference schedules or let a learner adjust their dose/order in place. A learner can already experiment with a *different* explicit schedule, but cannot yet treat the mature default schedule as a faithful, cheaply reconfigurable baseline arm. Extract a policy-layer slot-controller/adapter with typed configuration and runtime state, then route both the default and learner through the same atomic rule executor where behavior can be preserved. Require trace/proof-byte and runtime controls before retiring duplicated scheduling paths. This is a **major, staged policy refactor**; there is no evidence that a kernel/rule rewrite is needed.

The 29-action online smoke test previously counted Condition through its inert canonical placeholder. This checkpoint adds an opt-in v3 typed `(clause,cube)` payload, so that the test now exercises actual X5 progress. The request copies both vectors and stores stable external identities until each atomic application, remapping the clause after any prior variable compaction. Existing v1/v2 ABI and DefaultPolicy remain unchanged. Fixed original-CNF certificate/model routes pass, but its affected-scope Claude fuzz has not yet accepted this new path.

## 2. Switching sub-strategies

`src/policies/subpolicy.hpp` defines names and interfaces for branching, reduction-victim count, candidate order, arena order, BVE growth bound, and Factor matching. `DefaultPolicy` and `DefaultInprocessScheduler` can inject some of these through borrowed raw pointers (`default_policy.hpp`, `default_inprocess.hpp`). That is useful for offline substitution, but not a safe live-switch contract: ownership and lifetime are unspecified, no event boundary or state migration/reset is defined, the public C API has no mid-solve switch request, and `Policy::solve` runs synchronously. A raw pointer change during solving is therefore not an online-learning API.

The interfaces also cover less than the user's intended strategy space. Branching selection and Reduce victim *count* are pluggable, but search-mode/restart/rephase clocks, dynamic glue-tier and deletion-candidate collection, and the inprocessing slot schedule remain embedded in DefaultPolicy. The `SubPolicy` source itself marks restart, mode, rephase and effort scheduling as unfinished. Switching a `ReductionPolicy` alone does not mean the learner can switch a complete clause-deletion strategy.

Add owned, typed strategy handles and a deterministic safe reconfiguration point at each affected action boundary. Specify which state remains shared, resets, or migrates on an arm switch; separate reduction candidate collection/tiering from victim-count choice; then expose stable observations and low-cost action/reward records. Start with branching and reduction adapters that exactly replay the default, add restart/mode/phase and slot controls incrementally, and test interruption, replay, proof/model validity, and lifetime/migration. This is the second **major policy-layer refactor**. An online learner/reward hook is not implemented yet, as `SubPolicy` explicitly notes.

## Gates and sequence

1. Finish terminal owner fuzz and the unresolved N1/N2 and N3/N4 gates; focused green fixtures are evidence, not universal acceptance.
2. Preserve exact DefaultPolicy behavior while introducing one shared policy action/observation boundary; measure dynamic-dispatch overhead against the plan's <=5% limit and stronger retained performance gates.
3. Add owned, event-boundary sub-strategy switches, with exact default replay and reconfiguration/interruption tests.
4. Validate reward proxies offline, then apply plan §8.5: L-safe (Tier C PAR-2 within +2%), learner decision cost <=0.1% of ticks, and L-win (Tier D gains across at least two families). Do not submit the user-requested full Tier D comparison until the earlier goal gates are satisfied.

Measured current evidence: Release focused online/Condition CTests 6/6 and Debug/ASan 5/5 each after the external-identity fix; the new compacted-identity online regression passes Release, Debug and ASan (one `online_inprocess` CTest each). The final online-v3 original-CNF suite checked 20 UNSAT format/shape routes and two SAT models each in Release, Debug and ASan+UBSan, including invalid controls; harness self-test passed. An initial broad Debug/ASan CTest regex also selected `conditional_index`, whose executable was not built in those focused builds; exact reruns passed 5/5. No performance claim is made from these tests.

# Codebase review — 2026-07-15 (revisit-codebase)

- **SHA:** `4def76e` (after V1 Probe + canonical-batch fuzz exposure)
- **Method:** `revisit-codebase` skill — 5 parallel dimension auditors
  (redundancy, verbosity/dead-ends, data structures, rule-vs-policy leakage,
  untested-for-learning), evidence cited to `file:line`.
- **Headline:** no active soundness bug. The duplications are drift *risks*
  (copies are currently byte-identical and fuzz-verified), not defects. The
  most valuable findings are (a) proof/reward code that is untested at the C
  API boundary the learner uses, and (b) the inprocessing slot's order +
  no-re-propagation semantics, which become live and unprincipled the moment
  the slot enters the default schedule (V1/E1).

Routing key: **simplify** = simplify-codebase (behavior-preserving), **revamp**
= §5.4 structural, **rule-dev** = rule/test work, **fuzz** = harness,
**decide** = a design call for the user/plan.

---

## P0 — do now / fold into imminent work (cheap, high value, correctness- or learner-critical)

| # | Finding | file:line | Fix | Route | Effort |
|---|---|---|---|---|---|
| 1 | **Reason-walk (`walk_reasons` + conflict-seed) duplicated byte-for-byte across probe & vivify** — proof-emitting code (N2). Drift in one copy = a silently-wrong LRAT chain in one rule only. | probe.cpp:62 / vivify.cpp:73 | **V2 Backbone is the 3rd consumer → extract now** into a shared proof-mechanics helper (alongside `kernel_lrat_chain`), consumed by probe/vivify/backbone. Share only the LRAT antecedent mechanics; keep each rule's guard/instances/apply local (not "rules calling rules"). | rule-dev (do in V2) | medium |
| 22 | **4 inprocessing reward stats never read through `tactix_stat`** (SUBSUMPTIONS/STRENGTHENED/VIVIFIED/PROBE_FAILED). The learner reads rewards via the C API; a miswired `case` in api.cpp would silently corrupt rewards and no test catches it. | api.cpp:182–194 | Add a stat-mapping test: drive a state that moves exactly one inprocessing counter and assert `tactix_stat(s,id)==N`; ideally pin every id→counter wire. | rule-dev | small |
| 20 | **Default inprocessing-slot builder is dead code** (`SLOT_IN_DEFAULT=false`), never built or asserted. It *is* the learner's initial arm-space enumeration; a wrong/incomplete default slot is uncatchable today. | default_policy.cpp:198 | Extract `build_default_slot()` (callable independent of `SLOT_IN_DEFAULT`) + unit test asserting the composed slot = [Probe,Strengthen,SubsumeForward,Vivify] with only SubsumeForward disabled. | rule-dev | small |
| 16 | **Default slot order = alphabetical registry sort** (Probe,Strengthen,SubsumeForward,Vivify) — an unprincipled schedule decided by an accident of rule naming; ORDER is a §5-rev-17 policy decision / §5.6 arm. Latent now, live+arbitrary at slot activation. | default_policy.cpp:198 | Define the default slot order explicitly as a named list (via `tactix_rule_id`), decoupled from the name-sort. Literature order ≈ subsume→strengthen→vivify→probe. | decide→rule-dev | small |
| 6 | **Dead include** `subsume_forward.hpp` in default_policy.cpp (no symbol used; slot is registry-driven). | default_policy.cpp:11 | Delete the line. | simplify | small |
| 7 | **Write-only stats** `strengthen_checked` / `vivify_checked` — incremented, never read (unlike `forward_checked`, which is printed+tested). Ostensible reward substrate that surfaces to nothing. | kernel.hpp:57,63 | Add them to the C2/C3 `main.cpp` stat lines (make them real signal) or delete both + their increments. Do not leave write-only. | simplify | small |
| 8 | **Stale slot comments** in default_policy.hpp claim "C1 SubsumeForward for now; C2/C3/E1 join it" — contradicts the landed Strengthen-enabled/SubsumeForward-disabled composition. | default_policy.hpp:12–14,62–64 | Update to the landed state. | simplify | small |

## P1 — address at inprocessing-slot activation (V1/E1) or when the subsystem is next touched

| # | Finding | file:line | Note | Route | Effort |
|---|---|---|---|---|---|
| 15 | **Slot runs no re-propagation between entries** → the first rule to derive a unit breaks quiescence, so every *later* slot rule's guard fails and it is skipped that pass (combined with #16, "Probe first = Probe wins"). Not permanent starvation (later rules run next pass after the loop re-propagates), but the slot's real semantics are "run until one breaks quiescence," not "run all enabled in order" — and this coupling is invisible in the learner's `{rule,enabled}` arm vector. | default_policy.cpp:298 | Decide at activation: re-propagate (`propagate_apply`) between entries so every enabled rule gets a quiescent root each pass, **or** document + expose the order-gates-reachability semantics so the learner can model it. | decide | medium |
| 21 | **Slot posability under-tested** — the only posed-slot test toggles `enabled` on 2 of 4 rules and never verifies *order* (one entry enabled, so swaps are invisible); Probe/Vivify are never placed in a posed slot; demo3 poses only a single Strengthen. Reorder + enable/disable across all 4 rules is the learner's inprocessing arm space. | test_strengthen.cpp:167 | Add a 4-entry non-registry-order slot on a formula where each enabled rule does observable work; assert each enabled rule's counter advanced, disabled didn't, and order is observable. | rule-dev | medium |
| 23 | **No determinism/exact-replay test; no `--trace`** — §5.6 requires deterministic replay for credit assignment; "deterministic repeats" so far were ad-hoc. | (missing) | Commit a test: run random_solve + default_solve twice on a battery at fixed seed; assert byte-identical proof output **and** identical 20-stat vector. | rule-dev | medium |
| 9 | **`vivify_one` allocates+zeroes `seen(nvars)` per candidate, even proofs-off** — O(nvars·ncandidates)/round of pure waste in the N3/proof-off config; `seen` is read only under `if(lrat)`. | vivify.cpp:201 | Hoist one reusable buffer, clear via a touched-var list, gate on `lrat`. **FIRM → folded into #1's shared reason-walk helper in V2** (helper owns the reusable `seen`); bundle probe's #12. | V2 (step 1) | small |
| 17 | **Lucky's 6-probe cascade order + first-success lives in the rule**, while the analogous rephase rotation lives in the policy. The 6 probes are distinct strategies (a WHICH/ORDER choice = §5.6 arm), not ranked candidates. Mitigant: all 6 are N1-exposed as instances, so an external policy *can* drive them; only the *default* order is rule-owned. | lucky.cpp:148 | **Disposition: deferred to step 4 (policy-scheduling consolidation at slot activation)** — move the cascade into DefaultPolicy alongside #15/#16. Not blocking (N1-exposed). | policy (step 4) | medium |
| 24 | **Native-LRAT nightly asymmetry** (native DRAT at 100k, native LRAT only ~400 quick). | run.sh:19 | **Already addressed this session:** fuzz skill updated to run both formats at nightly scale + promote the random-policy+LRAT per-commit leg; both-format nightly to be launched at session end. | fuzz (done) | small |

## P2 — record as watch items (need a profile before acting; escalate via §5.4)

| # | Finding | file:line | Note |
|---|---|---|---|
| 10 | `binary_ids` as `unordered_map` on the LRAT reason-resolution hot path (binaries dominate reason chains). Already a recorded watch item; cheaper structure = a permanent per-literal `(other,id)` index or id-in-watch-entry. | kernel.hpp:165 | Keep as watch item until a profile attributes a major share to `kernel_binary_id`. |
| 11 | Occurrence lists as `vector<vector<Ref>>(2*nvars)` per round in Strengthen/SubsumeForward — 2·nvars small heaps, poor locality. Already a recorded watch item. | strengthen.cpp:367 | Flat-arena head/offset (like WatchLists) when next touched. |
| 2 | Arena-Ref instance-payload validation loop duplicated 3× (untrusted-payload safety, learner arm surface). | strengthen.cpp:461 +2 | Optional: a `kernel_is_clause_start()` predicate; low risk, do opportunistically. |
| 3 | DIMACS-int payload codec duplicated decide↔probe (replay-determinism contract). | probe.cpp:196 | Optional: shared `encode/decode_lit_payload` next to internalize/externalize; fold into #1's extraction. |
| 4 | LRAT chain flatten-and-emit implemented 3× (kernel/vivify/probe). | probe.cpp:95 | Fold into #1: generalize `kernel_lrat_chain` to take entries + optional closing id. |
| 25 | No per-rule cost counters for reward attribution; only global TICKS; bracket-diff protocol untested. | tactix.h:46 | When the learner's reward attribution is designed: a TICKS-bracket test around one Inprocess apply, or per-rule tick stats. |
| 5,12,13,14,18,19 | Candidate-enumeration scaffold dup (defensible); per-call `todo`/`chain` allocs (fold into #9/#4); arena/watch/header layout (designed kissat shape, **no action**); candidate processing order (**verified legitimate instance-ranking, no action**); Strengthen flush-then-round sequence (acceptable; promote flush to its own rule if E1 adds a 2nd consumer). | — | Mostly no-action or bundle-with-above. |

---

## Recommended sequencing

1. **In V2 Backbone (next):** extract the shared reason-walk helper (#1) — V2 is
   the third consumer, so the rule-of-three triggers cleanly. The shared helper
   takes a **reusable `seen` buffer + touched-list** and is **gated on `lrat`**,
   which folds in #3 (payload codec), #4/#13 (LRAT emit), **#9 (vivify per-
   candidate `seen` alloc) and #12 (probe per-failed-probe `seen`)** — the
   per-iteration alloc + proofs-off waste disappear as a property of the shared
   helper rather than a separate edit. (#9 promoted from "if bundled" to firm.)
2. **A short "learner-readiness tests" commit** (independent of V2): #22 stat
   mapping, #20 slot builder + test, #21 slot posability, #23 determinism —
   these close the exact "untested components crucial for the learned policy"
   gaps and are all small/medium. Owner: rule-dev.
3. **A `simplify:` commit:** #6 dead include, #7 write-only stats, #8 stale
   comments. Behavior-preserving.
4. **At slot activation (V1/E1) — the policy-scheduling consolidation:** resolve
   #15 (re-propagation semantics), #16 (explicit slot order), **and #17 (move
   Lucky's cascade order into the policy, matching the rephase rotation model)**
   together — all three are "the policy, not a rule, owns the scheduling ORDER,"
   and they jointly define the learner's arm-space contract. Deferred to
   activation because the re-propagation call needs the measurement that only
   exists once the slot is live; not blocking (every choice is already
   N1-exposed as an instance, so an external policy can drive it today).
5. **Watch items (P2):** leave until a profile (§5.4) or a second consumer
   forces the issue.

No item blocks V2 or the T3 attack; #22/#20/#21/#23 block *trusting* the phase-6
learner and should land before LearnedPolicy, not necessarily before V2.

## Deferred action items (backlog) — nothing dropped

Every finding not fixed by steps 1–3 above, with its trigger and owner. These
are tracked in PROGRESS.md so they survive this report.

| # | Item | Trigger to act | Owner | Blocking? |
|---|---|---|---|---|
| 15 | Slot runs no re-propagation between entries (order gates reachability) | slot activation (V1/E1) | decide→policy | learner arm-space contract; not correctness |
| 16 | Default slot order = alphabetical registry sort → make explicit | slot activation (V1/E1) | decide→policy | same |
| 17 | Lucky cascade order lives in the rule (vs rephase rotation in policy) | slot activation (policy consolidation) | policy | no (N1-exposed) |
| 19 | Strengthen's flush-then-round hardcoded; promote flush to its own rule | E1/BVE lands (2nd flush consumer) | rule-dev | no |
| 10 | `binary_ids` `unordered_map`: `emplace` on the **live CDCL hot path** (every learned binary, proof-only value word) + `kernel_binary_id` lookup on the LRAT reason path. `.size()` is load-bearing (want_inprocess / irredundant count / stat 16), so it cannot compile out — a leaner id table carried only in LRAT mode is the shape. *(sharpened by the N1/N2/N3 audit: insert is hotter than the review's LRAT-only framing.)* | a profile attributes a major share to the map | revamp (§5.4) | no |
| 11 | Occ lists `vector<vector<Ref>>(2*nvars)` per round | Strengthen/SubsumeForward next touched, or a profile | revamp/simplify | no |
| 25 | No per-rule cost counters for reward attribution (only global TICKS) | phase-6 reward-attribution design | rule-dev | blocks trusting per-slot rewards |
| 2 | Arena-Ref instance-payload validation loop duplicated 3× | opportunistic (or fold into #1's helper) | simplify | no (safety hardening) |

**Intentional no-action (recorded so silence isn't mistaken for an oversight):**
#5 candidate-enumeration scaffold dup (defensible §5.3 self-containment),
#14 arena/watch/header layout (designed kissat shape, no profile signal),
#18 candidate processing order (verified legitimate instance-ranking).

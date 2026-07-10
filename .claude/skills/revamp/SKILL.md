---
name: revamp
description: Evaluate and execute a significant restructuring of solver internals (data structures, kernel primitives, rule contracts, module boundaries) instead of incremental patching. Use at every phase gate, when profiles show a structural bottleneck, when a rule cannot meet its guard/instances/apply contract without contortions, or when invariant complexity keeps growing around the same design decision.
---

# Revamping solver internals

Authoritative spec: DEVELOPMENT_PLAN.md §5.4. Incremental patches around a
wrong structure compound into the real cost; this skill exists so large
rewrites are a normal, disciplined tool — not a failure mode. Precedents
that were worth it: per-literal `std::vector` watches → flat watch arena
(2.4× single-instance speedup); scheduling budgets fused into guards →
guard/advised split (restored N1's meaning).

## When to evaluate (triggers, not permission)

- **Every phase gate (§9)**: before ticking the gate, ask explicitly — is
  any part of the codebase fighting us? Record the answer in the gate
  report even when it is "no".
- A profile attributes a large share of runtime to *structure* (layout,
  indirection, allocation), not algorithm.
- A new rule or policy needs contortions, special cases, or kernel-reaching
  to satisfy its contract (§5.3) — fix the architecture, not the rule.
- The invariant checker / fuzzing keeps finding holes clustered around the
  same design decision.
- The plan's trade-off ordering (§1.1) is being bent to avoid a rewrite.

## Anti-triggers — do NOT revamp for these

- Style, taste, or "modern C++" appeal with no measured or contractual win.
- A hot spot that a local algorithmic fix addresses (try that first).
- Right before a gate measurement (land it after, on a green base).

## Procedure

1. **Evidence first.** A profile, a fuzz-failure cluster, or a concrete
   contract violation — written down. No evidence, no revamp.
2. **Mini-design (in the commit message or a docs/ note):** what changes,
   what stays, which invariants are preserved, how N1–N3 survive, and what
   the measurable success criterion is (e.g. "props/sec ≥ 2×", "rule X
   registers with zero kernel edits").
3. **Green base:** start from a commit where units, both fuzz modes
   (default + `--random-policy`), and the tier runs are green.
4. **Implement in one coherent unit.** Don't interleave the revamp with
   feature work; keep transliteration attributions (kissat is MIT).
5. **Revalidate the full ladder:** unit tests (debug = ASan/UBSan +
   invariant checker), `run.sh --quick` under BOTH policies, compliance
   check, Tier A, and — if the revamp claimed performance — Tier B with
   repeated runs and per-instance medians (§8.1 caveat).
6. **Compare against the success criterion from step 2.** If it missed,
   revert is a first-class outcome; keep the evidence and the design note.
7. Update PROGRESS.md (log entry with before/after numbers) in the same
   commit; plan changes go through revise-plan.

## Relation to the non-negotiables

A revamp may change any representation or interface EXCEPT: the public
rule-applicability surface incl. branching (N1), proof emission on every
path (N2), and the DefaultPolicy performance trajectory (N3). If the best
design seems to require weakening one of these, that is a plan revision
discussion (revise-plan), not a unilateral rewrite.

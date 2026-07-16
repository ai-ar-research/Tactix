---
name: revisit-codebase
description: Step back and audit the whole codebase for room-for-improvement across five axes — redundancy/duplication, verbosity/dead-ends, sub-optimal data structures, rule-vs-policy leakage (application-order decisions left in rules that belong to the policy), and untested/unfuzzed components crucial for the learned policy (§5.6). Produces a prioritized findings report and routes each fix to the right skill; it does not auto-fix. Use periodically, after a rule family or subsystem lands, before a phase gate, or whenever you want an architecture + quality + learning-readiness review that goes beyond behavior-preserving hygiene.
---

# Revisit the codebase — a multi-axis review

Authoritative specs: DEVELOPMENT_PLAN.md §5 (architecture, esp. §5.1 API,
§5.3 rule contract, §5.4 revamp, §5.5 hygiene, §5.6 the learned-policy
end-game) and §8.2 (fuzzing). This skill **reviews and prioritizes**; it is
deliberately broader than its neighbors and hands the actual change to them:

| This finds… | Route the fix to… |
|---|---|
| behavior-preserving clutter (dead code, drift, duplication) | **simplify-codebase** (§5.5) |
| wrong data structure / module boundary / structural rewrite | **revamp** (§5.4) |
| a rule/policy family that needs (re)implementing | **rule-dev** (§5.3) |
| a coverage gap (untested/unfuzzed) | **fuzz** (§8.2) / **rule-dev** DoD |
| a scope/plan question the review surfaces | **revise-plan** (§11) |

Unlike simplify-codebase (hygiene only, behavior-preserving) and the built-in
`/simplify` (current diff only), this sweep also judges *performance-adjacent*
data-structure choices, *architecture* (rule-vs-policy separation), and
*test/fuzz coverage for the learned policy* — the axes routine hygiene misses.
Output is a **prioritized list**, not a set of edits.

## The five axes

Sweep `src/`, `include/`, `tests/`, `tools/`, and the plan/docs. For each
axis collect concrete findings with `file:line` evidence — prefer a handful
of substantive findings over a long list of nits.

1. **Redundancy / duplication.** Logic repeated where one helper would be
   safer — especially *proof-emitting* code, where drift between copies is a
   soundness risk. Apply the rule of three: two copies can be a deliberate
   §5.3 self-containment (record why at each site); a third consumer usually
   warrants extraction. Never deduplicate across the kernel/rule/policy
   boundary or make a rule call another rule.

2. **Verbosity / dead-ends.** Unused stats/fields/`RuleVTable` slots,
   unreachable branches, `ApplyResult` variants never produced, speculative
   generality with no consumer, over-long functions that hide intent, stale
   deviation/TODO comments that no longer match the code, vestigial includes.
   Exception by design (N1): the rule-introspection/application API surface
   is *not* "unused" where DefaultPolicy never exercises it.

3. **Sub-optimal data structures.** Judge with evidence, and separate the
   measured hot path from cold introspection/rule paths. Typical suspects:
   maps on a proof/analysis hot path, per-round / per-candidate `std::vector`
   allocations in rule apply paths, interleaved scans, header/word layouts.
   For each: state the cost, *when* it bites (hot path? per-round? fuzz-only?),
   the concrete better structure, and whether it's worth doing now or belongs
   as a recorded watch item pending a profile (escalate real ones to revamp).

4. **Rule-vs-policy leakage** *(highest priority — the differentiator).*
   Per §5 rev 17, every WHEN / WHICH / ORDER-of-application decision belongs
   to the Policy; rules encode only the SOUNDNESS guard + instance ranking
   (H). Hunt for scheduling decisions baked into a rule or the kernel:
   fixed cascades/rotations/intervals/thresholds inside a rule, a rule that
   assumes it runs after another, a canonical round whose internal ordering
   is really a schedule, a slot/list order that is effectively fixed rather
   than posable. For each: quote the code, name the §5 principle it tensions
   with, and decide *real leak* (move to policy / make posable) vs *defensible
   instance-ranking* (and say why). Crucially, check the **learned policy can
   actually control it through the API** (N1) — a decision the external/RL
   policy cannot reach is a leak even if it "looks" rule-local.

5. **Untested / unfuzzed components crucial for the learned policy (§5.6).**
   The phase-6 learner poses slots, reorders/enables/disables inprocessing
   entries, reads stats as rewards, and needs deterministic replay. Verify
   what it depends on is actually exercised: every rule's instance surface
   fuzzed at **both** single-instance **and** canonical/batch granularity
   (proof-emitting batches are the classic gap); all `tactix_stat` reward ids
   correct and tested; slot posability (reorder/disable) tested; exact
   `--trace`/seed determinism holds for random *and* would hold for a learned
   policy; proof coverage spans native DRAT **and** native LRAT under the
   random policy at meaningful scale (the two exercise different emission
   code and catch different bugs — neither subsumes the other); the pull-based
   external-policy API (examples/) reaches the arms the learner needs. For
   each gap: what breaks for the learner if it stays untested, and the
   concrete test/fuzz addition that closes it.

## Method

- For a full review, fan out one auditor per axis (the Workflow tool is a
  good fit — parallel readers returning structured findings, then a
  synthesis pass) so coverage is exhaustive and the axes don't blur. For a
  quick check of one subsystem, a single focused pass is fine.
- Ground every finding in the code (`file:line`) and in the §5 principle or
  the learner requirement it serves. Rank by severity × blast-radius:
  soundness/learner-blocking first, then architecture, then perf-adjacent,
  then hygiene.

## Output

Write the prioritized findings to `benchmarks/reports/YYYY-MM-DD-review-*.md`
(or `docs/reviews/`): the axis, severity, `file:line`, what/why, the bounded
recommendation, effort, and **which skill owns the fix**. This report is the
deliverable — it is what a later session (or the phase gate) acts on. Do not
mix fixes into the review; open the routed skill for those, in its own commit.

## Cadence

- After a rule family or subsystem lands (catch leakage/coverage while the
  design is fresh).
- At every phase gate, alongside the §5.4 revamp and §5.5 hygiene evaluations
  (this review frames both).
- When review keeps tripping over the same structural friction, or before
  trusting a new policy/learner path.

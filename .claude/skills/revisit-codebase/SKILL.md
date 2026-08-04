---
name: revisit-codebase
description: Step back and review the whole codebase for room-for-improvement across five axes — redundancy/duplication, verbosity/dead-ends, sub-optimal data structures, rule-vs-policy leakage (application-order decisions left in rules that belong to the policy), and untested/unfuzzed components crucial for the learned policy (§5.6) — THEN apply the behavior-preserving fixes it finds (dead code, drift, duplication, stale TODOs; §5.5 hygiene) in dedicated commits, and route the rest: structural rewrites to revamp (§5.4), coverage gaps to fuzz/rule-dev, scope questions to revise-plan. Use periodically, after a rule family or subsystem lands, or at a phase gate. (Merges the former simplify-codebase.)
---

# Revisit the codebase — review, clean, route

Authoritative specs: DEVELOPMENT_PLAN.md §5 (architecture — §5.1 API, §5.3 rule
contract, §5.4 revamp, §5.5 hygiene, §5.6 the learned-policy end-game) and §8.2
(fuzzing). This skill **reviews** the codebase across the axes below, **applies**
the behavior-preserving subset of what it finds (the §5.5 hygiene set), and
**routes** everything structural to the neighbor that owns it. The one hard line:
behavior-preserving cleanup lands here; anything that *changes structure or
performance* is a **revamp** (§5.4), a different discipline with a full
revalidation bar — never smuggle one into a hygiene commit.

| This finds… | Handled by… |
|---|---|
| behavior-preserving clutter (dead code, drift, duplication, stale TODOs) | **apply here** (§5.5 hygiene) |
| wrong data structure / module boundary / structural rewrite | route to **revamp** (§5.4) |
| a rule/policy family that needs (re)implementing | route to **rule-dev** (§5.3) |
| a coverage gap (untested/unfuzzed) | route to **fuzz** (§8.2) / **rule-dev** DoD |
| a scope/plan question the review surfaces | route to **revise-plan** (§11) |

Broader than the built-in `/simplify` (which reviews the current *diff* only):
this sweep also judges performance-adjacent data-structure choices, architecture
(rule-vs-policy separation), and test/fuzz coverage for the learned policy.

## The five axes

Sweep `src/`, `include/`, `tests/`, `tools/`, and the plan/docs. For each axis
collect concrete findings with `file:line` evidence — prefer a handful of
substantive findings over a long list of nits.

1. **Redundancy / duplication.** Logic repeated where one helper would be safer —
   especially *proof-emitting* code, where drift between copies is a soundness
   risk. Rule of three: two copies can be deliberate §5.3 self-containment (record
   why at each site); a third consumer usually warrants extraction. Never
   deduplicate across the kernel/rule/policy boundary or make a rule call a rule.

2. **Verbosity / dead-ends.** Unused stats/fields/`RuleVTable` slots, unreachable
   branches, `ApplyResult` variants never produced, speculative generality with no
   consumer, over-long functions that hide intent, stale deviation/TODO comments,
   vestigial includes. **Exception by design (N1): the rule-introspection/
   application API surface is *not* "unused" where DefaultPolicy never exercises
   it — deleting it is an N1 violation, not a simplification.**

3. **Sub-optimal data structures.** Judge with evidence, separating the measured
   hot path from cold introspection/rule paths. Suspects: maps on a proof/analysis
   hot path, per-round/per-candidate `std::vector` allocations in apply paths,
   interleaved scans, header/word layouts. For each: the cost, *when* it bites
   (hot path? per-round? fuzz-only?), the concrete better structure, and whether
   it's worth doing now or a recorded watch item pending a profile (real ones
   escalate to revamp).

4. **Rule-vs-policy leakage** *(highest priority — the differentiator).* Per §5
   rev 17, every WHEN/WHICH/ORDER-of-application decision belongs to the Policy;
   rules encode only the SOUNDNESS guard + instance ranking (H). Hunt for
   scheduling baked into a rule or the kernel: fixed cascades/rotations/intervals/
   thresholds inside a rule, a rule assuming it runs after another, a canonical
   round whose internal ordering is really a schedule, a slot/list order that is
   effectively fixed rather than posable. For each: quote the code, name the §5
   principle it tensions with, decide *real leak* (move to policy / make posable)
   vs *defensible instance-ranking* (say why). Check the **learned policy can
   actually control it through the API** (N1) — a decision the external/RL policy
   cannot reach is a leak even if it "looks" rule-local.

5. **Untested / unfuzzed components crucial for the learned policy (§5.6).** The
   phase-6 learner poses slots, reorders/enables/disables inprocessing entries,
   reads stats as rewards, needs deterministic replay. Verify: every rule's
   instance surface fuzzed at **both** single-instance **and** canonical/batch
   granularity (proof-emitting batches are the classic gap); all `tactix_stat`
   reward ids correct and tested; slot posability (reorder/disable) tested; exact
   `--trace`/seed determinism for random *and* would-be learned policies; proof
   coverage spans native DRAT **and** native LRAT under the random policy at
   meaningful scale (they exercise different emission code — neither subsumes the
   other); the pull-based external-policy API (examples/) reaches the arms the
   learner needs. For each gap: what breaks for the learner if it stays untested,
   and the concrete test/fuzz addition that closes it.

## Method

- For a full review, fan out one auditor per axis (the Workflow tool fits —
  parallel readers returning structured findings, then a synthesis pass) so
  coverage is exhaustive and the axes don't blur. For a quick check of one
  subsystem, a single focused pass is fine.
- Ground every finding in the code (`file:line`) and the §5 principle or learner
  requirement it serves. Rank by severity × blast-radius: soundness/learner-
  blocking first, then architecture, then perf-adjacent, then hygiene.

## Judging & applying

For each finding, exactly one outcome, written down:

- **Fix now (behavior-preserving).** The §5.5 hygiene set — dead code, drift,
  duplication, speculative generality, stale TODOs. Apply in a **dedicated commit**
  (prefix `simplify:`), never mixed with feature work or with the review report.
  *Behavior-preserving means verified:* debug build (ASan/UBSan) unit suites +
  quick fuzz including ≥1 random-policy round + (when solver code was touched)
  Tier A and the determinism check (identical stats on repeated runs). Performance
  must not regress — spot-check the Tier B reference instance if the change is
  near the hot path. **N1–N4 are never traded for simplicity; when in doubt
  whether an API or proof path is load-bearing, it is.**
- **Keep, with reason.** It looks removable but isn't (N1 surface, a proof
  obligation, performance-load-bearing per a profile, a future phase item already
  in the plan). Record the reason where the next reader looks — usually a comment
  at the site.
- **Escalate.** It's structural (wrong data structure / module boundary) → route
  to **revamp** (§5.4); a rule/policy to (re)implement → **rule-dev**; a coverage
  gap → **fuzz** / rule-dev DoD; a scope/plan question → **revise-plan**. Don't
  half-fix a structural problem here.

## Output & cadence

- The findings **not fixed inline** go to a prioritized report
  `benchmarks/reports/YYYY-MM-DD-review-*.md`: axis, severity, `file:line`,
  what/why, the bounded recommendation, effort, and which skill owns the fix. That
  report is what a later session or the phase gate acts on. Applied fixes land in
  their own `simplify:` commits.
- **At every phase gate,** record this review in the gate report
  (`benchmarks/reports/`) alongside the §5.4 revamp evaluation — including when the
  verdict is "clean, nothing to do." Also run after a rule family/subsystem lands
  (catch leakage/coverage while the design is fresh), and when review keeps
  tripping over the same structural friction.

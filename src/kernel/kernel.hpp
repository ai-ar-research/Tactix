// Kernel state (plan §5): owns representation only — no policy knowledge,
// no rule logic. One flat struct, index-based arrays, C-style discipline.
// Layout decisions transliterated from kissat/src/internal.h (MIT).
#pragma once

#include "kernel/clause.hpp"
#include "kernel/types.hpp"
#include "kernel/watch.hpp"

#include <csignal>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tactix {

class Proof;
struct FactorMatchPolicy;
// Search-guidance state `H` (VMTF queue, EVSIDS heap, phases). Held OPAQUELY by
// Kernel (only a pointer below) so kernel.hpp never depends on its layout; the
// full struct + operations live in policies/heuristics.hpp.
struct Heuristics;

// per-variable assignment metadata (kissat/src/assign.h)
struct Assigned {
  uint32_t level;
  uint32_t trail; // position on the trail
  uint32_t reason; // Ref, or other literal if binary, or sentinels
  bool binary;
  // Reason jumping (kissat `jumpreasons`, assign.c/fastassign.h): the binary
  // reason above is a VIRTUAL resolvent — the clause (lit ∨ reason) is not in
  // the database. `Kernel::jumped_over` names the literal resolved away, and
  // the two real antecedents are (lit ∨ jumped_over) and the reason of
  // ¬jumped_over. Never set while `Kernel::jump_reasons` is false, and never
  // at level 0. Fits in the padding after `binary` — no layout cost.
  bool jumped;
};

// VMTF queue links (kissat/src/queue.h)
struct Link {
  uint32_t prev, next, stamp;
};

struct Stats {
  uint64_t decisions = 0;
  uint64_t conflicts = 0;
  uint64_t propagations = 0;
  uint64_t ticks = 0; // propagation memory-traffic proxy (see propagate.cpp)
  // Share of `propagations` spent inside simplification phases — POLICY-
  // written bookkeeping (accumulated at phase exit, mirroring the policy's
  // inprocess-ticks split), never read by kernel or guards. Splitting the
  // headline props/s into search vs inprocessing is TB5's first step: the
  // 1.39× throughput gap vs kissat is only comparable on the search share
  // (kissat keeps search_ticks and probing_ticks apart).
  uint64_t inprocess_propagations = 0;
  // --- wall-clock accounting (DIAGNOSTIC ONLY) ------------------------------
  // Same contract as inprocess_propagations above: written by the rule-dispatch
  // wrapper and the policy, NEVER read by the kernel, a guard, or any decision
  // — so no trajectory can depend on a clock and runs stay bit-reproducible.
  // Only the statically-dispatched search path (Propagate/Decide/Analyze) is
  // deliberately NOT timed: it runs per-conflict, so a clock call there would
  // be the very hot-path cost N3 cannot afford. Search time is therefore
  // DERIVED as solve_nanos − (everything measured here), which is exactly the
  // quantity worth reporting anyway (see `c timing` in main.cpp).
  static constexpr uint32_t MAX_TIMED_RULES = 64;
  uint64_t rule_nanos[MAX_TIMED_RULES] = {}; // per registry rule id
  uint64_t rule_calls[MAX_TIMED_RULES] = {};
  uint64_t inprocess_nanos = 0; // whole slot, incl. scheduling not in a rule
  uint64_t solve_nanos = 0;     // wall time of solve(), set by the frontend
  uint64_t switched = 0; // mode switches (S4)
  uint64_t target_saved = 0; // target-phase snapshots
  uint64_t best_saved = 0;   // best-phase snapshots
  uint64_t rephased = 0;     // Rephase applications (S3)
  uint64_t reordered = 0;    // Reorder applications (S7, policy op)
  uint64_t lucky_probes = 0; // Lucky probes run (S5)
  // S6 Walk (policy op): applications, literal flips, and the rule's own
  // effort currency (occurrence-list entries scanned = kissat walk_steps).
  uint64_t walks = 0;
  uint64_t walk_flips = 0;
  uint64_t walk_steps = 0;
  uint64_t walk_improved = 0; // walks that lowered the unsatisfied count
  // unsatisfied-clause count the last walk reached (UINT64_MAX = never walked).
  // Read by the policy to decide whether to KEEP a walk's phases: the op owns
  // the mechanism, the policy owns the accept/reject decision.
  uint64_t walk_minimum = UINT64_MAX;
  uint64_t promoted = 0;     // glue promotions on use (kissat deduce.c)
  uint64_t learned = 0;
  uint64_t learned_units = 0;
  // Clause-QUALITY instrumentation: the size/glue distribution of what we
  // learn, which our conflict-count gap vs kissat is a symptom of. Counters
  // only — no trajectory effect.
  uint64_t learned_lits = 0;      // sum of FINAL learned clause sizes
  uint64_t learned_glue_sum = 0;  // sum of learned glues
  uint64_t minimized = 0; // literals removed by clause minimization
  uint64_t shrunken = 0;  // literals removed by all-UIP block shrinking
  uint64_t otfs_strengthened = 0; // reasons strengthened on the fly
  uint64_t otfs_subsumed = 0;     // conflicts subsumed by the resolvent
  // Eager subsumption of recently learned clauses (SP-EAGERSUB-1). The
  // pre-registered REACHABILITY readout: `tried` counts register entries the
  // containment test actually ran on (kissat `eagertried`), `subsumed` the
  // deletions (kissat `eagerly_subsumed`), `reason_skipped` the subsumed
  // candidates KEPT because they are still a trail reason — our one deviation
  // from kissat, so its cost has to be visible rather than inferred.
  uint64_t eager_tried = 0;
  uint64_t eager_subsumed = 0;
  uint64_t eager_reason_skipped = 0;
  // glue histogram of clauses USED in conflict analysis, per search mode
  // (kissat statistics.h used[2].glue): the input to Reduce's dynamic tier
  // quantiles. Always maintained; read only when the arm is on.
  static constexpr uint32_t MAX_GLUE_USED = 127;
  uint64_t used_glue[2][MAX_GLUE_USED + 1] = {};
  uint64_t retiered = 0;           // dynamic tier recomputations
  // Blast radius of the dynamic tier2 for a `likely_to_be_kept`-style
  // consumer (cadical internal.hpp:1059, whose first disjunct is
  // `glue <= tier2[false]`), summed over reductions: how many redundant
  // clauses the LIVE tier admits versus the static option default 6. The two
  // differing is the precondition for wiring any further consumer of the
  // tiers to be worth anything (SP-TIER-2); counted inside the aging walk
  // reduce_apply_refs already does, so it costs nothing.
  uint64_t tier2_band_dynamic = 0;
  uint64_t tier2_band_static = 0;
  uint64_t reason_bumped = 0;      // extra vars marked by reason-side bumping
  uint64_t reason_bump_rounds = 0; // conflicts the reason-side pass ran on
  uint64_t reason_bump_aborted = 0; // rounds rolled back at the mark limit
  uint64_t api_decisions = 0; // externally chosen branches (N1)
  // Input clauses whose FILE form repeated a literal and that we therefore had
  // to restate under a fresh id (frontend/parse.cpp). Printed so "did this
  // instance even reach the input-normalization path?" is answerable from a
  // run — the Sweep N2 post-mortem's standing lesson is that a green leg over
  // zero reachable cases proves nothing.
  uint64_t input_restated = 0;
  // Decisions that landed on a Factor EXTENSION variable (vars >=
  // first_extension_var). Diagnostic: cadical's `factorunbump` (kissat's
  // adjust_scores_and_phases_of_fresh_varaibles) is worth an order of
  // magnitude on some instances, so "are we branching on extension vars?"
  // must be answerable from a normal run rather than inferred.
  uint64_t decisions_extension = 0;
  // Decisions landing on a variable Substitute has already rewritten away.
  // Such a variable occurs in NO clause, so deciding it propagates nothing and
  // only deepens the trail — pure wasted depth.
  uint64_t decisions_substituted = 0;
  // How many rule applications ABORTED on the interrupt (plan §6).
  // Reachability evidence: a latency claim is unfalsifiable without it.
  mutable uint64_t rule_interrupted = 0;
  // Times kernel_terminating() ITSELF reported stop under the stress knob.
  // Counted at the source, so it is independent of whether any particular
  // abort site remembers to increment — the reachability instrument.
  mutable uint64_t stop_fuzz_fires = 0;
  uint64_t bumps = 0;                // bump events (both modes, both channels)
  // WHICH CHANNEL re-promotes extension variables. We branch on extension vars
  // far more often than cadical does while burying them at birth exactly as it
  // does, so something keeps lifting ours back out. The two candidates are
  // separable at the bump site: conflict-side (SEEN — the var was
  // resolution-visited or sits in the learned clause) vs reason-side ONLY
  // (REASON_BUMP without SEEN — the bumpreasons extension). A high reason-side
  // share is a MECHANISM gap with a targeted fix; a high conflict-side share
  // means our factorings are structurally more active and the lever is WHICH
  // factorings we choose.
  uint64_t bumps_extension = 0;      // ...of which landed on an extension var
  uint64_t bumps_ext_reason_only = 0; // ...of those, reason-side ONLY
  uint64_t restarts = 0;
  uint64_t restarts_reused = 0; // restarts that kept a trail prefix
  uint64_t reductions = 0;
  uint64_t deleted = 0; // learned clauses deleted by Reduce
  uint64_t binaries_deleted = 0; // binary clauses deleted (kernel_delete_binary)
  uint64_t forward_subsumptions = 0; // SubsumeForward rounds
  uint64_t forward_checked = 0;  // SubsumeForward candidates checked
  // PARTNER VISITS — cadical's `stats.subchecks` (subsume.cpp:86), bumped once
  // per candidate partner actually compared, which is the unit its budget is
  // denominated in (`check_limit = stats.subchecks + delta`,
  // subsume.cpp:362/459). `*_checked` counts CLAUSES, i.e. 1 for a clause whose
  // occurrence lists hold 10,000 partners and 1 for a clause holding 2 — it
  // under-prices exactly the expensive candidates, so a round metered on
  // `*_checked` can own most of a solve without exceeding its budget.
  uint64_t forward_visits = 0;
  uint64_t forward_subsumed = 0; // clauses deleted by SubsumeForward
  uint64_t strengthen_rounds = 0;  // Strengthen rounds
  uint64_t strengthen_checked = 0; // Strengthen candidate checks
  uint64_t strengthen_visits = 0;  // partner visits; see forward_visits
  uint64_t strengthened = 0;       // clauses strengthened (add+delete)
  uint64_t strengthen_subsumed = 0; // deletions by Strengthen's fused round
  // Rounds cut short by `Kernel::work_budget`. REACHABILITY WITNESS, not
  // decoration: a `break` that never executes makes every green fuzz leg
  // vacuous for the mid-round budget. Require these to be NONZERO in any run
  // that claims to have exercised it.
  uint64_t budget_stops_c1 = 0; // SubsumeForward rounds stopped mid-round
  uint64_t budget_stops_c2 = 0; // Strengthen rounds stopped mid-round
  uint64_t flushed = 0; // root-satisfied deletions + root-falsified
                        // literal strips by Strengthen's flush pre-pass
  uint64_t vivify_rounds = 0;  // Vivify rounds
  uint64_t vivify_checked = 0; // Vivify candidates probed
  uint64_t vivified = 0;       // clauses distilled by Vivify (add+delete)
  uint64_t vivify_deleted = 0; // root-satisfied deletions by Vivify
  uint64_t probe_rounds = 0;   // Probe rounds
  uint64_t probed = 0;         // probe assumptions made
  uint64_t probe_failed = 0;   // failed literals -> root units derived
  uint64_t backbone_rounds = 0;  // Backbone rounds
  uint64_t backbone_probed = 0;  // binary probe assumptions made
  uint64_t backbone_failed = 0;  // failed literals -> root units derived
  uint64_t transitive_rounds = 0;  // TransitiveReduce rounds
  uint64_t transitive_checked = 0; // candidate edges walked
  uint64_t bce_rounds = 0;         // BCE rounds that deleted something
  uint64_t bce_deleted = 0;        // blocked clauses deleted (witnessed)
  // Per-rule PRIVATE tick meters; the policy scopes `tick_sink` to one of
  // these around that rule's apply so its budget cannot be spent by another.
  uint64_t probe_ticks = 0;        // Probe's own propagation ticks
  uint64_t backbone_ticks = 0;     // Backbone's own
  uint64_t vivify_ticks = 0;       // Vivify's own
  uint64_t instantiate_ticks = 0;  // Instantiate's own
  uint64_t bce_ticks = 0;          // occurrences + pairs scanned — BCE's budget
                                   // currency for a policy
  uint64_t instantiate_rounds = 0;  // Instantiate rounds that strengthened
  uint64_t instantiate_checked = 0; // (clause, literal) pairs probed
  uint64_t instantiated = 0;        // literals removed by Instantiate

  // Sweep (src/rules/sweep/sweep.hpp). Also the reachability witness for its
  // fuzz legs — a zero `sweep_environments` means the leg never entered the
  // detector, so a green result there proves nothing.
  uint64_t sweep_rounds = 0;       // detection rounds run
  uint64_t sweep_environments = 0; // environments built AND handed to the solver
  uint64_t sweep_env_clauses = 0;  // clauses encoded into them
  uint64_t sweep_solves = 0;       // sub-solver solve() calls
  uint64_t sweep_candidates = 0;   // candidate-pair directions actually solved
  uint64_t sweep_equivalences = 0; // pairs PROVED equivalent
  uint64_t sweep_binaries = 0;     // equivalence binaries ADDED with a proof
  uint64_t sweep_not_rup = 0;      // equivalences declined (not RUP)
  uint64_t sweep_general = 0;      // pairs emitted via the general proof path
  // random decisions (kissat randec): H-only, no proof delta.
  uint64_t random_sequences = 0;   // random-decision sequences started
  uint64_t random_decisions = 0;   // decisions actually taken at random
  uint64_t sweep_known = 0;        // pairs the formula already states as binaries
  uint64_t sweep_env_unsat = 0;    // environments refuted outright (F is UNSAT)
  uint64_t sweep_budget = 0;       // solves abandoned on the tick budget
  uint64_t sweep_ticks = 0;        // Sweep's effort currency (scan+sub-solver)

  uint64_t ternary_rounds = 0;     // HyperTernaryRes rounds that added clauses
  uint64_t ternary_resolvents = 0; // ternary resolvents added (redundant)
  uint64_t ternary_binaries = 0;   // binary resolvents added (permanent)
  uint64_t ternary_ticks = 0;      // occurrence entries + pairs examined — its
                                   // budget currency for a policy
  uint64_t transitive_ticks = 0;   // cache lines scanned by the walks — the
                                   // policy's budget currency (kissat
                                   // transitive_ticks; same 16-words/line
                                   // unit as stats.ticks)
  uint64_t transitive_deleted = 0; // transitively redundant binaries deleted
  uint64_t transitive_failed = 0;  // sources abandoned to Probe as failed (V4)
  uint64_t substitute_rounds = 0;  // Substitute rounds — detection passes
  // Variables actually SUBSTITUTED away, summed over applications: the
  // consumer side of `congruence_merges`.
  uint64_t substitute_vars = 0;
  // Discriminates "Substitute never DETECTED an equivalence" from "Substitute
  // detected some and the caller DROPPED them". Non-trivial SCCs (size > 1)
  // returned by substitute_find_sccs, and the literals inside them, summed
  // over rounds. If both are 0 while congruence_merges is large, Congruence's
  // equivalence binaries are not in the binary implication graph by the time
  // Substitute walks it.
  uint64_t substitute_sccs = 0;
  uint64_t substitute_scc_lits = 0;
  // Rounds abandoned WHOLESALE by do_substitution's step-(A) hazard pre-check
  // (a binary pair with >=2 live copies cannot be deleted by name, so the round
  // is skipped rather than half-applied). The check is all-or-nothing while its
  // "rare" assumption is PER LITERAL, so the probability a round survives falls
  // as the marked set grows — exactly backwards, since the instances with the
  // most equivalences to exploit mark the most literals. Hence the per-SCC
  // form below.
  uint64_t substitute_hazard_skips = 0;
  // Rounds that survived the hazard check with SOME classes poisoned, and the
  // literals those classes cost — the per-SCC skip's yield.
  uint64_t substitute_hazard_partial = 0;
  uint64_t substitute_hazard_lits = 0;
  // Reachability: deactivations actually performed, and how many of those
  // variables were later RE-INSERTED into a ranking structure (which would
  // silently undo the deactivation). Counted at the site itself, because
  // without a counter AT the site a reading of 0 cannot tell "never fired"
  // from "never measured".
  uint64_t deactivations = 0;
  uint64_t reactivations = 0;
  // Conflicts resolved by REUSING the conflict as the driving clause (kissat
  // one_literal_on_conflict_level) — no clause learned, no proof line.
  uint64_t conflict_reused = 0;
  // Binary reasons replaced by their one-step resolvent (kissat
  // `jumped_reasons`). Reachability evidence for the `jump_reasons` arm: 0
  // here means the arm changed nothing and measured nothing.
  uint64_t jumped_reasons = 0;
  // Of those, the ones that needed the watch-repair primitive first.
  // Reachability evidence: 0 here means the repair path never executed, so any
  // A/B on it measured nothing.
  uint64_t conflict_watch_repaired = 0;
  // Chronological backtracking. `chronological` counts chrono targets taken;
  // `backjump_stale` counts pending clauses discarded because the asserting
  // literal was still assigned after the backtrack, and SHOULD BE 0 — a
  // nonzero value means conflicts are being dropped and re-derived, which is
  // invisible in wall-clock results.
  uint64_t chronological = 0;
  uint64_t backjump_stale = 0;
  // Inprocessing PHASES run by the policy (each = a root descent + one dose
  // per enabled technique). Counted in the kernel so it survives into the
  // stats block: repeated during-search BVE's cost is search-TRAJECTORY
  // damage rather than per-phase work, so whether that cost tracks the phase
  // COUNT is unanswerable without this counter. Policies that run more than one
  // inprocessing clock sum them here; the per-clock split is theirs to report
  // (DefaultPolicy's `c slot-clock` line).
  uint64_t inprocess_phases = 0;
  uint64_t congruence_rounds = 0;  // Congruence rounds — gate-closure passes
  // Merges dropped because no checkable LRAT chain could be built for them
  // (a bridging BIG path blocked by a root-assigned literal). Sound — the
  // inference is lost, never emitted unjustified. Expected 0; a nonzero count is
  // a real signal, not noise.
  uint64_t congruence_unjustified = 0;
  // Root units derived from degenerate gates (Congruence's unit channel): an
  // XOR whose representative inputs all cancel forces its output.
  uint64_t congruence_units = 0;
  // Extraction split by gate kind, mirroring kissat's congruent_gates_ands /
  // _ites / _xors so the two are directly comparable. Needed because we extract
  // orders of magnitude more gates than kissat and convert a tiny fraction of
  // them into merges — WHICH extractor produces the excess is the open
  // question, and the aggregate `congruence_gates` cannot say.
  mutable uint64_t congruence_gates_and = 0;
  mutable uint64_t congruence_gates_xor = 0;
  mutable uint64_t congruence_gates_ite = 0;
  uint64_t congruence_gates = 0;   // gates EXTRACTED, over all applications
  uint64_t congruence_merges = 0;  // equivalences MERGED, over all applications
  uint64_t factorings = 0;         // Factor/BVA applications — grids replaced
  // Multi-literal remainders OFFERED by the widened grid scope. Zero here with
  // factor_size >= 3 means the widened branch never fired at all, which is a
  // different defect from "fired but never won the delta contest".
  uint64_t factor_large_remainders = 0;
  // Demotion reachability, counted AT THE SITE (heuristics.cpp) — the only
  // place that can tell "never fired" from "never measured". `factor_demotions`
  // counts calls that actually moved something (once per Factor APPLICATION,
  // never per factoring); `factor_demoted_vars` counts extension variables
  // pushed to the queue front. An A/B arm of this knob whose
  // factor_demoted_vars is 0 was NOT tested.
  uint64_t factor_demotions = 0;
  uint64_t factor_demoted_vars = 0;
  // Factor's unit of WORK and hence its budget currency (kissat
  // statistics.factor_ticks, cache-line granularity): 1 + one per 128 bytes
  // touched, charged per watch-list walk and per heap (re-)score. The
  // factoring count cannot express the seed-scan cost, which dominates.
  uint64_t factor_ticks = 0;
  uint64_t bve_rounds = 0;         // BVE canonical-apply calls (a policy that
                                   // doses per instance never bumps this)
  uint64_t bve_eliminated = 0;     // variables eliminated by BVE resolution
  // resolvents attempted (kissat statistics.eliminate_resolutions): BVE's unit
  // of WORK, and so the currency an effort budget meters it in — one variable
  // costs occ(x)·occ(¬x) attempts, which the elimination count cannot express
  uint64_t bve_resolutions = 0;
  // NEW-ELIMINATION-MATERIAL counter: one per `kernel_mark_bve_retry` EVENT.
  // It counts material rather than traffic because the mark itself does — it is
  // raised only by irredundant clause add / delete / garbage-mark and by root
  // fixings, never by learned-clause churn (redundant clauses are excluded at
  // every call site).
  //
  // DEVIATION from the references, and the reason for it: kissat
  // (`variables_eliminate`, inline.h:44-49) and cadical (`stats.mark.elim`,
  // internal.hpp:1117) count the FLAG FLIP, not the event, because their
  // elimination round clears every scheduled variable's flag and so re-arms the
  // flip. Our `bve_retry` bit is cleared ONLY by a BVE attempt, and BVE's
  // during-search admission can decline every dose on a formula — after which
  // no bit is ever cleared, no flip ever happens, and a flip-counting watermark
  // wedges the elimination clock shut for the rest of the solve. Measured, not
  // theorised: it took `sted5` from 274 k to 1.15 M conflicts. Counting events
  // implements the same predicate ("did anything an eliminator could act on
  // change since the last phase?") against the bit lifecycle we actually have.
  uint64_t bve_marks = 0;
  // --- touched-epoch substrate (Kernel::touched) --------------------------
  // EVENTS, for the same reason bve_marks counts events (see above): the
  // per-variable stamp is monotone and is never cleared, so there is no flip
  // to count. `touch_marks` growing over a long solve is the positive evidence
  // that the substrate's producers are still firing.
  uint64_t touch_marks = 0;
  // C1/C2 round-candidate suppression by the touched-epoch filter: candidates
  // the round SKIPPED because nothing in them changed since its watermark, vs
  // the `forward_checked`/`strengthen_checked` it did examine. Both must stay
  // non-zero on a long solve — all-skipped would mean the filter had wedged
  // shut, all-checked that it never fires.
  uint64_t forward_mark_skipped = 0;
  uint64_t strengthen_mark_skipped = 0;
  // Rounds that ran to natural exhaustion and therefore ADVANCED the
  // watermark, vs rounds cut short (budget / derived unit) that left it where
  // it was. Zero advances with non-zero rounds means the filter is inert.
  uint64_t forward_mark_advances = 0;
  uint64_t strengthen_mark_advances = 0;
  // SCHED-F2 resume: candidates a cut round never reached, whose variables it
  // therefore re-armed so the next round starts at the tail instead of
  // replaying the head (the SCHED-B3 starvation shape, in C1/C2's currency).
  // Non-zero is the evidence that the resume path is live at all; it can only
  // be non-zero when a round was cut, so read it against `budgetstop`.
  uint64_t forward_rearmed = 0;
  uint64_t strengthen_rearmed = 0;
  // SCHED-F3's consumers, same reading contract: `skipped` is what the
  // suppressor bought, and it must be read against the rule's own attempt
  // counter — both non-zero, or the filter is inert (all-checked) or wedged
  // (all-skipped).
  uint64_t bce_mark_skipped = 0, bce_rearmed = 0;
  uint64_t ternary_mark_skipped = 0, ternary_rearmed = 0;
  uint64_t instantiate_mark_skipped = 0, instantiate_rearmed = 0;
  // SCHED-F4: Factor SEEDS suppressed / re-armed. Read `skipped` against
  // `factor_ticks` — the seed pass charges ticks proportional to each
  // literal's watch list, so this is a saving out of the rule's own budget.
  uint64_t factor_mark_skipped = 0, factor_rearmed = 0;
  // SCHED-F2's completeness wire: outer elimination rounds held open because
  // SubsumeForward/Strengthen had not finished. The pre-registered reading for
  // that arm, and it is PREDICTED ZERO under the shipped DefaultPolicy — see
  // the wire's comment in default_policy.cpp for why. A non-zero here means
  // some budget started binding and the wire went live.
  uint64_t subsume_holds = 0;
  // SCHED-F6 · the REDUNDANT half of the touch substrate (SP-TIER-2's residue).
  // `dynamic` counts redundant creations that stamped under the live
  // `kernel_likely_kept` predicate; `static6` counts the ones that would have
  // under a frozen tier2 of 6 and no survivor envelope. **The pair is the
  // pre-registered anti-vacuity reading**: `dynamic > 0` proves the stamps fire
  // at all, and `dynamic != static6` proves they MOVE with `k.tier2`, which a
  // single count cannot. Same idiom as `tier2_band_dynamic/static`.
  uint64_t touch_redundant_dynamic = 0;
  uint64_t touch_redundant_static6 = 0;
  // syntactic gates found per type, one per elimination ATTEMPT that had one
  // (kissat statistics.{equivalences,ands,if_then_else}_extracted / cadical
  // stats.elim{equivs,ands,ites,xors}). The ITE/XOR pair are also the
  // pre-registered reachability readout for ledger row SP-BVE-GATE-1.
  uint64_t bve_gates_equiv = 0;
  uint64_t bve_gates_and_or = 0;
  uint64_t bve_gates_ite = 0;
  uint64_t bve_gates_xor = 0;
  // resolvent PAIRS the gate reduction removed from the growth bound's input:
  // occ⁺·occ⁻ minus the gate×antecedent pairs actually charged. This is the
  // acceptance lever itself — the bound `resolvents ≤ occ⁺+occ⁻+slack` is
  // applied to a set this much smaller.
  uint64_t bve_gate_pairs_saved = 0;
};

// learned clause handed from Analyze to Backjump (τ changes are Backjump's)
struct PendingLearned {
  bool valid = false;
  std::vector<Lit> lits; // lits[0] = asserting literal
  Ref ref = INVALID_REF; // arena ref for size >= 3, INVALID otherwise
  uint32_t glue = 0;
  uint32_t jump_level = 0;
  uint64_t id = 0; // proof clause id (LRAT)
};

// Shared literal→large-clause occurrence index for a simplification PHASE
// (inprocessing-revamp increment 2, kissat dense.c analogue). Built once per
// phase (kernel_dense_connect), maintained incrementally — new irredundant
// large clauses push on add, deleted clauses are lazy (garbage bit, skipped on
// read), and kernel_collect_garbage rebuilds after it relocates Refs. Only
// irredundant large clauses (binaries are O(1)-enumerable from watches; kissat
// connects irredundant only). OPTIONAL acceleration: `Kernel::dense` is null in
// search and outside a phase, and every consumer (BVE/Subsume/Strengthen) keeps
// its round-local arena-scan path for that case (N1 — rules stay independently
// applicable via the public API).
struct OccIndex {
  std::vector<std::vector<Ref>> large; // by literal; may hold stale garbage Refs
};

// Undo log of the SEARCH BOOKKEEPING a probing propagation touches (kissat
// keeps probing propagation in its own engine, src/proprobe.c; ours is the same
// engine with this log switched on — see kernel_propagate_probing in
// kernel.cpp). Unit propagation only stays complete under the two-watched-
// literal scheme if a falsified watch is MOVED, so a probing propagation cannot
// simply refrain from moving it; instead it records every incidental update it
// makes — watch-list contents and per-clause literal permutation + `searched`
// scan hint — and puts them back once the speculative trail is unwound. The
// derived literals and the conflict are bit-identical to kernel_propagate's;
// only the bookkeeping is rolled back.
struct ProbingLog {
  bool recording = false;
  // kernel state the log is relative to: the restore only fires when the caller
  // has unwound the trail back to exactly this (see kernel_probing_end)
  uint32_t base_trail = 0, base_head = 0, base_level = 0;
  // watch-list snapshots, one per literal, taken at FIRST TOUCH (first scan
  // mutation or first delayed push, whichever comes first)
  struct ListSnap {
    Lit lit;
    uint32_t off, size; // offset into `words`, and the list's original length
  };
  std::vector<ListSnap> lists;
  std::vector<uint32_t> words; // flat backing store for the snapshots above
  std::vector<uint32_t> stamp; // by lit: epoch this literal was snapshotted in
  uint32_t epoch = 0;          // bumped per scope; 0 is "never snapshotted"
  // per-relocation clause records, replayed in REVERSE order
  struct ClauseSnap {
    Ref ref;
    uint32_t rpos;             // index the replacement literal came from (>= 2)
    Lit lit0, lit1;            // the watched pair before the relocation
    uint32_t searched;
  };
  std::vector<ClauseSnap> clauses;
};

// Retirement cause for a variable rewritten out of the formula. See
// Kernel::var_status. cadical carries the same concept as Flags::status
// {ACTIVE, FIXED, ELIMINATED, SUBSTITUTED, PURE}; we keep only what a rule of
// ours actually produces, and route it through advice rather than the decision
// loop (design §3).
enum class VarStatus : uint8_t { Active = 0, Substituted, Eliminated };

struct Kernel {
  uint32_t nvars = 0;
  // Variables at index >= first_extension_var are NON-ORIGINAL "extension"
  // variables introduced mid-solve (E2 Factor / BVA). They are assigned by search
  // like any other variable but are PROJECTED OUT of the reported model — the
  // competition `v` line ranges over original variables only (a model restricted
  // to the originals satisfies the original formula; see docs/design/
  // e2-factor-design.md §3). UINT32_MAX = none added yet, so all vars are original.
  uint32_t first_extension_var = UINT32_MAX;
  bool inconsistent = false; // root-level conflict happened

  std::vector<int8_t> values;     // by lit: 1 true, -1 false, 0 unassigned
  std::vector<Assigned> assigned; // by var
  // By var, MEANINGFUL ONLY while `assigned[v].jumped`: the literal the
  // virtual binary reason resolved away. Written on the jump, never cleared —
  // the `jumped` flag is what makes an entry live, and it is rewritten by
  // every assignment of v.
  std::vector<Lit> jumped_over;

  std::vector<Lit> trail;
  uint32_t propagate_head = 0;
  // off-fixpoint-decide taint (D-LEVEL-1). A Decide taken while the trail is
  // NOT at a propagation fixpoint (propagate_head < |τ|) lets a not-yet-run
  // watch scan observe an "ahead" assignment and treat a clause as satisfied
  // by it; if a later backtrack unassigns that ahead literal, the stale scan
  // is never redone and the clause can be left unit with no watch firing
  // (escaped unit). The cure is to re-propagate the WHOLE trail after any
  // trail-shrink that follows such a decision, so every stale scan is redone
  // against the post-shrink assignment: kernel_backtrack resets propagate_head
  // to 0 (not the level boundary) whenever this flag is set. DefaultPolicy
  // always decides at a fixpoint, so it NEVER sets this — its backtracks keep
  // the boundary reset and it stays bit-exact. Set by decide_apply; once set it
  // stays set for the rest of the solve (re-propagating a short N1/test trail
  // from 0 is cheap, and clearing it soundly is subtle — a fixpoint reached
  // while an ahead literal is still true does NOT make the trail safe).
  bool trail_tainted = false;
  // trail height at the start of each level; frames[0] = 0 exists from
  // construction — the `frames.size() == level + 1` invariant must hold
  // even for a kernel that never saw a variable (empty formula; corpus
  // regression 001-empty-formula-frames)
  std::vector<uint32_t> frames = {0};
  uint32_t level = 0;
  uint32_t unassigned = 0;
  // R-c2: of those, how many are NOT retired. Lets the retirement skip answer
  // "is any non-retired candidate left?" in O(1) instead of walking the queue.
  // Without it the skip is quadratic in the TAIL of every descent: once all
  // non-retired variables are assigned, each remaining decision walked ~195 k
  // links before falling back (measured — veer_axi 204 s -> 600 s TIMEOUT).
  // Kept exact by the same three sites that move `unassigned`, plus retirement.
  // A drift here is a PERFORMANCE bug only, never a soundness one: too low
  // falls back early (decides a retired variable while real ones remain), too
  // high scans and finds nothing, then falls through to the same answer.
  uint32_t unassigned_active = 0;

  Arena arena;
  WatchLists watches;
  // shared large-clause occurrence index, non-null only during a simplification
  // phase (increment 2). Owned by the caller (the policy); the kernel maintains
  // it through add/GC. Null everywhere else — see OccIndex.
  OccIndex *dense = nullptr;
  // delayed watch relocations from an in-flight propagation scan
  // (lit, blocking, ref) triples; see watch.hpp for why they must wait
  std::vector<uint32_t> delayed;
  // probing-propagation bookkeeping log; inert (recording == false) outside a
  // kernel_probing_begin/end() scope, so search never pays for it
  ProbingLog probing;

  // Search-guidance state `H` (VMTF queue, EVSIDS score heap, saved/target/best
  // phases): held OPAQUELY and NON-OWNINGLY — the full `struct
  // Heuristics` lives in policies/heuristics.hpp, and only code that includes it
  // may read `h->…`. The kernel NEITHER allocates NOR owns it: the session (or a
  // policy) that outlives a single `solve()` owns the storage and ATTACHES it
  // (`k.h = &heuristics`; see tactix_solver / main). OPTIONAL by design (plan rev
  // 60): a solver with `h == nullptr` is sound, just unguided — that is the
  // DEFAULT here (a bare `Kernel` is unguided until a heuristic is attached).
  // Grown by heuristics_reserve_vars alongside kernel_reserve_vars when present.
  Heuristics *h = nullptr;

  // --- reconstruction stack (rho, plan §4) ----------------------------
  // Witness-labelled clause segments for rebuilding a SAT model through
  // eliminated/substituted variables (Kissat's extension stack; the
  // Jarvisalo-Heule-Biere reverse reconstruction). Flat, length-prefixed:
  // each segment is [ rest_lits..., witness_lit, len ] with len = 1 +
  // #rest and witness_lit the blocking literal. kernel_extend_model walks
  // it in REVERSE and, for any segment whose clause is unsatisfied under
  // the current assignment, sets its witness true. Empty until a
  // variable-eliminating rule (V3 Substitute, E1 BVE, ...) pushes a
  // witness — a no-op for unit-only inprocessing (V1 Probe / V2 Backbone).
  std::vector<Lit> reconstruction;
  bool model_extended = false; // kernel_extend_model idempotency guard

  // conflict handoff: propagate -> analyze
  bool conflict_valid = false;
  Ref conflict_ref = INVALID_REF; // INVALID means binary conflict
  Lit conflict_bin[2] = {INVALID_LIT, INVALID_LIT};

  // learned-clause handoff: analyze -> backjump
  PendingLearned pending;

  // scratch (analyze) — persistent so the per-conflict hot path never
  // allocates (kissat keeps these as solver-level stacks for the same
  // reason: clause, analyzed, shrinkable in internal.h)
  std::vector<uint8_t> seen;   // by var
  std::vector<Var> analyzed;   // vars marked seen
  std::vector<Var> shrinkable; // vars marked during a block-shrink walk
  // |{v ∈ analyzed : seen[v] & (SEEN|REASON_BUMP)}| — the BUMP set size,
  // which is what kissat's `analyzed` stack holds (its removable/poisoned
  // marks live on separate stacks). Ours is a flag-cleanup list that also
  // carries minimization scratch and, in LRAT mode, recorded antecedents,
  // so `analyzed.size()` is neither the bump-set size nor mode-independent.
  uint32_t analyzed_seen = 0;
  std::vector<Var> reason_bumped; // REASON_BUMP marks, for rollback
  std::vector<Lit> learned_scratch; // the clause under construction
  std::vector<Lit> otfs_scratch;    // OTFS resolvent assembly
  std::vector<Var> radix_scratch;   // radix-sort double buffer
  std::vector<uint8_t> marked_levels;
  std::vector<uint32_t> touched_levels;

  // --- eager subsumption of recently learned clauses (SP-EAGERSUB-1) --------
  // Transliterated from kissat `last_learned` (learn.c:70-72,133-187, MIT): a
  // small SHIFT REGISTER of the refs of the most recently learned LARGE
  // clauses. Every learn tests the fresh clause against the register and
  // garbages any entry it STRICTLY subsumes. Kissat's register, not cadical's
  // backwards walk over `clauses` (analyze.cpp:728-765), because our arena is a
  // flat variable-record vector with no clause-pointer vector — it cannot be
  // traversed backwards at all, so cadical's shape has no home here (design
  // note: implementation-notes.md §15).
  static constexpr uint32_t EAGER_SUBSUME_SLOTS = 4; // kissat's option maximum
  Ref eager_recent[EAGER_SUBSUME_SLOTS] = {INVALID_REF, INVALID_REF,
                                           INVALID_REF, INVALID_REF};
  // per-LITERAL marks for the containment test. Persistent + touched-restore
  // (never fill-initialised per call): this runs once per conflict, and an
  // O(nvars) memset there is the per-call linear scratch hazard that has cost
  // this project four sightings. INVARIANT: all-zero between uses.
  std::vector<uint8_t> eager_marks;

  // --- proof clause ids (native LRAT; CaDiCaL's id scheme) ------------
  // every clause — including input tautologies/duplicates that are never
  // stored — consumes one id in input/derivation order. Large clauses
  // carry theirs in the header; units live in unit_ids (by var; 0 =
  // none); binaries, which never materialize, in binary_ids keyed by the
  // canonical literal pair. THE INVARIANT: while a pair is live in
  // the watch lists, its id here names a clause the checker still holds.
  // Before binaries were deletable this was free (ids only ever accumulated,
  // so "first id wins" was as good as last). kernel_delete_binary breaks that,
  // so two rules now hold it up: (a) kernel_add_binary_clause installs the
  // LAST id, because a pair re-derived after V4 deleted it must not keep
  // citing the id the `del` retired; (b) a pair with two live copies is NOT
  // deletable — one id cannot name two clauses (the primitive refuses).
  // Lookups of a DEAD pair never happen: every kernel_binary_id caller
  // resolves a pair it just read out of a watch list or a live binary reason.
  // The map itself is size-monotone — deletion keeps the key, since
  // `binary_ids.size()` is the "new binaries appeared" inprocessing trigger
  // (default_policy.cpp) and feeds kernel_irredundant_clauses; both want
  // monotone growth, not a live count. SubsumeForward's soundness rests on the
  // V4 equivalence argument, not on permanence — see subsume_forward.hpp.
  uint64_t next_clause_id = 0;
  std::vector<uint64_t> unit_ids; // by var
  // A flat open-addressing replacement was built and A/B-measured: it is
  // trajectory-identical but uniformly slightly SLOWER. This map is not a
  // bottleneck — do not re-derive that.
  std::unordered_map<uint64_t, uint64_t> binary_ids;
  // antecedent scratch (analyze, LRAT mode only): (trail position of the
  // literal the antecedent propagated, antecedent id). Emitted sorted by
  // trail position ascending with the conflict id last — that order is
  // checker-propagatable by construction (each hint's other literals are
  // clause negations, roots, or earlier-propagated literals)
  std::vector<std::pair<uint32_t, uint64_t>> lrat_entries;
  std::vector<uint64_t> lrat_chain; // built by kernel_lrat_chain below
  // Reason jumping, LRAT mode only: vars a jumped reason resolved past, whose
  // antecedent is settled against the FINAL clause rather than at record time
  // (analyze.cpp chain_expand_jumped). Same round scope as `lrat_entries`.
  std::vector<Var> jump_pending;

  // --- input normalization, deferred to end-of-input (N2) --------------
  // An input clause whose FILE form repeats a literal is stored deduped, so
  // id `i` names a different clause for us than for the checker (which reads
  // the file). `ingest_clause` therefore records a RESTATEMENT here instead
  // of citing `i`; `finish_input` (frontend/parse.hpp) emits each one as a
  // one-antecedent RUP lemma and re-points the stored id. The restatement id
  // must exceed EVERY input id, and input ids run 1..N with N unknown until
  // the input ends — hence the deferral. See parse.cpp for the full
  // enumeration of what can and cannot diverge.
  struct InputRestatement {
    uint64_t id;    // the file-position id; the lemma's sole antecedent
    uint32_t begin; // slice of input_restate_lits (the deduped clause)
    uint32_t size;
    Ref ref; // large clauses only: the header to re-point
  };
  std::vector<InputRestatement> input_restatements;
  std::vector<Lit> input_restate_lits;
  // A refutation found DURING input (empty input clause / contradicting input
  // units) is deferred for the same reason: emitting it mid-input would burn
  // an id the checker has already given to a later input clause.
  bool input_refuted = false;
  uint64_t input_refutation_id = 0; // the refuting input clause
  uint32_t input_refutation_var = UINT32_MAX; // its unit partner, if any

  Proof *proof = nullptr;
  Stats stats;
  // NOTE (plan §5): scheduling state (restart EMAs, reduce intervals, ...)
  // lives in the Policy object, not here; kernel H-state is confined to
  // instance-ranking heuristics (VMTF, phases).

  // --- BVE per-variable RETRY FILTER (kissat flags->eliminate) --------------
  // by var: 1 = "this variable's elimination neighbourhood has changed since
  // BVE last attempted it — retry", 0 = "BVE already attempted it against
  // exactly this neighbourhood and got nothing". Purely a WORK filter, never a
  // soundness one: it removes only re-attempts whose outcome is already known,
  // so no rule application that was possible becomes impossible (N1 — see
  // bve.hpp for the completeness argument).
  //
  // Set (kernel-side, so there is one home per mutation kind):
  //   - every variable of an IRREDUNDANT clause that is ADDED
  //     (kernel_add_large_clause / kernel_add_binary_clause) or DELETED
  //     (kernel_mark_garbage / kernel_delete_binary*). Redundant (learned)
  //     clauses are deliberately excluded — BVE never resolves them (they are
  //     delete-only collateral), and marking on learned-clause traffic would
  //     mark every variable on every conflict and defeat the filter.
  //   - every variable of a clause a newly ROOT-FIXED literal occurs in
  //     (kernel_bve_retry_sync_fixed) — fixing y makes the clauses y satisfies
  //     delete-only and strips ¬y from the rest, which changes what BVE
  //     computes for the OTHER variables in them.
  //   - every freshly reserved variable (kernel_reserve_vars initialises to 1),
  //     so the very first schedule considers everything.
  // Cleared by the rule when it ATTEMPTS a variable (bve_eliminate), matching
  // kissat eliminate.c's `FLAGS (idx)->eliminate = false` at attempt time.
  std::vector<uint8_t> bve_retry;
  // trail index up to which root-fixed literals have been folded into
  // `bve_retry`; see kernel_bve_retry_sync_fixed.
  uint32_t bve_retry_fixed_head = 0;

  // --- TOUCHED-EPOCH substrate (kissat flags->subsume, flags->factor) -------
  // The generic answer to "what is new since I last acted?", which the
  // repetition suppressors of Track SCHED stage F are all built on. A bit
  // would need a global clear pass per consumer and would then have exactly
  // the lifecycle hazard that wedged the elimination clock (Stats::bve_marks):
  // a stamp has no clear step at all, so it cannot be wedged by a clearer that
  // never runs.
  //
  //   `touch_epoch`  monotone event counter, incremented on every touch.
  //   `touched[v]`   the epoch at which v was last touched (0 = never).
  //
  // A consumer keeps its own watermark `w` and reads `touched[v] > w`; it
  // "clears" by assigning `w = touch_epoch` — a snapshot taken BEFORE it does
  // its work, so material the consumer itself creates stays new for its next
  // pass (kissat re-arms the same cases by hand, forward.c:589-645).
  //
  // PRODUCERS — one home per mutation kind, and they are exactly the mutations
  // that can create a new inference for a clause-rewriting rule:
  //   - every variable of an IRREDUNDANT large clause that is ADDED, and both
  //     variables of any binary that is added (kernel_add_large_clause /
  //     kernel_add_binary_clause; kissat marks the same two populations,
  //     clause.c:71-72,98). Strengthening a clause goes through
  //     kernel_replace_clause, i.e. an add, so it is covered here too.
  //   - every newly ROOT-FIXED variable (kernel_touch_sync_fixed).
  //   - every freshly reserved variable (kernel_reserve_vars).
  // Deliberately NOT a producer: clause DELETION. Deleting a clause can only
  // remove subsumers/partners, never create one, so a deletion-driven touch
  // would weaken the filter for nothing. This is where the substrate's mark
  // set differs from `bve_retry`, which must mark on deletion because BVE's
  // resolvent count changes when a neighbour disappears.
  //
  // ▶ WHY IT CANNOT WEDGE SHUT. The producers are on the kernel's clause
  // creation path, not on any rule's admitted path, so no scheduling decline
  // can starve them. And "no touch since w" is not a heuristic claim but a
  // theorem: with no add and no root fix, the irredundant clause set only
  // shrinks, and subsumption/strengthening are monotone in it — a consumer
  // that skips everything is skipping work that provably cannot succeed.
  uint64_t touch_epoch = 0;
  std::vector<uint64_t> touched; // by var
  // The DELETION side, stamped from the same clock. Deletion is not a producer
  // of `touched` because it cannot create a subsumer — but it is exactly what
  // creates a BLOCKED clause, so BCE needs it and only BCE does. Kept as a
  // second array rather than folded into `touched` so that adding BCE as a
  // consumer does not weaken C1/C2's filter, which is where the measured saving
  // is. cadical draws the same line: `mark_added` and `mark_removed` set
  // different flags (internal.hpp:1125-1137).
  std::vector<uint64_t> removed; // by var
  // trail index up to which root-fixed literals have been folded into
  // `touched`; see kernel_touch_sync_fixed.
  uint32_t touch_fixed_head = 0;
  // Consumer watermarks. Solver STATE, not config — the resumption category
  // SCHED-E1 moved `SlotEntry::cursor` into, except that these are read by the
  // RULE itself and must survive its stack frame, so they stay kernel-side (a
  // policy or a test can still reset one to 0 to force a full pass). Each is
  // advanced by a round of its own rule, which re-arms whatever it did not
  // reach first (SCHED-F2). N1 is untouched: `instances()` and the
  // single-candidate `apply_instance` path never read these.
  uint64_t subsume_touch_mark = 0;
  uint64_t strengthen_touch_mark = 0;
  // SCHED-F3's consumers. `bce_touch_mark` is read against BOTH stamps
  // (kernel_changed_since); the other two against `touched` alone.
  uint64_t bce_touch_mark = 0;
  uint64_t ternary_touch_mark = 0;
  uint64_t instantiate_touch_mark = 0;
  uint64_t factor_touch_mark = 0; // SCHED-F4 (kissat flags->factor)
  // Did the rule's LAST canonical round reach a fixpoint — every candidate
  // examined, and (for Strengthen) nothing produced that would want another
  // look? kissat's `completed` (forward.c:664-670), which it computes as "was
  // anything re-armed". OUTPUTS, written by the rule, read by a policy;
  // `true` when the rule has not run, so a policy that never schedules it is
  // never held up by a stale `false`. See SCHED-F2 for the one consumer.
  bool subsume_complete = true;
  bool strengthen_complete = true;
  // Master switch for the stage-F round filters (TACTIX_TOUCH_MARKS=0 is the
  // A/B off arm). False = every round scans its full candidate set, i.e. the
  // pre-stage-F behaviour, byte for byte.
  bool touch_filter = true;
  // SCHED-F2: let an INCOMPLETE SubsumeForward/Strengthen round hold the
  // elimination pipeline's outer loop open (kissat eliminate.c:519-520 — its
  // round loop exits on the forward round's `complete`, so a half-checked
  // candidate list buys another round rather than an advance on stale
  // material). Bounded by the policy's own round cap, so it can only ADD
  // rounds. TACTIX_SUBSUME_HOLD=0 is the A/B off arm.
  bool subsume_holds_elim = true;
  // SCHED-F6 · stamp the REDUNDANT creation path too, for the clauses cadical's
  // `likely_to_be_kept_clause` admits (see kernel_likely_kept). SHIPS OFF —
  // TACTIX_TOUCH_REDUNDANT=1 turns it on — and that default is an argued
  // decision, not an oversight. Extra stamps only ever WEAKEN the round filters
  // (more marks ⇒ fewer skips), and C1/C2/BCE/X2/X4 all scope themselves to
  // irredundant clauses today, so with no consumer for the redundant band this
  // would be a measured cost buying nothing. It exists now so the stamps can be
  // proven to fire (`Stats::touch_redundant_*`) BEFORE the consumer half —
  // opening C1/C2's scope to the tier band, cadical `subsume.cpp:388` — is
  // written; landing that half against an unpopulated set is exactly the
  // byte-identical-artefact trap that kept `SP-TIER-2` open.
  bool touch_redundant = false;
  // The reduce SURVIVOR ENVELOPE, published by `reduce_apply_refs` (cadical
  // `lim.keptglue`/`lim.keptsize`, reduce.cpp:147-156): the widest (glue, size)
  // still present after the last reduction. It is the THIRD disjunct of
  // `likely_to_be_kept_clause` and the one we had no counterpart for — without
  // it the predicate is just "irredundant or in tier2", which is not the
  // reference's set. RULE-written, like `tier1`/`tier2`.
  uint32_t kept_glue = 0;
  uint32_t kept_size = 0;
  // (SCHED-B3's `transitive_cursor` lived here. It is POLICY state — where a
  // caller resumes an enumeration is a scheduling decision, not kernel state —
  // and SCHED-E1 moved it to DefaultPolicy::SlotEntry::cursor, generalized to
  // every dosed slot entry. kissat's own device is the per-variable
  // `flags[idx].transitive` pending bit, cleared as each source is processed,
  // so whatever the budget did not reach stays scheduled for the next round
  // (`transitivekeep`, DEFAULT 1, options.h:157).)
  // by var: the MINIMUM growth slack at which x is currently eliminable (its
  // #resolvents − occ⁺ − occ⁻), as last computed by bve_eliminate; UINT16_MAX =
  // never (an oversized resolvent, or growth beyond the tracking ceiling). Only
  // meaningful while `bve_retry[x] == 0` (attempted, neighbourhood unchanged) —
  // a set retry bit means "recompute". A finer-grained WORK filter than the
  // retry bit for the DefaultPolicy BVE ramp: rejected at a low slack, x can be
  // skipped at every slack below what it actually needs instead of re-resolved
  // at each — same completeness/N1 story as `bve_retry` (bve.hpp). Written only
  // when `bve_track_min_bound` is set (the ramp); every other path leaves it
  // untouched and byte-identical. Sized with `bve_retry`.
  std::vector<uint16_t> bve_min_bound;
  // Opt-in: when set, bve_eliminate resolves a rejected variable up to the ramp
  // ceiling (not just the current slack) so it can record the EXACT `bve_min_bound`
  // the policy needs to skip precisely. Off by default → the abort is at the
  // current slack, exactly as before this filter existed. Policy-transient
  // (DefaultPolicy sets it around its ramp), never persisted.
  bool bve_track_min_bound = false;

  // --- rule configuration (set by a policy or the public API at any state) ---
  // BVE syntactic gate detection: bitmask of enabled gate types (see bve.hpp
  // BveGate). Default = all bits on. This is a PERFORMANCE knob, never a
  // soundness one — every setting is sound because a disabled gate type only
  // makes BVE fall back to the full resolvent cross product (N1: gate type is
  // *how* a chosen variable's resolvents are computed, not *whether* it can be
  // eliminated).
  uint32_t bve_gate_mask = ~0u;
  // BVE growth slack (kissat `bounds.eliminate.additional_clauses`, option
  // eliminatebound): eliminate iff #non-tautological resolvents ≤ occ⁺+occ⁻+this.
  // CONFIGURATION, like bve_gate_mask — not scheduling state: any policy may set
  // it at any time and any value is sound (a larger slack only admits more
  // eliminations, each individually sound). The default is the MAXIMUM
  // (BVE_MAX_BOUND in rules/bve/bve.hpp — keep the two in step), i.e. a caller
  // that ignores this field gets the old fixed-slack behaviour. A policy wanting
  // kissat's fine-grained dose ramps it 0,1,2,4,8,16, re-scheduling every
  // variable at each level (eliminate.c set_next_elimination_bound).
  uint32_t bve_bound = 16;
  // BCE candidate-occurrence ceiling (cadical option `blockocclim`, whose own
  // default is 1e2): decline a blocking literal occurring more than this many
  // times. CONFIGURATION like bve_bound — any policy may set it at any time and
  // any value is sound, because it only narrows WHICH candidates the rule offers
  // (a within-rule scope choice, the same category as BCE's irredundant-only
  // filter — implementation-notes §1's BCE row), never whether a deletion is
  // justified.
  //
  // Ships 0 = UNCAPPED. Porting cadical's 100 is MEASURED-DEAD as a default: it
  // is a real lever in BOTH directions on the same axis, roughly 3x faster on
  // one Tier C instance and a timeout on another whose solve depends on BCE
  // deleting exactly the clauses a dense-neighbourhood cap forbids. cadical can
  // afford the cap because it does not depend on BCE at all (it ships `block`
  // OFF); we do. Hence a §5.6 arm rather than a constant —
  // TACTIX_BCE_OCC_LIMIT.
  uint32_t bce_occ_limit = 0;
  // --- HyperTernaryRes governors, ported from cadical (`ternary.cpp`), which
  // bounds ternary resolution FOUR ways where we had NONE. CONFIGURATION like
  // bve_bound — the POLICY sets them, any value is sound (they only narrow which
  // resolvents the rule offers), 0 = unlimited.
  //
  // WHY THEY EXIST: without them HyperTernaryRes has no bound of its own, and
  // BCE was silently serving as one — deleting blocked clauses shrinks the
  // ternary candidate pool. Disable BCE on a formula it feeds and ternary
  // resolution runs away by a factor of five in resolvents and rounds, and the
  // solve dies. cadical allows 2 rounds. Giving the rule its own governor is
  // what lets the family be scheduled instead of having one rule police
  // another.
  //
  // `ternary_occ_limit` = cadical `ternaryocclim` (default 100): skip a pivot
  // whose EITHER polarity has more ternary occurrences than this
  // (`ternary.cpp:247`). `ternary_max_add_percent` = cadical `ternarymaxadd`
  // (default 1000, i.e. 1000 %): stop a round once it has added more clauses
  // than `total_clauses * pct / 100` (`ternary.cpp:387`). Declared below with
  // the rest of the rule config.
  //
  // Let Strengthen/SubsumeForward stop mid-round on `work_budget`
  // (`TACTIX_MIDROUND_BUDGET=1`).
  //
  // DEFAULT OFF — MEASURED NEGATIVE on both tiers for no solve gain
  // (benchmarks/reports/2026-08-04-midround-budget-negative.md), and inert on
  // its own sentinel instance. Mechanism, so the negative is understood rather
  // than merely recorded: at 100 per mille the budget binds about once per
  // solve, and one stopped round just defers its candidates to the next round —
  // the work is rescheduled, not saved. The DOSE is a real lever where
  // interruptibility is not, which is why the mechanism stays as a plan §5.6
  // dosing arm rather than being deleted.
  bool midround_budget = false;
  // Factor clause-size scope (kissat `factorsize`, default 5). 2 = a
  // binary-only variant; >= 3 lets the grid range over clauses up to this size,
  // i.e. remainders become sub-clauses rather than single literals.
  // CONFIGURATION, policy-owned, any value sound (it only widens which
  // factorings the rule offers; the add-RAT argument is indifferent to |r| —
  // docs/design/e2-factorsize-design.md §4). Restricting kissat to our old
  // binary-only variant (`kissat --factorsize=2`) costs it instances outright,
  // which is why the default follows kissat here. This is the SCOPE half only:
  // it admits no new instances, so it cannot cause the damage the
  // union-admission arm can (factor_scope_percent below).
  uint32_t factor_size = 5;
  // Factor SCOPE-FRACTION admission: admit Factor when at least this PERCENT of
  // the live irredundant clauses are within its clause-size scope
  // (<= factor_size). 0 = use the binaries-per-var density gate instead.
  //
  // Why a fraction and not a density: the shipped density gate is an overfit
  // threshold, and the obvious scope-aware version (small-clauses per variable)
  // does not separate at all — some instances Factor must NOT run on score
  // higher than instances it must. The FRACTION separates with a reason: "most
  // of the formula is inside Factor's scope" vs "Factor is nibbling at the edge
  // of a formula it cannot see". It is also automatically scope-aware — raising
  // factor_size moves exactly the instances that gain material across the
  // threshold, which makes the size widening and the admission ONE decision
  // rather than two interacting ones.
  uint32_t factor_scope_percent = 0;
  // TransitiveReduce: exit the reachability walk as soon as `dst` is reached
  // (kissat transitive.c:130). Ships FALSE — the deviation recorded in
  // transitive_reduce.hpp runs each walk to exhaustion so the failed-literal
  // detection sees the whole reachable set. So this changes INFERENCE (fewer
  // derived units), not just cost. Sound either way (dropping an inference never
  // makes a deletion unjustified). CONFIGURATION, policy-owned.
  bool transitive_early_exit = false;
  // BVE dose ADMISSION by measured COST-PER-YIELD (resolutions per eliminated
  // variable), an alternative to the binary-density proxy for during-search
  // doses. CONFIGURATION like bve_bound: policy-owned, any value sound (it only
  // declines doses), 0 = no limit. Consulted only under TACTIX_BVE_ADMIT=cost.
  //
  // Why cost rather than density: removing the density gate loses solves, so
  // some admission is load-bearing — but density is a proxy for "elimination is
  // safe here" that is false on essentially every sparse instance, and on Tier B
  // the gate is a one-instance defense that costs several others. The DIRECT
  // signal is cost per yield: a pathological instance can spend ~10^5
  // resolutions per elimination, an outlier no productive instance shows.
  uint32_t bve_cost_limit = 0;
  uint32_t ternary_occ_limit = 100;      // cadical ternaryocclim
  uint32_t ternary_max_add_percent = 1000; // cadical ternarymaxadd
  // Factor grid-selection strategy: consulted by factor_apply for its
  // scoring/accept scalars when set; null = the built-in watch-size path.
  // CONFIGURATION like bve_bound — any policy may set it at any time and any
  // value is sound (selection only chooses WHICH positive-gain grid reaches
  // apply_factoring, or whether one does). Non-owning; DefaultPolicy scopes it
  // to the inprocess phase (set at entry, cleared at exit) so it never dangles.
  FactorMatchPolicy *factor_match = nullptr;
  // Factor acceptance bound: accept a grid iff delta > this. Default 0 ≡
  // the FACTOR_MIN_GAIN = 1 floor. CONFIGURATION like bve_bound — any value
  // is sound (it only narrows WHICH positive-gain grids reach
  // apply_factoring); an attached FactorMatchPolicy keeps precedence. N1
  // config surface for external/learned policies; no shipped policy writes a
  // non-zero value. NOTE that DefaultPolicy always attaches a matcher which
  // MIRRORS this field, so setting it alone has no effect under the shipped
  // policy — see DefaultFactorMatch::bound.
  uint32_t factor_bound = 0;
  // Demote Factor's fresh extension variables to the BOTTOM of the decision
  // queue after an application (kissat factor.c
  // `adjust_scores_and_phases_of_fresh_variables`). Repairs a FOCUSED-mode
  // defect: heuristics_reserve_vars appends new vars at `queue_last`, which is
  // where VMTF's decision search starts, so every variable Factor adds becomes
  // a most-preferred branching candidate.
  // DEFAULT TRUE = reference parity (ledger SP-FACTOR-2). kissat calls the
  // adjustment UNCONDITIONALLY at the end of every `run_factorization`
  // (factor.c:951 — no option gates it); cadical gates it on `factorunbump`,
  // default 1, help text "[1: as in kissat]" (options.hpp:126). So OFF was our
  // invention, not the status quo either reference ships. `TACTIX_FACTOR_DEMOTE=0`
  // reverts (inverted A/B hatch; `=1` still means ON, so banked arms reproduce).
  // HEURISTIC-ONLY (`H`): no proof delta, no sigma change, no guard conjunct —
  // it reorders the VMTF queue and nothing else.
  bool factor_demote_fresh = true;
  // Factor CANDIDATE REDUCTION rounds (kissat `factorcandrounds`, cadical
  // `--factorcandrounds`, both default 2). A large clause is grid material only
  // if every literal in it occurs in >= 2 candidate clauses; the prune iterates
  // to fixpoint, bounded by this. 0 = the unfiltered candidate set.
  // CONFIGURATION, policy-owned, any value sound — it only narrows which grids
  // the rule OFFERS, never whether one is legal.
  uint32_t factor_cand_rounds = 0;
  // DIAGNOSTIC ONLY (`TACTIX_NO_EXT_DECISIONS`) — never ship this on. Skips
  // Factor's extension variables when branching (falling back only when
  // nothing else is unassigned). It makes `Decide` itself unable to RETURN
  // those variables, i.e. it removes the capability, which N1 forbids for a
  // default (plan §1.1 N1 scope note). Contrast RETIREMENT and
  // `no_ext_reason_bump`, which only change what the POLICY prefers and are
  // therefore compliant. It exists to test whether branching on extension vars
  // is what makes broad Factor admission damage our search.
  bool no_ext_decisions = false;
  // `TACTIX_NO_EXT_REASON_BUMP` (arm — default off). Withhold Factor's
  // extension variables from the REASON-SIDE bump channel only; the
  // conflict-side channel (resolution-visited / in the learned clause) is
  // untouched, so an extension var still rises whenever it earns it.
  //
  // UNLIKE `no_ext_decisions` above this is N1-CLEAN and shippable: reason-side
  // bumping is `H`-only advice with no proof obligation, and this changes only
  // the ORDER candidates are offered in, never which are legal (rank freely,
  // never omit). Every variable remains decidable at every point.
  //
  // Motivation: extension variables take several times as many bumps per
  // variable as original ones, and the large majority of those bumps are
  // reason-side ONLY. A BVA extension variable IS by construction the hub of
  // the clauses it replaced, so it appears in a great many reasons —
  // reason-side bumping promotes it for its structural position rather than
  // for evidence, and the promotion self-reinforces (promoted -> decided more
  // -> in more reasons). Withholding this ONE channel is discrimination rather
  // than suppression, which is why it is preferred to `no_ext_decisions`.
  bool no_ext_reason_bump = false;
  // A/B knob for the Substitute hazard skip (`TACTIX_SUBST_HAZARD_ROUND=1`):
  // restore the coarse per-ROUND skip, so both arms come from ONE binary and
  // are comparable in one measurement window. Default false = the per-SCC
  // skip, which is the fix: the per-round form discards every equivalence in a
  // round because a handful of the literals in it were hazardous.
  bool subst_hazard_round = false;
  // VARIABLE RETIREMENT (docs/design/variable-retirement-design.md, plan §11
  // O17 / §1.1 N1 scope note). A variable a rule has rewritten out of every
  // clause occurs in NO clause, so deciding it propagates nothing and only
  // deepens the trail — on substitution-heavy instances that is the large
  // majority of all decisions.
  //
  // ADVICE, NOT A RESTRICTION. `Decide::instances()` still enumerates these and
  // any policy may decide them (N1 binds the API, not DefaultPolicy's taste);
  // DefaultPolicy merely declines while a non-retired candidate exists. That is
  // the `bve_retry` shape, and the line that separates this from
  // `no_ext_decisions` (which removes the capability).
  //
  // The CAUSE is kept, not just a bit: the two causes have different owners and
  // different invariants, and `c retired` reports them apart when diagnosing.
  // Empty vector == every variable Active.
  std::vector<VarStatus> var_status;
  // Demote substituted variables to the bottom of the decision order after each
  // Substitute round (`TACTIX_SUBST_DEMOTE=1`). Ranking advice only.
  bool subst_demote = false;
  // SHIPPED ON. DefaultPolicy retires variables a rule has rewritten out of the
  // formula: `heuristics_deactivate_var` unlinks them from the VMTF queue and
  // the score heap (kissat `flags.c` deactivate_variable), `kernel_unassign`
  // keeps them out, and the SAT test counts ACTIVE variables. `TACTIX_RETIRE=0`
  // reverts. N1 intact: `decide_candidates` enumerates from `k.nvars`, never
  // from `H`, so the API still offers every variable.
  bool retire_enabled = true;
  // §6 COMPLIANCE. External interrupt, published to the RULES.
  //
  // As a `Policy::solve` parameter alone, `stop` is invisible to the
  // longest-running rule applications, and a single unmetered closure can leave
  // the solver SIGTERM-deaf for many minutes. A competition harness then
  // escalates to SIGKILL and THE PROOF IS LOST — an N2/§6 defect, not a latency
  // nuisance. Polling at the phase round loop and slot boundaries bounds
  // latency to ONE application.
  //
  // NOT policy knowledge (the kernel stays policy-free, §5.3): this is
  // external-environment state, the same category as `proof`. The policy
  // publishes its own `stop` here on entry and clears it on exit.
  //
  // SOUNDNESS: aborting is always legal because a rule that stops early has
  // simply done LESS. Everything it did is already proof-logged, and abort
  // points are chosen between complete units of work, never mid-mutation — the
  // same contract budget-cut doses already satisfy (Factor's tick limit). It
  // can therefore never change a verdict, only how much simplification happens.
  volatile sig_atomic_t *stop = nullptr;
  // Stress knob for the abort paths above (0 = off). `mutable` because the
  // extraction half of Congruence takes a `const Kernel &` — diagnostic state
  // only, never read by a guard, so it cannot affect which rules are
  // APPLICABLE (N1).
  // NB: the poll sites bypass their 256-iteration cadence when this is set.
  // Without that, formulas smaller than one poll window never reach a first
  // poll at all, and a fuzz leg over small CNFs passes vacuously.
  uint32_t stop_fuzz = 0;
  mutable uint64_t stop_fuzz_state = 0x9E3779B97F4A7C15ull;
  // Composite grids (kissat resize_factoring): widen Factor's
  // factor/remainder universe to ALL live literals — extension vars included,
  // growing as the pass applies. Default false because composition over-applies
  // in a single preprocessing blast; it belongs to the small interleaved
  // during-search doses, and DefaultPolicy sets it there. CONFIGURATION like
  // bve_bound — any value sound, termination stays delta-monotone
  // (docs/design/s2b-composite-grids-design.md §4). This field is the only
  // switch; there is no env override.
  bool factor_composite = false;
  // ADVICE (N1): may the policy's scheduling of Factor decline this occasion?
  //
  // DEFAULT true = NO RESTRICTION, and that default IS the N1 guarantee. The
  // binary-density admission heuristic must live in DefaultPolicy (where BVE's
  // identical gate lives) and reach the rule through this field — the same
  // advice shape as `bve_retry`. As a NoOp inside `factor_apply` it instead
  // removed a capability: `factor_guard` reports APPLICABLE and
  // `factor_instances` offers its canonical instance, but applying it does
  // NOTHING on a binary-sparse formula, so no caller of the public API (an LLM
  // policy, an RL policy, RandomPolicy) can apply a legal factoring at all. The
  // N1 carve-out permits the built-in POLICY to decline, never the RULE to
  // refuse everyone.
  //
  // An API caller that never writes this sees the complete surface. Sound at
  // any value: declining an application is always sound, and forcing one is
  // what the rule would have done anyway.
  bool factor_admit = true;
  // PRIVATE PER-RULE TICK METERING.
  //
  // `stats.ticks` is the GLOBAL propagation counter, and four slot entries
  // (Probe, Backbone, Vivify, Instantiate) budget against it. Both references
  // keep PRIVATE per-technique counters instead, and the difference is not
  // cosmetic: with one shared meter an earlier slot entry spends the budget a
  // later one was handed, and nothing distinguishes "this rule was STARVED"
  // from "this rule ran and found nothing".
  //
  // When non-null, every kernel tick charge is added HERE as well as to
  // `stats.ticks`. Pure accounting: the kernel never reads it, no guard
  // consults it, and no `instances()`/`apply` branches on it, so a null sink
  // (every caller except DefaultPolicy) is byte-identical to before. The policy
  // scopes it around one apply, the same discipline as `work_budget`.
  //
  // Placement argument: the alternative is each rule charging its own counter,
  // which would put per-technique accounting inside rules that delegate
  // propagation to the kernel, and would not survive a rule calling
  // `kernel_propagate_probing`. The sink keeps the accounting where the work
  // happens and the INTERPRETATION where the schedule lives.
  uint64_t *tick_sink = nullptr;
  // Per-round work budget, read by any budget-aware canonical apply in its
  // OWN currency (kissat SET_EFFORT_LIMIT): Vivify meters propagation ticks
  // (stats.ticks), Factor its cache-line factor_ticks (S4 dosing — kissat
  // factor.c:442,914 quits mid-round on the same counter). A round stops
  // visiting further candidates once it has spent this much; 0 = unbounded
  // (the default — byte-identical legacy behaviour). CONFIGURATION like
  // bve_bound, not a guard: stopping early only makes the round's effect a
  // prefix/subset of the unbounded round's, and every prefix is sound (each
  // rewrite is individually justified). Required by the during-search phases,
  // which must dose their techniques — an unbounded round is the dominant
  // diffuse Tier C cost.
  uint64_t work_budget = 0;
  // Vivify round scope (kissat's redundant-tier vs irredundant split):
  // 0 = all candidates (default), 1 = redundant (learned) clauses only,
  // 2 = irredundant only. CONFIGURATION like work_budget — a scoped round's
  // effect is a subset of the full round's, each rewrite individually
  // justified. Lets the policy mirror kissat's structure where the redundant
  // tiers ALWAYS run within budget and only the irredundant pass is
  // delay-gated (vivifyirr); delaying the whole round instead is measurably
  // worse.
  uint8_t vivify_scope = 0;
  // Reuse the conflict as its own driving clause when exactly one of its
  // literals sits at the conflict level (kissat one_literal_on_conflict_level)
  // instead of learning a fresh clause. CONFIGURATION like
  // work_budget/vivify_scope — the frontend owns the on/off
  // (TACTIX_CONFLICT_REUSE=1 enables), the rule owns the mechanism. Both
  // settings are sound; they differ only in trajectory and in how many clauses
  // are learned. DEFAULT OFF, awaiting a tier measurement — see
  // docs/implementation-notes.md §12.
  bool conflict_reuse = false;
  // DIAGNOSTIC ONLY (TACTIX_CONFLICT_REUSE_NOREPAIR=1, needs conflict_reuse
  // too): restore the CRIPPLED behaviour — decline the shortcut instead of
  // repairing the watches. It exists to attribute the arm's tier verdict
  // between "the crippling" and "the mechanism", which is otherwise
  // unanswerable: an ON-vs-OFF pair cannot tell them apart. Not a shipped
  // setting and not a policy surface; both values are sound.
  bool conflict_reuse_no_repair = false;
  // Reason jumping (kissat `jumpreasons`, default 1 there; assign.c:38 +
  // fastassign.h:12). When a binary clause (lit ∨ other) propagates `lit` and
  // `other`'s own reason is binary, store *that* reason instead of `other`:
  // the recorded antecedent becomes the one-step resolvent (lit ∨ r), a clause
  // that is implied but NOT in the database. Analysis then resolves straight
  // past `other`. CONFIGURATION like conflict_reuse — the frontend owns the
  // on/off (TACTIX_JUMP_REASONS=1), the kernel owns the mechanism; both
  // settings are sound and differ only in trajectory.
  // DEFAULT OFF, awaiting a tier measurement — see
  // docs/implementation-notes.md §13.2 (row SP-JUMP-1), which records why the
  // mechanism is NOT free here: every LRAT consumer of a binary reason has to
  // expand the virtual resolvent back into its two real antecedents, and the
  // delete-binary primitive has to protect both of them.
  bool jump_reasons = false;
  // Reason-side bumping (kissat option bumpreasonslimit, cadical
  // bumpreasonlimit — both 10): after the learned clause is final, also mark
  // for bumping the literals of the REASONS of the clause's own literals, up
  // to `this × |bump set|` marks; 0 = off (the default, byte-identical to not
  // having the mechanism). CONFIGURATION like bve_bound: it changes only
  // which variables `H` bumps, so every value is sound and proof-neutral —
  // the clause database, the trail and the certificate are untouched. The
  // GATE (decision-rate EMA + delay ladder) is scheduling and lives in the
  // policy, which scopes this field around ConflictAnalysis.
  uint32_t bump_reason_limit = 0;
  // Eager subsumption of recently learned clauses (kissat option
  // `eagersubsume`, default and maximum 4): how many slots of the
  // `eager_recent` shift register a learn compares itself against; 0 = off
  // (byte-identical to not having the mechanism). CONFIGURATION like
  // bump_reason_limit: every value is sound, because the only clauses it
  // deletes are REDUNDANT ones, which S2 Reduce may delete unconditionally —
  // the subsumption test is a *selection* heuristic, not the justification
  // (implementation-notes.md §15). Ships ON at kissat's default;
  // TACTIX_EAGER_SUBSUME=0 reverts (the A/B arm).
  uint32_t eager_subsume = EAGER_SUBSUME_SLOTS;
  // Reduce retention tiers (kissat options tier1/tier2, defaults 2/6): glue
  // bands that survive a reduction while recently used. CONFIGURATION like
  // bve_bound — retention is policy, and every value is sound (Reduce only
  // ever deletes redundant clauses). A policy that wants kissat 4.0's
  // DYNAMIC tiers calls reduce_retier(), which overwrites these with
  // quantiles of Stats::used_glue.
  uint32_t tier1 = 2, tier2 = 6;

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

// --- lifecycle -------------------------------------------------------------
void kernel_reserve_vars(Kernel &, uint32_t nvars);
// heuristics_reserve_vars (the `H`-growth half) is declared in
// policies/heuristics.hpp; kernel_reserve_vars calls it.

// Allocate `count` fresh NON-ORIGINAL (extension) variables at a root fixpoint and
// return the index of the first one. Grows all per-variable state via
// kernel_reserve_vars (additive) and records the extension boundary so the model
// printer projects them out. Used by E2 Factor (BVA). Caller must be at level 0.
Var kernel_new_extension_vars(Kernel &, uint32_t count);

// Number of variables to emit in the model `v` line — original variables only
// (extension variables introduced by Factor are projected out).
inline uint32_t kernel_num_output_vars(const Kernel &k) {
  return k.first_extension_var == UINT32_MAX ? k.nvars : k.first_extension_var;
}

// --- reconstruction (rho) --------------------------------------------------
// Push one witness-labelled clause segment {witness} u {rest} onto rho.
// `witness` is the blocking literal (set true at extend if the segment's
// clause is unsatisfied). Internal state only — emits NO proof line
// (push-witness is kernel-side; the DRAT/LRAT justification is the rule's
// add-RUP/delete of the actual clauses).
void kernel_push_witness(Kernel &, Lit witness, const Lit *rest, uint32_t nrest);
// Rebuild a full model through rho: replay the stack in reverse, forcing
// each unsatisfied segment's witness true. Idempotent (model_extended).
// Call once on a SAT result before reading k.values as the model.
void kernel_extend_model(Kernel &);

// --- shared proof idioms ---------------------------------------------------
// {a} + {¬a} ⊢ ∅: emit the empty clause (LRAT chain {id_a, id_b}; DRAT bare)
// and set `inconsistent`. The pair-refutation emission lives in exactly one
// place (the kernel_reason_walk precedent) — copy drift here is a soundness
// risk, which is why four rules share this instead of inlining it.
void kernel_emit_empty_from_pair(Kernel &, uint64_t id_a, uint64_t id_b);

// Batched root units for one simplification round (V3's record_unit; corpus
// 007 is the regression that mandates the opposite-sign refutation): deduped
// per variable; {u} and {¬u} recorded in one round refute via
// kernel_emit_empty_from_pair. The caller has already emitted the unit's
// proof clause under `id`; Recorded installs it as the variable's unit id.
struct PendingUnits {
  std::vector<Lit> units;  // first-recorded order (assert after the round)
  std::vector<Lit> by_var; // INVALID_LIT = none recorded yet
  enum class Outcome { Recorded, Duplicate, Refuted };
  void init(uint32_t nvars) {
    units.clear();
    by_var.assign(nvars, INVALID_LIT);
  }
  Outcome record(Kernel &, Lit u, uint64_t id);
};

// Clause-replacement tail: after the caller has
// emitted the shrunken clause's proof ADD under `id`, delete the original
// (proof `del` + garbage mark) and materialize `lits` by size. Unit handling
// stays with the CALLER (rules differ: Vivify asserts + ends its round,
// Substitute batches via PendingUnits); the empty clause refutes here. Large
// clauses carry (redundant, glue) — glue is a redundant-clause concept, so
// callers pass 0 for irredundant (the divergence the review aligned).
enum class ReplaceOutcome { Refuted, Unit, Binary, Large };
ReplaceOutcome kernel_replace_clause(Kernel &, Clause *orig, uint64_t id,
                                     const Lit *lits, uint32_t size,
                                     bool redundant, uint32_t glue,
                                     Ref *large_ref); // nullable

// --- BVE retry filter (see Kernel::bve_retry) -------------------------------
inline void kernel_mark_bve_retry(Kernel &k, Var v) {
  k.bve_retry[v] = 1;
  k.stats.bve_marks++; // see Stats::bve_marks — EVENTS, not bit flips
}
// Fold every ROOT-FIXED literal not yet accounted for into `bve_retry`: fixing
// y changes the effective content of every clause y occurs in (the ones y
// satisfies become delete-only, the rest lose the falsified literal), so every
// OTHER variable in those clauses may now be eliminable where it was not.
// Incremental and idempotent — `bve_retry_fixed_head` is the trail index up to
// which this has been done, and the root prefix of the trail never shrinks
// (kernel_backtrack compacts kept level-0 literals down but never touches the
// prefix below its boundary), so the watermark is monotone. Costs O(the new
// fixed literals' occurrences) with `dense` attached, one arena scan without.
void kernel_bve_retry_sync_fixed(Kernel &);

// --- touched-epoch substrate (see Kernel::touched) --------------------------
inline void kernel_touch_var(Kernel &k, Var v) {
  k.touched[v] = ++k.touch_epoch;
  k.stats.touch_marks++; // EVENTS; a stamp has no flip to count
}
// The deletion side (see Kernel::removed). Shares the clock, so the two are
// directly comparable against one watermark.
inline void kernel_removed_var(Kernel &k, Var v) {
  k.removed[v] = ++k.touch_epoch;
  k.stats.touch_marks++;
}
// Has v changed since the watermark `since`?
inline bool kernel_touched_since(const Kernel &k, Var v, uint64_t since) {
  return k.touched[v] > since;
}
// cadical's `likely_to_be_kept_clause` (internal.hpp:1059-1068), the scope
// predicate its whole `mark_added` family branches on. THREE disjuncts, and the
// third is the one we had no counterpart for until the reduce survivor envelope
// was published: irredundant · within the live tier2 · within the widest
// (glue, size) that survived the last reduction. Stated on the raw fields so
// `kernel_add_large_clause` can call it before the Clause exists.
inline bool kernel_likely_kept(const Kernel &k, bool redundant, uint32_t glue,
                               uint32_t size) {
  return !redundant || glue <= k.tier2 ||
         (k.kept_glue && glue <= k.kept_glue && size <= k.kept_size);
}
// ... by an ADD or by a DELETION. BCE's predicate: a clause becomes blocked on
// l either because it appeared or because an opposite occurrence went away.
inline bool kernel_changed_since(const Kernel &k, Var v, uint64_t since) {
  return k.touched[v] > since || k.removed[v] > since;
}
// Is clause `c` worth re-examining by a clause-rewriting rule whose watermark
// is `since`? (kissat forward.c:143-160, the `subsume < 2` test over
// flags->subsume.) Two independent reasons, both necessary in general:
//   (a) TWO OR MORE of c's variables touched. That is exactly the condition for
//       a new clause D with vars(D) ⊆ vars(c) and |D| >= 2 to exist — i.e. for
//       a new subsumer or self-subsuming partner. A c whose variables are all
//       older than the watermark was already checked against every clause that
//       can be built out of them.
//   (b) c holds a ROOT-FALSIFIED literal whose variable was fixed since the
//       watermark. That literal is absorbed by its unit, so c is effectively
//       shorter than when it was last checked and an OLD clause can cover it
//       now. Reading the case off the fixed variable itself is what lets
//       kernel_touch_sync_fixed skip the occurrence walk.
// This is a POLICY predicate about what to TRY. It must never appear in a
// guard or narrow an `instances()` enumeration (N1) — see the N1 note in
// subsume_forward.cpp.
// `need` is how many touched variables count as new material: 2 for the
// subsumption family (the argument above), 1 for a rule whose outcome on c can
// turn on a single changed variable — Instantiate, whose probe is a
// propagation and so depends on any clause over any of c's variables.
inline bool kernel_clause_new_material(const Kernel &k, const Clause *c,
                                       uint64_t since, size_t need = 2) {
  size_t hits = 0;
  for (const Lit l : *c) {
    if (!kernel_touched_since(k, var_of(l), since))
      continue;
    if (k.value(l) < 0)
      return true; // (b)
    if (++hits >= need)
      return true; // (a)
  }
  return false;
}
// Fold every ROOT-FIXED variable not yet accounted for into `touched`. Only the
// fixed variable itself is stamped — NOT the variables it shares clauses with,
// which is where this deliberately stops short of kernel_bve_retry_sync_fixed
// and of its occurrence walk. What the consumers need from a root fix is
// "clause C holds a literal that is now absorbed", and C's own membership test
// can read that off the fixed variable directly (`value(l) < 0 &&
// kernel_touched_since(k, var_of(l), w)`), so the neighbours never have to be
// enumerated. Cost is O(the new fixed literals), not O(their occurrences).
// Incremental and idempotent; the head is monotone for the same reason
// `bve_retry_fixed_head` is.
void kernel_touch_sync_fixed(Kernel &);

// --- clauses ---------------------------------------------------------------
// size >= 3; watches lits[0] and lits[1]. `id` is the proof clause id
// (0 in id-free unit tests; solver paths pass ingest input ids or
// ++next_clause_id for derived clauses)
Ref kernel_add_large_clause(Kernel &, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue, uint64_t id = 0);
// `id` is the pair's proof clause id; a re-add overwrites a previous one (see
// the binary_ids invariant above)
void kernel_add_binary_clause(Kernel &, Lit a, Lit b, uint64_t id = 0);
// Delete binary (a v b): drop both mirror watch entries, retire a live ROOT
// reason to UNIT_REASON (the binary analog of corpus 003 — a root literal this
// binary propagated stays assigned as a root unit, already justified by its
// unit id), and emit the proof `delete`. binary_ids keeps the key, so the map
// stays size-monotone (see the binary_ids invariant above). Returns false and
// does nothing if the binary is
//   - absent (idempotent), or
//   - DUPLICATED — two live copies of a pair share one binary_ids id, and the
//     `del` would retire it out from under the survivor (an LRAT-only hazard:
//     DRAT deletes by literals, so it stays consistent), or
//   - a live reason for a NON-root literal (never at a quiescent root; keeps
//     direct/off-root callers safe).
// Redundancy of (a v b) is the CALLER's proof obligation, not checked here.
bool kernel_delete_binary(Kernel &, Lit a, Lit b);
// DRAT-only: remove EVERY copy of pair (a,b) (E2 Factor grid removal; see impl).
bool kernel_delete_binary_all_copies(Kernel &, Lit a, Lit b);

// --- watch repair (TB4b) ---------------------------------------------------
// The kernel could ADD a large-clause watch (WatchLists::push_large) but never
// REMOVE one without deleting the clause; every rule that needed to re-point a
// watch had to allocate a fresh clause instead (otfs_strengthen, Strengthen,
// Vivify). These two primitives close that gap. They mutate WHICH literals a
// live clause watches and nothing else: membership, size, id, redundancy and
// arena position are untouched, so the proof stream, the dense occurrence
// index, every recorded Ref and every LRAT chain stay valid. Sound for the
// same reason the propagation engine's own watch relocation is: two-watched-
// literal completeness constrains only which literals are watched, and the
// caller owns re-establishing it (see kernel_promote_watched_lit's contract).
//
// PRECONDITIONS (both):
//   - `ref` is a live (non-garbage) large clause currently watched by the
//     literal named — i.e. that literal is lits[0] or lits[1];
//   - NO propagation scan is in flight (`delayed` empty): a scan holds raw
//     pointers into the very slice these compact, and a push can relocate any
//     list (watch.hpp);
//   - NOT inside a kernel_probing_begin/end scope — the probing rollback log
//     records only the scan's own edits, so an unlogged watch move would
//     survive kernel_probing_end.
//
// Remove the large-clause watch of `ref` from watches[l]: the exact inverse of
// WatchLists::push_large (kissat watch.h kissat_unwatch_blocking). In-place
// compaction of l's slice — no growth, so no other list moves.
void kernel_unwatch_large(Kernel &, Lit l, Ref ref);

// Move clause `ref`'s literal at position `from` (>= 2) into WATCHED slot
// `slot` (0 or 1); the displaced literal takes position `from` and the watch
// follows the promoted literal (kissat analyze.c's unwatch / swap / watch
// triple). The caller is responsible for the two-watched-literal invariant it
// leaves behind: a false watched literal at level d must be paired with a
// watch that is TRUE at level <= d, or an unassigning backtrack can strand the
// clause unit with no watch to fire on (regression corpus 002/005).
void kernel_promote_watched_lit(Kernel &, Ref ref, uint32_t slot,
                                uint32_t from);

// Mark a large clause deleted. THE single home for large-clause deletion: every
// rule that retires a clause goes through here, so the bookkeeping that must
// accompany a deletion (today: the BVE retry filter) has exactly one place to
// live instead of ~10 open-coded `flags |= GARBAGE` sites. Deletion stays LAZY
// (every reader skips garbage; kernel_collect_garbage reclaims later), and the
// PROOF `delete` remains the caller's — a rule owns its proof delta (§5.3), and
// several callers must emit it before the clause's literals are overwritten.
// Idempotent: a second mark of the same clause does nothing. Header-inline
// because Reduce retires clauses by the hundred thousand.
inline void kernel_mark_garbage(Kernel &k, Clause *c) {
  if (c->garbage())
    return; // idempotent
  c->flags |= Clause::GARBAGE;
  // BVE retry filter: losing an irredundant clause shrinks the elimination
  // neighbourhood of every variable in it, so each becomes retry-worthy
  // (kissat clause.c mark_clause_as_garbage -> kissat_mark_removed_literals).
  // Redundant clauses are skipped: BVE never resolves them, and Reduce's
  // learned-clause traffic would otherwise mark every variable every round.
  if (!c->redundant())
    for (const Lit l : *c) {
      kernel_mark_bve_retry(k, var_of(l));
      kernel_removed_var(k, var_of(l)); // BCE's side of the substrate
    }
}

inline uint64_t kernel_binary_key(Lit a, Lit b) {
  const Lit lo = a < b ? a : b, hi = a < b ? b : a;
  return (static_cast<uint64_t>(lo) << 32) | hi;
}
inline uint64_t kernel_binary_id(const Kernel &k, Lit a, Lit b) {
  const auto it = k.binary_ids.find(kernel_binary_key(a, b));
  return it == k.binary_ids.end() ? 0 : it->second;
}

// --- assignment / trail ----------------------------------------------------
// Header-inline (kissat fastassign.h pattern): propagation executes an
// assignment per propagated literal, and a cross-TU call there costs
// measurably at 10^6+ props/sec — the store sequence must inline into the
// watch-scan loop.
// Has a rule rewritten v out of the formula? (Empty status vector == none.)
inline bool kernel_var_retired(const Kernel &k, Var v) {
  return v < k.var_status.size() && k.var_status[v] != VarStatus::Active;
}

// SCHED-B2: the ONE place a tick is charged. Adds to the global counter (the
// search/mode machinery reads it) and, when the policy has scoped a sink around
// an apply, to that rule's private counter as well. Routing every site through
// here is what stops the two drifting: a new charge site cannot accidentally be
// invisible to per-rule metering, because there is nowhere else to charge.
inline void kernel_charge_ticks(Kernel &k, uint64_t n) {
  k.stats.ticks += n;
  if (k.tick_sink)
    *k.tick_sink += n;
}

inline void kernel_assign(Kernel &k, Lit lit, uint32_t level,
                          uint32_t reason, bool binary, bool jumped = false) {
  assert(k.values[lit] == 0);
  const Var v = var_of(lit);
  k.values[lit] = 1;
  k.values[negate(lit)] = -1;
  k.assigned[v] = {level, static_cast<uint32_t>(k.trail.size()), reason,
                   binary, jumped};
  k.trail.push_back(lit);
  assert(k.unassigned > 0);
  k.unassigned--;
  if (!kernel_var_retired(k, v)) {
    assert(k.unassigned_active > 0);
    k.unassigned_active--;
  }
}
inline void kernel_assign_decision(Kernel &k, Lit lit) { // level++ inside
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}
inline void kernel_assign_unit(Kernel &k, Lit lit) { // root level
  kernel_assign(k, lit, 0, UNIT_REASON, false);
}
// True-level assignment (calculus D-LEVEL): a propagation-forced literal is
// stamped at its TRUE level — the max decision level among its reason clause's
// other (falsified) literals — not the current decision level. This is what
// lets a chronological backtrack keep a still-implied literal without stranding
// it. Transliterated from kissat inlineassign.h assignment_level. It is inert
// while chrono is off (at a propagation fixpoint on a level-sorted trail the
// true level always equals the current level), and load-bearing once chrono is
// on, which it is by default.
inline void kernel_assign_binary(Kernel &k, Lit lit, Lit other_false) {
  const Assigned &a = k.assigned[var_of(other_false)];
  const uint32_t level = a.level;
  // Reason jumping (kissat assign.c:38 / fastassign.h:12): record the
  // resolvent of (lit ∨ other_false) and ¬other_false's own binary reason.
  // `level` stays the level of `other_false` — equal to its reason's, since a
  // binary propagation inherits the level of the literal that forced it.
  // Chains are allowed (a jumped reason may itself be jumped); every LRAT
  // consumer unwinds them through `jumped_over` — `kernel_reason_walk` on its
  // own stack, Analyze via `chain_expand_jumped`.
  if (k.jump_reasons && level && a.binary) {
    k.jumped_over[var_of(lit)] = other_false;
    k.stats.jumped_reasons++;
    kernel_assign(k, lit, level, a.reason, true, true);
    return;
  }
  kernel_assign(k, lit, level, other_false, true);
}
inline void kernel_assign_ref(Kernel &k, Lit lit, Ref reason) {
  const Clause *c = clause_at(k.arena, reason);
  const Var v = var_of(lit);
  uint32_t lvl = 0;
  for (const Lit m : *c)
    if (var_of(m) != v) {
      const uint32_t ml = k.assigned[var_of(m)].level;
      if (ml > lvl)
        lvl = ml;
    }
  kernel_assign(k, lit, lvl, reason, false);
}
// update_phases=false is the probing variant (kissat backtrack.c
// kissat_backtrack_without_updating_phases): probe assignments must not
// pollute saved/target/best phases
void kernel_backtrack(Kernel &, uint32_t target_level,
                      bool update_phases = true);

// core propagation engine (kissat proplit.h; a reviewed §5.3 kernel-primitive
// rather than part of the Propagate rule): propagates the trail to fixpoint or
// first conflict, recording the conflict in the handoff fields; returns true
// iff a conflict was found. Updates propagations/ticks, NOT conflicts — the
// Propagate rule owns the ApplyResult/proof semantics and the conflict count,
// while the probing rules reuse this engine directly.
// `ignore`: optional clause ref excluded from propagation (CaDiCaL's
// ignore pointer — for rules probing a candidate's own literals; see
// propagate_literal in kernel.cpp). INVALID_REF = normal propagation.
bool kernel_propagate(Kernel &, Ref ignore = INVALID_REF);

// binary-only propagation to fixpoint or first conflict (kissat backbone.c):
// follows ONLY the binary-implication graph — no large-clause propagation,
// no watch mutation. Backbone's engine; cheaper than kernel_propagate and
// derives a subset of its units. Same conflict-handoff semantics.
bool kernel_propagate_binary(Kernel &);

// --- probing propagation (kissat proprobe.c) --------------------------------
// FULL (large + binary) propagation for the probing rules that leaves search's
// bookkeeping alone: same engine, same implied literals, same conflict as
// kernel_propagate, but the watch-list updates and per-clause `searched` hints
// it makes along the way are recorded and rolled back at the end of the scope.
// Motivation, and it is NOT a micro-optimisation: a probing round that derives
// NOTHING still relocates watches and rewrites `searched`, and on a fragile
// instance that content-free perturbation alone can cost an order of magnitude
// in conflicts — with the formula bit-identical afterwards.
//
// Usage — one scope per speculative episode:
//     kernel_probing_begin(k);                   // at a quiescent point
//     ... assume(); kernel_propagate_probing(k, ignore); ...
//     kernel_backtrack(k, base_level, false);    // unwind the assumptions
//     kernel_probing_end(k);                     // restore, THEN mutate the formula
//
// CONTRACT (all three are checked, not assumed):
//  * the restore only fires when trail size / propagate_head / level are back
//    at the scope's entry values. Any other state (e.g. a probing-derived
//    literal that turned out to be root-implied and was KEPT by the backtrack)
//    means the speculation left a permanent mark, so the log is DISCARDED and
//    the updates stay — i.e. exactly today's behaviour, always sound.
//  * no clause may be added, deleted, or relocated (GC) inside a scope: the log
//    holds Refs and watch-list contents. Both rules call end() before rewriting.
//  * kernel_propagate_probing outside a scope is plain kernel_propagate.
void kernel_probing_begin(Kernel &);
void kernel_probing_end(Kernel &); // idempotent; a no-op outside a scope
bool kernel_propagate_probing(Kernel &, Ref ignore = INVALID_REF);

// (VMTF / EVSIDS / phase-snapshot `H` operations live in
// policies/heuristics.hpp.)

// number of root-fixed assignments (the level-0 trail prefix)
// §6: has an external interrupt arrived? Rules poll this at their own safe
// abort points (see Kernel::stop). Cheap enough for a coarse cadence inside a
// long rule loop; never call it per-literal.
inline bool kernel_terminating(const Kernel &k) {
  // STRESS MODE (`TACTIX_STOP_FUZZ=n`, diagnostic): report "terminating" on a
  // deterministic 1-in-n cadence so rules abort at MANY different interior
  // points, while the POLICY's own `stop` (a separate parameter it never reads
  // through here) stays clear — so the solve still runs to a real verdict and
  // the fuzz oracles can check it. This is what makes "aborting a rule
  // mid-application is sound" a TESTED claim rather than an argued one: the
  // abort points are new control flow, and the risk they carry is leaving
  // kernel state or the proof stream inconsistent.
  if (k.stop_fuzz) {
    // LCG, deterministic in the seed — same input + same knob ⇒ same trace.
    k.stop_fuzz_state = k.stop_fuzz_state * 6364136223846793005ull + 1442695040888963407ull;
    if ((k.stop_fuzz_state >> 33) % k.stop_fuzz == 0) {
      k.stats.stop_fuzz_fires++;
      return true;
    }
  }
  return k.stop && *k.stop;
}

// RAII publisher for Kernel::stop — a policy's solve() publishes its interrupt
// for the duration of the call and always withdraws it, so an early return can
// never leave a dangling pointer in the kernel. Restores the previous value
// rather than nulling, so nested/reentrant solves compose.
struct KernelStopPublisher {
  Kernel &k;
  volatile sig_atomic_t *saved;
  KernelStopPublisher(Kernel &kk, volatile sig_atomic_t *s)
      : k(kk), saved(kk.stop) {
    k.stop = s;
  }
  ~KernelStopPublisher() { k.stop = saved; }
  KernelStopPublisher(const KernelStopPublisher &) = delete;
  KernelStopPublisher &operator=(const KernelStopPublisher &) = delete;
};

inline uint32_t kernel_root_fixed(const Kernel &k) {
  return k.level == 0 ? static_cast<uint32_t>(k.trail.size()) : k.frames[1];
}

// assemble k.lrat_chain from k.lrat_entries: antecedent ids sorted by
// trail position ascending — the order that unit-propagates for the
// checker by construction — with `closing_id` (the conflicting/falsified
// clause, or the unit's reason) last. Shared by every chain-emitting
// rule so the ordering invariant is defined exactly once.
void kernel_lrat_chain(Kernel &, uint64_t closing_id);

// LRAT reason walk for probing rules (C3 Vivify, V1 Probe, V2 Backbone).
// Pure kernel-state read: collect (trail position, antecedent id) for every
// reason reachable from `start` down to root units and probe assumptions
// (assumptions are DECISION_REASON and skipped — they are the negation of the
// emitted clause). Ascending trail position unit-propagates for the checker;
// the caller closes the chain with the conflicting/final hint. `seen` is a
// caller-owned scratch (a var-indexed byte vector), so the walk never
// allocates and consecutive calls can share/dedup one buffer. When `touched`
// is non-null every newly-marked var is recorded there, so a PERSISTENT
// `seen` can be restored all-zero in O(marked) instead of a fresh
// O(nvars) zero-fill per walk (C4-perf: that memset was 97 % of ALL cycles
// on the hwmcc native-LRAT path — one per replayed ladder lemma; probe
// 1563224).
void kernel_reason_walk(Kernel &, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &entries,
                        std::vector<Var> *touched = nullptr);
// Seed the walk from the recorded conflict (binary in conflict_bin, else the
// large clause at conflict_ref): walk every conflicting literal's reason and
// return the conflicting clause's id (the RUP chain's closing hint). The
// probe-conflict case of every probing rule.
uint64_t kernel_conflict_reason_walk(
    Kernel &, std::vector<uint8_t> &seen,
    std::vector<std::pair<uint32_t, uint64_t>> &entries,
    std::vector<Var> *touched = nullptr);

// (decision-ranking dispatch — heuristics_next_decision_var /
// heuristics_decide_lit — lives in policies/heuristics.hpp.)

// --- garbage collection ------------------------------------------------
// compacts the arena (dropping GARBAGE clauses), fixes trail reasons via
// forwarding pointers, and rebuilds large watches (binaries kept in place)
void kernel_collect_garbage(Kernel &);

// Dense simplification phase (increment 2). connect builds `occ.large` from the
// current irredundant large clauses (one arena scan) and attaches it as
// `k.dense`; disconnect detaches it (the caller owns the storage). While
// attached, kernel_add_large_clause pushes new irredundant large clauses and
// kernel_collect_garbage rebuilds the index after relocating Refs. Precondition:
// level 0 (a quiescent root), like kissat's dense mode.
void kernel_dense_connect(Kernel &, OccIndex &occ);
void kernel_dense_disconnect(Kernel &);

// irredundant clause count: live irredundant large clauses + all binaries
// ever added (binaries have an irredundant lifecycle in this kernel — never
// reduced, and deleted only by V4 TransitiveReduce, whose deletions are
// equivalence-preserving; since binary_ids keeps a deleted binary's id, the
// binary term over-approximates the live count by the deleted ones — the
// log-scale consumers below tolerate it, and V4 is not in the default
// schedule). The kissat BINIRR_CLAUSES
// equivalent: feeds inprocessing interval scaling (kimits.c
// kissat_scale_delta) and TACTIX_STAT_IRREDUNDANT_CLAUSES. O(arena) walk —
// a per-round/statistics query, never a hot-path one.
uint64_t kernel_irredundant_clauses(const Kernel &);

// --- internal invariants (plan §8.2 oracle 4; debug builds) --------------
// aborts on: values/trail/unassigned incoherence, frame/level corruption,
// watch-list damage, or a missed conflict (fully propagated state with a
// falsified clause). No-op in NDEBUG builds.
void kernel_check_invariants(const Kernel &);

} // namespace tactix

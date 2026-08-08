// Kernel state (plan §5): owns representation only — no policy knowledge,
// no rule logic. One flat struct, index-based arrays, C-style discipline.
// Layout decisions transliterated from kissat/src/internal.h (MIT).
#pragma once

#include "kernel/clause.hpp"
#include "kernel/types.hpp"
#include "kernel/watch.hpp"
// A full include, not a forward declaration: `kernel_delete_clause` inlines the
// `del` it emits on the conflict-analysis path. proof.hpp is a leaf.
#include "proof/proof.hpp"

#include <csignal>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {

struct FactorMatchPolicy;
// Search-guidance state `H` (VMTF queue, EVSIDS heap, phases), held opaquely by
// Kernel; the struct and its operations live in policies/heuristics.hpp.
struct Heuristics;

// per-variable assignment metadata (kissat/src/assign.h)
struct Assigned {
  uint32_t level;
  uint32_t trail; // position on the trail
  uint32_t reason; // Ref, or other literal if binary, or sentinels
  bool binary;
  // Reason jumping (kissat `jumpreasons`, assign.c/fastassign.h): the binary
  // reason above is then a virtual resolvent — (lit ∨ reason) is not in the
  // database. `Kernel::jumped_over` names the literal resolved away; the real
  // antecedents are (lit ∨ jumped_over) and the reason of ¬jumped_over. Never
  // set while `Kernel::jump_reasons` is false, and never at level 0.
  bool jumped;
};

// VMTF queue links (kissat/src/queue.h)
struct Link {
  uint32_t prev, next, stamp;
};

struct Stats {
  // Counters only: nothing here is read by the kernel, by a guard, or by an
  // `instances()` enumeration, so no trajectory can depend on a statistic.
  // Many come in PAIRS — an "attempted" beside a "fired" — because one count
  // cannot separate "the site never ran" from "it ran and did nothing", and a
  // fuzz leg over an unreached site is vacuous. Read such a pair both ways:
  // all-fired and never-fired are both defects.
  uint64_t decisions = 0;
  uint64_t conflicts = 0;
  uint64_t propagations = 0;
  uint64_t ticks = 0; // propagation memory-traffic proxy (see propagate.cpp)
  // Of those, the ones charged OUTSIDE a simplification phase (kissat
  // `statistics.search_ticks`, propsearch.c:24). Maintained here rather than
  // reconstructed by a policy subtraction, because it is the firewall that
  // stops a technique inflating its own next budget: every effort limit is a
  // fraction of recent SEARCH work, so a technique whose spend re-entered the
  // reference would compound. Monotone by construction.
  uint64_t search_ticks = 0;
  uint64_t inprocess_propagations = 0; // of those, inside a simplification
                                       // phase; policy-written at phase exit
  // --- wall-clock accounting (diagnostic) -----------------------------------
  // No trajectory may depend on a clock, so runs stay bit-reproducible. The
  // search path (Propagate/Decide/Analyze) is deliberately untimed — a
  // per-conflict clock call is a hot-path cost N3 cannot afford — so search
  // time is derived as solve_nanos minus everything measured here.
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
  uint64_t walks = 0;         // S6 Walk applications (policy op)
  uint64_t walk_flips = 0;    // literal flips
  uint64_t walk_steps = 0;    // occurrence entries scanned (kissat walk_steps)
  uint64_t walk_improved = 0; // walks that lowered the unsatisfied count
  // Unsatisfied count the LAST walk reached; the write is an assignment, not a
  // `min`. No consumer yet by design: reserved as the phase-6 Walk-dosing
  // reward proxy, so it is not dead code.
  uint64_t walk_minimum = UINT64_MAX;
  uint64_t promoted = 0;     // glue promotions on use (kissat deduce.c)
  uint64_t learned = 0;
  uint64_t learned_units = 0;
  uint64_t learned_lits = 0;      // sum of FINAL learned clause sizes
  uint64_t learned_glue_sum = 0;  // sum of learned glues
  uint64_t minimized = 0; // literals removed by clause minimization
  uint64_t shrunken = 0;  // literals removed by all-UIP block shrinking
  uint64_t otfs_strengthened = 0; // reasons strengthened on the fly
  uint64_t otfs_subsumed = 0;     // conflicts subsumed by the resolvent
  uint64_t eager_tried = 0;     // containment tests run (kissat eagertried)
  uint64_t eager_subsumed = 0;  // deletions (kissat eagerly_subsumed)
  uint64_t eager_reason_skipped = 0; // subsumed but kept — still a trail reason
  // shrinks where the post-shrink clamp promotes a band (kissat promote.c);
  // counted only when new_size-1 and new_size differ, so it is the arm's
  // reachability proof — see kernel_shrunken_glue
  uint64_t shrink_promoted = 0;
  // variables the initial decision order was seeded from (kissat `tumble`); 0
  // means the seeding never ran, so its arm is untested
  uint64_t tumble_seeded = 0;
  // glue histogram of clauses used in conflict analysis, per search mode
  // (kissat statistics.h used[2].glue): Reduce's dynamic-tier input.
  static constexpr uint32_t MAX_GLUE_USED = 127;
  uint64_t used_glue[2][MAX_GLUE_USED + 1] = {};
  uint64_t retiered = 0;           // dynamic tier recomputations
  // Redundant clauses a `likely_to_be_kept`-style consumer (cadical
  // internal.hpp:1059) admits under the live tier2 vs the static default 6.
  // The two differing is the precondition for wiring any further consumer.
  uint64_t tier2_band_dynamic = 0;
  uint64_t tier2_band_static = 0;
  // Reduce's whole-arena enumeration walk, and the leading run a kissat
  // `first_reducible` watermark (reduce.c:161) would skip — a bound not
  // implemented here, since that skip is large only because kissat's collector
  // partitions the arena (collect.c:213) while this one preserves order.
  uint64_t reduce_scans = 0;
  uint64_t reduce_scan_words = 0;
  uint64_t reduce_skippable_words = 0;
  uint64_t reason_bumped = 0;      // extra vars marked by reason-side bumping
  uint64_t reason_bump_rounds = 0; // conflicts the reason-side pass ran on
  uint64_t reason_bump_aborted = 0; // rounds rolled back at the mark limit
  uint64_t api_decisions = 0; // externally chosen branches (N1)
  uint64_t input_restated = 0; // input clauses restated under a fresh id
  uint64_t decisions_extension = 0; // decisions on a Factor extension variable
  uint64_t decisions_substituted = 0; // ... on one Substitute rewrote away
  mutable uint64_t rule_interrupted = 0; // applications aborted on the
                                         // interrupt (plan §6)
  mutable uint64_t stop_fuzz_fires = 0;  // stops reported by the stress knob,
                                         // counted at the source
  uint64_t bumps = 0;                // bump events (both modes, both channels)
  // Which channel re-promotes extension variables: a high reason-side share is
  // a mechanism gap with a targeted fix, a high conflict-side share says the
  // choice of factorings is the lever instead.
  uint64_t bumps_extension = 0;      // ...of which landed on an extension var
  uint64_t bumps_ext_reason_only = 0; // ...of those, reason-side ONLY
  uint64_t restarts = 0;
  uint64_t restarts_reused = 0; // restarts that kept a trail prefix
  uint64_t reductions = 0;
  uint64_t deleted = 0; // learned clauses deleted by Reduce
  uint64_t binaries_deleted = 0; // binary clauses deleted (kernel_delete_binary)
  uint64_t forward_subsumptions = 0; // SubsumeForward rounds
  uint64_t forward_checked = 0;  // SubsumeForward candidates checked
  // Partner visits — cadical's `stats.subchecks` (subsume.cpp:86), the unit its
  // budget is denominated in. `*_checked` counts clauses instead, which
  // under-prices exactly the expensive candidates, so a round metered on
  // `*_checked` can own most of a solve without exceeding its budget.
  uint64_t forward_visits = 0;
  uint64_t forward_subsumed = 0; // clauses deleted by SubsumeForward
  uint64_t strengthen_rounds = 0;  // Strengthen rounds
  uint64_t strengthen_checked = 0; // Strengthen candidate checks
  uint64_t strengthen_visits = 0;  // partner visits; see forward_visits
  uint64_t strengthened = 0;       // clauses strengthened (add+delete)
  uint64_t strengthen_subsumed = 0; // deletions by Strengthen's fused round
  uint64_t budget_stops_c1 = 0; // SubsumeForward rounds cut by `work_budget`
  uint64_t budget_stops_c2 = 0; // Strengthen rounds cut by `work_budget`
  // Interrupt attribution for the C1/C2 share. A different axis from the budget
  // pair above: `work_budget` is 0 whenever the policy leaves a round unmetered,
  // so budget-awareness never implied interruptibility.
  uint64_t subsume_interrupted = 0;
  uint64_t strengthen_interrupted = 0;
  // Shadow ledger (diagnostic; see `Kernel::subsume_shadow`). Absorption by any
  // rule other than Strengthen is deliberately not counted, so
  // `observed - (deleted + rewritten)` bounds C1's unique yield from above.
  uint64_t c1_shadow_observed = 0;  // clauses C1 would have deleted but did not
  uint64_t c1_shadow_deleted = 0;   // C2 deleted the same clause outright
  uint64_t c1_shadow_rewritten = 0; // C2 strengthened it away (old id retired)
  uint64_t flushed = 0; // root-satisfied deletions + root-falsified literal
                        // strips by Strengthen's flush pre-pass
  uint64_t vivify_rounds = 0;  // Vivify rounds
  uint64_t vivify_checked = 0; // Vivify candidates probed
  uint64_t vivified = 0;       // clauses distilled by Vivify (add+delete)
  uint64_t vivify_deleted = 0; // root-satisfied deletions by Vivify
  // Trail-reuse readout, counted unconditionally so a reuse rate of zero is
  // reportable (docs/design/2026-08-05-vivify-trail-reuse.md §4).
  uint64_t vivify_reused = 0;     // levels taken from the retained trail
  uint64_t vivify_probes = 0;     // assumptions made; reuse rate = reused/both
  uint64_t vivify_reuse_full = 0; // candidates wholly covered by the prefix
  uint64_t vivify_tier3 = 0;      // tier3 candidates probed; 0 = that half of
                                  // the scope is inert
  uint64_t vivify_prioritized = 0;   // candidates carrying the leftover bit
  uint64_t vivify_reprioritized = 0; // rounds whose leftover set was empty
  uint64_t vivify_polls = 0;      // kernel_terminating evaluations at the site
  uint64_t vivify_terminated = 0; // rounds cut short by one
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
  // Per-rule private tick meters; the policy scopes `tick_sink` to one of these
  // around that rule's apply so its budget cannot be spent by another.
  uint64_t probe_ticks = 0;        // Probe's own propagation ticks
  uint64_t backbone_ticks = 0;     // Backbone's own
  uint64_t vivify_ticks = 0;       // Vivify's own
  uint64_t instantiate_ticks = 0;  // Instantiate's own
  uint64_t bce_ticks = 0;          // occurrences + pairs scanned — BCE's budget
  uint64_t instantiate_rounds = 0;  // Instantiate rounds that strengthened
  uint64_t instantiate_checked = 0; // (clause, literal) pairs probed
  uint64_t instantiated = 0;        // literals removed by Instantiate

  // Covered clause elimination (src/rules/cce/cce.hpp). `cce_cla` is its
  // witness: a leg with zero covered literals added exercised only BCE's
  // blockedness test.
  uint64_t cce_rounds = 0;   // rounds attempted; nothing may read it — a
                             // rotation on it is the defect the touched-epoch
                             // resume replaced
  uint64_t cce_tried = 0;    // candidate clauses extended
  uint64_t cce_deleted = 0;  // clauses eliminated
  uint64_t cce_blocked = 0;  // ... of which by the blocked exit (witnessed)
  uint64_t cce_ata = 0;      // ... of which by the asymmetric-tautology exit
  uint64_t cce_ala = 0;      // asymmetric literals added
  uint64_t cce_cla = 0;      // covered literals added (CCE's own mechanism)
  uint64_t cce_ticks = 0;    // clause literals scanned — CCE's budget currency

  // Sweep (src/rules/sweep/sweep.hpp); `sweep_environments` is its witness —
  // zero means the leg never entered the detector.
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
  // Environment shape and candidate-solve outcomes: a zero yield has several
  // mechanisms (too weak an environment, a candidate set with no true pair, a
  // sub-solver out of budget) that `equivalences 0` cannot tell apart.
  uint64_t sweep_env_lits = 0;      // literal occurrences encoded
  uint64_t sweep_env_false = 0;     // of those, root-FALSE occurrences
  uint64_t sweep_env_freefalse = 0; // of those, with no unit id => free var
  uint64_t sweep_env_stripped = 0;  // root-false occurrences REMOVED
  uint64_t sweep_strip_cited = 0;   // ... with their root units cited in an
                                    // LRAT chain. The strip is the only part of
                                    // Sweep's proof obligation that moved, so 0
                                    // here voids an LRAT fuzz leg
  uint64_t sweep_env_units = 0;     // unit clauses added alongside (off arm)
  uint64_t sweep_ticks_dedup = 0;   // the environment's clause-key scan
  uint64_t sweep_ticks_sub = 0;     // the sub-solver's own ticks
  // Environment closure: how much of an environment variable's neighbourhood
  // the environment holds. Large clauses only, so both sides are one population.
  uint64_t sweep_occ_f = 0;         // env vars' large-clause degree in F
  uint64_t sweep_occ_env = 0;       // ... and inside the environment
  uint64_t sweep_vars_closed = 0;   // env vars holding their WHOLE degree
  uint64_t sweep_vars_total = 0;    // env vars
  uint64_t sweep_cand_seeded = 0;   // candidates entering the loop
  uint64_t sweep_cand_refined = 0;  // candidates dropped by model refinement
  uint64_t sweep_dir_sat = 0;       // candidate directions that MODELLED
  uint64_t sweep_dir_unsat = 0;     // candidate directions that REFUTED

  uint64_t ternary_rounds = 0;     // HyperTernaryRes rounds that added clauses
  uint64_t ternary_resolvents = 0; // ternary resolvents added (redundant)
  uint64_t hbr_rounds = 0;      // X6 HyperBinaryRes: canonical rounds entered
  uint64_t hbr_probes = 0;      // root literals closed under propagation
  uint64_t hbr_binaries = 0;    // resolvents emitted
  uint64_t hbr_subsumed = 0;    // reasons deleted because the resolvent subsumed them
  // A resolvent is emitted only if the shared RUP replay re-derives it, so a
  // decline is the rule refusing to write a line it cannot back.
  uint64_t hbr_declined = 0;    // resolvents the RUP replay could not re-derive
  uint64_t hbr_interrupts = 0;  // termination polls that fired mid-round
  uint64_t hbr_ticks = 0;       // private meter (see Kernel::tick_sink)
  uint64_t ternary_binaries = 0;   // binary resolvents added (permanent)
  uint64_t ternary_ticks = 0;      // occurrence entries + pairs examined
  uint64_t transitive_ticks = 0;   // cache lines scanned by the walks (kissat
                                   // transitive_ticks, same unit as ticks)
  uint64_t transitive_deleted = 0; // transitively redundant binaries deleted
  uint64_t transitive_failed = 0;  // sources abandoned to Probe as failed (V4)
  uint64_t transitive_polls = 0;   // kernel_terminating evaluations at the site
  uint64_t transitive_terminated = 0; // dst walks cut by an interrupt
  uint64_t substitute_rounds = 0;  // Substitute rounds — detection passes
  uint64_t substitute_vars = 0;    // variables substituted away
  // Non-trivial SCCs from substitute_find_sccs and the literals in them, which
  // separate "never detected an equivalence" from "detected some and the caller
  // dropped them". Both 0 while congruence_merges is large means the
  // equivalence binaries are not in the BIG by the time Substitute walks it.
  uint64_t substitute_sccs = 0;
  uint64_t substitute_scc_lits = 0;
  // Rounds abandoned wholesale by do_substitution's step-(A) hazard pre-check:
  // a binary pair with >=2 live copies cannot be deleted by name, so the round
  // is skipped rather than half-applied. All-or-nothing while its rarity
  // assumption is per literal, hence the per-SCC form below, whose yield is the
  // `partial`/`lits` pair — rounds that survived with some classes poisoned.
  uint64_t substitute_hazard_skips = 0;
  uint64_t substitute_hazard_partial = 0;
  uint64_t substitute_hazard_lits = 0;
  uint64_t deactivations = 0;  // counted at the site, so 0 means "never fired"
  uint64_t reactivations = 0;  // ... of those, later re-inserted into a ranking
                               // structure, which silently undoes one
  uint64_t conflict_reused = 0; // conflicts resolved by reusing the conflict as
                                // the driving clause: no clause, no proof line
  uint64_t jumped_reasons = 0;  // binary reasons replaced by their resolvent
  uint64_t conflict_watch_repaired = 0; // ... of those, needing watch repair
  // Binary-only propagation scans, and what a binary-first watch order is worth
  // to them (kissat `stop_early`, backbone.c:186-193). `flagged` counts the
  // scans `binaries_watched_first` covered and `skipped` the words that break
  // saved; `ordered` and `large` describe the remainder, which is the ceiling
  // on what a per-list order bit could add over one global flag.
  uint64_t binprop_scans = 0;
  uint64_t binprop_ordered = 0;
  uint64_t binprop_flagged = 0;
  uint64_t binprop_words = 0;
  uint64_t binprop_skipped = 0;
  uint64_t binprop_large = 0;
  uint64_t chronological = 0;  // chrono backtrack targets taken
  // Pending clauses discarded because the asserting literal was still assigned
  // after the backtrack. Must stay 0: non-zero means conflicts are dropped and
  // re-derived, which wall-clock results hide.
  uint64_t backjump_stale = 0;
  uint64_t inprocess_phases = 0;   // policy inprocessing phases (a root descent
                                   // plus one dose per enabled technique)
  uint64_t congruence_rounds = 0;  // Congruence rounds — gate-closure passes
  // Merges dropped because no checkable LRAT chain could be built (a bridging
  // BIG path blocked by a root-assigned literal). Sound: the inference is lost,
  // never emitted unjustified. Expected 0, so non-zero is a real signal.
  uint64_t congruence_unjustified = 0;
  uint64_t congruence_units = 0; // root units from degenerate gates
  // The binary-resolution unit pre-pass, kept apart from `congruence_units`
  // because (l∨o),(l∨¬o) resolving to {l} is a different inference with no gate
  // involved. The same units are reachable through V1 Probe / V2 Backbone, so
  // reading only one of these counters mis-attributes them.
  uint64_t congruence_y1_found = 0; // detections
  uint64_t congruence_y1_units = 0; // ... emitted and batched
  // Extraction split by gate kind, mirroring kissat's congruent_gates_ands /
  // _ites / _xors; the aggregate cannot say which extractor produces an excess.
  mutable uint64_t congruence_gates_and = 0;
  mutable uint64_t congruence_gates_xor = 0;
  mutable uint64_t congruence_gates_ite = 0;
  uint64_t congruence_gates = 0;   // gates EXTRACTED, over all applications
  uint64_t congruence_merges = 0;  // equivalences MERGED, over all applications
  // Congruence's budget currency: scan steps over the clause / gate / worklist
  // structures, in the same unit as transitive_ticks and sweep_ticks, charged
  // unconditionally so `EFFORT_SPECS` can price a round. `mutable` as
  // `rule_interrupted` is: extraction takes the kernel by const reference.
  mutable uint64_t congruence_ticks = 0;
  mutable uint64_t congruence_budget_stops = 0; // rounds cut by `work_budget`
  uint64_t factorings = 0;         // Factor/BVA applications — grids replaced
  uint64_t factor_large_remainders = 0; // remainders offered by the widened
                                        // grid scope; 0 with factor_size >= 3
                                        // means that branch never fired
  uint64_t factor_demotions = 0;    // demotion calls that moved something
  uint64_t factor_demoted_vars = 0; // extension vars pushed to the queue front
  uint64_t factor_ticks = 0;   // Factor's budget currency (kissat
                               // statistics.factor_ticks, cache-line
                               // granularity); the factoring count cannot
                               // express the dominant seed-scan cost
  uint64_t bve_rounds = 0;         // BVE canonical-apply calls (a policy that
                                   // doses per instance never bumps this)
  uint64_t bve_eliminated = 0;     // variables eliminated by BVE resolution
  uint64_t bve_resolutions = 0;    // resolvents attempted — BVE's unit of work
  // Eager backward subsumption from fresh resolvents (cadical elimbackward).
  // `queued` is the reachability witness: zero means the pass never ran.
  uint64_t bve_bwd_queued = 0;      // fresh resolvents used as subsumers
  uint64_t bve_bwd_subsumed = 0;    // victims deleted (cadical elimbwsub)
  uint64_t bve_bwd_strengthened = 0; // victims shortened (cadical elimbwstr)
  uint64_t bve_bwd_units = 0;       // strengthenings that collapsed to a unit
  uint64_t flushed_retired_redundant = 0; // reference-parity flush at phase exit
  uint64_t factor_rejected_by_bound = 0; // grids the coupled bound declined
                                   // and its effort currency, since one
                                   // variable costs occ(x)·occ(¬x) attempts
  // One per `kernel_mark_bve_retry` EVENT. Deviation from the references, which
  // count the flag FLIP (kissat inline.h:44-49, cadical internal.hpp:1117)
  // because their elimination round re-arms it by clearing every scheduled
  // variable's flag. Here `bve_retry` is cleared only by a BVE attempt, and
  // during-search admission may decline every dose, after which no flip ever
  // happens and a flip-counting watermark would wedge the elimination clock.
  uint64_t bve_marks = 0;
  // Touched-epoch substrate events (Kernel::touched), for the same reason
  // bve_marks counts events: the stamp is monotone and never cleared.
  // `touch_kept_marks` is the redundant band's own counter, separate so that
  // turning the band on leaves every irredundant reading byte-identical.
  uint64_t touch_marks = 0;
  uint64_t touch_kept_marks = 0;
  // Round-candidate suppression by the touched-epoch filter, per consumer.
  // `*_skipped`: candidates whose variables had not moved since the round's
  // watermark, read against that rule's own attempt counter. `*_advances`:
  // rounds that ran to exhaustion and so advanced the watermark, against rounds
  // cut short that left it. `*_rearmed`: candidates a cut round never reached
  // and re-armed, so the next round starts at the tail, not the head.
  uint64_t forward_mark_skipped = 0;
  uint64_t strengthen_mark_skipped = 0;
  uint64_t forward_mark_advances = 0;
  uint64_t strengthen_mark_advances = 0;
  uint64_t forward_rearmed = 0;
  uint64_t strengthen_rearmed = 0;
  uint64_t bce_mark_skipped = 0, bce_rearmed = 0;
  uint64_t ternary_mark_skipped = 0, ternary_rearmed = 0;
  uint64_t instantiate_mark_skipped = 0, instantiate_rearmed = 0;
  // Interrupt attribution. `rule_interrupted` is not a total over every polling
  // rule: Congruence bumps it from raw `kernel_terminating` sites and Vivify
  // polls through its own pair without bumping it, so read it as "some rule
  // aborted", never as a sum.
  uint64_t bce_interrupted = 0;
  uint64_t ternary_interrupted = 0;
  uint64_t instantiate_interrupted = 0;
  uint64_t factor_mark_skipped = 0, factor_rearmed = 0;
  uint64_t subsume_holds = 0; // outer elimination rounds held open because
                              // SubsumeForward/Strengthen had not finished;
                              // predicted zero under the shipped DefaultPolicy
  // The redundant half of the touch substrate: `dynamic` counts redundant
  // creations stamped under the live `kernel_likely_kept`, `static6` the ones a
  // frozen tier2 of 6 with no survivor envelope would have stamped. So
  // `dynamic > 0` proves the stamps fire and `dynamic != static6` that they
  // move with `k.tier2`.
  uint64_t touch_redundant_dynamic = 0;
  uint64_t touch_redundant_static6 = 0;
  // The consumer half of the band, Strengthen alone. `red_checked > 0` proves
  // the widened scope admits anything, `red_skipped > 0` that the band filter
  // still fires inside it; the other two are the yield.
  uint64_t strengthen_red_checked = 0;
  uint64_t strengthen_red_skipped = 0;
  uint64_t strengthen_red_subsumed = 0;
  uint64_t strengthen_red_strengthened = 0;
  uint64_t cce_mark_skipped = 0, cce_rearmed = 0, cce_mark_advances = 0;
  // syntactic gates found per type, one per elimination attempt that had one
  // (kissat statistics.{equivalences,ands,if_then_else}_extracted / cadical
  // stats.elim{equivs,ands,ites,xors}).
  uint64_t bve_gates_equiv = 0;
  uint64_t bve_gates_and_or = 0;
  uint64_t bve_gates_ite = 0;
  uint64_t bve_gates_xor = 0;
  // R10's general definitions, extracted by the sub-solver rather than matched
  // (kissat statistics.definitions_{checked,extracted}).
  uint64_t bve_definitions_checked = 0; // attempts
  uint64_t bve_gates_definition = 0;    // ... that found a definition
  uint64_t bve_definition_ticks = 0;
  uint64_t bve_definition_budget = 0;   // attempts the per-attempt tick ceiling
                                        // cut short — the only readout saying
                                        // whether that ceiling binds
  uint64_t bve_gate_pairs_saved = 0; // resolvent pairs the gate reduction
                                     // removed from the growth bound's input;
                                     // the acceptance lever itself
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

// Shared literal→large-clause occurrence index for one simplification phase
// (kissat dense.c analogue), built by kernel_dense_connect and maintained
// incrementally: adds push, deletions are lazy, and kernel_collect_garbage
// rebuilds after relocating Refs. Irredundant large clauses only. An optional
// acceleration — `Kernel::dense` is null in search, and every consumer keeps a
// round-local arena scan for that case, so rules stay independently applicable
// through the public API (N1).
struct OccIndex {
  std::vector<std::vector<Ref>> large; // by literal; may hold stale garbage Refs
};

// Undo log of the search bookkeeping a probing propagation touches (kissat uses
// a separate engine, src/proprobe.c; this is the one engine with the log on).
// Propagation stays complete under the two-watched-literal scheme only if a
// falsified watch is moved, so probing cannot refrain from moving it; instead
// every incidental update (watch-list contents, literal permutation, `searched`
// hint) is recorded and put back once the speculative trail is unwound.
struct ProbingLog {
  bool recording = false;
  // kernel state the log is relative to: the restore only fires when the caller
  // has unwound the trail back to exactly this (see kernel_probing_end)
  uint32_t base_trail = 0, base_head = 0, base_level = 0;
  // watch-list snapshots, one per literal, taken at first touch (the first scan
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

// Retirement cause for a variable rewritten out of the formula (see
// Kernel::var_status). cadical's Flags::status is the same concept; only the
// causes a rule here produces are kept, and they act as advice rather than
// through the decision loop (docs/design/variable-retirement-design.md).
enum class VarStatus : uint8_t { Active = 0, Substituted, Eliminated };

struct Kernel {
  uint32_t nvars = 0;
  // Variables at index >= this are non-original "extension" variables added
  // mid-solve by E2 Factor / BVA. Search assigns them like any other, but they
  // are projected out of the reported model: a model restricted to the
  // originals satisfies the original formula (docs/design/e2-factor-design.md
  // §3). UINT32_MAX = none added yet.
  uint32_t first_extension_var = UINT32_MAX;
  bool inconsistent = false; // root-level conflict happened

  std::vector<int8_t> values;     // by lit: 1 true, -1 false, 0 unassigned
  std::vector<Assigned> assigned; // by var
  // By var, meaningful only while `assigned[v].jumped`: the literal the virtual
  // binary reason resolved away. Written on the jump and never cleared.
  std::vector<Lit> jumped_over;

  std::vector<Lit> trail;
  uint32_t propagate_head = 0;
  // Off-fixpoint-decide taint (calculus D-LEVEL-1). A Decide taken while
  // propagate_head < |τ| lets a not-yet-run watch scan see an "ahead"
  // assignment and treat a clause as satisfied by it; a later backtrack that
  // unassigns that literal never redoes the scan, so the clause can be left
  // unit with no watch to fire. Cure: while set, kernel_backtrack resets
  // propagate_head to 0 rather than to the level boundary. Never cleared — a
  // fixpoint reached while an ahead literal is still true is not safe.
  bool trail_tainted = false;
  // Trail height at the start of each level. Invariant `frames.size() == level
  // + 1` holds even for a kernel that never saw a variable, hence frames[0] = 0
  // from construction (corpus 001-empty-formula-frames).
  std::vector<uint32_t> frames = {0};
  uint32_t level = 0;
  uint32_t unassigned = 0;
  // Of those, how many are NOT retired, so the retirement skip answers "is any
  // non-retired candidate left?" in O(1) instead of walking the queue. Drift is
  // a performance bug only, never a soundness one.
  uint32_t unassigned_active = 0;

  Arena arena;
  WatchLists watches;
  // kissat's `large_clauses_watched_after_binary_clauses` (internal.h:99): does
  // every watch list hold its binary entries before its large ones? A full
  // re-watch establishes it (kernel_collect_garbage), any binary add breaks it.
  // Nothing reads it to decide anything; it feeds `Stats::binprop_flagged`.
  bool binaries_watched_first = true;
  // shared large-clause occurrence index, non-null only during a simplification
  // phase; owned by the caller, maintained by the kernel through add/GC
  OccIndex *dense = nullptr;
  // delayed watch relocations from an in-flight propagation scan
  // (lit, blocking, ref) triples; see watch.hpp for why they must wait
  std::vector<uint32_t> delayed;
  // probing-propagation bookkeeping log; inert outside a
  // kernel_probing_begin/end() scope, so search never pays for it
  ProbingLog probing;

  // Search-guidance state `H` (VMTF queue, EVSIDS heap, phases), held opaquely
  // and non-owningly: `struct Heuristics` lives in policies/heuristics.hpp and
  // only code that includes it may read `h->…`. Whoever outlives a single
  // `solve()` owns the storage. Optional — `h == nullptr` is sound, unguided.
  Heuristics *h = nullptr;

  // Reconstruction stack (rho, plan §4): witness-labelled clause segments for
  // rebuilding a SAT model through eliminated/substituted variables (kissat's
  // extension stack; Jarvisalo-Heule-Biere reverse reconstruction). Flat,
  // each segment [ rest_lits..., witness_lit, len ] with len = 1 + #rest;
  // kernel_extend_model replays it in reverse.
  std::vector<Lit> reconstruction;
  bool model_extended = false; // kernel_extend_model idempotency guard

  // conflict handoff: propagate -> analyze
  bool conflict_valid = false;
  Ref conflict_ref = INVALID_REF; // INVALID means binary conflict
  Lit conflict_bin[2] = {INVALID_LIT, INVALID_LIT};

  // learned-clause handoff: analyze -> backjump
  PendingLearned pending;

  // scratch (analyze) — persistent so the per-conflict hot path never allocates
  std::vector<uint8_t> seen;   // by var
  std::vector<Var> analyzed;   // vars marked seen
  std::vector<Var> shrinkable; // vars marked during a block-shrink walk
  // |{v ∈ analyzed : seen[v] & (SEEN|REASON_BUMP)}| — the bump-set size, which
  // is what kissat's `analyzed` stack holds. This `analyzed` also carries
  // minimization scratch and, in LRAT mode, antecedents, so its size is neither
  // the bump-set size nor mode-independent.
  uint32_t analyzed_seen = 0;
  std::vector<Var> reason_bumped; // REASON_BUMP marks, for rollback
  std::vector<Lit> learned_scratch; // the clause under construction
  std::vector<Lit> otfs_scratch;    // OTFS resolvent assembly
  std::vector<Var> radix_scratch;   // radix-sort double buffer
  std::vector<uint8_t> marked_levels;
  std::vector<uint32_t> touched_levels;

  // Eager subsumption: a shift register of the most recently learned large
  // clauses (kissat `last_learned`, learn.c:70-72,133-187, MIT). Every learn
  // tests the fresh clause against it and garbages any entry it strictly
  // subsumes. Kissat's register rather than cadical's backwards walk over
  // `clauses`, since this arena cannot be traversed backwards
  // (docs/implementation-notes.md §15).
  static constexpr uint32_t EAGER_SUBSUME_SLOTS = 4; // kissat's option maximum
  Ref eager_recent[EAGER_SUBSUME_SLOTS] = {INVALID_REF, INVALID_REF,
                                           INVALID_REF, INVALID_REF};
  // Per-literal marks for the containment test. Invariant: all-zero between
  // uses. Persistent with touched-restore, never fill-initialised per call —
  // this runs once per conflict, where an O(nvars) memset is the known
  // per-call linear-scratch hazard (docs/lessons.md).
  std::vector<uint8_t> eager_marks;

  // --- proof clause ids (native LRAT; CaDiCaL's id scheme) ------------
  // Every clause, including input tautologies and duplicates that are never
  // stored, consumes one id in input/derivation order. Large clauses carry
  // theirs in the header, units in unit_ids (by var; 0 = none), binaries in
  // binary_ids keyed by the canonical literal pair.
  //
  // Invariant: while a pair is live in the watch lists, its id here names a
  // clause the checker still holds. An add installs the LAST id, so a pair
  // re-derived after a deletion does not cite the id the `del` retired; and a
  // pair with two live copies is not deletable, since one id cannot name two
  // clauses. The map is size-monotone — deletion keeps the key.
  uint64_t next_clause_id = 0;
  std::vector<uint64_t> unit_ids; // by var
  // A flat open-addressing replacement is trajectory-identical but uniformly
  // slightly slower; this map is not a bottleneck.
  std::unordered_map<uint64_t, uint64_t> binary_ids;
  // antecedent scratch (analyze, LRAT only): (trail position of the literal the
  // antecedent propagated, antecedent id). Emitted sorted by trail position
  // ascending with the conflict id last, checker-propagatable by construction.
  std::vector<std::pair<uint32_t, uint64_t>> lrat_entries;
  std::vector<uint64_t> lrat_chain; // built by kernel_lrat_chain below
  // Reason jumping, LRAT only: vars a jumped reason resolved past, whose
  // antecedent is settled against the FINAL clause, not at record time.
  std::vector<Var> jump_pending;

  // --- input normalization, deferred to end-of-input (N2) --------------
  // An input clause whose FILE form repeats a literal is stored deduped, so id
  // `i` names a different clause here than for the checker. `ingest_clause`
  // records a restatement instead of citing `i`, and `finish_input` emits each
  // as a one-antecedent RUP lemma and re-points the stored id. The deferral is
  // forced: a restatement id must exceed every input id, and input ids run 1..N
  // with N unknown until the input ends (parse.cpp).
  struct InputRestatement {
    uint64_t id;    // the file-position id; the lemma's sole antecedent
    uint32_t begin; // slice of input_restate_lits (the deduped clause)
    uint32_t size;
    Ref ref; // large clauses only: the header to re-point
  };
  std::vector<InputRestatement> input_restatements;
  std::vector<Lit> input_restate_lits;
  // Variables in order of first ACTIVATION — the order in which they first
  // appear in a clause the input actually stores. Kissat's decision order is
  // seeded from this, not from the file's variable numbering, and not from
  // import order: it activates per CLAUSE in the final `else` of `kissat_add`
  // (kissat add.c), so a tautology or a satisfied clause activates nothing,
  // while import happens per literal unconditionally. `finish_input` hands this
  // to `H` and clears it; a variable that occurs in no stored clause never
  // appears, exactly as kissat never imports it.
  std::vector<Var> input_activation_order;
  std::vector<uint8_t> input_activated; // per-var membership, parse-scoped
  // A refutation found during input is deferred for the same reason: emitting
  // it mid-input would burn an id the checker gave a later input clause.
  bool input_refuted = false;
  uint64_t input_refutation_id = 0; // the refuting input clause
  uint32_t input_refutation_var = UINT32_MAX; // its unit partner, if any

  Proof *proof = nullptr;
  Stats stats;
  // Scheduling state (restart EMAs, reduce intervals, ...) lives in the Policy
  // object, not here (plan §5).

  // BVE per-variable retry filter (kissat flags->eliminate). By var: 1 = this
  // variable's elimination neighbourhood changed since BVE last attempted it.
  // A work filter, never a soundness one: it removes only re-attempts whose
  // outcome is already known (N1; completeness argument in bve.hpp). Set
  // kernel-side for every variable of an irredundant clause added or deleted
  // (redundant excluded, or learned-clause traffic would mark everything), of a
  // clause a newly root-fixed literal occurs in, and of every freshly reserved
  // variable. Cleared when the rule ATTEMPTS a variable.
  std::vector<uint8_t> bve_retry;
  // trail index up to which root-fixed literals have been folded in; see
  // kernel_bve_retry_sync_fixed.
  uint32_t bve_retry_fixed_head = 0;

  // --- touched-epoch substrate (kissat flags->subsume, flags->factor) -------
  // "What is new since I last acted?", the base of every repetition suppressor
  // here. `touch_epoch` is a monotone event counter, `touched[v]` the epoch v
  // was last touched in (0 = never). A consumer keeps a watermark `w`, reads
  // `touched[v] > w`, and "clears" by snapshotting `w = touch_epoch` BEFORE it
  // works, so material it creates itself stays new for its next pass (kissat
  // re-arms the same cases by hand, forward.c:589-645). A stamp rather than a
  // bit, because a clear pass that never runs is what wedged the elimination
  // clock (Stats::bve_marks).
  uint64_t touch_epoch = 0;
  // Producers: every variable of an added irredundant large clause and both of
  // an added binary (kissat clause.c:71-72,98; a strengthen is an add), every
  // newly root-fixed variable, every freshly reserved one. Deletion is
  // deliberately not a producer — it can only remove a subsumer, never create
  // one. Hence "no touch since w" is a theorem: with no add and no root fix the
  // irredundant clause set only shrinks and subsumption is monotone in it, so
  // the filter cannot wedge shut.
  std::vector<uint64_t> touched; // by var
  // The deletion side, on the same clock: deletion cannot create a subsumer but
  // is exactly what creates a blocked clause, so BCE needs it and only BCE
  // does. Separate, so that consumer does not weaken C1/C2's filter; cadical
  // splits `mark_added` from `mark_removed` the same way (internal.hpp:1125).
  std::vector<uint64_t> removed; // by var
  // The redundant-band add side, on the same clock: written only under
  // `touch_redundant`, only for creations `kernel_likely_kept` admits, read
  // only by C2 Strengthen. A third array because folding the band into
  // `touched` hands every learned clause to all six consumers and saturates the
  // shared clock; cadical withholds `block`/`factor` from a likely-kept
  // redundant add for the same reason (clause.cpp:59-67). Isolation is then a
  // theorem: interleaved increments on one monotone clock cannot change a
  // temporal order (docs/implementation-notes.md §13).
  std::vector<uint64_t> touched_kept; // by var
  // trail index up to which root-fixed literals have been folded into
  // `touched`; see kernel_touch_sync_fixed.
  uint32_t touch_fixed_head = 0;
  // Consumer watermarks: solver state, not config, kept kernel-side because the
  // rule itself reads them and they must outlive its stack frame. Each is
  // advanced by a round of its own rule, which re-arms whatever it did not
  // reach. N1 is untouched: `instances()` and `apply_instance` never read them.
  // `bce_touch_mark` and `cce_touch_mark` are read against BOTH stamps
  // (kernel_changed_since), the rest against `touched` alone.
  uint64_t subsume_touch_mark = 0;
  uint64_t strengthen_touch_mark = 0;
  uint64_t bce_touch_mark = 0;
  uint64_t ternary_touch_mark = 0;
  uint64_t instantiate_touch_mark = 0;
  uint64_t factor_touch_mark = 0; // kissat flags->factor
  uint64_t cce_touch_mark = 0;
  // Did the rule's last canonical round reach a fixpoint (kissat's `completed`,
  // forward.c:664-670)? Outputs, written by the rule and read by a policy, and
  // `true` while the rule has not run, so a policy that never schedules it is
  // never held up by a stale `false`.
  bool subsume_complete = true;
  bool strengthen_complete = true;
  // Master switch for the round filters (TACTIX_TOUCH_MARKS=0 is the A/B off
  // arm). False = every round scans its full candidate set.
  bool touch_filter = true;
  // Let an incomplete SubsumeForward/Strengthen round hold the elimination
  // pipeline's outer loop open (kissat eliminate.c:519-520), so a half-checked
  // candidate list buys another round rather than an advance on stale material.
  // Bounded by the policy's round cap, so it can only add rounds.
  bool subsume_holds_elim = true;
  // The redundant tier band, both halves behind one switch
  // (TACTIX_TOUCH_REDUNDANT=1): stamping a redundant creation into
  // `touched_kept`, and C2 Strengthen's round scope plus its filter's read of
  // those stamps. One decision, because stamps with no consumer are pure cost
  // and a scope with no stamps is a filter that does not cover itself. Ships
  // off pending its A/B (docs/implementation-notes.md §2, D-C1C2-VICTIM).
  bool touch_redundant = false;
  // Shadow mode for SubsumeForward's canonical round (diagnostic, ships off):
  // C1 records each victim's clause id in `shadow_victims` instead of deleting
  // it, and Strengthen retires the id when it removes the same clause, so one
  // run measures C1's unique yield on a C1-less trajectory. N1: this narrows
  // the canonical ROUND, never the guard; `subsume_forward_try` ignores it.
  bool subsume_shadow = false;
  // Clause ids C1 declined to delete and Strengthen has not yet removed;
  // populated only under `subsume_shadow`.
  std::unordered_set<uint64_t> shadow_victims;
  // The reduce survivor envelope, published by `reduce_apply_refs` (cadical
  // `lim.keptglue`/`lim.keptsize`, reduce.cpp:147-156): the widest (glue, size)
  // still present after the last reduction, and the third disjunct of
  // `likely_to_be_kept_clause` — without it that predicate degenerates to
  // "irredundant or in tier2", which is not the reference's set. Rule-written.
  uint32_t kept_glue = 0;
  uint32_t kept_size = 0;
  // By var: the minimum growth slack at which x is currently eliminable (its
  // #resolvents − occ⁺ − occ⁻), as last computed by bve_eliminate; UINT16_MAX =
  // never. Meaningful only while `bve_retry[x] == 0`. A finer-grained work
  // filter than the retry bit for a BVE ramp, with the same completeness/N1
  // story (bve.hpp). Written only when `bve_track_min_bound` is set.
  std::vector<uint16_t> bve_min_bound;
  // Opt-in: resolve a rejected variable up to the ramp ceiling rather than the
  // current slack, so bve_eliminate can record the exact `bve_min_bound` a
  // precise skip needs. Policy-transient, never persisted.
  bool bve_track_min_bound = false;

  // --- rule configuration (set by a policy or the public API at any state) ---
  // Every field below is configuration, not scheduling state: any policy may
  // set it at any time and every value is sound, because these only narrow or
  // widen which instances a rule OFFERS, never whether one is justified.
  //
  // BVE gate detection: bitmask of enabled gate types (bve.hpp BveGate).
  // Disabling one only falls back to the full resolvent cross product — gate
  // type is *how* resolvents are computed, not *whether* x can be eliminated.
  uint32_t bve_gate_mask = ~0u;
  // BVE growth slack (kissat eliminatebound): eliminate iff #non-tautological
  // resolvents ≤ occ⁺+occ⁻+this. Default = BVE_MAX_BOUND in rules/bve/bve.hpp;
  // keep the two in step.
  uint32_t bve_bound = 16;
  // Eager backward subsumption from fresh resolvents (cadical `elimbackward`,
  // default 1). Ships ON; `TACTIX_BVE_BACKWARD=0` is the permanent off arm.
  bool bve_backward = true;
  // BVE general-definition extraction: per-attempt sub-solver tick ceiling
  // (kissat `definitionticks`, 1e6 there); 0 = unlimited. Two orders below the
  // reference because of a metering gap — an attempt that finds nothing
  // produces no `bve_resolutions`, so this is the only bound on the work.
  uint64_t bve_definition_budget = 10000;
  // BCE candidate-occurrence ceiling (cadical `blockocclim`, 1e2 there): a
  // within-rule scope choice, the category of BCE's irredundant-only filter
  // (docs/implementation-notes.md §1). Ships uncapped, cadical's 100 being
  // measured dead as a default here; a plan §5.6 arm, not a constant.
  uint32_t bce_occ_limit = 0;
  // Let Strengthen/SubsumeForward stop mid-round on `work_budget`. Off because
  // a stopped round defers its candidates rather than saving the work, and is
  // measured negative — see
  // benchmarks/reports/2026-08-04-midround-budget-negative.md. Kept as a plan
  // §5.6 dosing arm.
  bool midround_budget = false;
  // Factor clause-size scope (kissat `factorsize`, default 5). 2 = binary-only;
  // >= 3 lets remainders be sub-clauses. The add-RAT argument is indifferent to
  // |r| (docs/design/e2-factorsize-design.md §4). Scope only: it admits no new
  // instances.
  uint32_t factor_size = 5;
  // TransitiveReduce: exit the reachability walk as soon as `dst` is reached
  // (kissat transitive.c:130). Ships false — transitive_reduce.hpp runs each
  // walk to exhaustion so failed-literal detection sees the whole reachable
  // set, so this changes inference, not just cost. Sound either way.
  bool transitive_early_exit = false;
  // TransitiveReduce: credit `transitive_ticks` after every dst walk rather
  // than only when the source finishes (kissat transitive.c:159-161). Ships
  // TRUE: with it false the meter cannot see a source's spend until the source
  // is over, so the dose binds only between sources.
  // `TACTIX_TRANSITIVE_LIVE_METER=0` is the permanent baseline arm
  // (plan §5 obligation 6). Measurement: PROGRESS.md's phase-4 table.
  bool transitive_live_meter = true;
  // HyperTernaryRes governors (cadical `ternary.cpp`); 0 = unlimited. Without
  // them the rule has no bound of its own and BCE silently serves as one, since
  // deleting blocked clauses shrinks the ternary candidate pool.
  uint32_t ternary_occ_limit = 100;      // cadical ternaryocclim: skip a pivot
                                         // with more ternary occurrences than
                                         // this in either polarity
  uint32_t ternary_max_add_percent = 1000; // cadical ternarymaxadd: stop a round
                                           // past total_clauses * pct / 100
  // Factor grid-selection strategy, consulted by factor_apply when set; null =
  // the built-in watch-size path. Non-owning, and DefaultPolicy scopes it to
  // the inprocess phase so it never dangles.
  FactorMatchPolicy *factor_match = nullptr;
  // Factor acceptance bound: accept a grid iff delta > this; 0 is the
  // FACTOR_MIN_GAIN = 1 floor. An N1 config surface for external policies. An
  // attached FactorMatchPolicy takes precedence, and DefaultPolicy always
  // attaches one mirroring this field — see DefaultFactorMatch::bound.
  uint32_t factor_bound = 0;
  // Demote Factor's fresh extension variables to the bottom of the decision
  // queue after an application (kissat
  // `adjust_scores_and_phases_of_fresh_variables`). Repairs a focused-mode
  // defect: heuristics_reserve_vars appends new vars at `queue_last`, where
  // VMTF's search starts, so each would become a most-preferred candidate.
  // Reference parity. Heuristic-only: no proof delta, no guard conjunct.
  bool factor_demote_fresh = true;
  // Factor candidate-reduction rounds (kissat `factorcandrounds`, default 2). A
  // large clause is grid material only if every literal in it occurs in >= 2
  // candidate clauses; the prune iterates to fixpoint, bounded by this.
  uint32_t factor_cand_rounds = 0;
  // Diagnostic only (`TACTIX_NO_EXT_DECISIONS`) — never ship this on. Skipping
  // extension variables when branching makes `Decide` unable to return them,
  // which removes a capability N1 forbids removing by default (plan §1.1).
  // Contrast retirement and `no_ext_reason_bump`, which change only preference.
  bool no_ext_decisions = false;
  // Withhold extension variables from the reason-side bump channel only, so one
  // still rises whenever the conflict side earns it. N1-clean, unlike
  // `no_ext_decisions`: reason-side bumping is `H`-only advice and this changes
  // the order candidates are offered in, never which are legal.
  bool no_ext_reason_bump = false;
  // A/B knob for the Substitute hazard skip: restore the coarse per-round skip
  // so both arms come from one binary. Default false = the per-SCC skip.
  bool subst_hazard_round = false;
  // Variable retirement (docs/design/variable-retirement-design.md, plan §11
  // O17). A variable rewritten out of every clause propagates nothing when
  // decided. Advice, not a restriction: `Decide::instances()` still enumerates
  // these and any policy may decide them (N1 binds the API, not DefaultPolicy's
  // taste). The cause is kept rather than a bit, since the two causes have
  // different owners. Empty vector == every variable Active.
  std::vector<VarStatus> var_status;
  // Variables rules retired this phase (BVE/Substitute push, a policy drains).
  // Rules own the kernel-side fact (var_status, unassigned_active); whatever
  // ranking structures a policy keeps are its own to update — rules never call
  // into policy code (§5 layering). Until drained, a retired variable may
  // still be decided once: it propagates nothing and reconstruction supplies
  // its value, so the cost is a wasted decision, not a soundness issue.
  std::vector<Var> retired_events;
  // Demote substituted variables to the bottom of the decision order after each
  // Substitute round. Ranking advice only.
  bool subst_demote = false;
  // Ships on. DefaultPolicy retires variables a rule has rewritten out of the
  // formula: `heuristics_deactivate_var` unlinks them from the VMTF queue and
  // the score heap, `kernel_unassign` keeps them out, and the SAT test counts
  // active variables. N1 intact: `decide_candidates` enumerates from `k.nvars`.
  bool retire_enabled = true;
  // External interrupt, published to the rules (plan §6). As a `Policy::solve`
  // parameter alone it is invisible to the longest-running applications, and
  // one unmetered closure can leave the solver SIGTERM-deaf until a harness
  // escalates to SIGKILL and the proof is lost — an N2 defect. Not policy
  // knowledge (plan §5.3): external-environment state, the category of `proof`.
  // Soundness: a rule that stops early has simply done less, everything it did
  // is proof-logged, and abort points sit between complete units of work.
  volatile sig_atomic_t *stop = nullptr;
  // Stress knob for the abort paths above (0 = off). `mutable` because
  // Congruence's extraction half takes a `const Kernel &`; never read by a
  // guard, so it cannot affect applicability (N1). The poll sites bypass their
  // 256-iteration cadence when this is set — otherwise a formula smaller than
  // one poll window never reaches a first poll and a fuzz leg passes vacuously.
  uint32_t stop_fuzz = 0;
  mutable uint64_t stop_fuzz_state = 0x9E3779B97F4A7C15ull;
  // Composite grids (kissat resize_factoring): widen Factor's factor/remainder
  // universe to all live literals, growing as the pass applies. Default false
  // because composition over-applies in a single preprocessing blast; it
  // belongs to the interleaved during-search doses. Termination stays
  // delta-monotone (docs/design/s2b-composite-grids-design.md §4).
  bool factor_composite = false;
  // Advice (N1): may the policy's scheduling of Factor decline this occasion?
  // Default true = no restriction, and that default IS the N1 guarantee. The
  // admission heuristic belongs in DefaultPolicy and must reach the rule
  // through this field — the `bve_retry` advice shape. As a no-op inside
  // `factor_apply` it instead removed a capability: the guard reports
  // applicable and the instance is offered, yet applying it does nothing.
  bool factor_admit = true;
  // Private per-rule tick metering. `stats.ticks` is the global propagation
  // counter that four slot entries budget against; with one shared meter an
  // earlier entry spends the budget a later one was handed, and nothing
  // separates "starved" from "ran and found nothing". Pure accounting.
  uint64_t *tick_sink = nullptr;
  // True while a simplification phase is running, which is what routes a tick
  // away from `stats.search_ticks`. kissat gets the same split from the call
  // site — search propagation charges `search_ticks`, the probing and
  // elimination propagators charge their own — so this flag is that choice
  // expressed for one shared propagator. Pure accounting: no inference reads
  // it, and an API caller applying a rule outside a phase simply charges
  // search, exactly as a direct `kissat_propagate` would.
  bool inprocessing = false;
  // Per-round work budget, read by any budget-aware canonical apply in its own
  // currency (kissat SET_EFFORT_LIMIT): Vivify meters propagation ticks, Factor
  // its cache-line factor_ticks; 0 = unbounded. Not a guard: stopping early
  // makes the round's effect a prefix of the unbounded round's, and every
  // prefix is sound.
  uint64_t work_budget = 0;
  // Vivify round scope (kissat's redundant-tier vs irredundant split). A scoped
  // round's effect is a subset of the full round's. The band encoding lets the
  // policy spend kissat's four shares (vivifyirr 3 / tier1 3 / tier2 3 / tier3
  // 1) instead of collapsing the redundant 7 onto one glue<=tier2 pass:
  //   0 = all candidates (default)      3 = redundant glue <= tier1
  //   1 = the whole redundant band up   4 = redundant tier1 < glue <= tier2
  //       to tier2 (legacy meaning)     5 = redundant glue > tier2
  //   2 = irredundant only
  uint8_t vivify_scope = 0;
  // Carry the probing trail across candidates whose assumption prefixes share a
  // head, instead of re-propagating from the root for each (kissat
  // vivify.c:1033-1046, cadical vivify.cpp:966-988). The off arm restores both
  // the per-candidate root return and the older probe order, since that order
  // is what makes prefixes shared at all. Validity of a reused prefix:
  // docs/design/2026-08-05-vivify-trail-reuse.md §3 (invariant VP).
  bool vivify_reuse = true;
  // Reuse the conflict as its own driving clause when exactly one of its
  // literals sits at the conflict level (kissat one_literal_on_conflict_level)
  // instead of learning a fresh clause. Both settings sound, differing only in
  // trajectory. Default off — docs/implementation-notes.md §12.
  bool conflict_reuse = false;
  // Diagnostic only (needs conflict_reuse): decline the shortcut instead of
  // repairing the watches, which attributes the arm's verdict between the
  // crippling and the mechanism. Both values sound.
  bool conflict_reuse_no_repair = false;
  // Reason jumping (kissat `jumpreasons`, default 1 there; assign.c:38 +
  // fastassign.h:12). When a binary clause (lit ∨ other) propagates `lit` and
  // `other`'s own reason is binary, store *that* reason: the recorded
  // antecedent becomes the one-step resolvent (lit ∨ r), implied but not in the
  // database. Both settings sound; default off, and
  // docs/implementation-notes.md §13.2 records why it is not free here.
  bool jump_reasons = false;
  // Reason-side bumping (kissat bumpreasonslimit, cadical bumpreasonlimit, both
  // 10): after the learned clause is final, also mark the literals of its
  // literals' reasons, up to `this × |bump set|` marks; 0 = off. Changes only
  // which variables `H` bumps, so every value is proof-neutral.
  uint32_t bump_reason_limit = 0;
  // Eager subsumption (kissat `eagersubsume`, default and maximum 4): how many
  // `eager_recent` slots a learn compares itself against; 0 = off. Sound at
  // every value because the only clauses it deletes are redundant, which S2
  // Reduce may delete unconditionally — the subsumption test is a selection
  // heuristic, not the justification (docs/implementation-notes.md §15).
  uint32_t eager_subsume = EAGER_SUBSUME_SLOTS;
  // Reduce retention tiers (kissat tier1/tier2, defaults 2/6): glue bands that
  // survive a reduction while recently used. Sound at every value, since Reduce
  // only ever deletes redundant clauses. A policy wanting dynamic tiers calls
  // reduce_retier(), which overwrites these with Stats::used_glue quantiles.
  uint32_t tier1 = 2, tier2 = 6;
  // Post-shrink glue clamp (kissat `promote`, default 1). false reverts to the
  // pre-parity `min(glue, new_size)`; see kernel_shrunken_glue. Both settings
  // sound — glue is a retention heuristic, never part of a justification.
  bool shrink_glue_promote = true;
  // Initial decision order (kissat `tumble`, default 1): seed `H` in
  // first-activation order rather than ascending variable index. false reverts.
  // Ranking only, so every setting is proof-neutral.
  bool tumble = true;

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

// --- lifecycle -------------------------------------------------------------
void kernel_reserve_vars(Kernel &, uint32_t nvars);
// heuristics_reserve_vars (the `H`-growth half) is declared in
// policies/heuristics.hpp; kernel_reserve_vars calls it.

// Allocate `count` fresh extension variables at a root fixpoint and return the
// index of the first. Grows all per-variable state and records the extension
// boundary so the model printer projects them out. Caller must be at level 0.
Var kernel_new_extension_vars(Kernel &, uint32_t count);

// Number of variables to emit in the model `v` line — originals only.
inline uint32_t kernel_num_output_vars(const Kernel &k) {
  return k.first_extension_var == UINT32_MAX ? k.nvars : k.first_extension_var;
}

// --- reconstruction (rho) --------------------------------------------------
// Push one witness-labelled clause segment {witness} u {rest} onto rho, with
// `witness` the blocking literal. Internal state only — no proof line: the
// DRAT/LRAT justification is the rule's own add-RUP/delete of the clauses.
void kernel_push_witness(Kernel &, Lit witness, const Lit *rest, uint32_t nrest);
// Rebuild a full model through rho: replay the stack in reverse, forcing each
// unsatisfied segment's witness true. Idempotent; call once on a SAT result
// before reading k.values as the model.
void kernel_extend_model(Kernel &);

// --- shared proof idioms ---------------------------------------------------
// {a} + {¬a} ⊢ ∅: emit the empty clause (LRAT chain {id_a, id_b}; DRAT bare)
// and set `inconsistent`. Shared by four rules rather than inlined, because
// copy drift in a refutation emission is a soundness risk.
void kernel_emit_empty_from_pair(Kernel &, uint64_t id_a, uint64_t id_b);

// Batched root units for one simplification round, deduped per variable; {u}
// and {¬u} recorded in the same round refute via kernel_emit_empty_from_pair
// (corpus 007 is the regression that mandates that). The caller has already
// emitted the unit's proof clause under `id`; Recorded installs it as the
// variable's unit id.
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

// Clause-replacement tail: after the caller has emitted the shrunken clause's
// proof add under `id`, delete the original (proof `del` + garbage mark) and
// materialize `lits` by size. Unit handling stays with the caller, since rules
// differ; the empty clause refutes here. Glue is a redundant-clause concept, so
// callers pass 0 for irredundant.
enum class ReplaceOutcome { Refuted, Unit, Binary, Large };
ReplaceOutcome kernel_replace_clause(Kernel &, Clause *orig, uint64_t id,
                                     const Lit *lits, uint32_t size,
                                     bool redundant, uint32_t glue,
                                     Ref *large_ref); // nullable

// --- BVE retry filter (see Kernel::bve_retry) -------------------------------
inline void kernel_mark_bve_retry(Kernel &k, Var v) {
  k.bve_retry[v] = 1;
  k.stats.bve_marks++; // events, not bit flips — see Stats::bve_marks
}
// Fold every root-fixed literal not yet accounted for into `bve_retry`: fixing
// y makes the clauses y satisfies delete-only and strips ¬y from the rest, so
// every other variable in them may now be eliminable. Incremental and
// idempotent, since the root prefix of the trail never shrinks.
void kernel_bve_retry_sync_fixed(Kernel &);

// --- touched-epoch substrate (see Kernel::touched) --------------------------
inline void kernel_touch_var(Kernel &k, Var v) {
  k.touched[v] = ++k.touch_epoch;
  k.stats.touch_marks++; // events; a stamp has no flip to count
}
// The deletion side (see Kernel::removed), on the same clock, so the two are
// comparable against one watermark.
inline void kernel_removed_var(Kernel &k, Var v) {
  k.removed[v] = ++k.touch_epoch;
  k.stats.touch_marks++;
}
// Has v changed since the watermark `since`?
inline bool kernel_touched_since(const Kernel &k, Var v, uint64_t since) {
  return k.touched[v] > since;
}
// The band add side (see Kernel::touched_kept), on the same clock.
inline void kernel_touch_kept_var(Kernel &k, Var v) {
  k.touched_kept[v] = ++k.touch_epoch;
  k.stats.touch_kept_marks++;
}
inline bool kernel_kept_since(const Kernel &k, Var v, uint64_t since) {
  return k.touched_kept[v] > since;
}
// cadical's `likely_to_be_kept_clause` (internal.hpp:1059-1068), the scope
// predicate its whole `mark_added` family branches on. Stated on the raw fields
// so `kernel_add_large_clause` can call it before the Clause exists.
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
// is `since`? (kissat forward.c:143-160.) Two independent reasons: (a) two or
// more of c's variables touched, exactly the condition for a new subsumer or
// self-subsuming partner over vars(c) to exist; (b) c holds a root-falsified
// literal fixed since the watermark, so it is effectively shorter than when
// last checked. A policy predicate about what to TRY: it must never appear in a
// guard or narrow an `instances()` enumeration (N1). `need` is how many touched
// variables count as new material — 2 for the subsumption family, 1 for a rule
// whose outcome can turn on one changed variable. `kept` widens the read to the
// band's stamps, and is set only for redundant candidates.
inline bool kernel_clause_new_material(const Kernel &k, const Clause *c,
                                       uint64_t since, size_t need = 2,
                                       bool kept = false) {
  size_t hits = 0;
  for (const Lit l : *c) {
    if (!kernel_touched_since(k, var_of(l), since) &&
        !(kept && kernel_kept_since(k, var_of(l), since)))
      continue;
    if (k.value(l) < 0)
      return true; // (b)
    if (++hits >= need)
      return true; // (a)
  }
  return false;
}
// Fold every root-fixed variable not yet accounted for into `touched`. Only the
// fixed variable is stamped, not its clause neighbours, which is where this
// stops short of kernel_bve_retry_sync_fixed's occurrence walk: a consumer
// reads "C holds a literal that is now absorbed" off the fixed variable itself.
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
// Delete binary (a v b): drop both mirror watch entries, retire a live root
// reason to UNIT_REASON (corpus 003's binary analog), emit the proof `delete`,
// and keep the binary_ids key. Refuses, returning false, when the pair is
// absent (so deletion is idempotent), duplicated (two live copies share one id,
// and the `del` would retire it under the survivor), or a live reason for a
// non-root literal. Redundancy of (a v b) is the caller's proof obligation.
bool kernel_delete_binary(Kernel &, Lit a, Lit b);
// DRAT-only: remove EVERY copy of pair (a,b) (E2 Factor grid removal; see impl).
bool kernel_delete_binary_all_copies(Kernel &, Lit a, Lit b);

// Live copies of the binary pair (a v b), counted in a's watch list, stopping
// at `cap`. The liveness/duplication predicate of the binary layer, and a
// soundness one: the watch lists are the only liveness signal, since a deleted
// pair's id lingers in binary_ids. Defined once here rather than per rule, a
// divergent copy having shipped a wrong answer already.
inline uint32_t kernel_binary_copies(const Kernel &k, Lit a, Lit b,
                                     uint32_t cap = UINT32_MAX) {
  uint32_t n = 0;
  for (const WatchEntry w : k.watches.entries(a))
    if (w.binary() && w.other() == b && ++n == cap)
      break;
  return n;
}
inline bool kernel_binary_live(const Kernel &k, Lit a, Lit b) {
  return kernel_binary_copies(k, a, b, 1) == 1;
}

// --- watch repair -----------------------------------------------------------
// These two primitives mutate WHICH literals a live clause watches and nothing
// else: membership, size, id, redundancy and arena position are untouched, so
// the proof stream, the dense index, every recorded Ref and every LRAT chain
// stay valid. Sound for the same reason the propagation engine's own watch
// relocation is: completeness constrains only which literals are watched, and
// the caller owns re-establishing it. Preconditions for both: `ref` is live and
// watched by the literal named; no propagation scan is in flight (`delayed`
// empty), since a scan holds raw pointers into the slice these compact; and not
// inside a probing scope, whose rollback log would miss the move.
// kernel_unwatch_large is the exact inverse of WatchLists::push_large.
void kernel_unwatch_large(Kernel &, Lit l, Ref ref);

// Move clause `ref`'s literal at position `from` (>= 2) into watched slot
// `slot` (0 or 1); the displaced literal takes position `from` and the watch
// follows the promoted literal (kissat analyze.c's unwatch / swap / watch
// triple). The caller owns the two-watched-literal invariant it leaves behind:
// a false watched literal at level d must be paired with a watch that is TRUE
// at level <= d, or an unassigning backtrack can strand the clause unit with no
// watch to fire on (regression corpus 002/005).
void kernel_promote_watched_lit(Kernel &, Ref ref, uint32_t slot,
                                uint32_t from);

// Retire a large clause: emit the proof `delete` AND mark it garbage. The
// single home for large-clause deletion, and the two halves are inseparable —
// a `del` without the mark leaves a deleted clause live, and a mark without the
// `del` is an N2 breach. Deletion stays lazy: readers skip garbage and
// kernel_collect_garbage reclaims later. The mark is idempotent; the `del`
// deliberately is not, since every caller tests `c->garbage()` first.
// Header-inline: Reduce retires clauses by the hundred thousand.
inline void kernel_delete_clause(Kernel &k, Clause *c) {
  if (k.proof)
    k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
  if (c->garbage())
    return; // already retired: the mark and its bookkeeping are idempotent
  c->flags |= Clause::GARBAGE;
  // BVE retry filter: losing an irredundant clause shrinks the elimination
  // neighbourhood of every variable in it. Redundant clauses are skipped — BVE
  // never resolves them, and Reduce's traffic would mark everything every round.
  if (!c->redundant())
    for (const Lit l : *c) {
      kernel_mark_bve_retry(k, var_of(l));
      kernel_removed_var(k, var_of(l)); // BCE's side of the substrate
    }
}

// The glue a redundant clause carries after shrinking to `new_size`: the one
// home for a rule three rules and the on-the-fly path all need. Both references
// clamp to `new_size - 1` (kissat `strengthen.c:42-43`, cadical
// `clause.cpp:230`), so a shrunken clause is promoted a band, not merely kept.
// Rationale and the tier effect: docs/implementation-notes.md §13.
//
// `new_size < 3` cannot hold for a stored large clause; the guard keeps the
// expression total, since callers compute it before the replacement decides
// between large, binary and unit. `Stats::shrink_promoted` counts only the
// shrinks where the two rules disagree, so a zero proves the arm inert.
inline uint32_t kernel_shrunken_glue(Kernel &k, uint32_t old_glue,
                                     uint32_t new_size) {
  if (new_size > 2 && old_glue >= new_size) {
    if (!k.shrink_glue_promote)
      return new_size;
    k.stats.shrink_promoted++;
    return new_size - 1;
  }
  return old_glue < new_size ? old_glue : new_size;
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
// Header-inline (kissat fastassign.h pattern): propagation executes one
// assignment per propagated literal, so the store sequence must inline into the
// watch-scan loop rather than cross a translation unit.
// Has a rule rewritten v out of the formula? (Empty status vector == none.)
inline bool kernel_var_retired(const Kernel &k, Var v) {
  return v < k.var_status.size() && k.var_status[v] != VarStatus::Active;
}

// Cache lines touched by scanning `n` four-byte watch words, kissat's
// `kissat_cache_lines(n, sizeof(watch))` (utilities.h:21-35): a 128-byte line
// (ASSUMED_LD_CACHE_LINE_BYTES 7) holds 32 of them, and the count rounds up,
// with an empty list touching none. The unit matters beyond this file: every
// effort per-mille either reference ships is a fraction of a tick count charged
// this way, so a different line size or a floor here silently re-prices the
// whole ledger. One home, so the four scan sites cannot drift apart.
inline uint64_t kernel_cache_lines(uint64_t n) { return (n + 31u) >> 5; }

// The one place a tick is charged: to the global counter, and to the rule's
// private counter when a sink is scoped around an apply. Routing every site
// through here is what stops the two drifting.
inline void kernel_charge_ticks(Kernel &k, uint64_t n) {
  k.stats.ticks += n;
  if (!k.inprocessing)
    k.stats.search_ticks += n;
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
// stamped at its true level — the max decision level among its reason clause's
// other, falsified literals — not at the current decision level. That is what
// lets a chronological backtrack keep a still-implied literal without stranding
// it (kissat inlineassign.h assignment_level). Inert while chrono is off, since
// at a fixpoint on a level-sorted trail the two levels coincide.
inline void kernel_assign_binary(Kernel &k, Lit lit, Lit other_false) {
  const Assigned &a = k.assigned[var_of(other_false)];
  const uint32_t level = a.level;
  // Reason jumping (kissat assign.c:38 / fastassign.h:12): record the resolvent
  // of (lit ∨ other_false) and ¬other_false's own binary reason. `level` stays
  // that of `other_false`, since a binary propagation inherits the level of the
  // literal that forced it. Chains are allowed, and every LRAT consumer unwinds
  // them through `jumped_over`.
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
// update_phases=false is the probing variant (kissat
// kissat_backtrack_without_updating_phases): probe assignments must not
// pollute saved/target/best phases
void kernel_backtrack(Kernel &, uint32_t target_level,
                      bool update_phases = true);

// Core propagation engine (kissat proplit.h; a reviewed kernel primitive per
// plan §5.3, not part of the Propagate rule): propagates the trail to fixpoint
// or first conflict and returns true iff a conflict was found. Updates
// propagations/ticks but NOT conflicts — the Propagate rule owns the
// ApplyResult/proof semantics, while the probing rules reuse this engine.
// `ignore` excludes one clause (cadical's ignore pointer).
bool kernel_propagate(Kernel &, Ref ignore = INVALID_REF);

// Binary-only propagation to fixpoint or first conflict (kissat backbone.c):
// follows only the binary-implication graph — no large-clause propagation, no
// watch mutation. Cheaper than kernel_propagate and derives a subset of its
// units; same conflict-handoff semantics.
bool kernel_propagate_binary(Kernel &);

// --- probing propagation (kissat proprobe.c) --------------------------------
// Full propagation for the probing rules that leaves search's bookkeeping
// alone: same engine, same implied literals, same conflict as kernel_propagate,
// but the watch-list updates and `searched` hints made along the way are rolled
// back at the end of the scope. Not a micro-optimisation: a round that derives
// nothing still relocates watches, and on a fragile instance that content-free
// perturbation alone can cost an order of magnitude in conflicts.
void kernel_probing_begin(Kernel &);
// One scope per speculative episode: begin at a quiescent point, assume and
// propagate, backtrack to unwind the assumptions, end — then mutate. Contract,
// all three checked rather than assumed: the restore only fires when trail size
// / propagate_head / level are back at the scope's entry values, since any
// other state means the speculation left a permanent mark, so the log is
// discarded and the updates stay; no clause may be added, deleted, or relocated
// inside a scope, because the log holds Refs and watch-list contents; and
// kernel_propagate_probing outside a scope is plain kernel_propagate.
void kernel_probing_end(Kernel &); // idempotent; a no-op outside a scope
bool kernel_propagate_probing(Kernel &, Ref ignore = INVALID_REF);

// (VMTF / EVSIDS / phase-snapshot `H` operations live in
// policies/heuristics.hpp.)

// Has an external interrupt arrived (plan §6)? Rules poll this at their own
// safe abort points (see Kernel::stop). Cheap enough for a coarse cadence
// inside a long rule loop; never call it per-literal.
inline bool kernel_terminating(const Kernel &k) {
  // Stress mode (`TACTIX_STOP_FUZZ=n`, diagnostic): report "terminating" on a
  // deterministic 1-in-n cadence so rules abort at many different interior
  // points, while the policy's own `stop` stays clear and the solve still runs
  // to a real verdict the fuzz oracles can check. This is what makes "aborting
  // a rule mid-application is sound" a tested claim rather than an argued one.
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

// Poll and attribute in one call (plan §6). The increments live inside this
// helper, never at the call sites, so a new abort site cannot forget to be
// counted and a reading of 0 always means "did not happen" rather than "was not
// measured" (docs/lessons.md L22).
inline bool kernel_rule_interrupt(Kernel &k, uint64_t &site) {
  if (!kernel_terminating(k))
    return false;
  k.stats.rule_interrupted++;
  site++;
  return true;
}

// Shadow-ledger primitives (see Kernel::subsume_shadow). Both are no-ops with
// the hatch off; `retire` is the only one on a shipped hot path and it
// short-circuits on the flag before touching the set.
inline void kernel_shadow_observe(Kernel &k, uint64_t id) {
  if (k.shadow_victims.insert(id).second)
    k.stats.c1_shadow_observed++;
}
// Did Strengthen just remove a clause C1 had shadowed? Retires the entry so a
// re-observation of the same clause id cannot be counted twice.
inline bool kernel_shadow_retire(Kernel &k, uint64_t id) {
  return k.subsume_shadow && k.shadow_victims.erase(id) != 0;
}

// RAII publisher for Kernel::stop — a policy's solve() publishes its interrupt
// for the duration of the call and always withdraws it, so an early return can
// never leave a dangling pointer in the kernel. Restores the previous value
// rather than nulling, so nested solves compose.
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

// number of root-fixed assignments (the level-0 trail prefix)
inline uint32_t kernel_root_fixed(const Kernel &k) {
  return k.level == 0 ? static_cast<uint32_t>(k.trail.size()) : k.frames[1];
}

// Assemble k.lrat_chain from k.lrat_entries: antecedent ids sorted by trail
// position ascending — the order that unit-propagates for the checker by
// construction — with `closing_id` last. Shared by every chain-emitting rule,
// so the ordering invariant is defined exactly once.
void kernel_lrat_chain(Kernel &, uint64_t closing_id);

// LRAT reason walk for the probing rules. A pure kernel-state read: collect
// (trail position, antecedent id) for every reason reachable from `start` down
// to root units and probe assumptions, which carry DECISION_REASON and are
// skipped as the negation of the emitted clause. The caller closes the chain
// and owns `seen` as scratch. Rule: a caller on the native-LRAT path must pass
// `touched`, so a persistent `seen` is restored in O(marked) — the O(nvars)
// zero-fill otherwise dominates it (docs/lessons.md).
void kernel_reason_walk(Kernel &, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &entries,
                        std::vector<Var> *touched = nullptr);
// Seed the walk from the recorded conflict (binary in conflict_bin, else the
// large clause at conflict_ref): walk every conflicting literal's reason and
// return the conflicting clause's id, the RUP chain's closing hint.
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

// Dense simplification phase. connect builds `occ.large` from the current
// irredundant large clauses in one arena scan and attaches it as `k.dense`;
// disconnect detaches it, and the caller owns the storage. While attached,
// adds push and kernel_collect_garbage rebuilds the index after relocating
// Refs. Precondition: level 0, like kissat's dense mode.
void kernel_dense_connect(Kernel &, OccIndex &occ);
void kernel_dense_disconnect(Kernel &);
// Garbage redundant clauses mentioning a retired variable (reference parity;
// called at elimination-phase exit).
void kernel_flush_retired_redundant(Kernel &);

// Irredundant clause count (kissat BINIRR_CLAUSES): live irredundant large
// clauses plus all binaries ever added. Binaries have an irredundant lifecycle
// here, and binary_ids keeps a deleted binary's id, so the binary term
// over-approximates the live count by the deleted ones — which the log-scale
// consumers tolerate. O(arena) walk: a per-round query, never a hot-path one.
uint64_t kernel_irredundant_clauses(const Kernel &);

// --- internal invariants (plan §8.2 oracle 4; debug builds) --------------
// aborts on: values/trail/unassigned incoherence, frame/level corruption,
// watch-list damage, or a missed conflict (fully propagated state with a
// falsified clause). No-op in NDEBUG builds.
void kernel_check_invariants(const Kernel &);

} // namespace tactix

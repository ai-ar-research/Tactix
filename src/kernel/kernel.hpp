#pragma once

#include "kernel/clause.hpp"
#include "kernel/types.hpp"
#include "kernel/watch.hpp"
#include "proof/proof.hpp"

#include <csignal>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tactix {

struct Kernel;

#ifdef TACTIX_ENABLE_EVENT_TRACE
void event_trace_phase_save(const Kernel &, Lit);
void event_trace_sweep_seed(const Kernel &, Var);
void event_trace_sweep_environment(const Kernel &, Var, uint32_t, uint32_t,
                                   uint64_t);
void event_trace_sweep_environment_clause(const Kernel &, Var, uint32_t,
                                          const Lit *, uint32_t);
void event_trace_sweep_query(const Kernel &, Var, uint32_t, const char *, Lit,
                             Lit, int, uint64_t);
void event_trace_sweep_effort(const Kernel &, uint32_t, uint64_t, uint64_t,
                              uint64_t);
void event_trace_sweep_environment_var(const Kernel &, Var, uint32_t, Var,
                                       uint32_t);
void event_trace_sweep_watch(const Kernel &, Var, uint32_t, Lit);
void event_trace_sweep_equivalence(const Kernel &, Lit, Lit);
void event_trace_sweep_unit(const Kernel &, Lit);
void event_trace_factor_seed(const Kernel &, Lit, uint32_t, uint32_t);
void event_trace_factor_grid(const Kernel &, Lit, uint32_t, uint32_t, int);
void event_trace_factor_grid_member(const Kernel &, uint32_t, Lit);
#endif

struct FactorSelection;
struct Heuristics;

struct Assigned {
  uint32_t level;
  uint32_t trail; // position on the trail
  uint32_t reason; // Ref, or other literal if binary, or sentinels
  bool binary;
  bool jumped;
};

struct Link {
  uint32_t prev, next, stamp;
};

struct Stats {
  uint64_t decisions = 0;
  uint64_t conflicts = 0;
  uint64_t propagations = 0;
  uint64_t ticks = 0; // propagation memory-traffic proxy (see propagate.cpp)
  uint64_t search_ticks = 0;
  uint64_t inprocess_propagations = 0; // of those, inside a simplification
  static constexpr uint32_t MAX_TIMED_RULES = 64;
  uint64_t rule_nanos[MAX_TIMED_RULES] = {}; // per registry rule id
  uint64_t rule_calls[MAX_TIMED_RULES] = {};
  uint64_t inprocess_nanos = 0; // whole slot, incl. scheduling not in a rule
  uint64_t solve_nanos = 0;     // wall time of solve(), set by the frontend
  uint64_t switched = 0; // mode switches (S4)
  uint64_t target_saved = 0; // target-phase snapshots
  uint64_t best_saved = 0;   // best-phase snapshots
  uint64_t rephased = 0;     // Rephase applications (S3)
  uint64_t reorder_eligible = 0; // scheduled occasions past the policy gate
  uint64_t reordered = 0;    // Reorder applications (S7, policy op)
  uint64_t lucky_early_calls = 0; // DefaultPolicy startup opportunities
  uint64_t lucky_late_calls = 0;  // ... after initial preprocessing
  uint64_t lucky_late_sat = 0;    // late opportunities proving SAT
  uint64_t lucky_probes = 0; // Lucky probes run (S5)
  uint64_t walks = 0;         // S6 Walk applications (policy op)
  uint64_t walk_flips = 0;    // literal flips
  uint64_t walk_steps = 0;    // occurrence entries scanned (kissat walk_steps)
  uint64_t walk_improved = 0; // walks that lowered the unsatisfied count
  uint64_t walk_minimum = UINT64_MAX;
  uint64_t warmups = 0; // root decision/propagation sweeps before Walk
  uint64_t warming_decisions = 0;
  uint64_t warming_conflicts = 0;
  uint64_t warming_propagations = 0;
  uint64_t promoted = 0;     // glue promotions on use (kissat deduce.c)
  uint64_t learned = 0;
  uint64_t learned_units = 0;
  uint64_t learned_lits = 0;      // sum of FINAL learned clause sizes
  uint64_t learned_glue_sum = 0;  // sum of learned glues
  uint64_t minimized = 0; // literals removed by clause minimization
  uint64_t minimize_depth_cutoffs = 0; // recursive walks stopped at Kissat cap
  uint64_t shrunken = 0;  // literals removed by all-UIP block shrinking
  uint64_t otfs_strengthened = 0; // reasons strengthened on the fly
  uint64_t otfs_subsumed = 0;     // conflicts subsumed by the resolvent
  uint64_t eager_tried = 0;     // containment tests run (kissat eagertried)
  uint64_t eager_subsumed = 0;  // deletions (kissat eagerly_subsumed)
  uint64_t eager_reason_skipped = 0; // subsumed but kept — still a trail reason
  uint64_t shrink_promoted = 0;
  uint64_t tumble_seeded = 0;
  static constexpr uint32_t MAX_GLUE_USED = 127;
  uint64_t used_glue[2][MAX_GLUE_USED + 1] = {};
  uint64_t retiered = 0;           // dynamic tier recomputations
  uint64_t tier2_band_dynamic = 0;
  uint64_t tier2_band_static = 0;
  uint64_t reduce_scans = 0;
  uint64_t reduce_scan_words = 0;
  uint64_t reduce_skippable_words = 0;
  uint64_t reason_bumped = 0;      // extra vars marked by reason-side bumping
  uint64_t reason_bump_rounds = 0; // conflicts the reason-side pass ran on
  uint64_t reason_bump_depth2 = 0; // rounds allowed to recurse in stable mode
  uint64_t reason_bump_aborted = 0; // rounds rolled back at the mark limit
  uint64_t api_decisions = 0; // externally chosen branches (N1)
  uint64_t input_restated = 0; // normalized input clauses restated by fresh id
  uint64_t decisions_extension = 0; // decisions on a Factor extension variable
  uint64_t decisions_substituted = 0; // ... on one Substitute rewrote away
  mutable uint64_t rule_interrupted = 0; // applications aborted on the
  mutable uint64_t stop_fuzz_fires = 0;  // stops reported by the stress knob,
  uint64_t bumps = 0;                // bump events (both modes, both channels)
  uint64_t bumps_extension = 0;      // ...of which landed on an extension var
  uint64_t bumps_ext_reason_only = 0; // ...of those, reason-side ONLY
  uint64_t restarts = 0;
  uint64_t restarts_reused = 0; // restarts that kept a trail prefix
  uint64_t reductions = 0;
  uint64_t deleted = 0; // learned clauses deleted by Reduce
  uint64_t variable_compactions = 0;
  uint64_t compacted_variables = 0;
  uint64_t binaries_deleted = 0; // binary clauses deleted (kernel_delete_binary)
  uint64_t forward_subsumptions = 0; // SubsumeForward rounds
  uint64_t forward_checked = 0;  // SubsumeForward candidates checked
  uint64_t forward_visits = 0;
  uint64_t forward_subsumed = 0; // clauses deleted by SubsumeForward
  uint64_t strengthen_rounds = 0;  // Strengthen rounds
  uint64_t strengthen_checked = 0; // Strengthen candidate checks
  uint64_t strengthen_visits = 0;  // partner visits; see forward_visits
  uint64_t strengthened = 0;       // clauses strengthened (add+delete)
  uint64_t strengthen_subsumed = 0; // deletions by Strengthen's fused round
  uint64_t budget_stops_c1 = 0; // SubsumeForward rounds cut by `work_budget`
  uint64_t budget_stops_c2 = 0; // Strengthen rounds cut by `work_budget`
  uint64_t subsume_interrupted = 0;
  uint64_t strengthen_interrupted = 0;
  uint64_t c1_shadow_observed = 0;  // clauses C1 would have deleted but did not
  uint64_t c1_shadow_deleted = 0;   // C2 deleted the same clause outright
  uint64_t c1_shadow_rewritten = 0; // C2 strengthened it away (old id retired)
  uint64_t flushed = 0; // root-satisfied deletions + root-falsified literal
  uint64_t sparse_collections = 0; // startup whole-arena root collections
  uint64_t sparse_collected = 0;   // clauses deleted or rewritten by them
  uint64_t vivify_rounds = 0;  // Vivify rounds
  uint64_t vivify_checked = 0; // Vivify candidates probed
  uint64_t vivified = 0;       // clauses distilled by Vivify (add+delete)
  uint64_t vivify_deleted = 0; // root-satisfied deletions by Vivify
  uint64_t vivify_flushed = 0; // scheduled clauses removed by prefix subsumption
  uint64_t vivify_reused = 0;     // levels taken from the retained trail
  uint64_t vivify_probes = 0;     // assumptions made; reuse rate = reused/both
  uint64_t vivify_reuse_full = 0; // candidates wholly covered by the prefix
  uint64_t vivify_tier3 = 0;      // tier3 candidates probed; 0 = that half of
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
  uint64_t probe_ticks = 0;        // Probe's own propagation ticks
  uint64_t backbone_ticks = 0;     // Backbone's own
  uint64_t vivify_ticks = 0;       // Vivify's own
  uint64_t instantiate_ticks = 0;  // Instantiate's own
  uint64_t bce_ticks = 0;          // occurrences + pairs scanned — BCE's budget
  uint64_t instantiate_rounds = 0;  // Instantiate rounds that strengthened
  uint64_t instantiate_checked = 0; // (clause, literal) pairs probed
  uint64_t instantiated = 0;        // literals removed by Instantiate

  uint64_t cce_rounds = 0;   // rounds attempted; nothing may read it — a
  uint64_t cce_tried = 0;    // candidate clauses extended
  uint64_t cce_deleted = 0;  // clauses eliminated
  uint64_t cce_blocked = 0;  // ... of which by the blocked exit (witnessed)
  uint64_t cce_ata = 0;      // ... of which by the asymmetric-tautology exit
  uint64_t cce_ala = 0;      // asymmetric literals added
  uint64_t cce_cla = 0;      // covered literals added (CCE's own mechanism)
  uint64_t cce_ticks = 0;    // clause literals scanned — CCE's budget currency

  uint64_t sweep_rounds = 0;       // detection rounds run
  uint64_t sweep_environments = 0; // environments built AND handed to the solver
  uint64_t sweep_env_clauses = 0;  // clauses encoded into them
  uint64_t sweep_solves = 0;       // sub-solver solve() calls
  uint64_t sweep_candidates = 0;   // candidate-pair directions actually solved
  uint64_t sweep_equivalences = 0; // pairs PROVED equivalent
  uint64_t sweep_units = 0;        // root units proved by local environments
  uint64_t sweep_backbone_candidates = 0;
  uint64_t sweep_backbone_flipped = 0;
  uint64_t sweep_binaries = 0;     // equivalence binaries ADDED with a proof
  uint64_t sweep_not_rup = 0;      // equivalences declined (not RUP)
  uint64_t sweep_general = 0;      // pairs emitted via the general proof path
  uint64_t random_sequences = 0;   // random-decision sequences started
  uint64_t random_decisions = 0;   // decisions actually taken at random
  uint64_t sweep_known = 0;        // pairs the formula already states as binaries
  uint64_t sweep_env_unsat = 0;    // environments refuted outright (F is UNSAT)
  uint64_t sweep_budget = 0;       // solves abandoned on the tick budget
  uint64_t sweep_ticks = 0;        // Sweep scan+sub-solver diagnostic
  uint64_t sweep_env_lits = 0;      // literal occurrences encoded
  uint64_t sweep_env_false = 0;     // of those, root-FALSE occurrences
  uint64_t sweep_env_freefalse = 0; // of those, with no unit id => free var
  uint64_t sweep_env_stripped = 0;  // root-false occurrences REMOVED
  uint64_t sweep_strip_cited = 0;   // ... with their root units cited in an
  uint64_t sweep_env_units = 0;     // unit clauses added alongside (off arm)
  uint64_t sweep_ticks_dedup = 0;   // the environment's clause-key scan
  uint64_t sweep_ticks_sub = 0;     // Sweep's scheduled effort currency
  uint64_t sweep_occ_f = 0;         // env vars' large-clause degree in F
  uint64_t sweep_occ_env = 0;       // ... and inside the environment
  uint64_t sweep_vars_closed = 0;   // env vars holding their WHOLE degree
  uint64_t sweep_vars_total = 0;    // env vars
  uint64_t sweep_cand_seeded = 0;   // candidates entering the loop
  uint64_t sweep_cand_refined = 0;  // candidates dropped by model refinement
  uint64_t sweep_flip_attempts = 0; // partition literals tested by model flip
  uint64_t sweep_flipped = 0;       // successful model flips
  uint64_t sweep_dir_sat = 0;       // candidate directions that MODELLED
  uint64_t sweep_dir_unsat = 0;     // candidate directions that REFUTED
  uint64_t sweep_scheduled = 0;     // eligible seeds placed in canonical tails
  uint64_t sweep_rescheduled = 0;   // equivalence representatives moved next
  uint64_t sweep_resumed = 0;       // seeds consumed from a retained tail
  uint64_t sweep_completed = 0;     // canonical tails exhausted
  uint32_t sweep_limit_vars = 0;    // last canonical environment limits
  uint32_t sweep_limit_clauses = 0;
  uint32_t sweep_limit_depth = 0;

  uint64_t ternary_rounds = 0;     // HyperTernaryRes rounds that added clauses
  uint64_t ternary_resolvents = 0; // ternary resolvents added (redundant)
  uint64_t hbr_rounds = 0;      // X6 HyperBinaryRes: canonical rounds entered
  uint64_t hbr_probes = 0;      // root literals closed under propagation
  uint64_t hbr_binaries = 0;    // resolvents emitted
  uint64_t hbr_subsumed = 0;    // reasons deleted because the resolvent subsumed them
  uint64_t hbr_declined = 0;    // resolvents the RUP replay could not re-derive
  uint64_t hbr_interrupts = 0;  // termination polls that fired mid-round
  uint64_t hbr_ticks = 0;       // private meter (see Kernel::tick_sink)
  uint64_t ternary_binaries = 0;   // binary resolvents added (permanent)
  uint64_t ternary_ticks = 0;      // occurrence entries + pairs examined
  uint64_t transitive_ticks = 0;   // cache lines scanned by the walks (kissat
  uint64_t transitive_deleted = 0; // transitively redundant binaries deleted
  uint64_t transitive_failed = 0;  // sources abandoned to Probe as failed (V4)
  uint64_t transitive_polls = 0;   // kernel_terminating evaluations at the site
  uint64_t transitive_terminated = 0; // dst walks cut by an interrupt
  uint64_t substitute_rounds = 0;  // Substitute rounds — detection passes
  mutable uint64_t substitute_ticks = 0; // binary-watch scan cache lines
  uint64_t substitute_vars = 0;    // variables substituted away
  uint64_t substitute_sccs = 0;
  uint64_t substitute_scc_lits = 0;
  uint64_t substitute_hazard_skips = 0;
  uint64_t substitute_hazard_partial = 0;
  uint64_t substitute_hazard_lits = 0;
  uint64_t deactivations = 0;  // counted at the site, so 0 means "never fired"
  uint64_t reactivations = 0;  // ... of those, later re-inserted into a ranking
  uint64_t conflict_reused = 0; // conflicts resolved by reusing the conflict as
  uint64_t jumped_reasons = 0;  // binary reasons replaced by their resolvent
  uint64_t jump_reason_classifications = 0;
  uint64_t jump_reason_bigbig = 0;
  uint64_t conflict_watch_repaired = 0; // ... of those, needing watch repair
  uint64_t binprop_scans = 0;
  uint64_t binprop_ordered = 0;
  uint64_t binprop_flagged = 0;
  uint64_t binprop_words = 0;
  uint64_t binprop_skipped = 0;
  uint64_t binprop_large = 0;
  uint64_t chronological = 0;  // chrono backtrack targets taken
  uint64_t backjump_stale = 0;
  uint64_t inprocess_phases = 0;   // policy inprocessing phases (a root descent
  uint64_t congruence_rounds = 0;  // Congruence rounds — gate-closure passes
  uint64_t congruence_binary_rebuilds = 0;
  uint64_t congruence_unjustified = 0;
  uint64_t congruence_units = 0; // root units from degenerate gates
  uint64_t congruence_y1_found = 0; // detections
  uint64_t congruence_y1_units = 0; // ... emitted and batched
  uint64_t congruence_binary_equivalences = 0; // paired binaries imported
  mutable uint64_t congruence_gates_and = 0;
  mutable uint64_t congruence_gates_xor = 0;
  mutable uint64_t congruence_gates_ite = 0;
  uint64_t congruence_gates = 0;   // gates EXTRACTED, over all applications
  uint64_t congruence_merges = 0;  // equivalences MERGED, over all applications
  mutable uint64_t congruence_ticks = 0;
  mutable uint64_t congruence_budget_stops = 0; // rounds cut by `work_budget`
  uint64_t factorings = 0;         // Factor/BVA applications — grids replaced
  uint64_t factor_large_remainders = 0; // remainders offered by the widened
  uint64_t factor_demotions = 0;    // demotion calls that moved something
  uint64_t factor_demoted_vars = 0; // extension vars pushed to the queue front
  uint64_t factor_ticks = 0;   // Factor's budget currency (kissat
  uint64_t bve_rounds = 0;         // BVE canonical-apply calls (a policy that
  uint64_t bve_eliminated = 0;     // variables eliminated by BVE resolution
  uint64_t bve_resolutions = 0;    // resolvents attempted — BVE's unit of work
  uint64_t bve_bwd_queued = 0;      // fresh resolvents used as subsumers
  uint64_t bve_bwd_subsumed = 0;    // victims deleted (cadical elimbwsub)
  uint64_t bve_bwd_strengthened = 0; // victims shortened (cadical elimbwstr)
  uint64_t bve_bwd_units = 0;       // strengthenings that collapsed to a unit
  uint64_t flushed_retired_redundant = 0; // reference-parity flush at phase exit
  uint64_t factor_rejected_by_bound = 0; // grids the coupled bound declined
  uint64_t bve_marks = 0;
  uint64_t touch_marks = 0;
  uint64_t touch_kept_marks = 0;
  uint64_t forward_mark_skipped = 0;
  uint64_t strengthen_mark_skipped = 0;
  uint64_t forward_mark_advances = 0;
  uint64_t strengthen_mark_advances = 0;
  uint64_t forward_rearmed = 0;
  uint64_t strengthen_rearmed = 0;
  uint64_t bce_mark_skipped = 0, bce_rearmed = 0;
  uint64_t ternary_mark_skipped = 0, ternary_rearmed = 0;
  uint64_t instantiate_mark_skipped = 0, instantiate_rearmed = 0;
  uint64_t bce_interrupted = 0;
  uint64_t ternary_interrupted = 0;
  uint64_t instantiate_interrupted = 0;
  uint64_t factor_mark_skipped = 0, factor_rearmed = 0;
  uint64_t subsume_holds = 0; // outer elimination rounds held open because
  uint64_t touch_redundant_dynamic = 0;
  uint64_t touch_redundant_static6 = 0;
  uint64_t strengthen_red_checked = 0;
  uint64_t strengthen_red_skipped = 0;
  uint64_t strengthen_red_subsumed = 0;
  uint64_t strengthen_red_strengthened = 0;
  uint64_t cce_mark_skipped = 0, cce_rearmed = 0, cce_mark_advances = 0;
  uint64_t bve_gates_equiv = 0;
  uint64_t bve_gates_and_or = 0;
  uint64_t bve_gates_ite = 0;
  uint64_t bve_gates_xor = 0;
  uint64_t bve_definitions_checked = 0; // attempts
  uint64_t bve_gates_definition = 0;    // ... that found a definition
  uint64_t bve_definition_ticks = 0;
  uint64_t bve_definition_budget = 0;   // attempts the per-attempt tick ceiling
  uint64_t bve_gate_pairs_saved = 0; // resolvent pairs the gate reduction
};

struct PendingLearned {
  bool valid = false;
  std::vector<Lit> lits; // lits[0] = asserting literal
  Ref ref = INVALID_REF; // arena ref for size >= 3, INVALID otherwise
  uint32_t glue = 0;
  uint32_t jump_level = 0;
  uint64_t id = 0; // proof clause id (LRAT)
};

struct OccIndex {
  std::vector<std::vector<Ref>> large; // by literal; may hold stale garbage Refs
};

struct ArenaOrdering {
  virtual ~ArenaOrdering() = default;
  virtual void order(const Kernel &, std::vector<Ref> &) = 0;
};

struct ProbingLog {
  bool recording = false;
  uint32_t base_trail = 0, base_head = 0, base_level = 0;
  struct ListSnap {
    Lit lit;
    uint32_t off, size; // offset into `words`, and the list's original length
    bool has_large, binaries_first;
  };
  std::vector<ListSnap> lists;
  std::vector<uint32_t> words; // flat backing store for the snapshots above
  std::vector<uint32_t> stamp; // by lit: epoch this literal was snapshotted in
  uint32_t epoch = 0;          // bumped per scope; 0 is "never snapshotted"
  struct ClauseSnap {
    Ref ref;
    uint32_t rpos;             // index the replacement literal came from (>= 2)
    Lit lit0, lit1;            // the watched pair before the relocation
    uint32_t searched;
  };
  std::vector<ClauseSnap> clauses;
};

enum class VarStatus : uint8_t { Active = 0, Substituted, Eliminated };

// Analyze is atomic at the rule boundary, but Kissat updates a policy clock
// between OTFS re-analysis rounds. Preserve the exact observation made at
// that internal boundary so DefaultPolicy can consume it after the rule
// returns without moving strategy logic into the proof rule.
struct AnalysisConflictCheckpoint {
  uint64_t conflicts = 0;
  uint32_t mode = 0;
  uint64_t used_glue[Stats::MAX_GLUE_USED + 1] = {};
};

struct Kernel {
  uint32_t nvars = 0;
  // Stable external identities are separate from the dense indices used by
  // search.  Before the first compaction both maps are the identity.  A dead
  // external variable maps to INVALID_VAR; every live internal variable maps
  // back to the permanent DIMACS/proof variable that names it.
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  uint32_t original_external_vars = UINT32_MAX;
  uint32_t first_extension_var = UINT32_MAX;
  bool inconsistent = false; // root-level conflict happened

  std::vector<int8_t> values;     // by lit: 1 true, -1 false, 0 unassigned
  std::vector<Assigned> assigned; // by var
  std::vector<Lit> jumped_over;

  std::vector<Lit> trail;
  uint32_t propagate_head = 0;
  bool trail_tainted = false;
  std::vector<uint32_t> frames = {0};
  uint32_t level = 0;
  uint32_t unassigned = 0;
  uint32_t unassigned_active = 0;
  bool warming = false; // Kissat walk warm-up: suppress random branching.

  Arena arena;
  WatchLists watches;
  ArenaOrdering *arena_ordering = nullptr;
  OccIndex *dense = nullptr;
  std::vector<uint32_t> delayed;
  ProbingLog probing;

  Heuristics *h = nullptr;

  std::vector<Lit> reconstruction;
  std::vector<int8_t> model_values; // by stable external literal, after extend
  bool model_extended = false; // kernel_extend_model idempotency guard

  bool conflict_valid = false;
  Ref conflict_ref = INVALID_REF; // INVALID means binary conflict
  Lit conflict_bin[2] = {INVALID_LIT, INVALID_LIT};

  PendingLearned pending;

  std::vector<uint8_t> seen;   // by var
  std::vector<Var> analyzed;   // vars marked seen
  std::vector<Var> shrinkable; // vars marked during a block-shrink walk
  uint32_t analyzed_seen = 0;
  std::vector<Var> reason_bumped; // REASON_BUMP marks, for rollback
  std::vector<Var> minimize_scratch; // iterative binary-reason chain
  std::vector<Lit> learned_scratch; // the clause under construction
  std::vector<Lit> otfs_scratch;    // OTFS resolvent assembly
  // OTFS materializes a replacement because variable-size arena records can
  // not shrink in place.  The old reason stays valid until Backjump unassigns
  // its propagated literal, then is retired before the fused rule returns.
  std::vector<Ref> otfs_retired;
  std::vector<AnalysisConflictCheckpoint> analysis_conflict_checkpoints;
  std::vector<Var> radix_scratch;   // radix-sort double buffer
  std::vector<uint8_t> marked_levels;
  std::vector<uint32_t> touched_levels;

  static constexpr uint32_t EAGER_SUBSUME_SLOTS = 4; // kissat's option maximum
  Ref eager_recent[EAGER_SUBSUME_SLOTS] = {INVALID_REF, INVALID_REF,
                                           INVALID_REF, INVALID_REF};
  std::vector<uint8_t> eager_marks;

  uint64_t next_clause_id = 0;
  std::vector<uint64_t> unit_ids; // by var
  std::unordered_map<uint64_t, uint64_t> binary_ids;
  std::vector<std::pair<uint32_t, uint64_t>> lrat_entries;
  std::vector<uint64_t> lrat_chain; // built by kernel_lrat_chain below
  std::vector<Var> jump_pending;

  struct InputRestatement {
    uint64_t id;    // the file-position id; final antecedent of the lemma
    uint32_t begin; // slice of input_restate_lits (the normalized clause)
    uint32_t size;
    uint32_t unit_begin; // root-true units used to remove false literals
    uint32_t unit_size;
    Ref ref; // large clauses only: the header to re-point
  };
  std::vector<InputRestatement> input_restatements;
  std::vector<Lit> input_restate_lits;
  std::vector<Lit> input_restate_units;
  std::vector<Var> input_activation_order;
  std::vector<uint8_t> input_activated; // per-var membership, parse-scoped
  // Permanent external-variable import ranks. Kissat's tumble=1 assigns
  // internal variables on first input occurrence; Tactix keeps public internal
  // identities stable and consults this equivalent order at reference-ordered
  // traversals instead.
  std::vector<uint32_t> external_import_rank;
  uint32_t next_import_rank = 0;
  bool input_refuted = false;
  uint64_t input_refutation_id = 0; // the refuting input clause
  std::vector<Lit> input_refutation_units; // root-true antecedent units

  Proof *proof = nullptr;
  Stats stats;

  std::vector<uint8_t> bve_retry;
  uint32_t bve_retry_fixed_head = 0;

  // Exact persistent Kissat `flags->factor` bit, by literal polarity. Clause
  // addition sets it; Factor clears it when that literal is popped. Unlike the
  // shared per-variable touch epochs, this does not spuriously arm the opposite
  // polarity.
  std::vector<uint8_t> factor_marks;

  uint64_t touch_epoch = 0;
  std::vector<uint64_t> touched; // by var
  std::vector<uint64_t> removed; // by var
  std::vector<uint64_t> touched_kept; // by var
  uint32_t touch_fixed_head = 0;
  uint64_t subsume_touch_mark = 0;
  uint64_t strengthen_touch_mark = 0;
  uint64_t bce_touch_mark = 0;
  uint64_t ternary_touch_mark = 0;
  uint64_t instantiate_touch_mark = 0;
  uint64_t cce_touch_mark = 0;
  bool subsume_complete = true;
  bool strengthen_complete = true;
  bool touch_filter = true;
  bool subsume_holds_elim = true;
  bool touch_redundant = true;
  bool subsume_shadow = false;
  std::unordered_set<uint64_t> shadow_victims;
  uint32_t kept_glue = 0;
  uint32_t kept_size = 0;
  std::vector<uint16_t> bve_min_bound;
  bool bve_track_min_bound = false;

  uint32_t bve_gate_mask = ~0u;
  uint32_t bve_bound = 16;
  // Policy-owned BVE occurrence cap.  UINT32_MAX leaves the public rule
  // unrestricted; DefaultPolicy installs its named strategy configuration.
  uint32_t bve_occurrence_limit = UINT32_MAX;
  bool bve_backward = true;
  uint64_t bve_definition_budget = 10000;
  uint32_t bce_occ_limit = 0;
  bool midround_budget = false;
  uint32_t factor_size = 5;
  bool transitive_early_exit = false;
  bool transitive_live_meter = true;
  uint32_t ternary_occ_limit = 100;      // cadical ternaryocclim: skip a pivot
  uint32_t ternary_max_add_percent = 1000; // cadical ternarymaxadd: stop a round
  FactorSelection *factor_selection = nullptr;
  uint32_t factor_bound = 0;
  bool factor_demote_fresh = true;
  uint32_t factor_cand_rounds = 2;
  bool no_ext_decisions = false;
  bool no_ext_reason_bump = false;
  bool subst_hazard_round = false;
  std::vector<VarStatus> var_status;
  std::vector<Var> retired_events;
  std::vector<Var> substituted_events;
  struct FreshVars {
    Var first = 0;
    Var end = 0;
  };
  std::vector<FreshVars> fresh_var_events;
  bool subst_demote = false;
  bool retire_enabled = true;
  volatile sig_atomic_t *stop = nullptr;
  uint32_t stop_fuzz = 0;
  mutable uint64_t stop_fuzz_state = 0x9E3779B97F4A7C15ull;
  bool factor_composite = false;
  bool factor_admit = true;
  uint64_t *tick_sink = nullptr;
  bool inprocessing = false;
  // Non-owning fast path into policy-owned H.  Kissat updates saved phases at
  // assignment time; keeping the vector hook here lets the inline assignment
  // primitive do the same without exposing the rest of Heuristics to rules.
  // ScopedHeuristics/the frontend install it together with `h`.
  std::vector<int8_t> *saved_phases = nullptr;
  bool phase_saving_suppressed = false; // true only for probing assignments
  uint64_t work_budget = 0;
  uint8_t vivify_scope = 0;
  bool vivify_reuse = true;
  bool vivify_flush = true;
  bool sweep_resume = true;
  std::vector<Var> sweep_tail; // stable external identities
  uint32_t sweep_tail_next = 0;
  uint32_t sweep_passes_completed = 0;
  bool conflict_reuse = false;
  bool conflict_reuse_no_repair = false;
  bool jump_reasons = true;
  bool jump_reasons_bigbig = false;
  uint32_t bump_reason_limit = 0;
  uint32_t bump_reason_depth = 1;
  uint32_t eager_subsume = EAGER_SUBSUME_SLOTS;
  // DefaultPolicy retains Kissat's separate focused/stable tier snapshots.
  // `tier1/tier2` is the pair posed for the rule currently being applied.
  uint32_t tier1_by_mode[2] = {2, 2};
  uint32_t tier2_by_mode[2] = {6, 6};
  uint32_t tier1 = 2, tier2 = 6;
  bool shrink_glue_promote = true;
  bool tumble = true;

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

void kernel_reserve_vars(Kernel &, uint32_t nvars);
void kernel_record_import(Kernel &, Var);
uint64_t kernel_import_var_order(const Kernel &, Var);
uint64_t kernel_import_lit_order(const Kernel &, Lit);

Var kernel_new_extension_vars(Kernel &, uint32_t count);

inline uint32_t kernel_num_output_vars(const Kernel &k) {
  return k.original_external_vars == UINT32_MAX
             ? static_cast<uint32_t>(k.external_to_internal.size())
             : k.original_external_vars;
}

inline uint32_t kernel_num_original_internal_vars(const Kernel &k) {
  return k.first_extension_var == UINT32_MAX ? k.nvars : k.first_extension_var;
}

inline Lit kernel_external_lit(const Kernel &k, Lit l) {
  assert(var_of(l) < k.internal_to_external.size());
  return make_lit(k.internal_to_external[var_of(l)], negated(l));
}

inline int kernel_externalize(const Kernel &k, Lit l) {
  return externalize(kernel_external_lit(k, l));
}

inline Lit kernel_internalize(const Kernel &k, int elit) {
  const Lit external = internalize(elit);
  const Var ev = var_of(external);
  if (ev >= k.external_to_internal.size())
    return INVALID_LIT;
  const Var iv = k.external_to_internal[ev];
  return iv == INVALID_VAR ? INVALID_LIT : make_lit(iv, negated(external));
}

void kernel_push_witness(Kernel &, Lit witness, const Lit *rest, uint32_t nrest);
void kernel_extend_model(Kernel &);

// Policy-free support primitive: densely renumber variables whose formula
// retirement is already complete.  External proof/API/model identities do not
// change.  Returns false when there is nothing to remove or a retirement
// invariant is not yet strong enough to compact safely.
bool kernel_compact_variables(Kernel &);
bool kernel_should_compact_variables(const Kernel &, uint32_t percent);

void kernel_emit_empty_from_pair(Kernel &, uint64_t id_a, uint64_t id_b);

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

enum class ReplaceOutcome { Refuted, Unit, Binary, Large };
ReplaceOutcome kernel_replace_clause(Kernel &, Clause *orig, uint64_t id,
                                     const Lit *lits, uint32_t size,
                                     bool redundant, uint32_t glue,
                                     Ref *large_ref); // nullable

// Proof-safe root-unit consumption shared by startup sparse collection and C2.
// The clause form does not collect the arena; callers can continue scanning by
// the original clause width. The whole-arena form collects once at its exit.
enum class RootFlushOutcome { None, Deleted, Rewritten, Binary, Unit, Refuted };
RootFlushOutcome kernel_flush_root_clause(Kernel &, Ref *ref,
                                          uint64_t *changed = nullptr);
enum class RootCollectOutcome { NoChange, Changed, Unit, Refuted };
RootCollectOutcome kernel_sparse_collect_root(Kernel &);

inline void kernel_mark_bve_retry(Kernel &k, Var v) {
  k.bve_retry[v] = 1;
  k.stats.bve_marks++; // events, not bit flips — see Stats::bve_marks
}
void kernel_bve_retry_sync_fixed(Kernel &);

inline void kernel_touch_var(Kernel &k, Var v) {
  k.touched[v] = ++k.touch_epoch;
  k.stats.touch_marks++; // events; a stamp has no flip to count
}
inline void kernel_removed_var(Kernel &k, Var v) {
  k.removed[v] = ++k.touch_epoch;
  k.stats.touch_marks++;
}
inline bool kernel_touched_since(const Kernel &k, Var v, uint64_t since) {
  return k.touched[v] > since;
}
inline void kernel_touch_kept_var(Kernel &k, Var v) {
  k.touched_kept[v] = ++k.touch_epoch;
  k.stats.touch_kept_marks++;
}
inline bool kernel_kept_since(const Kernel &k, Var v, uint64_t since) {
  return k.touched_kept[v] > since;
}
inline bool kernel_likely_kept(const Kernel &k, bool redundant, uint32_t glue,
                               uint32_t size) {
  return !redundant || glue <= k.tier2 ||
         (k.kept_glue && glue <= k.kept_glue && size <= k.kept_size);
}
inline bool kernel_changed_since(const Kernel &k, Var v, uint64_t since) {
  return k.touched[v] > since || k.removed[v] > since;
}
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
void kernel_touch_sync_fixed(Kernel &);

Ref kernel_add_large_clause(Kernel &, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue, uint64_t id = 0);
void kernel_add_binary_clause(Kernel &, Lit a, Lit b, uint64_t id = 0);
bool kernel_delete_binary(Kernel &, Lit a, Lit b);
bool kernel_delete_binary_all_copies(Kernel &, Lit a, Lit b);

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

void kernel_unwatch_large(Kernel &, Lit l, Ref ref);

// Exchange two large-clause entries in one watch list without changing their
// positions.  OTFS uses this to give a freshly allocated replacement the old
// clause's surviving-watch position while both clauses remain fully watched.
// The last two arguments are the blockers each Ref must have AFTER the swap.
void kernel_swap_large_watch_refs(Kernel &, Lit l, Ref first, Ref second,
                                  Lit blocking_for_first,
                                  Lit blocking_for_second);

void kernel_promote_watched_lit(Kernel &, Ref ref, uint32_t slot,
                                uint32_t from);

inline void kernel_delete_clause(Kernel &k, Clause *c) {
  if (k.proof)
    k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
  if (c->garbage())
    return; // already retired: the mark and its bookkeeping are idempotent
  c->flags |= Clause::GARBAGE;
  if (!c->redundant())
    for (const Lit l : *c) {
      kernel_mark_bve_retry(k, var_of(l));
      kernel_removed_var(k, var_of(l)); // BCE's side of the substrate
    }
}

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

inline bool kernel_var_retired(const Kernel &k, Var v) {
  return v < k.var_status.size() && k.var_status[v] != VarStatus::Active;
}

inline uint64_t kernel_cache_lines(uint64_t n) { return (n + 31u) >> 5; }

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
  // kissat inlineassign.h: every non-probing assignment, including a root
  // implication, refreshes the saved phase immediately.  Unassign-time saving
  // is not equivalent when a schedule changes mode while literals remain on
  // the trail.  Probing callers suppress this hook around speculative work.
  if (k.saved_phases && !k.phase_saving_suppressed && !k.probing.recording)
    (*k.saved_phases)[v] = negated(lit) ? -1 : 1;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (k.saved_phases && !k.phase_saving_suppressed && !k.probing.recording)
    event_trace_phase_save(k, lit);
#endif
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
inline void kernel_assign_binary(Kernel &k, Lit lit, Lit other_false,
                                 bool require_bigbig = true) {
  const Assigned &a = k.assigned[var_of(other_false)];
  const uint32_t level = a.level;
  if (k.jump_reasons && (!require_bigbig || k.jump_reasons_bigbig) && level &&
      a.binary) {
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
void kernel_backtrack(Kernel &, uint32_t target_level,
                      bool search_backtrack = true);

bool kernel_propagate(Kernel &, Ref ignore = INVALID_REF);

// Walk warm-up propagation (kissat propbeyond.c): scan to a full assignment
// and keep going after conflicting clauses.  Conflicts are counted but never
// installed as search conflicts; non-conflicting implications use the normal
// assignment path and therefore refresh saved phases.
void kernel_propagate_beyond_conflicts(Kernel &);

bool kernel_propagate_binary(Kernel &);

void kernel_probing_begin(Kernel &);
void kernel_probing_end(Kernel &); // idempotent; a no-op outside a scope
bool kernel_propagate_probing(Kernel &, Ref ignore = INVALID_REF);


inline bool kernel_terminating(const Kernel &k) {
  if (k.stop_fuzz) {
    k.stop_fuzz_state = k.stop_fuzz_state * 6364136223846793005ull + 1442695040888963407ull;
    if ((k.stop_fuzz_state >> 33) % k.stop_fuzz == 0) {
      k.stats.stop_fuzz_fires++;
      return true;
    }
  }
  return k.stop && *k.stop;
}

inline bool kernel_rule_interrupt(Kernel &k, uint64_t &site) {
  if (!kernel_terminating(k))
    return false;
  k.stats.rule_interrupted++;
  site++;
  return true;
}

inline void kernel_shadow_observe(Kernel &k, uint64_t id) {
  if (k.shadow_victims.insert(id).second)
    k.stats.c1_shadow_observed++;
}
inline bool kernel_shadow_retire(Kernel &k, uint64_t id) {
  return k.subsume_shadow && k.shadow_victims.erase(id) != 0;
}

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

void kernel_lrat_chain(Kernel &, uint64_t closing_id);

void kernel_reason_walk(Kernel &, Var start, std::vector<uint8_t> &seen,
                        std::vector<std::pair<uint32_t, uint64_t>> &entries,
                        std::vector<Var> *touched = nullptr);
uint64_t kernel_conflict_reason_walk(
    Kernel &, std::vector<uint8_t> &seen,
    std::vector<std::pair<uint32_t, uint64_t>> &entries,
    std::vector<Var> *touched = nullptr);


// `watch_start` is Kissat's sparse-collection watermark: watches below it keep
// their current order, while relocated clauses at and above it are rewatched
// in arena order. The default performs a whole-arena rebuild.
void kernel_collect_garbage(Kernel &, Ref watch_start = 0);
void kernel_rebuild_large_watches(Kernel &);
void kernel_canonicalize_binary_watches(Kernel &);

void kernel_dense_connect(Kernel &, OccIndex &occ);
void kernel_dense_disconnect(Kernel &);
void kernel_flush_retired_redundant(Kernel &);

uint64_t kernel_irredundant_clauses(const Kernel &);

void kernel_check_invariants(const Kernel &);

} // namespace tactix

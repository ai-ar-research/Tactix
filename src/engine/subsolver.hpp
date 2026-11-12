// The embedded sub-solver: a minimal CDCL refuting questions on small extracted
// clause sets (docs/design/v5-sweep-design.md §3). An engine shared by Sweep
// (candidate equivalences) and BVE (definition extraction, kissat
// definition.c), so it lives outside `src/rules/`.
#pragma once

#include <csignal>
#include <cstdint>
#include <vector>

namespace tactix {

// Invariants: it owns its arena, watches, trail and values and never touches
// `Kernel`; it is budgeted in its own ticks and returns `Budget` instead of
// guessing (§5.2); it is deterministic (fixed fallback order, or Kitten's
// per-environment generator, VMTF queue and saved phases for Sweep).

// Same literal shape as the kernel, but indexing the extracted compact space.
using SubLit = uint32_t;
using SubVar = uint32_t;

inline SubLit sub_lit(SubVar v, bool negated) { return 2 * v + (negated ? 1 : 0); }
inline SubVar sub_var_of(SubLit l) { return l >> 1; }
inline SubLit sub_negate(SubLit l) { return l ^ 1u; }

enum class SubResult {
  Sat,    // a total model was found (`model()` holds it)
  Unsat,  // refuted under the assumptions
  Budget, // the tick budget ran out — NO conclusion may be drawn
};

enum class SubPropagationResult {
  Consistent,
  Conflict,
  Interrupted,
  Budget,
};

enum class SubRupAdmission {
  Accepted,
  Tautology,
  NotRup,
  Invalid,
  Unavailable,
  Budget,
  Interrupted,
  Exhausted,
};

enum class SubTickMode {
  Entries, // legacy shared-engine work unit used by BVE definition extraction
  Kitten,  // Kissat cache-line currency used by Sweep scheduling
};

enum class SubSearchMode {
  Fixed,  // deterministic shared-engine fallback used by BVE definitions
  Kitten, // Kissat's imported-variable VMTF queue and randomized phases
};

// One derived clause: assuming every literal of `lits` false, propagating
// `chain` in order makes each entry unit and the last conflicting; empty `lits`
// is the empty clause. `antecedents` is what Kitten records on the lemma
// (`kitten.c:965`) and its clausal core walks; failing-assumption analysis
// stops at a root reason (`kitten.c:1461-1477`), so it can differ from the
// cone. Empty means the two coincide.
struct SubStep {
  uint32_t id = 0;
  std::vector<SubLit> lits;
  std::vector<uint32_t> chain;
  std::vector<uint32_t> antecedents;

  const std::vector<uint32_t> &core_antecedents() const {
    return antecedents.empty() ? chain : antecedents;
  }
};

// A complete refutation: a swept equivalence is entailed but not RUP in general
// (design §4), so the chain comes from this solver's resolution. `steps` is in
// derivation order, pruned to the target's dependencies, and ends with the
// target clause; `core` lists the ascending original ids used, which the caller
// maps to the main formula.
struct SubDerivation {
  std::vector<SubStep> steps;
  std::vector<uint32_t> core;
  bool resource_exhausted = false;
  // Kitten's clausal core: every clause reachable from the refuting lemma over
  // `antecedents`, once each, in `kitten_compute_clausal_core` post-order
  // (`kitten.c:1978-2008`); `learned` is what Kissat's `save_core_clause`
  // filters on (`sweep.c:333-337`).
  struct CoreClause {
    uint32_t id = 0;
    bool learned = false;
    std::vector<SubLit> lits;
  };
  std::vector<CoreClause> core_clauses;
};

class SubSolver {
public:
  // Reset to an empty formula over `nvars`, reusing allocations.
  void reset(uint32_t nvars,
             SubSearchMode mode = SubSearchMode::Fixed);

  // Reset Kitten over dense variable ids already in first-occurrence order
  // (BVE's proof map), skipping a second import map; behaviour equals
  // `reset(nvars, SubSearchMode::Kitten)` with that order.
  void reset_compact_kitten(uint32_t nvars);

  // One Kitten per Sweep invocation; clear advances its generator.
  void start_kitten_session(bool clear_before_first_environment = false);

  // Kissat randomizes phases before each Sweep solve (not BVE).
  void randomize_phases();

  // Add an original clause and return its id: ids count from 1 in add order,
  // matching a DIMACS file written in that order, and every call (tautologies
  // included) consumes one. Not after `solve`.
  uint32_t add_clause(const std::vector<SubLit> &lits);

  // Solve under `assumptions` (budget 0 unlimited); deterministic.
  SubResult solve(const std::vector<SubLit> &assumptions,
                  uint64_t ticks_budget,
                  SubTickMode tick_mode = SubTickMode::Entries);
  // Polls `stop` and returns Budget without a verdict.
  SubResult solve_interruptible(const std::vector<SubLit> &assumptions,
                                uint64_t ticks_budget,
                                SubTickMode tick_mode,
                                const volatile sig_atomic_t *stop);

  // Unit propagation under temporary assumptions only.
  SubPropagationResult propagate_assumptions_only(
      const std::vector<SubLit> &assumptions, uint64_t ticks_budget,
      const volatile sig_atomic_t *stop = nullptr);

  // Persistent RUP oracle: check by BCP, unwind assumptions, install only an
  // accepted clause. `proof` captures the accepted clause and its ordered RUP
  // antecedents under stable oracle ids, and is cleared otherwise. Fixed mode
  // only, before any full solve; zero budget does no work.
  SubRupAdmission admit_rup(const std::vector<SubLit> &, uint64_t ticks_budget,
                           const volatile sig_atomic_t *stop = nullptr,
                           SubStep *proof = nullptr);

  // Append a trusted premise to a Fixed-mode oracle, extending its variables if
  // needed; assumptions unwind, roots, clauses and ids survive. False leaves it
  // unchanged.
  bool append_input(uint32_t nvars, const std::vector<SubLit> &);

  // The refutation behind the last `Unsat`; false if none (no refutation, or
  // contradictory assumptions with a tautological target). `with_core` also
  // fills Kitten's clausal core (one arena walk). False with
  // `out.resource_exhausted` means export could not allocate: the caller stops
  // that environment.
  bool derivation(SubDerivation &out, bool with_core = false) const noexcept;

  // The last refutation's target, without exporting its proof.
  const SubStep *refutation_target() const {
    return have_derivation_ ? &target_ : nullptr;
  }

  // Kitten definition-core refinement: keep only core originals, drop learned
  // state, then shuffle the VMTF queue, watches and units before a second
  // solve; original ids and external literals stay available.
  void shrink_to_clausal_core(const std::vector<uint32_t> &core);
  void shuffle_clauses();

  // Model, by variable: +1 true, -1 false (valid only after `Sat`).
  const std::vector<int8_t> &model() const { return model_; }
  // Flip `lit` in the SAT model holding all else fixed; false if a clause would
  // break or the variable is root-fixed. A model filter only; it proves
  // nothing.
  bool flip_model_literal(SubLit lit);
  // Root value of an external literal, else zero (Kitten's `kitten_fixed`).
  int8_t fixed_literal(SubLit lit) const;
  bool has_model() const { return model_valid_; }
  // Effort per watch entry inspected, as `transitive_ticks` and kin.
  uint64_t ticks() const { return ticks_; }
  uint64_t kitten_ticks() const { return kitten_ticks_; }
  uint64_t kitten_generator() const { return kitten_generator_; }
  uint32_t kitten_phase_capacity() const { return phase_capacity_vars_; }
  // Reported beside `ticks()` as Sweep's (yield, effort) pair.
  uint64_t conflicts() const { return conflicts_; }
  uint32_t nvars() const { return nvars_; }
  // Ids <= this are original clauses; above it are derived lemmas.
  uint32_t original_clauses() const { return original_clauses_; }
  // The formula itself is refuted, so every later `solve` returns `Unsat`;
  // per-assumption callers must check this rather than read two vacuous
  // `Unsat`s as an equivalence.
  bool inconsistent() const { return inconsistent_; }

private:
  template <bool Interruptible>
  SubResult solve_impl(const std::vector<SubLit> &, uint64_t, SubTickMode,
                       const volatile sig_atomic_t *);
  // Clause header: `arena_[ref - 2]` id, `arena_[ref - 1]` size.
  using SubRef = uint32_t;
  static constexpr SubRef NO_REF = UINT32_MAX;

  struct SubWatch {
    SubLit blocking;
    SubRef ref;
  };

  void assign(SubLit l, SubRef reason);
  bool propagate();            // false on conflict (conflict_ref_ is then set)
  void backtrack(uint32_t level);
  bool analyze();              // false when the conflict cannot be analyzed
  void fail_assumptions(const std::vector<SubLit> &);
  SubLit next_decision();
  void reset_impl(uint32_t nvars, SubSearchMode mode,
                  bool compact_identity);
  SubLit import_literal(SubLit);
  SubLit export_literal(SubLit) const;
  std::vector<SubLit> export_literals(const std::vector<SubLit> &) const;
  void queue_enqueue(SubVar);
  void queue_dequeue(SubVar);
  void queue_move_to_front(SubVar);
  void queue_update_search(SubVar);
  void queue_bump_analyzed();
  uint64_t next_kitten_random();
  uint32_t pick_kitten_random(uint32_t begin, uint32_t end);
  void clear_assignments();
  bool propagate_units();
  void attach(SubRef ref);
  void append_oracle_clause(std::vector<SubLit>);
  SubRef add_derived(const std::vector<SubLit> &lits,
                     const std::vector<uint32_t> &chain,
                     const std::vector<uint32_t> &ante = {});
  // An empty clause stored last starts at the arena end.
  SubLit *clause_lits(SubRef ref) { return arena_.data() + ref; }
  const SubLit *clause_lits(SubRef ref) const { return arena_.data() + ref; }
  uint32_t clause_size(SubRef ref) const { return arena_[ref - 1]; }
  uint32_t clause_id(SubRef ref) const { return arena_[ref - 2]; }

  // --- proof plumbing. Record `v`'s reason cone as (trail position, id) pairs,
  // whose sort is the checker's propagation order; decisions end the walk and
  // are reported through `decisions` when non-null.
  void record_cone(SubVar v, std::vector<SubLit> *decisions);
  void chain_clear();
  // sorted ids, with `closing` appended last when it is not NO_REF
  std::vector<uint32_t> chain_build(SubRef closing);
  std::vector<uint32_t> refute_antecedents(SubRef conflict);
  const SubStep *step_of(uint32_t id) const;
  void compute_clausal_core(SubDerivation &out) const;
  void refute_empty(SubRef conflict); // the empty clause, from a level-0 conflict

  uint32_t nvars_ = 0;
  uint32_t imported_vars_ = 0;
  uint32_t phase_capacity_vars_ = 0;
  // `kitten_clear` keeps the allocated size for phase randomization.
  uint32_t kitten_capacity_vars_ = 0;
  bool inconsistent_ = false; // the empty clause was derived or added
  bool solved_ = false;       // `solve` has run: no more original clauses
  bool model_valid_ = false;  // the most recent solve returned Sat
  bool search_started_ = false; // RUP admission cannot renumber a search proof
  // An input clause refuting alone; its target is built at first solve.
  SubRef pending_refutation_ = NO_REF;

  std::vector<SubLit> arena_;
  std::vector<std::vector<SubWatch>> watches_; // by literal
  // Fixed-mode-only flip index; Kitten never reads it.
  std::vector<std::vector<SubRef>> occurrences_;
  // Clause-import stack mirroring Kitten's `eclause`.
  std::vector<SubLit> imported_clause_;
  // Second-core membership scratch (Kitten marks clause headers).
  std::vector<uint8_t> core_keep_;
  std::vector<uint32_t> core_touched_;
  // Duplicate/tautology marks, one per variable, as in Kissat's Kitten.
  std::vector<int8_t> clause_marks_;       // 0 / positive / negative
  std::vector<SubVar> clause_touched_;     // marks to restore after one clause
  std::vector<int8_t> values_;                 // by literal: 1/-1/0
  std::vector<int8_t> model_;                  // by variable: +1/-1
  std::vector<uint8_t> phase_;                 // by internal variable: sign
  std::vector<uint32_t> import_; // external variable -> internal + 1
  std::vector<SubVar> export_;   // internal variable -> external variable
  std::vector<SubRef> units_;    // original and learned unit clauses

  // Kitten's newest-first VMTF queue, in import order.
  std::vector<SubVar> queue_prev_, queue_next_;
  std::vector<uint64_t> queue_stamp_;
  SubVar queue_first_ = UINT32_MAX;
  SubVar queue_last_ = UINT32_MAX;
  SubVar queue_search_ = UINT32_MAX;
  uint64_t queue_next_stamp_ = 0;

  std::vector<SubLit> trail_;
  std::vector<uint32_t> frames_;   // trail index where each level starts
  std::vector<uint32_t> level_;    // by variable
  std::vector<uint32_t> trail_pos_; // by variable
  std::vector<SubRef> reason_;     // by variable (NO_REF = decision)
  uint32_t propagate_head_ = 0;

  // Persistent conflict scratch so the loop never allocates.
  SubRef conflict_ref_ = NO_REF;
  std::vector<uint8_t> seen_; // by variable
  std::vector<SubVar> analyzed_;
  std::vector<SubLit> learned_;
  std::vector<uint8_t> cone_seen_; // by variable
  std::vector<SubVar> cone_touched_;
  std::vector<SubVar> cone_stack_;
  std::vector<std::pair<uint32_t, uint32_t>> chain_entries_; // (trail pos, id)
  std::vector<uint32_t> analyze_ante_; // Kitten's `resolved`, in its own order
  size_t next_root_ = 0;               // `inconsistent`'s breadth cursor
  // The clause Kitten leaves in `inconsistent`/`failing`, rooting the core; a
  // single-antecedent failure reuses that antecedent instead of allocating the
  // target.
  uint32_t core_root_ = 0;

  std::vector<SubStep> derived_; // every lemma learned, in derivation order
  SubStep target_;               // the last refutation's target clause
  bool have_derivation_ = false;
  bool target_is_derived_ = false;

  uint32_t next_id_ = 1;
  uint32_t original_clauses_ = 0;
  SubVar decision_cursor_ = 0; // fixed decision order (design §3: VSIDS-free)
  SubSearchMode search_mode_ = SubSearchMode::Fixed;
  bool compact_identity_ = false;
  uint64_t kitten_generator_ = 0;
  uint64_t kitten_initialized_ = 0;
  uint64_t ticks_ = 0;
  uint64_t kitten_ticks_ = 0;
  uint64_t budget_ = 0;
  SubTickMode tick_mode_ = SubTickMode::Entries;
  uint64_t conflicts_ = 0;
};

} // namespace tactix

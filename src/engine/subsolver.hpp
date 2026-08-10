// The embedded sub-solver and its refutation derivation: a minimal CDCL that
// refutes a question posed on a small extracted clause set
// (docs/design/v5-sweep-design.md §3). An engine, not a rule — no guard, no
// instances, no apply — hence outside `src/rules/`, and shared: Sweep refutes
// candidate equivalences on an environment, BVE refutes the reduced occurrence
// lists of a pivot to extract a definition (kissat's definition.c).
#pragma once

#include <cstdint>
#include <vector>

namespace tactix {

// Three properties this engine must keep:
//  - it owns its arena, watches, trail and values and never touches `Kernel`,
//    so it cannot perturb the main search trajectory;
//  - it is budgeted in its own tick currency and returns `Budget` rather than
//    guessing, so the effort scheduler doses it like any rule (§5.2);
//  - it is deterministic: no wall clock or ambient randomness. The shared
//    fallback has fixed order; Sweep selects Kitten's deterministic
//    per-environment generator, imported-variable VMTF queue and saved phases.

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

enum class SubTickMode {
  Entries, // legacy shared-engine work unit used by BVE definition extraction
  Kitten,  // Kissat cache-line currency used by Sweep scheduling
};

enum class SubSearchMode {
  Fixed,  // deterministic shared-engine fallback used by BVE definitions
  Kitten, // Kissat's imported-variable VMTF queue and randomized phases
};

// One derived clause: `lits` follows from `chain` in the LRAT sense — assume
// every literal of `lits` false, then propagate `chain` in order, each entry
// unit or (the last) conflicting. An empty `lits` is the empty clause.
struct SubStep {
  uint32_t id = 0;
  std::vector<SubLit> lits;
  std::vector<uint32_t> chain;
};

// A complete refutation, payable as a proof obligation: a swept equivalence is
// entailed but not RUP in general (design §4), so the chain must come from this
// solver's own resolution, not a RUP check over the main formula. `steps` is in
// derivation order, pruned to what the target depends on, and its last step is
// the target clause; `core` is the ascending ids of the original clauses used.
// The caller maps these ids to main-formula ones; nothing here knows about the
// main formula.
struct SubDerivation {
  std::vector<SubStep> steps;
  std::vector<uint32_t> core;
};

class SubSolver {
public:
  // Reset to an empty formula over `nvars` variables, reusing the existing
  // allocations so a round of many small environments pays no allocation each.
  void reset(uint32_t nvars,
             SubSearchMode mode = SubSearchMode::Fixed);

  // A Kissat Sweep invocation constructs one embedded Kitten and clears it
  // between environments. Clear advances Kitten's deterministic generator;
  // starting a new invocation resets that sequence.
  void start_kitten_session();

  // Kissat randomizes every active-capacity phase byte immediately before
  // every Sweep solve. Kept separate from solve because BVE shares this
  // engine but does not use the Sweep/Kitten search contract.
  void randomize_phases();

  // Add an original clause and return its id. Ids are handed out from 1 in add
  // order, matching the clause numbering of a DIMACS file written in the same
  // order, which is what makes the derivation directly replayable. Every call
  // consumes an id, tautologies included, so the id space stays aligned with
  // the caller's. Must not be called after `solve`.
  uint32_t add_clause(const std::vector<SubLit> &lits);

  // Solve under `assumptions`; `ticks_budget == 0` means unlimited. Same
  // formula and assumptions give the same result and the same tick count.
  SubResult solve(const std::vector<SubLit> &assumptions,
                  uint64_t ticks_budget,
                  SubTickMode tick_mode = SubTickMode::Entries);

  // The refutation behind the last `Unsat`. False when there is none: no
  // refutation yet, or the assumptions were directly contradictory, so nothing
  // was derived and the target clause would be a tautology — which only
  // `Proof::add_rup`'s assert catches, DRAT accepting it for free.
  bool derivation(SubDerivation &out) const;

  // Model, by variable: +1 true, -1 false (valid only after `Sat`).
  const std::vector<int8_t> &model() const { return model_; }
  // Flip `lit` in the current SAT model while holding every other variable
  // fixed. False means some clause would become false (or the variable is
  // root-fixed). Used only as a cheap model filter; it proves no clause.
  bool flip_model_literal(SubLit lit);
  // Current level-zero value of an external literal, or zero if it is not
  // imported, unassigned, or assigned above root (Kitten's `kitten_fixed`).
  int8_t fixed_literal(SubLit lit) const;
  bool has_model() const { return model_valid_; }
  // Effort currency, charged per watch-list entry inspected — the same unit
  // `transitive_ticks` / `ternary_ticks` / `bce_ticks` use.
  uint64_t ticks() const { return ticks_; }
  uint64_t kitten_ticks() const { return kitten_ticks_; }
  uint32_t kitten_phase_capacity() const { return phase_capacity_vars_; }
  // Reported beside `ticks()` as Sweep's (yield, effort) pair.
  uint64_t conflicts() const { return conflicts_; }
  uint32_t nvars() const { return nvars_; }
  // Ids <= this are original clauses; above it are derived lemmas.
  uint32_t original_clauses() const { return original_clauses_; }
  // The formula itself is refuted, not merely under assumptions, so every later
  // `solve` returns `Unsat` whatever it assumes. A caller reading
  // per-assumption refutations must check this, or read two vacuous `Unsat`s
  // as an equivalence.
  bool inconsistent() const { return inconsistent_; }

private:
  // A clause is a run in `arena_` with a two-word header: `arena_[ref - 2]` is
  // its id and `arena_[ref - 1]` its size, so a `SubRef` (the index of the
  // first literal) both identifies the clause and names it for the proof.
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
  SubLit import_literal(SubLit);
  SubLit export_literal(SubLit) const;
  std::vector<SubLit> export_literals(const std::vector<SubLit> &) const;
  void queue_enqueue(SubVar);
  void queue_dequeue(SubVar);
  void queue_move_to_front(SubVar);
  void queue_update_search(SubVar);
  void queue_bump_analyzed();
  void clear_assignments();
  bool propagate_units();
  void attach(SubRef ref);
  SubRef add_derived(const std::vector<SubLit> &lits,
                     const std::vector<uint32_t> &chain);
  SubLit *clause_lits(SubRef ref) { return &arena_[ref]; }
  const SubLit *clause_lits(SubRef ref) const { return &arena_[ref]; }
  uint32_t clause_size(SubRef ref) const { return arena_[ref - 1]; }
  uint32_t clause_id(SubRef ref) const { return arena_[ref - 2]; }

  // --- proof plumbing ---
  // Transitively record the reason cone of `v` as (trail position, id) pairs;
  // sorting by trail position reproduces the checker's own propagation order.
  // Decision variables terminate the walk and, when `decisions` is non-null,
  // are reported through it — they are the assumptions the target negates.
  void record_cone(SubVar v, std::vector<SubLit> *decisions);
  void chain_clear();
  // sorted ids, with `closing` appended last when it is not NO_REF
  std::vector<uint32_t> chain_build(SubRef closing);
  void refute_empty(SubRef conflict); // the empty clause, from a level-0 conflict

  uint32_t nvars_ = 0;
  uint32_t imported_vars_ = 0;
  uint32_t phase_capacity_vars_ = 0;
  // `kitten_clear` retains the allocated internal size. Phase randomization
  // visits that high-water capacity, including bytes above the current
  // environment's imported-variable count, until the Sweep session ends.
  uint32_t kitten_capacity_vars_ = 0;
  bool inconsistent_ = false; // the empty clause was derived or added
  bool solved_ = false;       // `solve` has run: no more original clauses
  bool model_valid_ = false;  // the most recent solve returned Sat
  // An input clause that refuted the formula on its own. Its target is built at
  // the first solve, so lemma ids never land below the original-id watershed.
  SubRef pending_refutation_ = NO_REF;

  std::vector<SubLit> arena_;
  std::vector<std::vector<SubWatch>> watches_; // by literal
  std::vector<std::vector<SubRef>> occurrences_; // flip safety index
  std::vector<int8_t> values_;                 // by literal: 1/-1/0
  std::vector<int8_t> model_;                  // by variable: +1/-1
  std::vector<uint8_t> phase_;                 // by internal variable: sign
  std::vector<uint32_t> import_; // external variable -> internal + 1
  std::vector<SubVar> export_;   // internal variable -> external variable
  std::vector<SubRef> units_;    // original and learned unit clauses

  // Kitten's newest-first VMTF decision queue. The queue is populated in
  // first-clause-occurrence import order, not the caller's variable order.
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

  // Conflict handoff and analysis scratch, persistent so the conflict loop
  // never allocates (the kernel keeps its analyze stacks for the same reason).
  SubRef conflict_ref_ = NO_REF;
  std::vector<uint8_t> seen_; // by variable
  std::vector<SubVar> analyzed_;
  std::vector<SubLit> learned_;
  std::vector<uint8_t> cone_seen_; // by variable
  std::vector<SubVar> cone_touched_;
  std::vector<SubVar> cone_stack_;
  std::vector<std::pair<uint32_t, uint32_t>> chain_entries_; // (trail pos, id)

  std::vector<SubStep> derived_; // every lemma learned, in derivation order
  SubStep target_;               // the last refutation's target clause
  bool have_derivation_ = false;
  bool target_is_derived_ = false;

  uint32_t next_id_ = 1;
  uint32_t original_clauses_ = 0;
  SubVar decision_cursor_ = 0; // fixed decision order (design §3: VSIDS-free)
  SubSearchMode search_mode_ = SubSearchMode::Fixed;
  uint64_t kitten_generator_ = 0;
  uint64_t kitten_initialized_ = 0;
  uint64_t ticks_ = 0;
  uint64_t kitten_ticks_ = 0;
  uint64_t budget_ = 0;
  SubTickMode tick_mode_ = SubTickMode::Entries;
  uint64_t conflicts_ = 0;
};

} // namespace tactix

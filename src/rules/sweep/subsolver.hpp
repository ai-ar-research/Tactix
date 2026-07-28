// V5 Sweep · increment S-a: the embedded sub-solver.
//
// A minimal, self-contained CDCL used to REFUTE candidate equivalences on a
// small extracted environment (`docs/design/v5-sweep-design.md` §3, decision
// O11: write ~600 focused lines rather than transliterate kitten's 2877).
// It is deliberately NOT the kernel:
//
//  - it owns its arena, watches, trail and values, and never reads or writes
//    `Kernel` state, so a policy may run it at any point without disturbing the
//    search's bookkeeping (the L3 lesson: even a non-mutating pass that
//    reorders watch lists changes the trajectory);
//  - it is budgeted in its own tick currency and returns `Budget` rather than
//    guessing, so the effort scheduler can dose it like every other rule
//    (rule charges, policy budgets — §5.2 rev 17);
//  - it is deterministic: no randomness, no wall-clock, fixed decision order.
//
// This increment is the engine alone — propagation, 1-UIP learning, restarts,
// assumptions, budget. It records NO antecedents and emits NO proof, so nothing
// here may be used to justify a formula rewrite yet; S-b adds antecedent
// recording and core extraction, and only then can Sweep emit a chain (§4 —
// a swept equivalence is entailed but NOT RUP, so the chain must come from
// this solver's refutation, never from a RUP check over the main formula).
#pragma once

#include <cstdint>
#include <vector>

namespace tactix {

// Local literal encoding, identical in shape to the kernel's: variable v has
// literals 2v (positive) and 2v+1 (negative). Kept separate from `Lit` because
// these index the SUB-SOLVER's compact variable space, not the kernel's — the
// environment extraction (S-c) owns the mapping between them.
using SubLit = uint32_t;
using SubVar = uint32_t;

inline SubLit sub_lit(SubVar v, bool negated) { return 2 * v + (negated ? 1 : 0); }
inline SubVar sub_var_of(SubLit l) { return l >> 1; }
inline SubLit sub_negate(SubLit l) { return l ^ 1u; }

enum class SubResult {
  Sat,    // a total model was found (`values` holds it)
  Unsat,  // refuted under the assumptions
  Budget, // the tick budget ran out — NO conclusion may be drawn
};

class SubSolver {
public:
  // Reset to an empty formula over `nvars` variables. Reuses the existing
  // allocations, so a caller sweeping many small environments in one round does
  // not pay an allocation per environment.
  void reset(uint32_t nvars);

  // Add an irredundant clause. Duplicate literals are collapsed and tautologies
  // dropped; the empty clause and root units are absorbed directly. Must not be
  // called after `solve` (the formula is fixed for the lifetime of a solve).
  void add_clause(const std::vector<SubLit> &lits);

  // Solve under `assumptions`. `ticks_budget == 0` means unlimited.
  // Deterministic: same formula + same assumptions => same result and same
  // tick count, which is what makes it usable inside a reproducible schedule.
  SubResult solve(const std::vector<SubLit> &assumptions,
                  uint64_t ticks_budget);

  // Model, by variable: +1 true, -1 false (valid only after `Sat`).
  const std::vector<int8_t> &model() const { return model_; }
  // The rule's effort currency, charged per watch-list entry inspected — the
  // same unit `transitive_ticks` / `ternary_ticks` / `bce_ticks` use.
  uint64_t ticks() const { return ticks_; }
  uint64_t conflicts() const { return conflicts_; }
  uint32_t nvars() const { return nvars_; }

private:
  // A clause is a length-prefixed run in `arena_`; a Ref is the index of its
  // first literal, so `arena_[ref - 1]` is the size. Binaries are inlined into
  // the watch lists exactly as the kernel does, so they never reach the arena.
  using SubRef = uint32_t;
  static constexpr SubRef NO_REF = UINT32_MAX;

  struct SubWatch {
    SubLit blocking; // binary: the other literal; large: a blocking literal
    SubRef ref;      // NO_REF marks a binary watch
  };

  void assign(SubLit l, SubRef reason, SubLit binary_reason);
  bool propagate();            // false on conflict (conflict_* are then set)
  void backtrack(uint32_t level);
  bool analyze();              // false when the conflict is at level 0
  SubLit next_decision();
  void attach(SubRef ref, SubLit a, SubLit b);
  const SubLit *clause_lits(SubRef ref) const { return &arena_[ref]; }
  uint32_t clause_size(SubRef ref) const { return arena_[ref - 1]; }

  uint32_t nvars_ = 0;
  bool inconsistent_ = false; // the empty clause was derived or added

  std::vector<SubLit> arena_;
  std::vector<std::vector<SubWatch>> watches_; // by literal
  std::vector<int8_t> values_;                 // by literal: 1/-1/0
  std::vector<int8_t> model_;                  // by variable: +1/-1
  std::vector<int8_t> phase_;                  // by variable: saved phase

  std::vector<SubLit> trail_;
  std::vector<uint32_t> frames_; // trail height at the start of each level
  std::vector<uint32_t> level_;  // by variable
  std::vector<SubRef> reason_;   // by variable (NO_REF = decision or binary)
  std::vector<SubLit> binary_reason_; // by variable, when reason_ is NO_REF
  uint32_t propagate_head_ = 0;

  // conflict handoff and analysis scratch, persistent so the conflict loop
  // never allocates (the kernel keeps its analyze stacks for the same reason)
  SubRef conflict_ref_ = NO_REF;
  SubLit conflict_bin_[2] = {0, 0};
  bool conflict_is_binary_ = false;
  std::vector<uint8_t> seen_; // by variable
  std::vector<SubVar> analyzed_;
  std::vector<SubLit> learned_;

  SubVar decision_cursor_ = 0; // fixed decision order (§3: VSIDS-free)
  uint64_t ticks_ = 0;
  uint64_t budget_ = 0;
  uint64_t conflicts_ = 0;
};

} // namespace tactix

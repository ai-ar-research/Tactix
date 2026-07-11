// Kernel state (plan §5): owns representation only — no policy knowledge,
// no rule logic. One flat struct, index-based arrays, C-style discipline.
// Layout decisions transliterated from kissat/src/internal.h (MIT).
#pragma once

#include "kernel/clause.hpp"
#include "kernel/types.hpp"
#include "kernel/watch.hpp"

#include <cstdint>
#include <vector>

namespace tactix {

class Proof;

// per-variable assignment metadata (kissat/src/assign.h)
struct Assigned {
  uint32_t level;
  uint32_t trail; // position on the trail
  uint32_t reason; // Ref, or other literal if binary, or sentinels
  bool binary;
};

// VMTF queue links (kissat/src/queue.h)
struct Link {
  uint32_t prev, next, stamp;
};

struct Stats {
  uint64_t decisions = 0;
  uint64_t conflicts = 0;
  uint64_t propagations = 0;
  uint64_t learned = 0;
  uint64_t learned_units = 0;
  uint64_t minimized = 0; // literals removed by clause minimization
  uint64_t shrunken = 0;  // literals removed by all-UIP block shrinking
  uint64_t otfs_strengthened = 0; // reasons strengthened on the fly
  uint64_t otfs_subsumed = 0;     // conflicts subsumed by the resolvent
  uint64_t api_decisions = 0; // externally chosen branches (N1)
  uint64_t restarts = 0;
  uint64_t restarts_reused = 0; // restarts that kept a trail prefix
  uint64_t reductions = 0;
  uint64_t deleted = 0; // learned clauses deleted by Reduce
};

// H-state for the macro rules (proof-irrelevant, plan §4)
struct RestartState {
  // EMA-triggered (Glucose-style) restarts for the (focused) phase-1 mode;
  // reluctant doubling returns with stable mode / SwitchMode in phase 2.
  uint64_t floor_conflicts = 50; // earliest next restart
  double fast_glue = 0.0;        // ~ last 32 conflicts
  double slow_glue = 0.0;        // ~ last 16384 conflicts
  bool ema_initialized = false;
};
struct ReduceState {
  uint64_t next_conflicts = 2000;
};

// learned clause handed from Analyze to Backjump (τ changes are Backjump's)
struct PendingLearned {
  bool valid = false;
  std::vector<Lit> lits; // lits[0] = asserting literal
  Ref ref = INVALID_REF; // arena ref for size >= 3, INVALID otherwise
  uint32_t glue = 0;
  uint32_t jump_level = 0;
};

struct Kernel {
  uint32_t nvars = 0;
  bool inconsistent = false; // root-level conflict happened

  std::vector<int8_t> values;     // by lit: 1 true, -1 false, 0 unassigned
  std::vector<Assigned> assigned; // by var
  std::vector<int8_t> saved;      // by var: saved phase (+1/-1)

  std::vector<Lit> trail;
  uint32_t propagate_head = 0;
  std::vector<uint32_t> frames; // trail height at start of each level
  uint32_t level = 0;
  uint32_t unassigned = 0;

  Arena arena;
  WatchLists watches;
  // delayed watch relocations from an in-flight propagation scan
  // (lit, blocking, ref) triples; see watch.hpp for why they must wait
  std::vector<uint32_t> delayed;

  // VMTF (focused-mode decision heuristic)
  std::vector<Link> links; // by var
  uint32_t queue_first = UINT32_MAX, queue_last = UINT32_MAX;
  uint32_t queue_stamp = 0;
  uint32_t search_idx = UINT32_MAX, search_stamp = 0;

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
  std::vector<Lit> learned_scratch; // the clause under construction
  std::vector<Lit> otfs_scratch;    // OTFS resolvent assembly
  std::vector<Var> radix_scratch;   // radix-sort double buffer
  std::vector<uint8_t> marked_levels;
  std::vector<uint32_t> touched_levels;

  Proof *proof = nullptr;
  Stats stats;
  RestartState restart;
  ReduceState reduce;

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

// --- lifecycle -------------------------------------------------------------
void kernel_reserve_vars(Kernel &, uint32_t nvars);

// --- clauses ---------------------------------------------------------------
// size >= 3; watches lits[0] and lits[1]
Ref kernel_add_large_clause(Kernel &, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue);
void kernel_add_binary_clause(Kernel &, Lit a, Lit b);

// --- assignment / trail ----------------------------------------------------
// Header-inline (kissat fastassign.h pattern): propagation executes an
// assignment per propagated literal, and a cross-TU call there costs
// measurably at 10^6+ props/sec — the store sequence must inline into the
// watch-scan loop.
inline void kernel_assign(Kernel &k, Lit lit, uint32_t level,
                          uint32_t reason, bool binary) {
  assert(k.values[lit] == 0);
  const Var v = var_of(lit);
  k.values[lit] = 1;
  k.values[negate(lit)] = -1;
  k.assigned[v] = {level, static_cast<uint32_t>(k.trail.size()), reason,
                   binary};
  k.trail.push_back(lit);
  assert(k.unassigned > 0);
  k.unassigned--;
}
inline void kernel_assign_decision(Kernel &k, Lit lit) { // level++ inside
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}
inline void kernel_assign_unit(Kernel &k, Lit lit) { // root level
  kernel_assign(k, lit, 0, UNIT_REASON, false);
}
inline void kernel_assign_binary(Kernel &k, Lit lit, Lit other_false) {
  kernel_assign(k, lit, k.level, other_false, true);
}
inline void kernel_assign_ref(Kernel &k, Lit lit, Ref reason) {
  kernel_assign(k, lit, k.level, reason, false);
}
void kernel_backtrack(Kernel &, uint32_t target_level);

// --- VMTF ------------------------------------------------------------------
void kernel_queue_init(Kernel &);
void kernel_queue_bump(Kernel &, Var v); // move to front (most recent)
Var kernel_queue_next_unassigned(Kernel &); // last-enqueued unassigned var

// --- garbage collection ------------------------------------------------
// compacts the arena (dropping GARBAGE clauses), fixes trail reasons via
// forwarding pointers, and rebuilds large watches (binaries kept in place)
void kernel_collect_garbage(Kernel &);

// --- internal invariants (plan §8.2 oracle 4; debug builds) --------------
// aborts on: values/trail/unassigned incoherence, frame/level corruption,
// watch-list damage, or a missed conflict (fully propagated state with a
// falsified clause). No-op in NDEBUG builds.
void kernel_check_invariants(const Kernel &);

} // namespace tactix

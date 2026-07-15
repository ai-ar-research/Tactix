// Kernel state (plan §5): owns representation only — no policy knowledge,
// no rule logic. One flat struct, index-based arrays, C-style discipline.
// Layout decisions transliterated from kissat/src/internal.h (MIT).
#pragma once

#include "kernel/clause.hpp"
#include "kernel/types.hpp"
#include "kernel/watch.hpp"

#include <cstdint>
#include <unordered_map>
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
  uint64_t ticks = 0; // propagation memory-traffic proxy (see propagate.cpp)
  uint64_t switched = 0; // mode switches (S4)
  uint64_t target_saved = 0; // target-phase snapshots
  uint64_t best_saved = 0;   // best-phase snapshots
  uint64_t rephased = 0;     // Rephase applications (S3)
  uint64_t lucky_probes = 0; // Lucky probes run (S5)
  uint64_t promoted = 0;     // glue promotions on use (kissat deduce.c)
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
  uint64_t forward_subsumptions = 0; // SubsumeForward rounds (C1)
  uint64_t forward_checked = 0;  // SubsumeForward candidates checked (C1)
  uint64_t forward_subsumed = 0; // clauses deleted by SubsumeForward
  uint64_t strengthen_rounds = 0;  // Strengthen rounds (C2)
  uint64_t strengthen_checked = 0; // Strengthen candidate checks (C2)
  uint64_t strengthened = 0;       // clauses strengthened (C2 add+delete)
  uint64_t strengthen_subsumed = 0; // deletions by C2's fused round
  uint64_t flushed = 0; // root-satisfied deletions + root-falsified
                        // literal strips by C2's flush pre-pass
  uint64_t vivify_rounds = 0;  // Vivify rounds (C3)
  uint64_t vivify_checked = 0; // Vivify candidates probed (C3)
  uint64_t vivified = 0;       // clauses distilled by Vivify (add+delete)
  uint64_t vivify_deleted = 0; // root-satisfied deletions by Vivify
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

struct Kernel {
  uint32_t nvars = 0;
  bool inconsistent = false; // root-level conflict happened

  std::vector<int8_t> values;     // by lit: 1 true, -1 false, 0 unassigned
  std::vector<Assigned> assigned; // by var
  std::vector<int8_t> saved;      // by var: saved phase (+1/-1)

  std::vector<Lit> trail;
  uint32_t propagate_head = 0;
  // trail height at the start of each level; frames[0] = 0 exists from
  // construction — the `frames.size() == level + 1` invariant must hold
  // even for a kernel that never saw a variable (empty formula; corpus
  // regression 001-empty-formula-frames)
  std::vector<uint32_t> frames = {0};
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

  // EVSIDS score heap (stable-mode decision heuristic; kissat heap.h /
  // inlineheap.h): pos-indexed binary max-heap over vars. pos UINT32_MAX
  // means "not in heap" (lazily re-inserted on unassign while stable).
  struct {
    std::vector<uint32_t> stack; // heap array of vars, max at stack[0]
    std::vector<uint32_t> pos;   // by var: index into stack, or UINT32_MAX
    std::vector<double> score;   // by var
  } scores;
  double scinc = 1.0; // EVSIDS bump increment (grows by 1/(1-decay))

  // active instance-ranking mode for Decide (§5.1 H-state): false = VMTF
  // queue (focused), true = score heap (stable). Flipped only by the S4
  // SwitchMode rule; WHEN to flip is policy state (rev 17).
  bool stable = false;

  // target phases (kissat phases.c): assignment snapshot at the deepest
  // consistent trail seen while stable; preferred over saved phase there
  std::vector<int8_t> target; // by var: +1/-1, 0 = never saved
  uint32_t target_assigned = 0;
  // best phases (kissat phases.c, S3 Rephase): like target, but the
  // watermark only resets when Rephase consumes it (type Best)
  std::vector<int8_t> best; // by var: +1/-1, 0 = never saved
  uint32_t best_assigned = 0;

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

  // --- proof clause ids (native LRAT; CaDiCaL's id scheme) ------------
  // every clause — including input tautologies/duplicates that are never
  // stored — consumes one id in input/derivation order. Large clauses
  // carry theirs in the header; units live in unit_ids (by var; 0 =
  // none); binaries, which never materialize, in binary_ids keyed by the
  // canonical literal pair (first id wins on duplicates — binaries are
  // never deleted, so the map only grows; that permanence is load-bearing
  // for SubsumeForward's soundness — see subsume_forward.hpp).
  uint64_t next_clause_id = 0;
  std::vector<uint64_t> unit_ids; // by var
  std::unordered_map<uint64_t, uint64_t> binary_ids;
  // antecedent scratch (analyze, LRAT mode only): (trail position of the
  // literal the antecedent propagated, antecedent id). Emitted sorted by
  // trail position ascending with the conflict id last — that order is
  // checker-propagatable by construction (each hint's other literals are
  // clause negations, roots, or earlier-propagated literals)
  std::vector<std::pair<uint32_t, uint64_t>> lrat_entries;
  std::vector<uint64_t> lrat_chain; // built by kernel_lrat_chain below

  Proof *proof = nullptr;
  Stats stats;
  // NOTE (plan §5 rev 17): scheduling state (restart EMAs, reduce
  // intervals, ...) lives in the Policy object, not here; kernel H-state
  // is confined to instance-ranking heuristics (VMTF, phases).

  int8_t value(Lit l) const { return values[l]; }
  uint32_t var_level(Var v) const { return assigned[v].level; }
};

// --- lifecycle -------------------------------------------------------------
void kernel_reserve_vars(Kernel &, uint32_t nvars);

// --- clauses ---------------------------------------------------------------
// size >= 3; watches lits[0] and lits[1]. `id` is the proof clause id
// (0 in id-free unit tests; solver paths pass ingest input ids or
// ++next_clause_id for derived clauses)
Ref kernel_add_large_clause(Kernel &, const Lit *lits, uint32_t size,
                            bool redundant, uint32_t glue, uint64_t id = 0);
void kernel_add_binary_clause(Kernel &, Lit a, Lit b, uint64_t id = 0);

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
// update_phases=false is the probing variant (kissat backtrack.c
// kissat_backtrack_without_updating_phases): probe assignments must not
// pollute saved/target/best phases
void kernel_backtrack(Kernel &, uint32_t target_level,
                      bool update_phases = true);

// core propagation engine (kissat proplit.h; moved out of the M1 rule as
// a reviewed §5.3 kernel-primitive addition, 2026-07-14): propagates the
// trail to fixpoint or first conflict, recording the conflict in the
// handoff fields; returns true iff a conflict was found. Updates
// propagations/ticks, NOT conflicts — the M1 Propagate rule owns the
// ApplyResult/proof semantics and the conflict count; probing rules
// (S5 Lucky, phase-3 V-families) reuse the engine directly.
// `ignore`: optional clause ref excluded from propagation (CaDiCaL's
// ignore pointer — for rules probing a candidate's own literals; see
// propagate_literal in kernel.cpp). INVALID_REF = normal propagation.
bool kernel_propagate(Kernel &, Ref ignore = INVALID_REF);

// --- VMTF ------------------------------------------------------------------
void kernel_queue_init(Kernel &);
void kernel_queue_bump(Kernel &, Var v); // move to front (most recent)
Var kernel_queue_next_unassigned(Kernel &); // last-enqueued unassigned var
// reset the queue search cache to the queue tail (entering focused mode)
void kernel_reset_queue_search(Kernel &);

// --- EVSIDS score heap (stable mode) ----------------------------------------
void kernel_heap_push(Kernel &, Var v);    // insert (must not be contained)
void kernel_score_bump(Kernel &, Var v);   // score += scinc, sift, rescale
void kernel_scores_decay(Kernel &);        // scinc /= (1 - decay), rescale
void kernel_update_scores(Kernel &);       // push all missing vars (enter stable)
Var kernel_heap_next_unassigned(Kernel &); // max-score unassigned (lazy pop)
inline bool kernel_heap_contains(const Kernel &k, Var v) {
  return k.scores.pos[v] != UINT32_MAX;
}
// snapshot assigned values into target/best phases when the trail is the
// deepest consistent one seen past each watermark (kissat backtrack.c
// kissat_update_target_and_best_phases). Stable mode only; Rephase (S3)
// consumes best and resets the watermarks.
void kernel_update_target_phases(Kernel &);

// number of root-fixed assignments (the level-0 trail prefix)
inline uint32_t kernel_root_fixed(const Kernel &k) {
  return k.level == 0 ? static_cast<uint32_t>(k.trail.size()) : k.frames[1];
}

// assemble k.lrat_chain from k.lrat_entries: antecedent ids sorted by
// trail position ascending — the order that unit-propagates for the
// checker by construction — with `closing_id` (the conflicting/falsified
// clause, or the unit's reason) last. Shared by every chain-emitting
// rule so the ordering invariant is defined exactly once.
void kernel_lrat_chain(Kernel &, uint64_t closing_id);

// --- decision ranking dispatch (§5.1) ---------------------------------------
inline Var kernel_next_decision_var(Kernel &k) {
  return k.stable ? kernel_heap_next_unassigned(k)
                  : kernel_queue_next_unassigned(k);
}
// phase priority (kissat decide.c kissat_decide_phase): target (stable
// only) -> saved; both default positive
inline Lit kernel_decide_lit(const Kernel &k, Var v) {
  int8_t phase = k.stable ? k.target[v] : 0;
  if (phase == 0)
    phase = k.saved[v];
  return make_lit(v, phase < 0);
}

// --- garbage collection ------------------------------------------------
// compacts the arena (dropping GARBAGE clauses), fixes trail reasons via
// forwarding pointers, and rebuilds large watches (binaries kept in place)
void kernel_collect_garbage(Kernel &);

// irredundant clause count: live irredundant large clauses + all binaries
// (binaries have an irredundant lifecycle in this kernel — never reduced,
// never deleted; see subsume_forward.hpp). The kissat BINIRR_CLAUSES
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

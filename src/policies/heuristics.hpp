// Search guidance `H` (VMTF queue, EVSIDS heap, saved/target/best phases) and
// its operations. `H` is policy-owned and optional: `struct Kernel` only
// forward-declares it, kernel.hpp must not include this header, and operations
// reach through `k.h`.
#pragma once

#include "kernel/kernel.hpp"
#include "strategy/heuristic_execution.hpp"

#include <algorithm>

namespace tactix {

// Consume formula-rule advice without exposing policy state to rules.
// Retirement is separate: DefaultPolicy publishes it at the matching rule
// boundary (Sweep through its follow-up Substitute), stress policies after each
// application.
void heuristics_consume_rule_advice(Kernel &);
void heuristics_consume_retirements(Kernel &);
void heuristics_discard_rule_events(Kernel &);
// Remove root-fixed or retired identities (kissat ranks only actives).
void heuristics_deactivate_inactive_vars(Kernel &);

struct Heuristics {
  // `H`'s own variable count; heuristics_check asserts it.
  uint32_t nvars = 0;
  // Cumulative Kissat `variables_activated` rank: fresh Factor variables enter
  // with activation score 1-1/rank before demotion to zero, which fixes
  // equal-score heap topology.
  uint64_t activation_count = 0;
  std::vector<int8_t> saved; // by var: saved phase (+1/-1)
  // The root-fixed identity kept by initial sparse compaction; later
  // preprocessing units stay in Kernel::compact_import_order (Kissat's random
  // modulus) but are inactive in the stable heap.
  Var fixed_representative = INVALID_VAR;

  // VMTF (focused-mode decision heuristic; kissat queue.h)
  std::vector<Link> links; // by var
  uint32_t queue_first = UINT32_MAX, queue_last = UINT32_MAX;
  uint32_t queue_stamp = 0;
  uint32_t search_idx = UINT32_MAX, search_stamp = 0;

  // EVSIDS score heap (stable mode; kissat heap.h / inlineheap.h): pos-indexed
  // binary max-heap; pos UINT32_MAX means absent (re-inserted lazily on
  // unassign while stable).
  struct {
    std::vector<uint32_t> stack; // heap array of vars, max at stack[0]
    std::vector<uint32_t> pos;   // by var: index into stack, or UINT32_MAX
    std::vector<double> score;   // by var
  } scores;
  double scinc = 1.0; // EVSIDS bump increment (grows by 1/(1-decay))

  // Decide's mode (§5.1): VMTF queue or score heap; S4 SwitchMode flips it.
  bool stable = false;

  // Target phases (kissat phases.c): deepest consistent stable trail.
  std::vector<int8_t> target; // by var: +1/-1, 0 = never saved
  // Candidate that installed each target phase (zero: native search).
  std::vector<uint64_t> target_candidate;
  uint32_t target_assigned = 0;
  // Best phases (kissat phases.c, S3 Rephase): reset only by Rephase.
  std::vector<int8_t> best; // by var: +1/-1, 0 = never saved
  uint32_t best_assigned = 0;
};

enum class TargetGuidanceOverwrite : uint8_t { NativeCapture, Rephase, Walk };

// Clear candidate provenance when a native writer supersedes target.
void heuristics_supersede_target_guidance(Kernel &, Var,
                                          TargetGuidanceOverwrite);
void heuristics_supersede_all_target_guidance(Kernel &,
                                              TargetGuidanceOverwrite);
// kissat reset_phases: saved becomes target; the rephase is counted.
inline void heuristics_reset_targets_from_saved(Kernel &k,
                                                TargetGuidanceOverwrite source) {
  heuristics_supersede_all_target_guidance(k, source);
  std::copy(k.h->saved.begin(), k.h->saved.end(), k.h->target.begin());
  k.h->target_assigned = 0;
  k.stats.rephased++;
}

// Record a successful Decide and whether it consumed the target.
void heuristics_note_target_decision(Kernel &, Lit, bool consumed_target);

// Grow `H` state for vars [old, nvars) whenever the kernel grows.
void heuristics_reserve_vars(Kernel &, uint32_t old, uint32_t nvars);
void heuristics_compact_vars(Kernel &, const std::vector<Var> &old_to_new,
                             uint32_t new_nvars);

// Seed queue/scores from first activation and the compact-index permutation
// from first import filtered by activation, the two orders of Kissat's
// `tumble`. No-op without `H`.
void heuristics_seed_activation_order(Kernel &, const std::vector<Var> &order);

// Prepare the first-search seed as if a fresh solver parsed the live root
// residual; changes only the deferred H seed and must run before
// ScopedHeuristics attaches.
size_t heuristics_prepare_residual_activation_order(Kernel &);

// Replace the compact/import permutation with a residual parse's order.
void heuristics_seed_residual_import_order(Kernel &,
                                           const std::vector<Var> &order);

// Mirror Kissat's initial literal compaction in the compact-index permutation:
// keep the first root-fixed variable as the fixed representative, drop the
// rest; only the random modulus follows Kissat's `VARS`.
void heuristics_compact_random_order_fixed(Kernel &);

// The kernel does not own `H`; this RAII owner serves tests and simple
// embeddings. Sizing at attach makes a fresh `H` bit-identical to one held
// since parse.
struct ScopedHeuristics {
  Heuristics h;
  Kernel &kref;
  explicit ScopedHeuristics(Kernel &k) : kref(k) {
    execution::execute_action(k, execution::HeuristicAttachmentRequest{h});
  }
  ~ScopedHeuristics() {
    execution::execute_action(kref, execution::HeuristicDetachmentRequest{h});
  }
  ScopedHeuristics(const ScopedHeuristics &) = delete;
  ScopedHeuristics &operator=(const ScopedHeuristics &) = delete;
};

// --- VMTF ------------------------------------------------------------------
void heuristics_queue_bump(Kernel &, Var v); // move to front (most recent)
Var heuristics_queue_next_unassigned(Kernel &); // last-enqueued unassigned var
// reset the queue search cache to the queue tail (entering focused mode)
void heuristics_reset_queue_search(Kernel &);
// Demote extension variables [first, end) to the queue bottom (kissat factor.c
// adjust_scores_and_phases_of_fresh_variables); an empty interval still
// restamps and resets the cursor. O(#queue), once per application.
void heuristics_demote_fresh_vars(Kernel &, Var first, Var end);
// Demote marked variables to the queue end and floor their scores.
void heuristics_demote_marked_vars(Kernel &, const std::vector<uint8_t> &mark);

// Remove a retired variable from the rankings (kissat flags.c
// deactivate_variable); Decide builds its instances from `k.nvars`, so N1
// holds.
void heuristics_deactivate_var(Kernel &, Var v);
// Kissat fixes new level-zero variables and removes them from rankings.
void heuristics_consume_root_assignments(Kernel &, size_t trail_begin);
// Kissat's flags[idx].active predicate: root-fixed and retired variables are
// disconnected but keep kernel ids. Inline because the analyze.cpp hot loop
// calls it.
inline bool heuristics_queue_contains(const Kernel &k, Var v) {
  if (k.h->queue_first == v || k.h->queue_last == v)
    return true;
  const Link &l = k.h->links[v];
  return l.prev != INVALID_VAR && l.prev < k.nvars &&
         k.h->links[l.prev].next == v;
}
inline bool heuristics_var_active(const Kernel &k, Var v) {
  return k.h && v < k.h->nvars && heuristics_queue_contains(k, v);
}

// --- EVSIDS score heap (stable mode) ----------------------------------------
void heuristics_heap_push(Kernel &, Var v);    // insert (must not be contained)
void heuristics_score_bump(Kernel &, Var v);   // score += scinc, sift, rescale
void heuristics_scores_decay(Kernel &);        // scinc /= (1 - decay), rescale
// Set a score and re-sift (kissat inlineheap.h kissat_update_heap).
void heuristics_score_set(Kernel &, Var v, double score);
// Normalize scores by the maximum (S7 Reorder adds weights after).
void heuristics_rescale_scores(Kernel &);
void heuristics_update_scores(Kernel &);       // push all missing vars (enter stable)
Var heuristics_heap_next_unassigned(Kernel &); // max-score unassigned (lazy pop)
Var heuristics_heap_next_active_unassigned(Kernel &);
inline bool heuristics_heap_contains(const Kernel &k, Var v) {
  return k.h->scores.pos[v] != UINT32_MAX;
}
// Snapshot values into target/best phases when the trail is the deepest
// consistent one past each watermark (kissat backtrack.c
// kissat_update_target_and_best_phases); stable mode only, S3 Rephase consumes
// best and resets the watermarks.
void heuristics_update_target_phases(Kernel &);
// Capture independent of the active ranking.
void heuristics_capture_target_phases(Kernel &);

// --- decision ranking dispatch (§5.1) ---------------------------------------
Var heuristics_queue_next_active_unassigned(Kernel &);

inline Var heuristics_next_decision_var(Kernel &k) {
  return k.h->stable ? heuristics_heap_next_unassigned(k)
                     : heuristics_queue_next_unassigned(k);
}

// Guards prove an active variable remains, as Kissat assumes.
inline Var heuristics_next_active_decision_var(Kernel &k) {
  return k.h->stable ? heuristics_heap_next_active_unassigned(k)
                     : heuristics_queue_next_active_unassigned(k);
}
// Phase priority (kissat decide.c): target (stable), saved, initial.
inline Lit heuristics_decide_lit(const Kernel &k, Var v) {
  int8_t phase = k.h->stable ? k.h->target[v] : 0;
  if (phase == 0)
    phase = k.h->saved[v];
  return make_lit(v, phase < 0);
}

// Policy-ranked candidates outside the Decide proof rule.
uint32_t heuristics_decide_candidates(const Kernel &, Lit *out, uint32_t cap);

// Restart trail reuse (kissat restart.c): keep the outranking prefix.
uint32_t heuristics_restart_reuse_level_for(const Kernel &, Var next, uint32_t floor = 0);
uint32_t heuristics_restart_reuse_level(Kernel &);

#ifndef NDEBUG
// debug invariant: `H` exists and its per-var state is sized to Kernel::nvars.
void heuristics_check(const Kernel &k);
#endif

} // namespace tactix

// Search-guidance state `H` — VMTF decision queue, EVSIDS score heap, saved /
// target / best phases — and its operations. `H` is policy-owned and optional:
// `struct Kernel` only forward-declares it, the include direction is one-way
// (kernel.hpp must not include this), and operations reach through `k.h`.
#pragma once

#include "kernel/kernel.hpp"

namespace tactix {

// Consume formula-rule advice without giving rules access to policy state.
// Retirement is separate because DefaultPolicy removes variables at phase
// exit, while stress policies react after each application.
void heuristics_consume_rule_advice(Kernel &);
void heuristics_consume_retirements(Kernel &);
void heuristics_discard_rule_events(Kernel &);

struct Heuristics {
  // `H` tracks its own variable count; heuristics_check asserts it equals
  // Kernel::nvars and every per-var vector is sized to it, so a missed growth
  // trips an assert rather than reading out of bounds later.
  uint32_t nvars = 0;

  std::vector<int8_t> saved; // by var: saved phase (+1/-1)

  // VMTF (focused-mode decision heuristic; kissat queue.h)
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

  // Active instance-ranking mode for Decide (§5.1): false = VMTF queue
  // (focused), true = score heap (stable). Flipped only by S4 SwitchMode; when
  // to flip is policy state.
  bool stable = false;

  // target phases (kissat phases.c): assignment snapshot at the deepest
  // consistent trail seen while stable; preferred over saved phase there.
  std::vector<int8_t> target; // by var: +1/-1, 0 = never saved
  uint32_t target_assigned = 0;
  // best phases (kissat phases.c, S3 Rephase): like target, but the watermark
  // only resets when Rephase consumes it (type Best).
  std::vector<int8_t> best; // by var: +1/-1, 0 = never saved
  uint32_t best_assigned = 0;
  // cadical phases.prev (S6 Walk, diagnostic arm TACTIX_WALK_PREV): the
  // previous walk's assignment, so local search resumes from it. Empty = none.
  std::vector<int8_t> walk_prev;
};

// Grow the `H` per-variable state for vars [old, nvars) and set h->nvars.
// Called whenever the kernel grows nvars.
void heuristics_reserve_vars(Kernel &, uint32_t old, uint32_t nvars);

// Seed the initial decision order from `order` (variables in first-activation
// order), kissat's `tumble`. No-op without `H`. See the definition for why this
// is an ordering port and not the renumbering kissat performs.
void heuristics_seed_activation_order(Kernel &, const std::vector<Var> &order);

// The kernel does not own `H`: whoever outlives a solve() owns the storage and
// attaches it. This RAII helper is that owner for tests and simple embeddings.
// Sizing at attach is what makes a fresh `H` on an already-parsed kernel
// bit-identical to one held since parse.
struct ScopedHeuristics {
  Heuristics h;
  Kernel &kref;
  explicit ScopedHeuristics(Kernel &k) : kref(k) {
    k.h = &h;
    if (k.nvars)
      heuristics_reserve_vars(k, 0, k.nvars);
  }
  ~ScopedHeuristics() { kref.h = nullptr; }
  ScopedHeuristics(const ScopedHeuristics &) = delete;
  ScopedHeuristics &operator=(const ScopedHeuristics &) = delete;
};

// --- VMTF ------------------------------------------------------------------
void heuristics_queue_bump(Kernel &, Var v); // move to front (most recent)
Var heuristics_queue_next_unassigned(Kernel &); // last-enqueued unassigned var
// reset the queue search cache to the queue tail (entering focused mode)
void heuristics_reset_queue_search(Kernel &);
// Demote the extension variables [first, nvars) to the bottom of the decision
// queue (kissat factor.c adjust_scores_and_phases_of_fresh_varaibles).
// O(#queue) per call, so call it once per variable-adding rule application.
void heuristics_demote_fresh_vars(Kernel &, Var first);
// The same for a scattered set: move every marked variable to the
// least-preferred end of the VMTF queue, preserving relative order, and floor
// its EVSIDS score. Ranking advice only, every variable stays decidable (N1).
void heuristics_demote_marked_vars(Kernel &, const std::vector<uint8_t> &mark);

// Remove a retired variable from the ranking structures outright (kissat
// flags.c deactivate_variable), at no per-decision cost since they then hold
// only active variables. N1 is untouched: `decide_candidates` builds the
// instance surface from `k.nvars`, never from `H`.
void heuristics_deactivate_var(Kernel &, Var v);

// --- EVSIDS score heap (stable mode) ----------------------------------------
void heuristics_heap_push(Kernel &, Var v);    // insert (must not be contained)
void heuristics_score_bump(Kernel &, Var v);   // score += scinc, sift, rescale
void heuristics_scores_decay(Kernel &);        // scinc /= (1 - decay), rescale
// Set an arbitrary score and re-sift (kissat inlineheap.h kissat_update_heap).
// Unlike score_bump the new value may be lower, so this sifts both directions;
// a var not in the heap only gets its score written.
void heuristics_score_set(Kernel &, Var v, double score);
// Normalize every score (and scinc) by the current maximum, relative order
// untouched. Public because S7 Reorder adds structural weights and needs the
// scores back in [0,1] first, or they vanish into a long-running EVSIDS scale.
void heuristics_rescale_scores(Kernel &);
void heuristics_update_scores(Kernel &);       // push all missing vars (enter stable)
Var heuristics_heap_next_unassigned(Kernel &); // max-score unassigned (lazy pop)
inline bool heuristics_heap_contains(const Kernel &k, Var v) {
  return k.h->scores.pos[v] != UINT32_MAX;
}
// Snapshot assigned values into target/best phases when the trail is the
// deepest consistent one seen past each watermark (kissat backtrack.c
// kissat_update_target_and_best_phases). Stable mode only; S3 Rephase consumes
// best and resets the watermarks.
void heuristics_update_target_phases(Kernel &);

// --- decision ranking dispatch (§5.1) ---------------------------------------
// Diagnostic only (`TACTIX_NO_EXT_DECISIONS`): skip Factor's extension
// variables when branching, falling back only when nothing else is unassigned.
// Never shippable — it narrows the policy's action space, which N1 forbids.
Var heuristics_next_decision_var_no_ext(Kernel &k);

inline Var heuristics_next_decision_var(Kernel &k) {
  if (k.no_ext_decisions && k.first_extension_var != UINT32_MAX)
    return heuristics_next_decision_var_no_ext(k);
  return k.h->stable ? heuristics_heap_next_unassigned(k)
                     : heuristics_queue_next_unassigned(k);
}
// phase priority (kissat decide.c kissat_decide_phase): target (stable
// only) -> saved; both default positive
inline Lit heuristics_decide_lit(const Kernel &k, Var v) {
  int8_t phase = k.h->stable ? k.h->target[v] : 0;
  if (phase == 0)
    phase = k.h->saved[v];
  return make_lit(v, phase < 0);
}

#ifndef NDEBUG
// debug invariant: `H` exists and its per-var state is sized to Kernel::nvars.
void heuristics_check(const Kernel &k);
#endif

} // namespace tactix

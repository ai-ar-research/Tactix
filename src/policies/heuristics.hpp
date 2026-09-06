// Search-guidance state `H` — VMTF decision queue, EVSIDS score heap, saved /
// target / best phases — and its operations. `H` is policy-owned and optional:
// `struct Kernel` only forward-declares it, the include direction is one-way
// (kernel.hpp must not include this), and operations reach through `k.h`.
#pragma once

#include "kernel/kernel.hpp"

namespace tactix {

// Consume formula-rule advice without giving rules access to policy state.
// Retirement is separate because DefaultPolicy publishes it at the matching
// rule boundary (with Sweep held through its follow-up Substitute), while
// stress policies react after each application.
void heuristics_consume_rule_advice(Kernel &);
void heuristics_consume_retirements(Kernel &);
void heuristics_discard_rule_events(Kernel &);
// Dequeue and heap-remove every identity that is already root-fixed or
// retired (kissat holds only active variables in its ranking structures).
void heuristics_deactivate_inactive_vars(Kernel &);

struct Heuristics {
  // `H` tracks its own variable count; heuristics_check asserts it equals
  // Kernel::nvars and every per-var vector is sized to it, so a missed growth
  // trips an assert rather than reading out of bounds later.
  uint32_t nvars = 0;
  // Cumulative Kissat `variables_activated` rank.  Fresh Factor variables are
  // first inserted with the ordinary activation score 1-1/rank and only then
  // demoted to zero; skipping that transient score changes equal-score heap
  // topology even though the final score vector is identical.
  uint64_t activation_count = 0;
  std::vector<int8_t> saved; // by var: saved phase (+1/-1)
  // The one root-fixed identity retained by initial sparse compaction. Later
  // preprocessing units remain in Kernel::compact_import_order (Kissat's
  // random modulus) but are inactive in its stable heap.
  Var fixed_representative = INVALID_VAR;

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
void heuristics_compact_vars(Kernel &, const std::vector<Var> &old_to_new,
                             uint32_t new_nvars);

// Seed queue/scores from first activation and the compact-index permutation
// from first import filtered by that activated population, the two orders
// produced by Kissat's `tumble`. No-op without `H`.
void heuristics_seed_activation_order(Kernel &, const std::vector<Var> &order);

// Mirror Kissat's initial literal compaction in the compact-index permutation:
// retain the first root-fixed variable as the shared fixed representative and
// remove the rest.  Kernel identities and the public Decide surface stay
// unchanged; only the policy-owned random modulus follows Kissat's `VARS`.
void heuristics_compact_random_order_fixed(Kernel &);

// The kernel does not own `H`: whoever outlives a solve() owns the storage and
// attaches it. This RAII helper is that owner for tests and simple embeddings.
// Sizing at attach is what makes a fresh `H` on an already-parsed kernel
// bit-identical to one held since parse.
struct ScopedHeuristics {
  Heuristics h;
  Kernel &kref;
  explicit ScopedHeuristics(Kernel &k) : kref(k) {
    k.h = &h;
    k.saved_phases = &h.saved;
    if (k.nvars)
      heuristics_reserve_vars(k, 0, k.nvars);
    if (!k.input_activation_order.empty()) {
      heuristics_seed_activation_order(k, k.input_activation_order);
      k.input_activation_order.clear();
      k.input_activation_order.shrink_to_fit();
    }
    // Identities already inactive at attach (input units fixed during load,
    // or retired variables on a re-attached kernel) leave the ranking
    // structures immediately, as kissat deactivated them when they became
    // inactive.  See heuristics_deactivate_inactive_vars for the stamp-slot
    // contract.
    heuristics_deactivate_inactive_vars(k);
  }
  ~ScopedHeuristics() {
    kref.saved_phases = nullptr;
    kref.h = nullptr;
  }
  ScopedHeuristics(const ScopedHeuristics &) = delete;
  ScopedHeuristics &operator=(const ScopedHeuristics &) = delete;
};

// --- VMTF ------------------------------------------------------------------
void heuristics_queue_bump(Kernel &, Var v); // move to front (most recent)
Var heuristics_queue_next_unassigned(Kernel &); // last-enqueued unassigned var
// reset the queue search cache to the queue tail (entering focused mode)
void heuristics_reset_queue_search(Kernel &);
// Demote the extension variables [first, end) to the bottom of the decision
// queue (kissat factor.c adjust_scores_and_phases_of_fresh_variables).
// O(#queue) per call, so call it once per variable-adding rule application.
void heuristics_demote_fresh_vars(Kernel &, Var first, Var end);
// The same for a scattered set: move every marked variable to the
// least-preferred end of the VMTF queue, preserving relative order, and floor
// its EVSIDS score. Ranking advice only, every variable stays decidable (N1).
void heuristics_demote_marked_vars(Kernel &, const std::vector<uint8_t> &mark);

// Remove a retired variable from the ranking structures outright (kissat
// flags.c deactivate_variable), at no per-decision cost since they then hold
// only active variables. N1 is untouched: the Decide rule builds its instance
// surface from `k.nvars`, never from `H`.
void heuristics_deactivate_var(Kernel &, Var v);
// Consume the DefaultPolicy consequence of a Propagate application: Kissat
// marks every newly assigned true-level-zero variable fixed and removes it
// from both decision rankings. The proof rule itself remains policy-agnostic.
void heuristics_consume_root_assignments(Kernel &, size_t trail_begin);
// Kissat's flags[idx].active predicate for decision-guidance operations.
// Root-fixed and retired variables are disconnected but retain kernel IDs.
//
// Defined inline: this and its `queue_contains` helper are cheap pointer/link
// reads called in the conflict-analysis hot loop (analyze.cpp) and other TUs.
// Out of line they cost a cross-TU call frame per invocation -- the pervasive
// header-vs-.cpp overhead report 3.50 named (heuristics_var_active alone burned
// 18.7e9 instructions as an out-of-line symbol). Both Kernel (via the include
// above) and Heuristics are fully visible here, so the definition folds into
// each caller as Kissat's header-resident predicates do.
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
Var heuristics_heap_next_active_unassigned(Kernel &);
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

Var heuristics_queue_next_active_unassigned(Kernel &);

inline Var heuristics_next_decision_var(Kernel &k) {
  if (k.no_ext_decisions && k.first_extension_var != UINT32_MAX)
    return heuristics_next_decision_var_no_ext(k);
  return k.h->stable ? heuristics_heap_next_unassigned(k)
                     : heuristics_queue_next_unassigned(k);
}

// DefaultPolicy's solve/warm-up guards prove that at least one active variable
// remains. Kissat relies on the same invariant and therefore does not carry the
// policy-neutral N1 fallback checks through every ranked decision scan.
inline Var heuristics_next_active_decision_var(Kernel &k) {
  if (k.no_ext_decisions && k.first_extension_var != UINT32_MAX)
    return heuristics_next_decision_var_no_ext(k);
  return k.h->stable ? heuristics_heap_next_active_unassigned(k)
                     : heuristics_queue_next_active_unassigned(k);
}
// Phase priority (kissat decide.c kissat_decide_phase): target (stable only),
// then saved, then the positive initial phase.  A zero saved entry means the
// variable has not yet acquired a saved phase.
inline Lit heuristics_decide_lit(const Kernel &k, Var v) {
  int8_t phase = k.h->stable ? k.h->target[v] : 0;
  if (phase == 0)
    phase = k.h->saved[v];
  return make_lit(v, phase < 0);
}

// Policy-ranked candidate variables for clients that want the active search
// order without making that order part of the Decide proof rule.
uint32_t heuristics_decide_candidates(const Kernel &, Lit *out, uint32_t cap);

// Restart trail-reuse ranking (kissat restart.c): keep the decision prefix
// that outranks the next policy-selected variable.
uint32_t heuristics_restart_reuse_level_for(Kernel &, Var next);
uint32_t heuristics_restart_reuse_level(Kernel &);

#ifndef NDEBUG
// debug invariant: `H` exists and its per-var state is sized to Kernel::nvars.
void heuristics_check(const Kernel &k);
#endif

} // namespace tactix

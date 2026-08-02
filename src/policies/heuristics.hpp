// Search-guidance state `H` (VMTF decision queue, EVSIDS score heap, saved/
// target/best phases) + its operations. Track H · H4: `H` is policy-owned,
// OPTIONAL search guidance (plan rev 60), so `struct Kernel` holds only an opaque
// `std::unique_ptr<Heuristics>` (kernel.hpp forward-declares this struct and never
// depends on its layout). This header includes kernel.hpp for `Kernel`/`Var`/
// `Lit`/`Link`; kernel.hpp must NOT include it (that is an include cycle). The
// functions take `Kernel&` and reach through `k.h` — the plan's `Kernel::h` idiom,
// not `Heuristics&`-threading. At H4 `k.h` is always allocated (Kernel-owned), so
// behaviour is identical to the pre-H4 inline fields; H5 makes it Policy-owned +
// nullable and adds the null-`H` code path.
#pragma once

#include "kernel/kernel.hpp"

namespace tactix {

struct Heuristics {
  // `H` tracks its own variable count; heuristics_check asserts it equals
  // Kernel::nvars and every per-var vector is sized to it, so a missed growth
  // (E2 Factor allocates extension vars mid-solve, ON by default) trips an
  // assert/ASan report instead of a silent OOB on the next phase/score read.
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

  // active instance-ranking mode for Decide (§5.1): false = VMTF queue
  // (focused), true = score heap (stable). Flipped only by S4 SwitchMode; WHEN
  // to flip is policy state (rev 17).
  bool stable = false;

  // target phases (kissat phases.c): assignment snapshot at the deepest
  // consistent trail seen while stable; preferred over saved phase there.
  std::vector<int8_t> target; // by var: +1/-1, 0 = never saved
  uint32_t target_assigned = 0;
  // best phases (kissat phases.c, S3 Rephase): like target, but the watermark
  // only resets when Rephase consumes it (type Best).
  std::vector<int8_t> best; // by var: +1/-1, 0 = never saved
  uint32_t best_assigned = 0;
  // cadical phases.prev (S6 Walk, DIAGNOSTIC arm TACTIX_WALK_PREV): the
  // previous walk's assignment, so local search resumes where it left off
  // instead of restarting from the decision phases. Empty = no walk yet.
  std::vector<int8_t> walk_prev;
};

// grow the `H` per-variable state for vars [old, nvars) and set h->nvars. Split
// out of kernel_reserve_vars (Track H); called whenever the kernel grows nvars.
void heuristics_reserve_vars(Kernel &, uint32_t old, uint32_t nvars);

// The kernel does not own `H` (Track H · H7) — whoever outlives a single solve()
// owns the storage and attaches it. This RAII helper is the guided-default owner
// for tests and simple embeddings: it holds a `Heuristics` for its scope, attaches
// it to `k`, sizes it to the kernel's CURRENT var count, and detaches on
// destruction (so `k.h` never dangles). Sizing here is what makes attaching a
// fresh `H` to an already-parsed kernel bit-identical to having had it since parse.
// The session (tactix_solver) and main hold their own `Heuristics` directly.
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
// Demote the extension variables [first, nvars) to the BOTTOM of the decision
// queue (kissat factor.c adjust_scores_and_phases_of_fresh_varaibles). Call
// ONCE per variable-adding rule application — it is O(#queue) per call.
void heuristics_demote_fresh_vars(Kernel &, Var first);
// Same idea for a SCATTERED set (V3's substituted variables, which are not a
// contiguous suffix): move every marked variable to the least-preferred end of
// the VMTF queue, preserving relative order, and floor its EVSIDS score. Pure
// ranking advice — every variable stays decidable (N1). O(#queue) per CALL, so
// call it once per rule application, never per variable.
void heuristics_demote_marked_vars(Kernel &, const std::vector<uint8_t> &mark);

// R-d (design v2 §3, kissat `flags.c` deactivate_variable): remove a RETIRED
// variable from the ranking structures outright — unlink from the VMTF queue
// and drop it from the score heap. After this DefaultPolicy can never offer it,
// with NO per-decision cost: the structures hold only active variables, so
// there is no skip, no scan and no fallback (all three of which were measured
// dead — v1's R-c). N1 is untouched: `decide_candidates` builds the instance
// surface from `k.nvars`, never from `H`, so the API still offers the variable.
void heuristics_deactivate_var(Kernel &, Var v);

// --- EVSIDS score heap (stable mode) ----------------------------------------
void heuristics_heap_push(Kernel &, Var v);    // insert (must not be contained)
void heuristics_score_bump(Kernel &, Var v);   // score += scinc, sift, rescale
void heuristics_scores_decay(Kernel &);        // scinc /= (1 - decay), rescale
// set an arbitrary score and re-sift (kissat inlineheap.h kissat_update_heap).
// Unlike score_bump the new value may be LOWER, so this sifts in both
// directions; a var not currently in the heap only gets its score written (it
// is re-pushed, and bubbled, when stable mode next unassigns it).
void heuristics_score_set(Kernel &, Var v, double score);
// normalize every score (and scinc) by the current maximum — relative order is
// untouched. Public because S7 Reorder ADDS structural weights to the scores
// and needs the existing ones brought back into [0,1] first, or the weights
// would be lost in the noise of a long-running EVSIDS scale (kissat reorder.c
// calls kissat_rescale_scores for exactly this reason).
void heuristics_rescale_scores(Kernel &);
void heuristics_update_scores(Kernel &);       // push all missing vars (enter stable)
Var heuristics_heap_next_unassigned(Kernel &); // max-score unassigned (lazy pop)
inline bool heuristics_heap_contains(const Kernel &k, Var v) {
  return k.h->scores.pos[v] != UINT32_MAX;
}
// snapshot assigned values into target/best phases when the trail is the
// deepest consistent one seen past each watermark (kissat backtrack.c
// kissat_update_target_and_best_phases). Stable mode only; Rephase (S3)
// consumes best and resets the watermarks.
void heuristics_update_target_phases(Kernel &);

// --- decision ranking dispatch (§5.1) ---------------------------------------
// DIAGNOSTIC ONLY (task #11, `TACTIX_NO_EXT_DECISIONS`): skip E2 Factor's
// extension variables when choosing a branch, falling back to them only when
// nothing else is unassigned (so the search stays complete). This is NOT
// shippable — it narrows the policy's action space, which N1 forbids — it
// exists to answer one question: is the damage broad Factor admission does to
// our search actually *caused* by branching on extension variables? Measured
// 2026-07-31: 31 % of ContextModel's decisions land on 459 extension vars.
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

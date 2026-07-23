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

// --- EVSIDS score heap (stable mode) ----------------------------------------
void heuristics_heap_push(Kernel &, Var v);    // insert (must not be contained)
void heuristics_score_bump(Kernel &, Var v);   // score += scinc, sift, rescale
void heuristics_scores_decay(Kernel &);        // scinc /= (1 - decay), rescale
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
inline Var heuristics_next_decision_var(Kernel &k) {
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

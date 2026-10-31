#pragma once

// BVE candidate ordering profiles owned by BveInvocationSession (gap plan
// Step 5, C4): the policy chooses a profile, the session owns its state and
// the kernel's score notifications reach it for the session's lifetime.
#include "diagnostics/event_trace.hpp"
#include "kernel/indexed_max_heap.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/bve/bve.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace tactix {

// Kissat eliminate.c `variable_score`: preserve its named binary64
// intermediates (`prod`, `sum`, structural `score`, and `occlim2`).  Flattening
// the algebraically equivalent subtractions can move an equal-score candidate
// by one ULP and therefore changes mutable-heap topology.
inline double kissat_bve_variable_score(double relevance, uint64_t positive,
                                        uint64_t negative,
                                        uint64_t occurrence_limit) {
  positive = std::min(positive, occurrence_limit);
  negative = std::min(negative, occurrence_limit);
  const double product = positive * negative;
  const double sum = positive + negative;
  const double occurrence_limit_squared =
      occurrence_limit * static_cast<double>(occurrence_limit);
  const double structural_score = product - sum;
  return relevance + structural_score - occurrence_limit_squared;
}

namespace strategy {

// Kissat's elimination schedule is a mutable indexed max-heap.  It stores the
// negated elimination score, inserts candidates in compact-variable order,
// stops bubble-up at equality, and prefers the left child at equality while
// bubbling down.  Those topology rules are semantic scheduling state: after
// one candidate mutates several occurrence lists, a global re-sort can choose
// a different equal-score variable even when every scalar score is correct.
class BveCandidateHeap {
public:
  void initialize(Kernel &k, const uint64_t *insts, uint32_t n) {
    kernel_ = &k;
    assert(k.dense && !k.bve_occurrence_count_image);
    // Kissat eliminate.c::variable_score reads SIZE_WATCHES for each polarity:
    // one physical dense-stack length containing binary and large entries.
    // Tactix stores those populations separately, so materialize their exact
    // sum once and let the kernel mirror subsequent physical mutations.
    occurrence_counts_.assign(2 * static_cast<size_t>(k.nvars), 0);
    for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
      uint64_t count = k.dense->large_size(lit);
      for (const WatchEntry watch : k.watches.entries(lit))
        count += watch.binary();
      occurrence_counts_[lit] =
          static_cast<uint32_t>(std::min<uint64_t>(count, UINT32_MAX));
    }
    k.bve_occurrence_count_image = &occurrence_counts_;
    pop_ordinal_ = 0;
    pop_hash_ = 1469598103934665603ull;
    heap_.reset(k.nvars);
    payload_.assign(k.nvars, INSTANCE_CANONICAL);
    eligible_.assign(k.nvars, 0);
    std::vector<uint8_t> inserted(k.nvars, 0);
    for (uint32_t i = 0; i < n; i++) {
      Lit lit;
      if (!lit_of_instance(k, insts[i], &lit) || var_of(lit) >= k.nvars)
        continue;
      const Var v = var_of(lit);
      eligible_[v] = 1;
      payload_[v] = insts[i];
    }
    // `all_variables` in Kissat walks current compact internal indices.
    for (const Var v : k.compact_import_order)
      if (v < k.nvars && eligible_[v]) {
        insert_initial(v);
        inserted[v] = 1;
      }
    // Stable identities normally all occur in compact_import_order.  Preserve
    // a deterministic complete fallback for bare API kernels that have no H.
    for (Var v = 0; v < k.nvars; v++)
      if (eligible_[v] && !inserted[v])
        insert_initial(v);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (const char *selected = std::getenv("TACTIX_EVENT_TRACE_BVE_HEAP")) {
      const char *p = selected;
      while (*p) {
        char *end = nullptr;
        const long external = std::strtol(p, &end, 10);
        if (end == p)
          break;
        for (Var v = 0; v < k.nvars; v++)
          if (static_cast<long>(k.internal_to_external[v] + 1) == external) {
            trace_vars_.push_back(v);
            break;
          }
        p = *end ? end + 1 : end;
      }
    }
    if (std::getenv("SAT_EVENT_TRACE_BVE_SCHEDULE_OCCURRENCES")) {
      const char *selected = std::getenv("TACTIX_EVENT_TRACE_BVE_EXTERNAL");
      if (selected && *selected) {
        const long external = std::strtol(selected, nullptr, 10);
        for (Var v = 0; v < k.nvars; ++v) {
          if (static_cast<long>(k.internal_to_external[v] + 1) != external)
            continue;
          for (uint32_t side = 0; side != 2; ++side) {
            const Lit lit = make_lit(v, side != 0);
            uint32_t ordinal = side ? 0x40000000u : 0;
            for (const OccIndex::Entry entry : k.dense->ordered[lit]) {
              ++ordinal;
              if (entry.binary) {
                const Lit binary[2] = {lit, entry.payload};
                event_trace_bve_pair(
                    k, v, ordinal, binary, 2, INVALID_REF, 0, nullptr, 0,
                    INVALID_REF, 0,
                    side ? "schedule-negative-binary"
                         : "schedule-positive-binary");
              } else {
                const Clause *c = clause_at(k.arena, entry.payload);
                event_trace_bve_pair(
                    k, v, ordinal, c->lits, c->size, entry.payload, c->id(),
                    nullptr, 0, INVALID_REF, 0,
                    side ? "schedule-negative-large"
                         : "schedule-positive-large");
              }
            }
          }
          break;
        }
      }
    }
#endif
    trace_last_pos_.assign(trace_vars_.size(), UINT32_MAX);
    trace_last_score_.assign(trace_vars_.size(), 0.0);
    trace_selected("initial", INVALID_VAR, true);
    uint64_t insertion_hash = 1469598103934665603ull;
    for (const Var v : k.compact_import_order)
      if (v < k.nvars && contains(v)) {
        insertion_hash ^= k.internal_to_external[v] + 1ull;
        insertion_hash *= 1099511628211ull;
      }
    event_trace_bve_schedule(k, heap_.members().data(), heap_.scores().data(),
                             heap_.size(), k.bve_bound, insertion_hash);
  }

  bool empty() const { return heap_.empty(); }

  uint64_t pop_max() {
    const Var result = heap_.pop_max();
    pop_ordinal_++;
    pop_hash_ ^= kernel_->internal_to_external[result] + 1ull;
    pop_hash_ *= 1099511628211ull;
    event_trace_bve_pop(*kernel_, pop_ordinal_, pop_hash_, result,
                        heap_.score(result),
                        heap_.empty() ? INVALID_VAR : heap_.members()[0],
                        heap_.size());
    trace_selected("pop", result, false);
    return payload_[result];
  }

  static void update_hook(Kernel &, void *state, Var v, bool reinsert) {
    static_cast<BveCandidateHeap *>(state)->update(v, reinsert);
  }

private:
  bool contains(Var v) const {
    return heap_.contains(v);
  }

  bool active(Var v) const {
    const Kernel &k = *kernel_;
    return v < k.nvars && k.values[make_lit(v, false)] == 0 &&
           !kernel_var_retired(k, v);
  }

  double schedule_score(Var v) const {
    const Kernel &k = *kernel_;
#ifndef NDEBUG
    for (const Lit lit : {make_lit(v, false), make_lit(v, true)}) {
      uint64_t physical = k.dense->large_size(lit);
      for (const WatchEntry watch : k.watches.entries(lit))
        physical += watch.binary();
      assert(physical == occurrence_counts_[lit]);
    }
#endif
    const double relevance =
        k.h ? (k.h->stable ? k.h->scores.score[v] : k.h->links[v].stamp) : 0;
    const double reference = kissat_bve_variable_score(
        relevance, bve_schedule_occurrence_count(k, make_lit(v, false)),
        bve_schedule_occurrence_count(k, make_lit(v, true)),
        k.bve_occurrence_limit);
    return -reference; // Kissat stores the negated score in a max-heap.
  }

  void insert_initial(Var v) {
    const Kernel &k = *kernel_;
    if (!active(v) || v >= k.bve_retry.size())
      return;
    // Kissat's schedule contains exactly the active variables whose
    // `flags.eliminate` bit is set.  `bve_retry` is that bit's kernel image;
    // bound growth rearms it for every variable (RescheduleBveCandidatesRequest).
    if (!k.bve_retry[v])
      return;
    heap_.insert(v, schedule_score(v));
  }

  void update(Var v, bool reinsert) {
    // kissat eliminate.c:72-110 (update_after_removing_variable): any
    // non-fixed variable of a removed clause is rescored and, when absent,
    // pushed, whether or not this round scheduled it; the pop-time retry
    // check is the only eligibility test.  Adding a resolvent never pushes.
    if (v >= heap_.extent() || !active(v))
      return;
    const double now = schedule_score(v);
    if (contains(v)) {
      heap_.update_score(v, now);
    } else if (reinsert) {
      // eliminate.c::update_after_removing_variable pushes an active variable
      // which is absent from the physical schedule without consulting its
      // `flags.eliminate` bit.  A previously failed candidate can therefore
      // re-enter the heap as a ghost: can_eliminate_variable rejects it when it
      // is popped, but its interim presence and bubble path are observable heap
      // topology.  `bve_retry` is the flag image used at initial scheduling and
      // at the pre-apply guard; it must not suppress this physical reinsertion.
      if (payload_[v] == INSTANCE_CANONICAL)
        payload_[v] = lit_instance(*kernel_, make_lit(v, false));
      heap_.insert(v, now);
    } else
      heap_.assign_score(v, now);
    trace_selected(reinsert ? "update-reinsert" : "update-add", v, false);
  }

  void trace_selected(const char *stage, Var trigger, bool force) {
    const Var root = heap_.empty() ? INVALID_VAR : heap_.members()[0];
    for (size_t i = 0; i < trace_vars_.size(); i++) {
      const Var v = trace_vars_[i];
      const uint32_t position = heap_.position(v);
      const double score = heap_.score(v);
      if (!force && position == trace_last_pos_[i] &&
          score == trace_last_score_[i])
        continue;
      event_trace_bve_heap(*kernel_, stage, trigger, v, score, position, root,
                           heap_.size());
      trace_last_pos_[i] = position;
      trace_last_score_[i] = score;
    }
  }

  Kernel *kernel_ = nullptr;
  IndexedMaxHeap<Var, double> heap_;
  std::vector<uint32_t> occurrence_counts_;
  std::vector<uint64_t> payload_;
  std::vector<uint8_t> eligible_;
  std::vector<Var> trace_vars_;
  std::vector<uint32_t> trace_last_pos_;
  std::vector<double> trace_last_score_;
  uint64_t pop_ordinal_ = 0;
  uint64_t pop_hash_ = 1469598103934665603ull;
};

} // namespace strategy
} // namespace tactix

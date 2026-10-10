#pragma once

// BVE candidate ordering profiles owned by BveInvocationSession (gap plan
// Step 5, C4): the policy chooses a profile, the session owns its state and
// the kernel's score notifications reach it for the session's lifetime.
#include "diagnostics/event_trace.hpp"
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
    stack_.clear();
    pos_.assign(k.nvars, UINT32_MAX);
    score_.assign(k.nvars, 0.0);
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
    event_trace_bve_schedule(k, stack_.data(), score_.data(),
                             static_cast<uint32_t>(stack_.size()), k.bve_bound,
                             insertion_hash);
  }

  bool empty() const { return stack_.empty(); }

  uint64_t pop_max() {
    assert(!stack_.empty());
    const Var result = stack_[0];
    const Var last = stack_.back();
    stack_.pop_back();
    pos_[result] = UINT32_MAX;
    if (!stack_.empty()) {
      stack_[0] = last;
      pos_[last] = 0;
      bubble_down(last);
    }
    pop_ordinal_++;
    pop_hash_ ^= kernel_->internal_to_external[result] + 1ull;
    pop_hash_ *= 1099511628211ull;
    event_trace_bve_pop(*kernel_, pop_ordinal_, pop_hash_, result,
                        score_[result], stack_.empty() ? INVALID_VAR : stack_[0],
                        static_cast<uint32_t>(stack_.size()));
    trace_selected("pop", result, false);
    return payload_[result];
  }

  static void update_hook(Kernel &, void *state, Var v, bool reinsert) {
    static_cast<BveCandidateHeap *>(state)->update(v, reinsert);
  }

private:
  bool contains(Var v) const {
    return v < pos_.size() && pos_[v] != UINT32_MAX;
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
    score_[v] = schedule_score(v);
    pos_[v] = static_cast<uint32_t>(stack_.size());
    stack_.push_back(v);
    bubble_up(v);
  }

  void update(Var v, bool reinsert) {
    // kissat eliminate.c:72-110 (update_after_removing_variable): any
    // non-fixed variable of a removed clause is rescored and, when absent,
    // pushed, whether or not this round scheduled it; the pop-time retry
    // check is the only eligibility test.  Adding a resolvent never pushes.
    if (v >= pos_.size() || !active(v))
      return;
    const double old = score_[v];
    const double now = schedule_score(v);
    score_[v] = now;
    if (contains(v)) {
      if (now > old)
        bubble_up(v);
      else if (now < old)
        bubble_down(v);
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
      pos_[v] = static_cast<uint32_t>(stack_.size());
      stack_.push_back(v);
      bubble_up(v);
    }
    trace_selected(reinsert ? "update-reinsert" : "update-add", v, false);
  }

  void trace_selected(const char *stage, Var trigger, bool force) {
    const Var root = stack_.empty() ? INVALID_VAR : stack_[0];
    for (size_t i = 0; i < trace_vars_.size(); i++) {
      const Var v = trace_vars_[i];
      const uint32_t position = contains(v) ? pos_[v] : UINT32_MAX;
      const double score = score_[v];
      if (!force && position == trace_last_pos_[i] &&
          score == trace_last_score_[i])
        continue;
      event_trace_bve_heap(*kernel_, stage, trigger, v, score, position, root,
                           static_cast<uint32_t>(stack_.size()));
      trace_last_pos_[i] = position;
      trace_last_score_[i] = score;
    }
  }

  void bubble_up(Var v) {
    uint32_t at = pos_[v];
    while (at) {
      const uint32_t parent_at = (at - 1) / 2;
      const Var parent = stack_[parent_at];
      if (score_[parent] >= score_[v])
        break;
      stack_[at] = parent;
      pos_[parent] = at;
      at = parent_at;
    }
    stack_[at] = v;
    pos_[v] = at;
  }

  void bubble_down(Var v) {
    uint32_t at = pos_[v];
    for (;;) {
      uint32_t child_at = 2 * at + 1;
      if (child_at >= stack_.size())
        break;
      Var child = stack_[child_at];
      const uint32_t sibling_at = child_at + 1;
      if (sibling_at < stack_.size() &&
          score_[stack_[sibling_at]] > score_[child]) {
        child_at = sibling_at;
        child = stack_[child_at];
      }
      if (score_[child] <= score_[v])
        break;
      stack_[at] = child;
      pos_[child] = at;
      at = child_at;
    }
    stack_[at] = v;
    pos_[v] = at;
  }

  Kernel *kernel_ = nullptr;
  std::vector<Var> stack_;
  std::vector<uint32_t> pos_;
  std::vector<uint32_t> occurrence_counts_;
  std::vector<double> score_;
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

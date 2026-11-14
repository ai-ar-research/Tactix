#pragma once

// BVE candidate ordering profiles owned by BveInvocationSession: the policy
// chooses one, the session owns its state and receives the kernel's score
// notifications.
#include "diagnostics/event_trace.hpp"
#include "kernel/indexed_max_heap.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "rules/bve/bve.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"
#include "rules/registry.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace tactix {

// Kissat eliminate.c `variable_score`: keep its binary64 intermediates (`prod`,
// `sum`, `score`, `occlim2`); flattening can move a score by one ULP and change
// heap topology.
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

inline bool bve_affine_support_protected(const Kernel &kernel,Var variable) {
  const auto *session=kernel.affine_matrix_search_session;
  if(!session||!session->enabled()||!session->persistent())return false;
  const auto &support=session->support();
  return std::binary_search(support.begin(),support.end(),
      kernel_externalize(kernel,make_lit(variable,false)));
}

// Kissat's elimination schedule is a mutable indexed max-heap of negated
// scores, filled in compact-variable order, stopping bubble-up at equality and
// preferring the left child when bubbling down; a global re-sort would pick
// different equal-score variables.
class BveCandidateHeap {
public:
  void initialize(Kernel &k, const uint64_t *insts, uint32_t n) {
    kernel_ = &k;
    assert(k.dense && !k.bve_occurrence_count_image);
    // Kissat eliminate.c::variable_score reads SIZE_WATCHES per polarity
    // (binary plus large entries); materialize that sum once and let the kernel
    // mirror later mutations.
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
    // Deterministic fallback for bare API kernels without H.
    for (Var v = 0; v < k.nvars; v++)
      if (eligible_[v] && !inserted[v])
        insert_initial(v);
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
    return payload_[result];
  }

  static void update_hook(Kernel &, void *state, Var v, bool reinsert) {
    static_cast<BveCandidateHeap *>(state)->update(v, reinsert);
  }

private:
  friend class BveOrdering;
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
    // Kissat's schedule holds the active variables with `flags.eliminate` set;
    // `bve_retry` is that bit, rearmed for all by bound growth
    // (RescheduleBveCandidatesRequest).
    if (!k.bve_retry[v])
      return;
    heap_.insert(v, schedule_score(v));
  }

  void update(Var v, bool reinsert) {
    // kissat eliminate.c:72-110 (update_after_removing_variable): every
    // non-fixed variable of a removed clause is rescored and pushed if absent;
    // the pop-time retry check is the only eligibility test, and resolvents
    // never push.
    if (v >= heap_.extent() || !active(v))
      return;
    const double now = schedule_score(v);
    if (contains(v)) {
      heap_.update_score(v, now);
    } else if (reinsert) {
      // update_after_removing_variable pushes an absent active variable without
      // consulting `flags.eliminate`, so a failed candidate re-enters as a
      // ghost that can_eliminate_variable rejects at pop; its presence is heap
      // topology, so `bve_retry` must not suppress it.
      if (payload_[v] == INSTANCE_CANONICAL)
        payload_[v] = lit_instance(*kernel_, make_lit(v, false));
      heap_.insert(v, now);
    } else
      heap_.assign_score(v, now);
  }

  Kernel *kernel_ = nullptr;
  IndexedMaxHeap<Var, double> heap_;
  std::vector<uint32_t> occurrence_counts_;
  std::vector<uint64_t> payload_;
  std::vector<uint8_t> eligible_;
  // Unused; keep the layout tests compile against until the test-API pass.
  std::vector<Var> trace_vars_;
  std::vector<uint32_t> trace_last_pos_;
  std::vector<double> trace_last_score_;
  uint64_t pop_ordinal_ = 0;
  uint64_t pop_hash_ = 1469598103934665603ull;
};

// Shared ordering; public continuations connect only per atomic operation.
class BveOrdering {
public:
  BveOrdering(Kernel &k,const uint64_t *instances,uint32_t count,bool connected=true)
      :kernel_(k) {
    assert(!k.bve_score_update_hook&&!k.bve_occurrence_count_image);
    heap_.initialize(k,instances,count);
    k.bve_score_update_hook=BveCandidateHeap::update_hook;
    k.bve_score_update_state=&heap_;
    connected_=true;
    if(!connected)disconnect();
  }
  ~BveOrdering() {disconnect();}
  BveOrdering(const BveOrdering &)=delete;
  BveOrdering &operator=(const BveOrdering &)=delete;
  bool empty() const {return heap_.empty();}
  uint64_t next() {return heap_.pop_max();}
  void connect() {
    assert(!connected_&&!kernel_.bve_score_update_hook&&!kernel_.bve_occurrence_count_image);
    kernel_.bve_occurrence_count_image=&heap_.occurrence_counts_;
    kernel_.bve_score_update_hook=BveCandidateHeap::update_hook;
    kernel_.bve_score_update_state=&heap_;connected_=true;
  }
  void disconnect() {
    if(!connected_)return;
    assert(kernel_.bve_score_update_state==&heap_&&
        kernel_.bve_occurrence_count_image==&heap_.occurrence_counts_);
    kernel_.bve_score_update_hook=nullptr;kernel_.bve_score_update_state=nullptr;
    kernel_.bve_occurrence_count_image=nullptr;connected_=false;
  }
private:
  Kernel &kernel_;
  BveCandidateHeap heap_;
  bool connected_=false;
};

} // namespace strategy
} // namespace tactix

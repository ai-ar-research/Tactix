#include "policies/event_trace.hpp"

#ifdef TACTIX_ENABLE_EVENT_TRACE

#include "policies/heuristics.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace tactix {
namespace {

EventTrace *active_trace;

uint64_t mix(uint64_t h, uint64_t x) {
  h ^= x;
  return h * 1099511628211ull;
}

uint64_t state_digest(const Kernel &k) {
  uint64_t h = 1469598103934665603ull;
  h = mix(h, k.level);
  h = mix(h, k.trail.size());
  for (Var v = 0; v < k.nvars; v++) {
    h = mix(h, k.internal_to_external[v] + 1ull);
    const VarStatus status = k.var_status.empty() ? VarStatus::Active
                                                   : k.var_status[v];
    h = mix(h, static_cast<uint8_t>(status));
    h = mix(h, static_cast<uint8_t>(k.values[make_lit(v, false)] + 1));
  }
  if (k.h)
    for (Var v = k.h->queue_first, n = 0;
         v != INVALID_VAR && n++ <= k.nvars; v = k.h->links[v].next)
      h = mix(h, k.internal_to_external[v] + 1ull);
  return h;
}

struct RankingSummary {
  uint64_t queue = 0, heap = 0;
  uint64_t retired_queue = 0, retired_heap = 0;
  std::string top = "-";
};

RankingSummary summarize_ranking(const Kernel &k) {
  RankingSummary res;
  if (!k.h)
    return res;
  for (Var v = k.h->queue_first; v != INVALID_VAR; v = k.h->links[v].next) {
    res.queue++;
    if (res.queue > k.nvars) // encode a cycle as an impossible population
      break;
    res.retired_queue += kernel_var_retired(k, v);
  }
  res.heap = k.h->scores.stack.size();
  for (const Var v : k.h->scores.stack)
    res.retired_heap += kernel_var_retired(k, v);

  std::vector<Var> ranked;
  ranked.reserve(k.nvars);
  if (k.h->stable) {
    for (Var v = 0; v < k.nvars; v++)
      if (!kernel_var_retired(k, v) && k.values[make_lit(v, false)] == 0)
        ranked.push_back(v);
    std::stable_sort(ranked.begin(), ranked.end(), [&](Var a, Var b) {
      if (k.h->scores.score[a] != k.h->scores.score[b])
        return k.h->scores.score[a] > k.h->scores.score[b];
      return a < b;
    });
  } else {
    for (Var v = k.h->queue_last, n = 0;
         v != INVALID_VAR && n++ <= k.nvars; v = k.h->links[v].prev)
      if (!kernel_var_retired(k, v) && k.values[make_lit(v, false)] == 0)
        ranked.push_back(v);
  }
  std::string top;
  const size_t n = std::min<size_t>(32, ranked.size());
  for (size_t i = 0; i < n; i++) {
    if (i)
      top += ',';
    top += std::to_string(k.internal_to_external[ranked[i]] + 1);
  }
  if (!top.empty())
    res.top.swap(top);
  return res;
}

void live_clause_counts(const Kernel &k, uint64_t &irredundant,
                        uint64_t &redundant) {
  irredundant = redundant = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_words(c->size);
    if (c->garbage())
      continue;
    (c->redundant() ? redundant : irredundant)++;
  }
}

} // namespace

EventTrace::~EventTrace() { close(); }

bool EventTrace::open_from_env(const Kernel &k) {
  const char *path = std::getenv("TACTIX_EVENT_TRACE");
  if (!path || !*path)
    return false;
  if (const char *cap = std::getenv("TACTIX_EVENT_TRACE_LIMIT"))
    limit_ = std::strtoull(cap, nullptr, 10);
  if (const char *from = std::getenv("TACTIX_EVENT_TRACE_CONFLICT_FROM")) {
    detail_conflict_from_ = std::strtoull(from, nullptr, 10);
    detail_window_ = true;
  }
  if (const char *to = std::getenv("TACTIX_EVENT_TRACE_CONFLICT_TO")) {
    detail_conflict_to_ = std::strtoull(to, nullptr, 10);
    detail_window_ = true;
  }
  out_ = std::fopen(path, "w");
  if (!out_)
    return false;
  active_trace = this;
  std::fprintf(out_, "# tactix-event-trace\t1\n");
  if (detail_window_)
    std::fprintf(out_, "# detail-conflicts\t%llu\t%llu\n",
                 (unsigned long long)detail_conflict_from_,
                 (unsigned long long)detail_conflict_to_);
  std::fprintf(out_, "# solver\tseq\tevent\tconflicts\tdecisions\t"
                     "propagations\tlevel\tmode\tactive\tunassigned\ttrail\t"
                     "queue\theap\tretired_queue\tretired_heap\tirred\tred\t"
                     "binary\tdigest\tsearch_external\ttop32\tpayload\n");
  macro(k, "trace_begin");
  return true;
}

void EventTrace::close() {
  if (!out_)
    return;
  std::fflush(out_);
  std::fclose(out_);
  out_ = nullptr;
  if (active_trace == this)
    active_trace = nullptr;
}

void EventTrace::emit(const Kernel &k, const char *event, const char *payload,
                      bool snapshot) {
  if (!out_)
    return;
  if (emitted_ >= limit_) {
    if (!truncated_) {
      std::fprintf(out_, "# truncated at %llu records\n",
                   (unsigned long long)limit_);
      truncated_ = true;
    }
    return;
  }
  emitted_++;
  RankingSummary rank;
  uint64_t active = 0, irred = 0, red = 0, digest = 0;
  if (snapshot) {
    rank = summarize_ranking(k);
    live_clause_counts(k, irred, red);
    digest = state_digest(k);
    for (Var v = 0; v < k.nvars; v++)
      active += !kernel_var_retired(k, v);
  }
  int search_external = 0;
  if (k.h && k.h->search_idx != INVALID_VAR &&
      k.h->search_idx < k.internal_to_external.size())
    search_external = static_cast<int>(k.internal_to_external[k.h->search_idx]) + 1;
  std::fprintf(out_,
               "tactix\t%llu\t%s\t%llu\t%llu\t%llu\t%u\t%s\t%u\t%u\t"
               "%zu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%zu\t"
               "%016llx\t%d\t%s\t%s\n",
               (unsigned long long)sequence_++, event,
               (unsigned long long)k.stats.conflicts,
               (unsigned long long)k.stats.decisions,
               (unsigned long long)k.stats.propagations, k.level,
               !k.h ? "none" : k.h->stable ? "stable" : "focused",
               static_cast<unsigned>(active),
               snapshot ? k.unassigned_active : 0u,
               snapshot ? k.trail.size() : 0u,
               (unsigned long long)rank.queue, (unsigned long long)rank.heap,
               (unsigned long long)rank.retired_queue,
               (unsigned long long)rank.retired_heap,
               (unsigned long long)irred, (unsigned long long)red,
               k.binary_ids.size(), (unsigned long long)digest,
               search_external, rank.top.c_str(), payload ? payload : "-");
}

void EventTrace::macro(const Kernel &k, const char *event, const char *payload) {
  emit(k, event, payload, true);
}

bool EventTrace::want_detail(const Kernel &k) {
  if (!out_)
    return false;
  if (detail_window_)
    return k.stats.conflicts >= detail_conflict_from_ &&
           k.stats.conflicts <= detail_conflict_to_;
  return true;
}

void EventTrace::detail(const Kernel &k, const char *event, int64_t payload) {
  if (!want_detail(k))
    return;
  if (!detail_window_ && saw_reorder_) {
    if (!post_reorder_details_)
      return;
    post_reorder_details_--;
  }
  char buf[32];
  std::snprintf(buf, sizeof buf, "%lld", (long long)payload);
  // Analysis is the only detailed event that mutates the ranking.  Snapshot
  // its post-state so a later decision mismatch can be attributed to the
  // exact conflict that first changed VMTF/EVSIDS, not merely to the decision
  // where that latent difference became visible.  Diagnostic builds only.
  emit(k, event, buf, !std::strcmp(event, "analysis_end"));
}

void EventTrace::watch_scan(const Kernel &k, Lit propagated) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const Lit watched = negate(propagated);
  for (const WatchEntry entry : k.watches.entries(watched)) {
    if (emitted_ >= limit_)
      return;
    std::vector<int> clause;
    if (entry.binary()) {
      clause.push_back(kernel_externalize(k, watched));
      clause.push_back(kernel_externalize(k, entry.other()));
    } else {
      const Clause *c = clause_at(k.arena, entry.ref());
      clause.reserve(c->size);
      for (const Lit lit : *c)
        clause.push_back(kernel_externalize(k, lit));
    }
    std::sort(clause.begin(), clause.end());
    std::string payload =
        "scan=" + std::to_string(kernel_externalize(k, propagated)) +
        "|block=" + std::to_string(kernel_externalize(k, entry.other())) +
        "|clause=";
    for (const int lit : clause) {
      if (payload.back() != '=')
        payload += ',';
      payload += std::to_string(lit);
    }
    emit(k, "watch", payload.c_str(), false);
  }
}

void event_trace_watch_scan(const Kernel &k, Lit propagated) {
  if (active_trace)
    active_trace->watch_scan(k, propagated);
}

void event_trace_learned_stage(const Kernel &k, const char *event,
                               const std::vector<Lit> &clause) {
  if (active_trace)
    active_trace->learned_stage(k, event, clause);
}

void EventTrace::implications(const Kernel &k, size_t trail_begin) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  for (size_t pos = trail_begin; pos < k.trail.size(); pos++) {
    if (emitted_ >= limit_)
      return;
    const Lit assigned_lit = k.trail[pos];
    const Assigned &assignment = k.assigned[var_of(assigned_lit)];
    if (assignment.reason == DECISION_REASON || assignment.reason == UNIT_REASON)
      continue;
    std::vector<int> reason;
    if (assignment.binary) {
      reason.push_back(kernel_externalize(k, assigned_lit));
      reason.push_back(kernel_externalize(k, assignment.reason));
    } else {
      const Clause *clause = clause_at(k.arena, assignment.reason);
      reason.reserve(clause->size);
      for (const Lit lit : *clause)
        reason.push_back(kernel_externalize(k, lit));
    }
    std::sort(reason.begin(), reason.end());
    std::string payload = "lit=" +
                          std::to_string(kernel_externalize(k, assigned_lit)) +
                          "|level=" + std::to_string(assignment.level) +
                          "|reason=";
    for (const int lit : reason) {
      if (payload.back() != '=')
        payload += ',';
      payload += std::to_string(lit);
    }
    emit(k, "imply", payload.c_str(), false);
  }
}

void EventTrace::conflict(const Kernel &k) {
  if (!want_detail(k) || !k.conflict_valid)
    return;
  const Lit *lits;
  uint32_t size;
  if (k.conflict_ref == INVALID_REF) {
    lits = k.conflict_bin;
    size = 2;
  } else {
    const Clause *clause = clause_at(k.arena, k.conflict_ref);
    lits = clause->lits;
    size = clause->size;
  }
  std::string payload = "order=";
  std::vector<int> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; i++) {
    if (i)
      payload += ',';
    const int lit = kernel_externalize(k, lits[i]);
    payload += std::to_string(lit);
    external.push_back(lit);
  }
  payload += "|set=";
  std::sort(external.begin(), external.end());
  for (const int lit : external) {
    if (payload.back() != '=')
      payload += ',';
    payload += std::to_string(lit);
  }
  emit(k, "conflict", payload.c_str(), false);
}

void EventTrace::learned(const Kernel &k, const std::vector<Lit> &clause) {
  learned_stage(k, "learned", clause);
}

void EventTrace::learned_stage(const Kernel &k, const char *event,
                               const std::vector<Lit> &clause) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = "order=";
  for (size_t i = 0; i < clause.size(); i++) {
    if (i)
      payload += ',';
    payload += std::to_string(kernel_externalize(k, clause[i]));
  }
  payload += "|set=";
  std::vector<int> external;
  external.reserve(clause.size());
  for (const Lit lit : clause)
    external.push_back(kernel_externalize(k, lit));
  std::sort(external.begin(), external.end());
  for (const int lit : external) {
    if (payload.back() != '=')
      payload += ',';
    payload += std::to_string(lit);
  }
  emit(k, event, payload.c_str(), false);
}

void EventTrace::checkpoint(const Kernel &k) {
  if (!out_ || !saw_reorder_)
    return;
  const uint64_t bucket = k.stats.conflicts / 1000;
  if (bucket <= checkpoint_bucket_)
    return;
  checkpoint_bucket_ = bucket;
  macro(k, "checkpoint");
}

void EventTrace::reorder_begin(const Kernel &k) {
  macro(k, "reorder_begin");
  saw_reorder_ = true;
  post_reorder_details_ = 1024;
  checkpoint_bucket_ = k.stats.conflicts / 1000;
}

void EventTrace::reorder_end(const Kernel &k) { macro(k, "reorder_end"); }

} // namespace tactix
#endif

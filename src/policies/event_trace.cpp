#include "policies/event_trace.hpp"

#ifdef TACTIX_ENABLE_EVENT_TRACE

#include "policies/heuristics.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
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

RankingSummary summarize_ranking(const Kernel &k, bool include_top) {
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

  // SKIP_HEAP is also the low-memory tracing contract: retain scalar queue and
  // heap populations, but do not materialize and sort one Var per solver
  // variable merely to print top32.  On giant formulas that diagnostic copy
  // can itself be the allocation that changes the observed failure boundary.
  if (!include_top)
    return res;

  // Focused VMTF is already totally ordered from queue_last backwards.  Walk
  // only far enough to print the requested prefix, as Kissat's focused_top
  // does, instead of materializing every active variable merely to keep 32.
  // Besides being equivalent, this keeps tracing a multi-million-variable
  // instance from adding an O(nvars) allocation at every control snapshot.
  if (!k.h->stable) {
    std::string top;
    size_t ranked = 0;
    for (Var v = k.h->queue_last, seen = 0;
         v != INVALID_VAR && seen++ <= k.nvars && ranked < 32;
         v = k.h->links[v].prev) {
      if (kernel_var_retired(k, v) || k.values[make_lit(v, false)] != 0)
        continue;
      if (ranked++)
        top += ',';
      top += std::to_string(k.internal_to_external[v] + 1);
    }
    if (!top.empty())
      res.top.swap(top);
    return res;
  }

  std::vector<Var> ranked;
  ranked.reserve(k.nvars);
  for (Var v = 0; v < k.nvars; v++)
    if (!kernel_var_retired(k, v) && k.values[make_lit(v, false)] == 0)
      ranked.push_back(v);
  std::stable_sort(ranked.begin(), ranked.end(), [&](Var a, Var b) {
    if (k.h->scores.score[a] != k.h->scores.score[b])
      return k.h->scores.score[a] > k.h->scores.score[b];
    return a < b;
  });
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

uint64_t live_binary_clause_count(const Kernel &k) {
  // `binary_ids` is a proof-identity archive: deleted pairs intentionally stay
  // there, so its size is not comparable with Kissat's live
  // `clauses_binary` statistic. Count one watch direction per live copy.
  uint64_t result = 0;
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary() && lit < watch.other())
        result++;
  return result;
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
  compact_probe_ = std::getenv("TACTIX_EVENT_TRACE_COMPACT_PROBE") != nullptr;
  skip_watch_ = std::getenv("TACTIX_EVENT_TRACE_SKIP_WATCH") != nullptr;
  skip_imply_ = std::getenv("TACTIX_EVENT_TRACE_SKIP_IMPLY") != nullptr;
  skip_heap_ = std::getenv("TACTIX_EVENT_TRACE_SKIP_HEAP") != nullptr;
  skip_decisions_ =
      std::getenv("TACTIX_EVENT_TRACE_SKIP_DECISIONS") != nullptr;
  skip_randec_draws_ =
      std::getenv("TACTIX_EVENT_TRACE_SKIP_RANDEC_DRAWS") != nullptr;
  skip_analysis_ticks_ =
      std::getenv("TACTIX_EVENT_TRACE_SKIP_ANALYSIS_TICKS") != nullptr;
  search_only_ = std::getenv("TACTIX_EVENT_TRACE_SEARCH_ONLY") != nullptr;
  control_only_ = std::getenv("TACTIX_EVENT_TRACE_CONTROL_ONLY") != nullptr;
  out_ = std::fopen(path, "w");
  if (!out_)
    return false;
  // Diagnostic traces are often used to locate a solver stuck inside its
  // first long macro operation.  Line buffering makes the last completed
  // boundary observable while the process is still running instead of only
  // when stdio fills or the solver exits.
  std::setvbuf(out_, nullptr, _IOLBF, 0);
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
  if (skip_watch_ && !std::strcmp(event, "watch"))
    return;
  if (skip_imply_ && !std::strcmp(event, "imply"))
    return;
  if (skip_heap_ && !std::strncmp(event, "heap_", 5))
    return;
  if (skip_decisions_ &&
      (!std::strcmp(event, "decision") || !std::strcmp(event, "randec_draw") ||
       !std::strcmp(event, "phase_pick")))
    return;
  if (skip_randec_draws_ && !std::strcmp(event, "randec_draw"))
    return;
  if (skip_analysis_ticks_ && !std::strncmp(event, "analysis_tick", 13))
    return;
  if (search_only_ && !snapshot && std::strcmp(event, "decision") &&
      std::strcmp(event, "randec_draw") &&
      std::strcmp(event, "randec_state") &&
      std::strcmp(event, "phase_pick") &&
      std::strcmp(event, "score_bump") &&
      std::strcmp(event, "heap_scores") &&
      std::strcmp(event, "assignment") && std::strcmp(event, "watch") &&
      std::strcmp(event, "imply") &&
      std::strcmp(event, "conflict") && std::strncmp(event, "analyzed", 8) &&
      std::strncmp(event, "analysis_", 9) &&
      std::strncmp(event, "learned", 7))
    return;
  // Bounded trajectory comparison without the much larger implication stream.
  // Snapshots retain control transitions; detailed records retain only choices
  // and conflict-analysis transactions needed to find the first mismatch.
  if (control_only_ && !snapshot && std::strcmp(event, "decision") &&
      std::strcmp(event, "randec_draw") &&
      std::strcmp(event, "randec_state") &&
      std::strcmp(event, "score_bump") &&
      std::strcmp(event, "heap_scores") &&
      std::strcmp(event, "assignment") &&
      std::strcmp(event, "heap_reinsert") && std::strcmp(event, "watch") &&
      std::strcmp(event, "conflict") &&
      std::strncmp(event, "analyzed", 8) &&
      std::strncmp(event, "analysis_", 9) &&
      std::strncmp(event, "learned", 7) &&
      std::strncmp(event, "factor_", 7) &&
      std::strncmp(event, "vivify_", 7) &&
      std::strncmp(event, "root_", 5) &&
      std::strncmp(event, "reason_", 7))
    return;
  if (compact_probe_ && std::strcmp(event, "trace_begin") &&
      std::strcmp(event, "probe_begin") && std::strcmp(event, "probe_end") &&
      std::strcmp(event, "preprocess_begin") &&
      std::strcmp(event, "preprocess_end") &&
      std::strcmp(event, "slot_boundary") &&
      std::strncmp(event, "factor_", 7) &&
      std::strncmp(event, "bve_", 4) &&
      std::strncmp(event, "substitute_", 11))
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
  uint64_t active = 0, irred = 0, red = 0, binary = 0, digest = 0;
  if (snapshot) {
    rank = summarize_ranking(k, !skip_heap_);
    live_clause_counts(k, irred, red);
    binary = live_binary_clause_count(k);
    digest = state_digest(k);
    for (Var v = 0; v < k.nvars; v++)
      active += !kernel_var_retired(k, v);
  }
  int search_external = 0;
  if (k.h && k.h->search_idx != INVALID_VAR &&
      k.h->search_idx < k.internal_to_external.size())
    search_external = static_cast<int>(k.internal_to_external[k.h->search_idx]) + 1;
  std::string annotated_payload = payload ? payload : "-";
  if (snapshot) {
    if (const char *selected =
            std::getenv("SAT_EVENT_TRACE_WATCH_LIST")) {
      const int wanted = std::atoi(selected);
      Lit watched = INVALID_LIT;
      for (Lit lit = 0; lit < 2 * k.nvars; lit++)
        if (kernel_externalize(k, lit) == wanted) {
          watched = lit;
          break;
        }
      annotated_payload += "|watch_lit=" + std::to_string(wanted) +
                           "|binary=";
      if (watched != INVALID_LIT) {
        bool first = true;
        for (const WatchEntry entry : k.watches.entries(watched)) {
          if (!entry.binary())
            continue;
          if (!first)
            annotated_payload += ',';
          first = false;
          annotated_payload +=
              std::to_string(kernel_externalize(k, entry.other()));
        }
      }
    }
  }
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
               (size_t)binary, (unsigned long long)digest,
               search_external, rank.top.c_str(), annotated_payload.c_str());
}

void EventTrace::macro(const Kernel &k, const char *event, const char *payload) {
  emit(k, event, payload, true);
}

void EventTrace::detail_text(const Kernel &k, const char *event,
                             const char *payload) {
  if (want_detail(k))
    emit(k, event, payload, false);
}

void EventTrace::retier(const Kernel &k, uint32_t mode,
                        const uint64_t *used_glue) {
  assert(mode < 2);
  std::string payload =
      std::string("mode=") + (mode ? "stable" : "focused") +
      "|focused=" + std::to_string(k.tier1_by_mode[0]) + ',' +
      std::to_string(k.tier2_by_mode[0]) + "|stable=" +
      std::to_string(k.tier1_by_mode[1]) + ',' +
      std::to_string(k.tier2_by_mode[1]) + "|posed=" +
      std::to_string(k.tier1) + ',' + std::to_string(k.tier2) + "|used=";
  uint64_t total = 0;
  for (uint32_t glue = 0; glue <= Stats::MAX_GLUE_USED; ++glue) {
    if (glue)
      payload += ',';
    const uint64_t used = used_glue[glue];
    total += used;
    payload += std::to_string(glue) + ':' + std::to_string(used);
  }
  payload += "|total=" + std::to_string(total);
  macro(k, "retier", payload.c_str());
}

void EventTrace::reduction(const Kernel &k, const std::vector<Ref> &candidates,
                           size_t victims) {
  const char *path = std::getenv("TACTIX_REDUCE_TRACE");
  if (!path || !*path)
    return;
  if (const char *from =
          std::getenv("TACTIX_REDUCE_TRACE_CONFLICT_FROM"))
    if (k.stats.conflicts < std::strtoull(from, nullptr, 10))
      return;
  if (const char *to = std::getenv("TACTIX_REDUCE_TRACE_CONFLICT_TO"))
    if (k.stats.conflicts > std::strtoull(to, nullptr, 10))
      return;
  static std::FILE *file;
  static std::string opened_path;
  if (!file || opened_path != path) {
    if (file)
      std::fclose(file);
    file = std::fopen(path, "w");
    if (!file)
      return;
    opened_path = path;
    std::fprintf(file, "# tactix-kissat-reduce\t1\n");
    std::fprintf(file,
                 "# solver\tkind\treduction\tconflict\tmode\ttier1\t"
                 "tier2\tlive_red\tcandidates\tvictims\tref\tsize\t"
                 "glue\tused\treason\teligibility\trank\tselected\t"
                 "literals\n");
  }
  std::unordered_map<Ref, size_t> ranks;
  for (size_t rank = 0; rank < candidates.size(); ++rank)
    ranks.emplace(candidates[rank], rank);
  std::vector<uint8_t> reason(k.arena.size());
  if (k.pending.valid && k.pending.ref != INVALID_REF)
    reason[k.pending.ref] = 1;
  for (const Lit lit : k.trail) {
    const Assigned &a = k.assigned[var_of(lit)];
    if (!a.binary && a.reason != DECISION_REASON && a.reason != UNIT_REASON)
      reason[a.reason] = 1;
  }
  uint64_t live = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    live += c->redundant() && !c->garbage();
    ref += clause_words(c->size);
  }
  const uint64_t reduction = k.stats.reductions + 1;
  std::fprintf(file,
               "tactix\tsummary\t%llu\t%llu\t%s\t%u\t%u\t%llu\t"
               "%zu\t%zu\t-\t-\t-\t-\t-\t-\t-\t-\t-\n",
               (unsigned long long)reduction,
               (unsigned long long)k.stats.conflicts,
               k.h && k.h->stable ? "stable" : "focused", k.tier1, k.tier2,
               (unsigned long long)live, candidates.size(), victims);
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const Ref next = ref + clause_words(c->size);
    if (!c->redundant() || c->garbage()) {
      ref = next;
      continue;
    }
    const uint32_t used = c->used(), glue = c->glue();
    const char *eligibility =
        reason[ref] ? "reason"
        : glue <= k.tier1 && used
            ? "tier1-used"
        : glue <= std::max(k.tier1, k.tier2) &&
                  used >= Clause::MAX_USED - 1
            ? "tier2-recent"
            : "candidate";
    const auto found = ranks.find(ref);
    const bool candidate = found != ranks.end();
    const size_t rank = candidate ? found->second : 0;
    std::vector<int> literals;
    literals.reserve(c->size);
    for (const Lit lit : *c)
      literals.push_back(kernel_externalize(k, lit));
    std::sort(literals.begin(), literals.end());
    std::fprintf(file,
                 "tactix\tclause\t%llu\t%llu\t%s\t%u\t%u\t%llu\t"
                 "%zu\t%zu\t%u\t%u\t%u\t%u\t%u\t%s\t",
                 (unsigned long long)reduction,
                 (unsigned long long)k.stats.conflicts,
                 k.h && k.h->stable ? "stable" : "focused", k.tier1,
                 k.tier2, (unsigned long long)live, candidates.size(), victims,
                 ref, c->size, glue, used, reason[ref], eligibility);
    if (candidate)
      std::fprintf(file, "%zu\t%u\t", rank, rank < victims);
    else
      std::fprintf(file, "-\t0\t");
    for (size_t i = 0; i < literals.size(); ++i)
      std::fprintf(file, "%s%d", i ? "," : "", literals[i]);
    std::fputc('\n', file);
    ref = next;
  }
  std::fflush(file);
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

void EventTrace::randec_draw(const Kernel &k, uint32_t raw, uint32_t slot,
                             Var var, bool accepted) {
  if (!want_detail(k))
    return;
  const std::string payload =
      "raw=" + std::to_string(raw) + "|slot=" + std::to_string(slot) +
      "|external=" + std::to_string(k.internal_to_external[var] + 1) +
      "|accepted=" + std::to_string(accepted);
  emit(k, "randec_draw", payload.c_str(), false);
}

void EventTrace::randec_state(const Kernel &k, const char *stage,
                              uint64_t until, uint64_t next,
                              uint64_t sequence) {
  if (!want_detail(k))
    return;
  const std::string payload =
      "stage=" + std::string(stage) + "|until=" + std::to_string(until) +
      "|next=" + std::to_string(next) +
      "|sequence=" + std::to_string(sequence);
  emit(k, "randec_state", payload.c_str(), false);
}

void EventTrace::watch_scan(const Kernel &k, Lit propagated) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const Lit watched = negate(propagated);
  for (const WatchEntry entry : k.watches.entries(watched)) {
    if (emitted_ >= limit_)
      return;
    std::vector<int> clause;
    const Clause *large = nullptr;
    if (entry.binary()) {
      clause.push_back(kernel_externalize(k, watched));
      clause.push_back(kernel_externalize(k, entry.other()));
    } else {
      large = clause_at(k.arena, entry.ref());
      clause.reserve(large->size);
      for (const Lit lit : *large)
        clause.push_back(kernel_externalize(k, lit));
    }
    if (const char *selected =
            std::getenv("TACTIX_EVENT_TRACE_WATCH_LITERAL")) {
      const int wanted = std::abs(std::atoi(selected));
      bool contains = false;
      for (const int lit : clause)
        contains |= std::abs(lit) == wanted;
      if (!contains)
        continue;
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
    if (large && std::getenv("TACTIX_EVENT_TRACE_WATCH_POSE")) {
      payload += "|id=" + std::to_string(large->id()) +
                 "|order=" +
                 std::to_string(kernel_clause_order(k, large->id())) +
                 "|searched=" + std::to_string(large->searched) + "|pose=";
      for (uint32_t i = 0; i < large->size; i++) {
        if (i)
          payload += ',';
        payload += std::to_string(kernel_externalize(k, large->lits[i]));
      }
    }
    emit(k, "watch", payload.c_str(), false);
  }
}

void event_trace_watch_scan(const Kernel &k, Lit propagated) {
  if (active_trace)
    active_trace->watch_scan(k, propagated);
}

void event_trace_detail(const Kernel &k, const char *event, int64_t payload) {
  if (active_trace)
    active_trace->detail(k, event, payload);
}

void event_trace_learned_stage(const Kernel &k, const char *event,
                               const std::vector<Lit> &clause) {
  if (active_trace)
    active_trace->learned_stage(k, event, clause);
}

void event_trace_retier(const Kernel &k, uint32_t mode,
                        const uint64_t *used_glue) {
  if (active_trace)
    active_trace->retier(k, mode, used_glue);
}

void event_trace_randec_draw(const Kernel &k, uint32_t raw, uint32_t slot,
                             Var var, bool accepted) {
  if (active_trace)
    active_trace->randec_draw(k, raw, slot, var, accepted);
}

void event_trace_randec_state(const Kernel &k, const char *stage,
                              uint64_t until, uint64_t next,
                              uint64_t sequence) {
  if (active_trace)
    active_trace->randec_state(k, stage, until, next, sequence);
}

void EventTrace::phase_pick(const Kernel &k, Var var, int8_t target,
                            int8_t saved, int8_t result) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" +
      std::to_string(kernel_externalize(k, make_lit(var, false))) +
      "|target=" + std::to_string(target) +
      "|saved=" + std::to_string(saved) +
      "|result=" + std::to_string(result);
  emit(k, "phase_pick", payload.c_str(), false);
}

void event_trace_phase_pick(const Kernel &k, Var var, int8_t target,
                            int8_t saved, int8_t result) {
  if (active_trace)
    active_trace->phase_pick(k, var, target, saved, result);
}

void EventTrace::target_save(const Kernel &k) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::vector<int> assigned;
  assigned.reserve(k.trail.size());
  for (const Lit lit : k.trail)
    assigned.push_back(kernel_externalize(k, lit));
  std::sort(assigned.begin(), assigned.end());
  std::string payload = "assigned=" + std::to_string(assigned.size()) +
                        "|set=";
  for (const int lit : assigned) {
    if (payload.back() != '=')
      payload += ',';
    payload += std::to_string(lit);
  }
  emit(k, "target_save", payload.c_str(), false);
}

void event_trace_target_save(const Kernel &k) {
  if (active_trace)
    active_trace->target_save(k);
}

void EventTrace::phase_save(const Kernel &k, Lit lit) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "lit=" + std::to_string(kernel_externalize(k, lit));
  emit(k, "phase_save", payload.c_str(), false);
}

void event_trace_phase_save(const Kernel &k, Lit lit) {
  if (active_trace)
    active_trace->phase_save(k, lit);
}

void EventTrace::slot_boundary(const Kernel &k, const char *name,
                               const char *stage) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "name=" + std::string(name) + "|stage=" + std::string(stage);
  emit(k, "slot_boundary", payload.c_str(), true);
}

void event_trace_slot_boundary(const Kernel &k, const char *name,
                               const char *stage) {
  if (active_trace)
    active_trace->slot_boundary(k, name, stage);
}

void EventTrace::sweep_seed(const Kernel &k, Var seed) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false)));
  emit(k, "sweep_seed", payload.c_str(), false);
}

void event_trace_sweep_seed(const Kernel &k, Var seed) {
  if (active_trace)
    active_trace->sweep_seed(k, seed);
}

void EventTrace::sweep_environment(const Kernel &k, Var seed, uint32_t vars,
                                   uint32_t clauses, uint64_t hash) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  char digest[17];
  std::snprintf(digest, sizeof digest, "%016llx",
                static_cast<unsigned long long>(hash));
  const std::string payload =
      "external=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false))) +
      "|vars=" + std::to_string(vars) + "|clauses=" +
      std::to_string(clauses) + "|hash=" + digest;
  emit(k, "sweep_environment", payload.c_str(), false);
}

void event_trace_sweep_environment(const Kernel &k, Var seed, uint32_t vars,
                                   uint32_t clauses, uint64_t hash) {
  if (active_trace)
    active_trace->sweep_environment(k, seed, vars, clauses, hash);
}

void EventTrace::sweep_environment_clause(const Kernel &k, Var seed,
                                          uint32_t ordinal, const Lit *lits,
                                          uint32_t size) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_CLAUSES");
  if (!selected || std::strtol(selected, nullptr, 10) !=
                       kernel_externalize(k, make_lit(seed, false)))
    return;
  std::vector<int> external;
  external.reserve(size);
  std::string payload =
      "seed=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false))) +
      "|ordinal=" + std::to_string(ordinal) + "|order=";
  for (uint32_t i = 0; i < size; i++) {
    const int lit = kernel_externalize(k, lits[i]);
    if (i)
      payload += ',';
    payload += std::to_string(lit);
    external.push_back(lit);
  }
  payload += "|set=";
  std::sort(external.begin(), external.end());
  for (uint32_t i = 0; i < external.size(); i++) {
    if (i)
      payload += ',';
    payload += std::to_string(external[i]);
  }
  emit(k, "sweep_environment_clause", payload.c_str(), false);
}

void event_trace_sweep_environment_clause(const Kernel &k, Var seed,
                                          uint32_t ordinal, const Lit *lits,
                                          uint32_t size) {
  if (active_trace)
    active_trace->sweep_environment_clause(k, seed, ordinal, lits, size);
}

void EventTrace::sweep_query(const Kernel &k, Var seed, uint32_t ordinal,
                             const char *kind, Lit a, Lit b, int result,
                             uint64_t ticks, uint64_t conflicts) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_QUERIES");
  if (!selected || std::strtol(selected, nullptr, 10) !=
                       kernel_externalize(k, make_lit(seed, false)))
    return;
  const int ea = a == INVALID_LIT ? 0 : kernel_externalize(k, a);
  const int eb = b == INVALID_LIT ? 0 : kernel_externalize(k, b);
  const std::string payload =
      "seed=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false))) +
      "|ordinal=" + std::to_string(ordinal) + "|kind=" + kind +
      "|a=" + std::to_string(ea) + "|b=" + std::to_string(eb) +
      "|result=" + std::to_string(result) +
      "|ticks=" + std::to_string(ticks) +
      "|conflicts=" + std::to_string(conflicts);
  emit(k, "sweep_query", payload.c_str(), false);
}

void event_trace_sweep_query(const Kernel &k, Var seed, uint32_t ordinal,
                             const char *kind, Lit a, Lit b, int result,
                             uint64_t ticks, uint64_t conflicts) {
  if (active_trace)
    active_trace->sweep_query(k, seed, ordinal, kind, a, b, result, ticks,
                              conflicts);
}

void EventTrace::sweep_model(const Kernel &k, Var seed, uint32_t ordinal,
                             const Var *vars, const int8_t *values,
                             uint32_t size) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_QUERIES");
  if (!selected || std::strtol(selected, nullptr, 10) !=
                       kernel_externalize(k, make_lit(seed, false)))
    return;
  std::string payload =
      "seed=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false))) +
      "|ordinal=" + std::to_string(ordinal) + "|model=";
  for (uint32_t i = 0; i < size; i++) {
    if (i)
      payload += ',';
    payload += std::to_string(
        kernel_externalize(k, make_lit(vars[i], values[i] < 0)));
  }
  emit(k, "sweep_query_model", payload.c_str(), false);
}

void event_trace_sweep_model(const Kernel &k, Var seed, uint32_t ordinal,
                             const Var *vars, const int8_t *values,
                             uint32_t size) {
  if (active_trace)
    active_trace->sweep_model(k, seed, ordinal, vars, values, size);
}

void EventTrace::sweep_flip(const Kernel &k, Var seed, const char *stage,
                            Lit lit, bool result, uint64_t ticks) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_QUERIES");
  if (!selected || std::strtol(selected, nullptr, 10) !=
                       kernel_externalize(k, make_lit(seed, false)))
    return;
  const std::string payload =
      "seed=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false))) +
      "|stage=" + stage +
      "|lit=" + std::to_string(kernel_externalize(k, lit)) +
      "|result=" + std::to_string(result ? 1 : 0) +
      "|ticks=" + std::to_string(ticks);
  emit(k, "sweep_flip", payload.c_str(), false);
}

void event_trace_sweep_flip(const Kernel &k, Var seed, const char *stage,
                            Lit lit, bool result, uint64_t ticks) {
  if (active_trace)
    active_trace->sweep_flip(k, seed, stage, lit, result, ticks);
}

void EventTrace::sweep_effort(const Kernel &k, uint32_t external,
                              uint64_t kitten_ticks, uint64_t entry_ticks,
                              uint64_t solves) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" + std::to_string(external) +
      "|kitten_ticks=" + std::to_string(kitten_ticks) +
      "|entry_ticks=" + std::to_string(entry_ticks) +
      "|solves=" + std::to_string(solves);
  emit(k, "sweep_effort", payload.c_str(), false);
}

void event_trace_sweep_effort(const Kernel &k, uint32_t external,
                              uint64_t kitten_ticks, uint64_t entry_ticks,
                              uint64_t solves) {
  if (active_trace)
    active_trace->sweep_effort(k, external, kitten_ticks, entry_ticks, solves);
}

void EventTrace::sweep_environment_var(const Kernel &k, Var seed,
                                       uint32_t ordinal, Var var,
                                       uint32_t depth) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "seed=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false))) +
      "|ordinal=" + std::to_string(ordinal) + "|external=" +
      std::to_string(kernel_externalize(k, make_lit(var, false))) +
      "|depth=" + std::to_string(depth);
  emit(k, "sweep_environment_var", payload.c_str(), false);
}

void event_trace_sweep_environment_var(const Kernel &k, Var seed,
                                       uint32_t ordinal, Var var,
                                       uint32_t depth) {
  if (active_trace)
    active_trace->sweep_environment_var(k, seed, ordinal, var, depth);
}

void EventTrace::sweep_watch(const Kernel &k, Var seed, uint32_t ordinal,
                             Lit other) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_WATCH");
  if (!selected || std::strtol(selected, nullptr, 10) !=
                       kernel_externalize(k, make_lit(seed, false)))
    return;
  const std::string payload =
      "seed=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false))) +
      "|ordinal=" + std::to_string(ordinal) + "|other=" +
      std::to_string(kernel_externalize(k, other));
  emit(k, "sweep_watch", payload.c_str(), false);
}

void event_trace_sweep_watch(const Kernel &k, Var seed, uint32_t ordinal,
                             Lit other) {
  if (active_trace)
    active_trace->sweep_watch(k, seed, ordinal, other);
}

void EventTrace::sweep_equivalence(const Kernel &k, Lit a, Lit b) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "a=" + std::to_string(kernel_externalize(k, a)) +
      "|b=" + std::to_string(kernel_externalize(k, b));
  emit(k, "sweep_equivalence", payload.c_str(), false);
}

void event_trace_sweep_equivalence(const Kernel &k, Lit a, Lit b) {
  if (active_trace)
    active_trace->sweep_equivalence(k, a, b);
}

void EventTrace::sweep_unit(const Kernel &k, Lit lit) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "lit=" + std::to_string(kernel_externalize(k, lit));
  emit(k, "sweep_unit", payload.c_str(), false);
}

void event_trace_sweep_unit(const Kernel &k, Lit lit) {
  if (active_trace)
    active_trace->sweep_unit(k, lit);
}

void EventTrace::factor_seed(const Kernel &k, Lit seed, uint32_t score,
                             uint32_t remainders) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" + std::to_string(kernel_externalize(k, seed)) +
      "|score=" + std::to_string(score) +
      "|remainders=" + std::to_string(remainders);
  emit(k, "factor_seed", payload.c_str(), false);
}

void event_trace_factor_seed(const Kernel &k, Lit seed, uint32_t score,
                             uint32_t remainders) {
  if (active_trace)
    active_trace->factor_seed(k, seed, score, remainders);
}

void EventTrace::factor_remainder(const Kernel &k, Lit seed,
                                  uint32_t ordinal, bool large,
                                  const Lit *lits, uint32_t size) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
  if (!selected || std::atoi(selected) != kernel_externalize(k, seed))
    return;
  std::string payload =
      "seed=" + std::to_string(kernel_externalize(k, seed)) +
      "|ordinal=" + std::to_string(ordinal) +
      "|source=" + (large ? "large" : "binary") + "|lits=";
  for (uint32_t i = 0; i < size; i++) {
    if (i)
      payload += ',';
    payload += std::to_string(kernel_externalize(k, lits[i]));
  }
  emit(k, "factor_remainder", payload.c_str(), false);
}

void event_trace_factor_remainder(const Kernel &k, Lit seed,
                                  uint32_t ordinal, bool large,
                                  const Lit *lits, uint32_t size) {
  if (active_trace)
    active_trace->factor_remainder(k, seed, ordinal, large, lits, size);
}

void EventTrace::factor_candidate(const Kernel &k, Lit seed, uint32_t depth,
                                  uint32_t ordinal, Lit candidate,
                                  uint32_t count, uint32_t score) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
  if (!selected || std::atoi(selected) != kernel_externalize(k, seed))
    return;
  const std::string payload =
      "seed=" + std::to_string(kernel_externalize(k, seed)) +
      "|depth=" + std::to_string(depth) +
      "|ordinal=" + std::to_string(ordinal) +
      "|candidate=" + std::to_string(kernel_externalize(k, candidate)) +
      "|count=" + std::to_string(count) +
      "|score=" + std::to_string(score);
  emit(k, "factor_candidate", payload.c_str(), false);
}

void event_trace_factor_candidate(const Kernel &k, Lit seed, uint32_t depth,
                                  uint32_t ordinal, Lit candidate,
                                  uint32_t count, uint32_t score) {
  if (active_trace)
    active_trace->factor_candidate(k, seed, depth, ordinal, candidate, count,
                                   score);
}

void EventTrace::factor_score_entry(const Kernel &k, Lit seed, uint32_t depth,
                                    Lit candidate, uint32_t ordinal, bool live,
                                    Lit other, uint64_t id,
                                    const Clause *clause) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected_seed = std::getenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
  const char *selected_candidate =
      std::getenv("TACTIX_EVENT_TRACE_FACTOR_CANDIDATE");
  if (!selected_seed || !selected_candidate ||
      std::atoi(selected_seed) != kernel_externalize(k, seed) ||
      std::atoi(selected_candidate) != kernel_externalize(k, candidate))
    return;
  std::string payload =
      "seed=" + std::to_string(kernel_externalize(k, seed)) +
      "|depth=" + std::to_string(depth) +
      "|candidate=" + std::to_string(kernel_externalize(k, candidate)) +
      "|ordinal=" + std::to_string(ordinal) +
      "|kind=" + (clause ? "large" : "binary") +
      "|live=" + (live ? "1" : "0") + "|id=" + std::to_string(id) +
      "|lits=";
  if (clause) {
    std::vector<int> lits;
    for (const Lit lit : *clause)
      lits.push_back(kernel_externalize(k, lit));
    std::sort(lits.begin(), lits.end());
    for (size_t i = 0; i < lits.size(); i++) {
      if (i)
        payload += ',';
      payload += std::to_string(lits[i]);
    }
  } else {
    payload += std::to_string(kernel_externalize(k, candidate));
    payload += ',';
    payload += std::to_string(kernel_externalize(k, other));
  }
  emit(k, "factor_score_entry", payload.c_str(), false);
}

void event_trace_factor_score_entry(const Kernel &k, Lit seed, uint32_t depth,
                                    Lit candidate, uint32_t ordinal, bool live,
                                    Lit other, uint64_t id,
                                    const Clause *clause) {
  if (active_trace)
    active_trace->factor_score_entry(k, seed, depth, candidate, ordinal, live,
                                     other, id, clause);
}

void EventTrace::factor_literal_state(const Kernel &k, Lit lit, bool marked,
                                      bool retired, int value,
                                      uint32_t score) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_LITERAL");
  if (!selected || std::atoi(selected) != kernel_externalize(k, lit))
    return;
  const std::string payload =
      "external=" + std::to_string(kernel_externalize(k, lit)) +
      "|active=" + (retired ? "0" : "1") +
      "|marked=" + (marked ? "1" : "0") +
      "|value=" + std::to_string(value) +
      "|score=" + std::to_string(score);
  emit(k, "factor_literal_state", payload.c_str(), false);
}

void event_trace_factor_literal_state(const Kernel &k, Lit lit, bool marked,
                                      bool retired, int value,
                                      uint32_t score) {
  if (active_trace)
    active_trace->factor_literal_state(k, lit, marked, retired, value, score);
}

void EventTrace::substitute_map(const Kernel &k, Lit lit, Lit representative) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_SUBSTITUTE_LITERAL");
  if (!selected || std::atoi(selected) != kernel_externalize(k, lit))
    return;
  const std::string payload =
      "external=" + std::to_string(kernel_externalize(k, lit)) +
      "|representative=" +
      std::to_string(representative == INVALID_LIT
                         ? 0
                         : kernel_externalize(k, representative)) +
      "|active=" + (kernel_var_retired(k, var_of(lit)) ? "0" : "1") +
      "|value=" + std::to_string(k.value(lit));
  emit(k, "substitute_map", payload.c_str(), false);
}

void event_trace_substitute_map(const Kernel &k, Lit lit, Lit representative) {
  if (active_trace)
    active_trace->substitute_map(k, lit, representative);
}

void EventTrace::factor_literal_entry(const Kernel &k, Lit selected_lit,
                                      uint32_t ordinal, bool live, Lit other,
                                      uint64_t id, const Clause *clause) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_LITERAL");
  if (!selected ||
      std::atoi(selected) != kernel_externalize(k, selected_lit))
    return;
  std::string payload =
      "external=" + std::to_string(kernel_externalize(k, selected_lit)) +
      "|ordinal=" + std::to_string(ordinal) +
      "|kind=" + (clause ? "large" : "binary") +
      "|live=" + (live ? "1" : "0") + "|id=" + std::to_string(id) +
      "|lits=";
  if (clause) {
    std::vector<int> lits;
    for (const Lit lit : *clause)
      lits.push_back(kernel_externalize(k, lit));
    std::sort(lits.begin(), lits.end());
    for (size_t i = 0; i < lits.size(); i++) {
      if (i)
        payload += ',';
      payload += std::to_string(lits[i]);
    }
  } else {
    payload += std::to_string(kernel_externalize(k, selected_lit));
    payload += ',';
    payload += std::to_string(kernel_externalize(k, other));
  }
  emit(k, "factor_literal_entry", payload.c_str(), false);
}

void event_trace_factor_literal_entry(const Kernel &k, Lit lit,
                                      uint32_t ordinal, bool live, Lit other,
                                      uint64_t id, const Clause *clause) {
  if (active_trace)
    active_trace->factor_literal_entry(k, lit, ordinal, live, other, id,
                                       clause);
}

void EventTrace::factor_candidate_source(const Kernel &k, Lit seed,
                                         uint32_t depth, uint32_t column,
                                         Lit scan, Lit candidate,
                                         const Clause *current,
                                         const Clause *matched) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
  if (!selected || std::atoi(selected) != kernel_externalize(k, seed))
    return;
  std::string payload =
      "seed=" + std::to_string(kernel_externalize(k, seed)) +
      "|depth=" + std::to_string(depth) +
      "|column=" + std::to_string(column) +
      "|scan=" + std::to_string(kernel_externalize(k, scan)) +
      "|candidate=" + std::to_string(kernel_externalize(k, candidate)) +
      "|current=";
  std::vector<int> lits;
  for (const Lit lit : *current)
    lits.push_back(kernel_externalize(k, lit));
  std::sort(lits.begin(), lits.end());
  for (size_t i = 0; i < lits.size(); i++) {
    if (i)
      payload += ',';
    payload += std::to_string(lits[i]);
  }
  payload += "|matched=";
  lits.clear();
  for (const Lit lit : *matched)
    lits.push_back(kernel_externalize(k, lit));
  std::sort(lits.begin(), lits.end());
  for (size_t i = 0; i < lits.size(); i++) {
    if (i)
      payload += ',';
    payload += std::to_string(lits[i]);
  }
  emit(k, "factor_candidate_source", payload.c_str(), false);
}

void event_trace_factor_candidate_source(const Kernel &k, Lit seed,
                                         uint32_t depth, uint32_t column,
                                         Lit scan, Lit candidate,
                                         const Clause *current,
                                         const Clause *matched) {
  if (active_trace)
    active_trace->factor_candidate_source(k, seed, depth, column, scan,
                                          candidate, current, matched);
}

void EventTrace::factor_candidate_binary_source(const Kernel &k, Lit seed,
                                                uint32_t depth,
                                                uint32_t column, Lit q,
                                                Lit candidate) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
  if (!selected || std::atoi(selected) != kernel_externalize(k, seed))
    return;
  const std::string payload =
      "seed=" + std::to_string(kernel_externalize(k, seed)) +
      "|depth=" + std::to_string(depth) +
      "|column=" + std::to_string(column) +
      "|q=" + std::to_string(kernel_externalize(k, q)) +
      "|candidate=" + std::to_string(kernel_externalize(k, candidate));
  emit(k, "factor_candidate_binary_source", payload.c_str(), false);
}

void event_trace_factor_candidate_binary_source(const Kernel &k, Lit seed,
                                                uint32_t depth,
                                                uint32_t column, Lit q,
                                                Lit candidate) {
  if (active_trace)
    active_trace->factor_candidate_binary_source(k, seed, depth, column, q,
                                                 candidate);
}

void EventTrace::factor_pair(const Kernel &k, Lit grid_seed,
                             const char *stage) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_PAIR");
  int external_a = 0, external_b = 0;
  if (!selected || std::sscanf(selected, "%d,%d", &external_a, &external_b) !=
                       2)
    return;
  Lit a = INVALID_LIT, b = INVALID_LIT;
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++) {
    const int external = kernel_externalize(k, lit);
    if (external == external_a)
      a = lit;
    if (external == external_b)
      b = lit;
  }
  uint32_t copies = 0;
  if (a != INVALID_LIT && b != INVALID_LIT)
    for (const WatchEntry watch : k.watches.entries(a))
      copies += watch.binary() && watch.other() == b;
  const std::string payload =
      "grid=" + std::to_string(kernel_externalize(k, grid_seed)) +
      "|stage=" + stage + "|a=" + std::to_string(external_a) +
      "|b=" + std::to_string(external_b) +
      "|copies=" + std::to_string(copies) +
      "|value_a=" +
      std::to_string(a == INVALID_LIT ? 2 : static_cast<int>(k.value(a))) +
      "|value_b=" +
      std::to_string(b == INVALID_LIT ? 2 : static_cast<int>(k.value(b)));
  emit(k, "factor_pair", payload.c_str(), false);
}

void event_trace_factor_pair(const Kernel &k, Lit grid_seed,
                             const char *stage) {
  if (active_trace)
    active_trace->factor_pair(k, grid_seed, stage);
}

void EventTrace::factor_heap(const Kernel &k, Lit grid_seed,
                             const char *stage, Lit a, uint32_t pos_a,
                             uint32_t score_a, Lit b, uint32_t pos_b,
                             uint32_t score_b, uint32_t size) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const int grid = grid_seed == INVALID_LIT ? 0
                                            : kernel_externalize(k, grid_seed);
  const std::string payload =
      "grid=" + std::to_string(grid) + "|stage=" + stage +
      "|a=" + std::to_string(kernel_externalize(k, a)) +
      "|pos_a=" +
      std::to_string(pos_a == UINT32_MAX ? -1 : static_cast<int64_t>(pos_a)) +
      "|score_a=" + std::to_string(score_a) +
      "|b=" + std::to_string(kernel_externalize(k, b)) +
      "|pos_b=" +
      std::to_string(pos_b == UINT32_MAX ? -1 : static_cast<int64_t>(pos_b)) +
      "|score_b=" + std::to_string(score_b) +
      "|size=" + std::to_string(size);
  emit(k, "factor_heap", payload.c_str(), false);
}

void event_trace_factor_heap(const Kernel &k, Lit grid_seed,
                             const char *stage, Lit a, uint32_t pos_a,
                             uint32_t score_a, Lit b, uint32_t pos_b,
                             uint32_t score_b, uint32_t size) {
  if (active_trace)
    active_trace->factor_heap(k, grid_seed, stage, a, pos_a, score_a, b,
                              pos_b, score_b, size);
}

void EventTrace::factor_schedule(const Kernel &k, uint32_t ordinal, Lit lit,
                                 uint32_t score) {
  if (!want_detail(k) || emitted_ >= limit_ ||
      !std::getenv("TACTIX_EVENT_TRACE_FACTOR_SCHEDULE"))
    return;
  const std::string payload =
      "ordinal=" + std::to_string(ordinal) +
      "|internal=" + std::to_string(lit) +
      "|external=" + std::to_string(kernel_externalize(k, lit)) +
      "|score=" + std::to_string(score);
  emit(k, "factor_schedule", payload.c_str(), false);
}

void event_trace_factor_schedule(const Kernel &k, uint32_t ordinal, Lit lit,
                                 uint32_t score) {
  if (active_trace)
    active_trace->factor_schedule(k, ordinal, lit, score);
}

void EventTrace::factor_update(const Kernel &k, Lit grid_seed,
                               uint32_t ordinal, Lit lit, uint32_t score) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_UPDATE");
  if (!selected || std::atoi(selected) != kernel_externalize(k, grid_seed))
    return;
  const std::string payload =
      "grid=" + std::to_string(kernel_externalize(k, grid_seed)) +
      "|ordinal=" + std::to_string(ordinal) +
      "|external=" + std::to_string(kernel_externalize(k, lit)) +
      "|score=" + std::to_string(score);
  emit(k, "factor_update", payload.c_str(), false);
}

void event_trace_factor_update(const Kernel &k, Lit grid_seed,
                               uint32_t ordinal, Lit lit, uint32_t score) {
  if (active_trace)
    active_trace->factor_update(k, grid_seed, ordinal, lit, score);
}

void EventTrace::factor_grid(const Kernel &k, Lit seed, uint32_t factors,
                             uint32_t remainders, int delta) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" + std::to_string(kernel_externalize(k, seed)) +
      "|factors=" + std::to_string(factors) +
      "|remainders=" + std::to_string(remainders) +
      "|delta=" + std::to_string(delta);
  emit(k, "factor_grid", payload.c_str(), false);
}

void event_trace_factor_grid(const Kernel &k, Lit seed, uint32_t factors,
                             uint32_t remainders, int delta) {
  if (active_trace)
    active_trace->factor_grid(k, seed, factors, remainders, delta);
}

void EventTrace::factor_grid_member(const Kernel &k, uint32_t ordinal,
                                    Lit factor) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "ordinal=" + std::to_string(ordinal) +
      "|external=" + std::to_string(kernel_externalize(k, factor));
  emit(k, "factor_grid_member", payload.c_str(), false);
}

void event_trace_factor_grid_member(const Kernel &k, uint32_t ordinal,
                                    Lit factor) {
  if (active_trace)
    active_trace->factor_grid_member(k, ordinal, factor);
}

void EventTrace::bve_candidate(const Kernel &k, Var var) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" + std::to_string(k.internal_to_external[var] + 1);
  emit(k, "bve_candidate", payload.c_str(), false);
}

void event_trace_bve_candidate(const Kernel &k, Var var) {
  if (active_trace)
    active_trace->bve_candidate(k, var);
}

void EventTrace::analyzed(const Kernel &k, const std::vector<Var> &analyzed,
                          const std::vector<uint8_t> &seen,
                          uint8_t bump_mask) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = "vars=";
  char score[64];
  bool first = true;
  std::vector<uint8_t> emitted(k.nvars);
  for (const Var v : analyzed) {
    if (!(seen[v] & bump_mask) || emitted[v])
      continue;
    emitted[v] = 1;
    if (!first)
      payload += ',';
    first = false;
    payload += std::to_string(kernel_externalize(k, make_lit(v, false)));
    payload += ':';
    std::snprintf(score, sizeof score, "%.17g", k.h->scores.score[v]);
    payload += score;
    payload += ':';
    payload += heuristics_heap_contains(k, v)
                   ? std::to_string(k.h->scores.pos[v])
                   : "-";
  }
  emit(k, "analyzed", payload.c_str(), false);
}

void EventTrace::analyzed_post(const Kernel &k,
                               const std::vector<Var> &analyzed) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = "vars=";
  char score[64];
  for (size_t i = 0; i < analyzed.size(); i++) {
    const Var v = analyzed[i];
    if (i) payload += ',';
    payload += std::to_string(kernel_externalize(k, make_lit(v, false)));
    payload += ':';
    std::snprintf(score, sizeof score, "%.17g", k.h->scores.score[v]);
    payload += score;
    payload += ':';
    payload += heuristics_heap_contains(k, v)
                   ? std::to_string(k.h->scores.pos[v]) : "-";
  }
  emit(k, "analyzed_post", payload.c_str(), false);
}

void EventTrace::heap_scores(const Kernel &k) {
  if (!want_detail(k) || !k.h || !k.h->stable)
    return;
  uint64_t stack_hash = 1469598103934665603ull;
  for (const Var v : k.h->scores.stack) {
    const uint64_t ext = static_cast<uint64_t>(k.internal_to_external[v]) + 1;
    stack_hash ^= ext;
    stack_hash *= 1099511628211ull;
  }
  char hash[32];
  std::snprintf(hash, sizeof hash, "hash=%016llx|",
                (unsigned long long)stack_hash);
  std::string payload(hash);
  char score[64];
  const size_t size = std::min<size_t>(32, k.h->scores.stack.size());
  for (size_t i = 0; i < size; i++) {
    const Var v = k.h->scores.stack[i];
    if (i)
      payload += ',';
    payload += std::to_string(kernel_externalize(k, make_lit(v, false)));
    payload += ':';
    std::snprintf(score, sizeof score, "%.17g", k.h->scores.score[v]);
    payload += score;
  }
  emit(k, "heap_scores", payload.c_str(), false);
}

void EventTrace::mode_clock(const Kernel &k, const char *stage, uint64_t limit,
                            uint64_t anchor, uint64_t count) {
  if (!std::getenv("TACTIX_EVENT_TRACE_MODE_CLOCK") || !want_detail(k))
    return;
  const std::string payload =
      "stage=" + std::string(stage) +
      "|ticks=" + std::to_string(k.stats.search_ticks) +
      "|limit=" + std::to_string(limit) + "|anchor=" +
      std::to_string(anchor) + "|count=" + std::to_string(count);
  emit(k, "mode_clock", payload.c_str(), false);
}

void event_trace_analyzed(const Kernel &k, const std::vector<Var> &analyzed,
                          const std::vector<uint8_t> &seen,
                          uint8_t bump_mask) {
  if (active_trace)
    active_trace->analyzed(k, analyzed, seen, bump_mask);
}

void event_trace_analyzed_post(const Kernel &k,
                               const std::vector<Var> &analyzed) {
  if (active_trace)
    active_trace->analyzed_post(k, analyzed);
}

void event_trace_heap_scores(const Kernel &k) {
  if (active_trace)
    active_trace->heap_scores(k);
}

void event_trace_root_assign(const Kernel &k, Lit lit) {
  if (active_trace)
    active_trace->detail(k, "root_assign", kernel_externalize(k, lit));
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
  payload += "|levels=";
  for (uint32_t i = 0; i < size; i++) {
    if (i)
      payload += ',';
    payload += std::to_string(kernel_externalize(k, lits[i]));
    payload += ':';
    payload += std::to_string(k.var_level(var_of(lits[i])));
  }
  emit(k, "conflict", payload.c_str(), false);
  if (const char *selected = std::getenv("TACTIX_EVENT_TRACE_ASSIGNMENT")) {
    const unsigned ext = static_cast<unsigned>(std::strtoul(selected, nullptr, 10));
    if (ext && ext <= k.external_to_internal.size()) {
      const Var v = k.external_to_internal[ext - 1];
      if (v < k.nvars && k.values[make_lit(v, false)] != 0) {
        const Assigned &a = k.assigned[v];
        std::string state = "external=" + std::to_string(ext) +
                            "|level=" + std::to_string(a.level) +
                            "|trail=" + std::to_string(a.trail) +
                            "|binary=" + std::to_string(a.binary) +
                            "|jumped=" + std::to_string(a.jumped) +
                            "|reason=";
        if (a.reason == DECISION_REASON)
          state += "decision";
        else if (a.reason == UNIT_REASON)
          state += "unit";
        else if (a.binary)
          state += std::to_string(kernel_externalize(k, a.reason));
        else
          state += "clause:" +
                   std::to_string(clause_at(k.arena, a.reason)->id());
        emit(k, "assignment", state.c_str(), false);
      }
    }
  }
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

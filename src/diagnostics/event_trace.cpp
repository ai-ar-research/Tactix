#include "diagnostics/event_trace.hpp"

#ifdef TACTIX_ENABLE_EVENT_TRACE

#include "policies/heuristics.hpp"
#include "rules/bve/bve.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/resource.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace tactix {
namespace {

EventTrace *active_trace;

template <class T> size_t capacity_bytes(const std::vector<T> &values) {
  return values.capacity() * sizeof(T);
}

struct DenseStorage {
  size_t capacity_bytes = 0;
  size_t usable_bytes = 0;
  size_t allocations = 0;
};

DenseStorage dense_storage(const OccIndex *dense) {
  DenseStorage result;
  if (!dense)
    return result;
  result.capacity_bytes = capacity_bytes(dense->large);
  if (dense->large.data()) {
    result.allocations++;
#if defined(__GLIBC__)
    result.usable_bytes +=
        malloc_usable_size(const_cast<void *>(
            static_cast<const void *>(dense->large.data())));
#else
    result.usable_bytes += capacity_bytes(dense->large);
#endif
  }
  for (const auto &list : dense->large) {
    result.capacity_bytes += capacity_bytes(list);
    if (!list.data())
      continue;
    result.allocations++;
#if defined(__GLIBC__)
    result.usable_bytes += malloc_usable_size(
        const_cast<void *>(static_cast<const void *>(list.data())));
#else
    result.usable_bytes += capacity_bytes(list);
#endif
  }
  return result;
}

void trace_slot_memory(const Kernel &k, const char *name, const char *stage) {
  static bool initialized = false;
  static FILE *out = nullptr;
  if (!initialized) {
    initialized = true;
    const char *path = std::getenv("TACTIX_SLOT_MEMORY_TRACE");
    if (path && *path) {
      out = std::fopen(path, "w");
      if (out)
        std::setvbuf(out, nullptr, _IOLBF, 0);
    }
  }
  if (!out)
    return;
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  size_t heap_used_kib = 0, heap_free_kib = 0, mmap_kib = 0;
#if defined(__GLIBC__)
  const struct mallinfo2 allocator = mallinfo2();
  heap_used_kib = static_cast<size_t>(allocator.uordblks) / 1024;
  heap_free_kib = static_cast<size_t>(allocator.fordblks) / 1024;
  mmap_kib = static_cast<size_t>(allocator.hblkhd) / 1024;
#endif
  const size_t identity_bytes =
      capacity_bytes(k.internal_to_external) +
      capacity_bytes(k.external_to_internal) +
      capacity_bytes(k.compact_import_order) +
      capacity_bytes(k.compact_import_position) +
      capacity_bytes(k.external_import_rank) +
      capacity_bytes(k.internal_import_rank) +
      capacity_bytes(k.import_rank_to_external) + capacity_bytes(k.var_status);
  const size_t search_bytes =
      capacity_bytes(k.values) + capacity_bytes(k.assigned) +
      capacity_bytes(k.jumped_over) + k.trail.capacity() * sizeof(Lit) +
      capacity_bytes(k.frames) + capacity_bytes(k.model_values) +
      capacity_bytes(k.pending.lits) + capacity_bytes(k.seen) +
      capacity_bytes(k.analyzed) + capacity_bytes(k.bump_order) +
      capacity_bytes(k.shrinkable) + capacity_bytes(k.reason_bumped) +
      capacity_bytes(k.minimize_scratch) + capacity_bytes(k.learned_scratch) +
      capacity_bytes(k.otfs_scratch) + capacity_bytes(k.otfs_retired) +
      capacity_bytes(k.analysis_conflict_checkpoints) +
      capacity_bytes(k.radix_scratch) + capacity_bytes(k.marked_levels) +
      capacity_bytes(k.touched_levels) + capacity_bytes(k.eager_marks) +
      capacity_bytes(k.reason_walk_stack) + capacity_bytes(k.jump_pending) +
      capacity_bytes(k.sweep_tail);
  const size_t input_bytes =
      capacity_bytes(k.input_restatements) +
      capacity_bytes(k.input_restate_lits) +
      capacity_bytes(k.input_restate_units) +
      capacity_bytes(k.input_activation_order) +
      capacity_bytes(k.input_activated) +
      capacity_bytes(k.input_refutation_units);
  const size_t touch_bytes =
      capacity_bytes(k.bve_retry) + capacity_bytes(k.factor_marks) +
      capacity_bytes(k.backbone_priority) + capacity_bytes(k.touched) +
      capacity_bytes(k.removed) + capacity_bytes(k.touched_kept) +
      capacity_bytes(k.bve_min_bound);
  const size_t event_bytes =
      capacity_bytes(k.retired_events) + capacity_bytes(k.substituted_events) +
      capacity_bytes(k.fresh_var_events);
  const size_t probing_bytes =
      capacity_bytes(k.probing.lists) + capacity_bytes(k.probing.words) +
      capacity_bytes(k.probing.stamp) + capacity_bytes(k.probing.clauses);
  const size_t proof_index_bytes =
      capacity_bytes(k.unit_ids) + k.binary_ids.diagnostic_storage_bytes() +
      capacity_bytes(k.lrat_entries) + capacity_bytes(k.lrat_chain);
  size_t heuristics_bytes = 0;
  if (k.h) {
    heuristics_bytes =
        capacity_bytes(k.h->saved) + capacity_bytes(k.h->links) +
        capacity_bytes(k.h->scores.stack) + capacity_bytes(k.h->scores.pos) +
        capacity_bytes(k.h->scores.score) + capacity_bytes(k.h->target) +
        capacity_bytes(k.h->best) + capacity_bytes(k.h->walk_prev);
  }
  const size_t clause_alias_bytes =
      k.clause_position_alias.diagnostic_storage_bytes();
  const size_t binary_liveness_bytes =
      k.binary_liveness ? k.binary_liveness->diagnostic_storage_bytes() : 0;
  const DenseStorage dense = dense_storage(k.dense);
  unsigned long vm_kib = 0, current_rss_kib = 0;
  if (FILE *status = std::fopen("/proc/self/status", "r")) {
    char line[256];
    while (std::fgets(line, sizeof line, status)) {
      std::sscanf(line, "VmSize: %lu kB", &vm_kib);
      std::sscanf(line, "VmRSS: %lu kB", &current_rss_kib);
    }
    std::fclose(status);
  }
  std::fprintf(out,
               "%s\t%s\tmax_rss_kib=%ld\tcurrent_rss_kib=%lu\t"
               "vm_kib=%lu\tvars=%u\tarena_words=%zu\tarena_capacity=%zu\t"
               "watch_words=%zu\twatch_capacity=%zu\twatch_heads_capacity=%zu\t"
               "reconstruction_words=%zu\treconstruction_capacity=%zu\t"
               "identity_capacity_bytes=%zu\tsearch_capacity_bytes=%zu\t"
               "input_capacity_bytes=%zu\ttouch_capacity_bytes=%zu\t"
               "event_capacity_bytes=%zu\tprobing_capacity_bytes=%zu\t"
               "proof_index_capacity_bytes=%zu\theuristics_capacity_bytes=%zu\t"
               "clause_alias_capacity_bytes=%zu\t"
               "binary_liveness_capacity_bytes=%zu\tdense_capacity_bytes=%zu\t"
               "dense_usable_bytes=%zu\tdense_allocations=%zu\t"
               "policy_reduce_capacity_bytes=%zu\t"
               "policy_inprocess_capacity_bytes=%zu\t"
               "delayed_capacity_bytes=%zu\t"
               "heap_used_kib=%zu\theap_free_kib=%zu\tmmap_kib=%zu\n",
               name, stage, usage.ru_maxrss, current_rss_kib, vm_kib, k.nvars,
               k.arena.size(), k.arena.capacity(), k.watches.arena.size(),
               k.watches.arena.capacity(), k.watches.heads.capacity(),
               k.reconstruction.size(), k.reconstruction.capacity(),
               identity_bytes, search_bytes, input_bytes, touch_bytes,
               event_bytes, probing_bytes, proof_index_bytes, heuristics_bytes,
               clause_alias_bytes, binary_liveness_bytes,
               dense.capacity_bytes, dense.usable_bytes, dense.allocations,
               k.diagnostic_policy_reduce_bytes,
               k.diagnostic_policy_inprocess_bytes,
               capacity_bytes(k.delayed),
               heap_used_kib, heap_free_kib, mmap_kib);
}

struct BinaryLifecycleState {
  bool initialized = false;
  FILE *out = nullptr;
  std::vector<std::pair<int, int>> selected;
  uint64_t sequence = 0;
};
BinaryLifecycleState binary_lifecycle;
thread_local const char *binary_lifecycle_rule = "-";

struct BinarySetSnapshotState {
  bool initialized = false;
  FILE *out = nullptr;
  uint64_t sequence = 0;
};
BinarySetSnapshotState binary_set_snapshot;

void open_binary_lifecycle() {
  if (binary_lifecycle.initialized)
    return;
  binary_lifecycle.initialized = true;
  const char *path = std::getenv("TACTIX_BINARY_LIFECYCLE_TRACE");
  const char *spec = std::getenv("TACTIX_BINARY_LIFECYCLE_PAIRS");
  if (!path || !*path || !spec || !*spec)
    return;
  const char *cursor = spec;
  while (*cursor) {
    char *end = nullptr;
    const long first = std::strtol(cursor, &end, 10);
    if (end == cursor || *end != ',')
      return;
    cursor = end + 1;
    const long second = std::strtol(cursor, &end, 10);
    if (end == cursor || (*end && *end != ';'))
      return;
    int a = static_cast<int>(first), b = static_cast<int>(second);
    if (b < a)
      std::swap(a, b);
    binary_lifecycle.selected.emplace_back(a, b);
    cursor = *end ? end + 1 : end;
  }
  binary_lifecycle.out = std::fopen(path, "w");
  if (!binary_lifecycle.out)
    return;
  std::setvbuf(binary_lifecycle.out, nullptr, _IOLBF, 0);
  std::fputs("# tactix-binary-lifecycle\t3\n", binary_lifecycle.out);
  std::fputs("# solver\tseq\tevent\trule\tconflicts\tsearch_ticks\t"
             "probing_ticks\ta\tb\tvalue_a\tvalue_b\tid\tcopies\tpayload\n",
             binary_lifecycle.out);
}

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
    // Kissat's internal indices are assigned in first-import order.  Tactix
    // keeps public identities stable and stores that permutation separately,
    // so comparing the current dense indices fabricates a ranking divergence
    // whenever a large bump makes scores equal at double precision.
    return kernel_import_var_order(k, a) < kernel_import_var_order(k, b);
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
    ref += clause_actual_words(c);
    if (c->garbage())
      continue;
    (c->redundant() ? redundant : irredundant)++;
  }
}

void annotate_clause_set(const Kernel &k, std::string &payload) {
  const char *spec = std::getenv("SAT_EVENT_TRACE_CLAUSE_SET");
  if (!spec || !*spec)
    return;
  const char *conflict_from =
      std::getenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_FROM");
  if (conflict_from && *conflict_from) {
    char *end = nullptr;
    const unsigned long long limit = std::strtoull(conflict_from, &end, 10);
    if (end != conflict_from && !*end && k.stats.conflicts < limit)
      return;
  }
  const char *conflict_to =
      std::getenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_TO");
  if (conflict_to && *conflict_to) {
    char *end = nullptr;
    const unsigned long long limit = std::strtoull(conflict_to, &end, 10);
    if (end != conflict_to && !*end && k.stats.conflicts > limit)
      return;
  }
  std::vector<int> selected;
  const char *cursor = spec;
  while (*cursor) {
    if (selected.size() == 64)
      return;
    char *end = nullptr;
    const long value = std::strtol(cursor, &end, 10);
    if (end == cursor)
      return;
    selected.push_back(static_cast<int>(value));
    cursor = end;
    if (*cursor == ',')
      cursor++;
    else if (*cursor)
      return;
  }
  std::sort(selected.begin(), selected.end());
  payload += "|clause_set=" + std::string(spec) + "|clause_poses=";
  size_t matches = 0;
  std::vector<uint8_t> matched(selected.size());
  for (Ref ref = 0; ref < k.arena.size();) {
    const Ref clause_ref = ref;
    const Clause *clause = clause_at(k.arena, ref);
    ref += clause_actual_words(clause);
    if (clause->garbage() || clause->size != selected.size())
      continue;
    std::fill(matched.begin(), matched.end(), 0);
    bool same = true;
    for (const Lit lit : *clause) {
      const int external = kernel_externalize(k, lit);
      size_t i = 0;
      while (i != selected.size() &&
             (matched[i] || selected[i] != external))
        i++;
      if (i == selected.size()) {
        same = false;
        break;
      }
      matched[i] = 1;
    }
    if (!same)
      continue;
    if (matches++)
      payload.push_back(';');
    payload += "id=" + std::to_string(clause->id()) + ",ref=" +
               std::to_string(clause_ref) + ",order=" +
               std::to_string(kernel_clause_position(k, clause_ref)) +
               ",searched=" + std::to_string(clause->searched) +
               ",redundant=" + std::to_string(clause->redundant()) +
               ",glue=" + std::to_string(clause->glue()) +
               ",used=" + std::to_string(clause->used()) +
               ",priority=" + std::to_string(clause->vivify()) +
               ",pose=";
    size_t i = 0;
    for (const Lit lit : *clause) {
      if (i++)
        payload.push_back(',');
      payload += std::to_string(kernel_externalize(k, lit));
    }
  }
  if (!matches)
    payload.push_back('-');
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

void emit_binary_set_snapshot(const Kernel &k, const char *event,
                              const char *payload) {
  const char *path = std::getenv("SAT_EVENT_TRACE_BINARY_SET_PATH");
  const char *match = std::getenv("SAT_EVENT_TRACE_BINARY_SET_MATCH");
  const char *conflict = std::getenv("SAT_EVENT_TRACE_BINARY_SET_CONFLICT");
  if (!path || !*path || !match || !*match || !conflict || !*conflict)
    return;
  char *end = nullptr;
  const uint64_t selected = std::strtoull(conflict, &end, 10);
  if (end == conflict || *end || selected != k.stats.conflicts)
    return;
  const std::string selector =
      std::string(event) + "|" + (payload ? payload : "-");
  if (selector.find(match) == std::string::npos)
    return;
  if (!binary_set_snapshot.initialized) {
    binary_set_snapshot.initialized = true;
    binary_set_snapshot.out = std::fopen(path, "w");
    if (!binary_set_snapshot.out)
      return;
    std::fputs("# tactix-binary-set-snapshot\t1\n", binary_set_snapshot.out);
    std::fputs("# snapshot\ta\tb\n", binary_set_snapshot.out);
  }
  if (!binary_set_snapshot.out)
    return;
  std::vector<std::pair<int, int>> pairs;
  pairs.reserve(live_binary_clause_count(k));
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); ++lit)
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary() || lit >= watch.other())
        continue;
      int a = kernel_externalize(k, lit);
      int b = kernel_externalize(k, watch.other());
      if (b < a)
        std::swap(a, b);
      pairs.emplace_back(a, b);
    }
  std::sort(pairs.begin(), pairs.end());
  const uint64_t snapshot = binary_set_snapshot.sequence++;
  std::fprintf(binary_set_snapshot.out,
               "# begin\t%llu\ttactix\t%s\t%llu\t%zu\t%s\n",
               (unsigned long long)snapshot, event,
               (unsigned long long)k.stats.conflicts, pairs.size(),
               payload ? payload : "-");
  for (const auto &pair : pairs)
    std::fprintf(binary_set_snapshot.out, "%llu\t%d\t%d\n",
                 (unsigned long long)snapshot, pair.first, pair.second);
  std::fprintf(binary_set_snapshot.out, "# end\t%llu\n",
               (unsigned long long)snapshot);
  std::fflush(binary_set_snapshot.out);
}

} // namespace

static uint64_t eliminate_cleanup_clause_hash(const Kernel &, const Clause &);

void emit_formula_set_snapshot(const Kernel &k, const char *event,
                               const char *payload) {
  static bool dumped = false;
  if (dumped)
    return;
  const char *path = std::getenv("SAT_EVENT_TRACE_FORMULA_SET_PATH");
  const char *match = std::getenv("SAT_EVENT_TRACE_FORMULA_SET_MATCH");
  const char *conflict = std::getenv("SAT_EVENT_TRACE_FORMULA_SET_CONFLICT");
  if (!path || !*path || !match || !*match || !conflict || !*conflict)
    return;
  char *end = nullptr;
  const uint64_t selected = std::strtoull(conflict, &end, 10);
  if (end == conflict || *end || selected != k.stats.conflicts)
    return;
  const std::string selector =
      std::string(event) + "|" + (payload ? payload : "-");
  if (selector.find(match) == std::string::npos)
    return;
  FILE *out = std::fopen(path, "w");
  if (!out)
    return;
  std::fprintf(out, "# tactix-formula-set\t2\n");
  std::fprintf(out, "# redundant\tsize\thash\tliterals\n");
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      std::vector<int32_t> external;
      external.reserve(clause->size);
      for (const Lit lit : *clause)
        external.push_back(kernel_externalize(k, lit));
      std::sort(external.begin(), external.end());
      std::fprintf(out, "%u\t%u\t%016llx\t", clause->redundant(),
                   clause->size,
                   (unsigned long long)eliminate_cleanup_clause_hash(
                       k, *clause));
      for (size_t i = 0; i != external.size(); ++i)
        std::fprintf(out, "%s%d", i ? "," : "", external[i]);
      std::fputc('\n', out);
    }
    ref += clause_actual_words(clause);
  }
  std::fclose(out);
  dumped = true;
}

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
  if (const char *stop =
          std::getenv("TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT"))
    stop_after_conflict_ = std::strtoull(stop, nullptr, 10);
  if (const char *decision =
          std::getenv("TACTIX_EVENT_TRACE_HEAP_FROM_DECISION"))
    heap_from_decision_ = std::strtoull(decision, nullptr, 10);
  if (const char *decision =
          std::getenv("TACTIX_EVENT_TRACE_HEAP_POP_DECISION"))
    heap_pop_decision_ = std::strtoull(decision, nullptr, 10);
  if (const char *decision =
          std::getenv("TACTIX_EVENT_TRACE_HEAP_REMOVE_DECISION"))
    heap_remove_decision_ = std::strtoull(decision, nullptr, 10);
  compact_probe_ = std::getenv("TACTIX_EVENT_TRACE_COMPACT_PROBE") != nullptr;
  skip_watch_ = std::getenv("TACTIX_EVENT_TRACE_SKIP_WATCH") != nullptr;
  skip_imply_ = std::getenv("TACTIX_EVENT_TRACE_SKIP_IMPLY") != nullptr;
  skip_heap_ = std::getenv("TACTIX_EVENT_TRACE_SKIP_HEAP") != nullptr;
  heap_each_bump_ =
      std::getenv("TACTIX_EVENT_TRACE_HEAP_EACH_BUMP") != nullptr;
  heap_pop_only_ =
      std::getenv("TACTIX_EVENT_TRACE_HEAP_POP_ONLY") != nullptr;
  skip_decisions_ =
      std::getenv("TACTIX_EVENT_TRACE_SKIP_DECISIONS") != nullptr;
  skip_randec_draws_ =
      std::getenv("TACTIX_EVENT_TRACE_SKIP_RANDEC_DRAWS") != nullptr;
  skip_analysis_ticks_ =
      std::getenv("TACTIX_EVENT_TRACE_SKIP_ANALYSIS_TICKS") != nullptr;
  keep_snapshots_ =
      std::getenv("TACTIX_EVENT_TRACE_KEEP_SNAPSHOTS") != nullptr;
  search_only_ = std::getenv("TACTIX_EVENT_TRACE_SEARCH_ONLY") != nullptr;
  control_only_ = std::getenv("TACTIX_EVENT_TRACE_CONTROL_ONLY") != nullptr;
  glue_only_ = std::getenv("TACTIX_EVENT_TRACE_GLUE_ONLY") != nullptr;
  phase_only_ = std::getenv("TACTIX_EVENT_TRACE_PHASE_ONLY") != nullptr;
  formula_only_ =
      std::getenv("TACTIX_EVENT_TRACE_FORMULA_ONLY") != nullptr;
  trajectory_only_ =
      std::getenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY") != nullptr;
  sweep_only_ = std::getenv("TACTIX_EVENT_TRACE_SWEEP_ONLY") != nullptr;
  watch_only_ = std::getenv("TACTIX_EVENT_TRACE_WATCH_ONLY") != nullptr;
  watch_cost_ = std::getenv("TACTIX_EVENT_TRACE_WATCH_COST") != nullptr;
  watch_within_vivify_ =
      std::getenv("TACTIX_EVENT_TRACE_WATCH_WITHIN_VIVIFY") != nullptr;
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
  if (snapshot)
    emit_binary_set_snapshot(k, event, payload);
  if (snapshot)
    emit_formula_set_snapshot(k, event, payload);
  if (!out_)
    return;
  if (formula_only_ && std::strcmp(event, "trace_begin") &&
      std::strcmp(event, "formula_checkpoint") &&
      std::strcmp(event, "terminate"))
    return;
  if (heap_pop_only_ && std::strcmp(event, "heap_pop") &&
      std::strcmp(event, "heap_remove") &&
      std::strcmp(event, "heap_scores") && std::strcmp(event, "decision") &&
      std::strcmp(event, "analysis_end"))
    return;
  if (snapshot && !keep_snapshots_ && stop_after_conflict_ != UINT64_MAX &&
      k.stats.conflicts < stop_after_conflict_ &&
      std::strcmp(event, "analysis_end"))
    return;
  if (glue_only_ && std::strcmp(event, "reason_glue") &&
      std::strcmp(event, "reason_used") &&
      std::strcmp(event, "retier") &&
      std::strcmp(event, "analysis_end"))
    return;
  if (phase_only_ && std::strcmp(event, "trace_begin") &&
      std::strcmp(event, "target_state") &&
      std::strcmp(event, "phase_pick") &&
      std::strcmp(event, "phase_save") &&
      std::strcmp(event, "formula_checkpoint"))
    return;
  if (skip_watch_ && !std::strncmp(event, "watch", 5))
    return;
  if (skip_imply_ && !std::strcmp(event, "imply"))
    return;
  if (skip_heap_ && !std::strncmp(event, "heap_", 5) &&
      std::strcmp(event, "heap_reinsert"))
    return;
  if (skip_decisions_ &&
      (!std::strcmp(event, "decision") || !std::strcmp(event, "randec_draw") ||
       !std::strcmp(event, "randec_state") ||
       !std::strcmp(event, "phase_pick")))
    return;
  if (skip_randec_draws_ && !std::strcmp(event, "randec_draw"))
    return;
  if (skip_analysis_ticks_ && !std::strncmp(event, "analysis_tick", 13))
    return;
  if (watch_only_ && !snapshot && std::strcmp(event, "watch") &&
      std::strcmp(event, "watch_post") &&
      std::strcmp(event, "watch_post_summary") &&
      std::strcmp(event, "watch_cost") &&
      std::strcmp(event, "reason_glue") &&
      std::strcmp(event, "vivify_candidate") &&
      std::strcmp(event, "vivify_candidate_end") &&
      std::strcmp(event, "vivify_budget_stop"))
    return;
  if (search_only_ && !snapshot && std::strcmp(event, "decision") &&
      std::strcmp(event, "randec_draw") &&
      std::strcmp(event, "randec_state") &&
      std::strcmp(event, "phase_pick") &&
      std::strcmp(event, "score_bump") &&
      std::strcmp(event, "heap_insert") &&
      std::strcmp(event, "heap_remove") &&
      std::strcmp(event, "heap_reinsert") &&
      std::strcmp(event, "heap_scores") &&
      std::strcmp(event, "binary_assignment") &&
      std::strcmp(event, "binary_add") &&
      std::strcmp(event, "sweep_seed") &&
      std::strcmp(event, "sweep_environment") &&
      std::strcmp(event, "sweep_environment_clause") &&
      std::strcmp(event, "sweep_query") &&
      std::strcmp(event, "sweep_query_model") &&
      std::strcmp(event, "sweep_equivalence") &&
      std::strcmp(event, "sweep_commit") &&
      std::strcmp(event, "sweep_rewrite_binary") &&
      std::strcmp(event, "assignment") && std::strcmp(event, "watch") &&
      std::strcmp(event, "watch_post") &&
      std::strcmp(event, "watch_post_summary") &&
      std::strcmp(event, "imply") &&
      std::strcmp(event, "conflict") && std::strncmp(event, "analyzed", 8) &&
      std::strncmp(event, "analysis_", 9) &&
      std::strncmp(event, "reason_", 7) &&
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
      std::strcmp(event, "watch_post") &&
      std::strcmp(event, "watch_post_summary") &&
      std::strcmp(event, "conflict") &&
      std::strncmp(event, "analyzed", 8) &&
      std::strncmp(event, "analysis_", 9) &&
      std::strncmp(event, "learned", 7) &&
      std::strncmp(event, "factor_", 7) &&
      std::strncmp(event, "vivify_", 7) &&
      std::strncmp(event, "root_", 5) &&
      std::strncmp(event, "reason_", 7))
    return;
  // Minimal portable search trajectory.  Keep macro/snapshot boundaries and
  // only the choices plus learned-clause transaction needed to locate the
  // first semantic divergence before enabling the high-volume internals.
  if (sweep_only_ && !snapshot && std::strcmp(event, "sweep_seed") &&
      std::strcmp(event, "sweep_environment") &&
      std::strcmp(event, "sweep_environment_clause") &&
      std::strcmp(event, "sweep_environment_var") &&
      std::strcmp(event, "sweep_binary_attempt") &&
      std::strcmp(event, "sweep_effort") &&
      std::strcmp(event, "sweep_query") &&
      std::strcmp(event, "sweep_query_model") &&
      std::strcmp(event, "sweep_equivalence") &&
      std::strcmp(event, "sweep_commit") &&
      std::strcmp(event, "sweep_flip") &&
      std::strcmp(event, "sweep_rewrite_binary") &&
      std::strcmp(event, "sweep_rewrite_occurrence") &&
      std::strcmp(event, "sweep_watch") &&
      std::strcmp(event, "sweep_unit"))
    return;
  if (trajectory_only_ && !snapshot &&
      !(sweep_only_ && !std::strncmp(event, "sweep_", 6)) &&
      std::strcmp(event, "decision") &&
      std::strcmp(event, "randec_draw") &&
      std::strcmp(event, "randec_state") &&
      std::strcmp(event, "phase_pick") && std::strcmp(event, "conflict") &&
      std::strcmp(event, "mode_clock") &&
      std::strcmp(event, "restart_clock") &&
      std::strcmp(event, "analysis_begin") &&
      std::strcmp(event, "analysis_end") &&
      std::strncmp(event, "backbone_", 9) &&
      std::strncmp(event, "learned", 7))
    return;
  if (compact_probe_ && std::strcmp(event, "trace_begin") &&
      std::strcmp(event, "probe_begin") && std::strcmp(event, "probe_end") &&
      std::strcmp(event, "preprocess_begin") &&
      std::strcmp(event, "preprocess_end") &&
      std::strcmp(event, "slot_boundary") &&
      std::strncmp(event, "factor_", 7) &&
      std::strncmp(event, "bve_", 4) &&
      std::strncmp(event, "eliminate_", 10) &&
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
    if (std::getenv("TACTIX_EVENT_TRACE_FORMULA_MACRO")) {
      uint64_t formula_irredundant = 0, formula_redundant = 0;
      uint64_t formula_irredundant_hash = 0, formula_redundant_hash = 0;
      for (Ref ref = 0; ref < k.arena.size();) {
        const Clause *clause = clause_at(k.arena, ref);
        const uint32_t words = clause_actual_words(clause);
        if (!clause->garbage()) {
          const uint64_t hash = eliminate_cleanup_clause_hash(k, *clause);
          if (clause->redundant()) {
            formula_redundant++;
            formula_redundant_hash += hash;
          } else {
            formula_irredundant++;
            formula_irredundant_hash += hash;
          }
        }
        ref += words;
      }
      char formula[256];
      std::snprintf(
          formula, sizeof formula,
          "|formula_irredundant=%llu|formula_redundant=%llu|"
          "formula_irredundant_hash=%016llx|formula_redundant_hash=%016llx",
          (unsigned long long)formula_irredundant,
          (unsigned long long)formula_redundant,
          (unsigned long long)formula_irredundant_hash,
          (unsigned long long)formula_redundant_hash);
      annotated_payload += formula;
    }
    if (const char *selected =
            std::getenv("SAT_EVENT_TRACE_WATCH_LIST")) {
      const int wanted = std::atoi(selected);
      int target_a = 0, target_b = 0;
      const char *targets = std::getenv("SAT_EVENT_TRACE_WATCH_TARGETS");
      const bool compact_targets =
          targets && std::sscanf(targets, "%d,%d", &target_a, &target_b) == 2;
      Lit watched = INVALID_LIT;
      for (Lit lit = 0; lit < 2 * k.nvars; lit++)
        if (kernel_externalize(k, lit) == wanted) {
          watched = lit;
          break;
        }
      annotated_payload += "|watch_lit=" + std::to_string(wanted);
      if (std::getenv("SAT_EVENT_TRACE_WATCH_LARGE")) {
        uint64_t hash = 1469598103934665603ull;
        size_t count = 0;
        if (watched != INVALID_LIT)
          for (const WatchEntry entry : k.watches.entries(watched)) {
            if (entry.binary())
              continue;
            const Clause *clause = clause_at(k.arena, entry.ref());
            hash = mix(hash, static_cast<uint64_t>(static_cast<int64_t>(
                                 kernel_externalize(k, entry.blocking()))));
            hash = mix(hash, clause->size);
            for (const Lit lit : *clause)
              hash = mix(hash, static_cast<uint64_t>(static_cast<int64_t>(
                                   kernel_externalize(k, lit))));
            ++count;
          }
        char large[96];
        std::snprintf(large, sizeof large,
                      "|large_count=%zu|large_hash=%016llx", count,
                      (unsigned long long)hash);
        annotated_payload += large;
      }
      if (std::getenv("SAT_EVENT_TRACE_WATCH_LARGE_REFS")) {
        annotated_payload += "|large_refs=";
        size_t count = 0;
        if (watched != INVALID_LIT)
          for (const WatchEntry entry : k.watches.entries(watched)) {
            if (entry.binary())
              continue;
            if (count++)
              annotated_payload += ',';
            annotated_payload += std::to_string(entry.ref()) + '@' +
                                 std::to_string(
                                     kernel_clause_position(k, entry.ref()));
            if (count == 64)
              break;
          }
      }
      if (compact_targets) {
        uint64_t hash = 1469598103934665603ull;
        size_t count = 0, pos_a = 0, pos_b = 0;
        int lit_a = 0, lit_b = 0;
        if (watched != INVALID_LIT)
          for (const WatchEntry entry : k.watches.entries(watched)) {
            if (!entry.binary())
              continue;
            const int other = kernel_externalize(k, entry.other());
            hash = mix(hash, static_cast<uint64_t>(static_cast<int64_t>(other)));
            ++count;
            if (!pos_a && std::abs(other) == std::abs(target_a))
              pos_a = count, lit_a = other;
            if (!pos_b && std::abs(other) == std::abs(target_b))
              pos_b = count, lit_b = other;
          }
        char compact[256];
        std::snprintf(compact, sizeof compact,
                      "|binary_count=%zu|binary_hash=%016llx|target_a=%d"
                      "|target_a_pos=%zu|target_a_lit=%d|target_b=%d"
                      "|target_b_pos=%zu|target_b_lit=%d",
                      count, (unsigned long long)hash, target_a, pos_a, lit_a,
                      target_b, pos_b, lit_b);
        annotated_payload += compact;
      } else {
        annotated_payload += "|binary=";
        bool first = true;
        if (watched != INVALID_LIT)
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
    annotate_clause_set(k, annotated_payload);
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

void EventTrace::vivify_candidate(const Kernel &k, const char *event,
                                  uint64_t ordinal, Ref cref, uint64_t ticks,
                                  uint64_t limit) {
  bool selected = true;
  if (const char *from =
          std::getenv("TACTIX_EVENT_TRACE_VIVIFY_CANDIDATE_FROM"))
    if (ordinal < std::strtoull(from, nullptr, 10))
      selected = false;
  if (const char *to =
          std::getenv("TACTIX_EVENT_TRACE_VIVIFY_CANDIDATE_TO"))
    if (ordinal > std::strtoull(to, nullptr, 10))
      selected = false;
  const bool start = !std::strcmp(event, "vivify_candidate");
  const bool end = !std::strcmp(event, "vivify_candidate_end");
  if (watch_within_vivify_ && start) {
    vivify_watch_active_ = selected;
    vivify_watch_ordinal_ = ordinal;
  }
  if (!want_detail(k) || emitted_ >= limit_ || !selected) {
    if (watch_within_vivify_ && end)
      vivify_watch_active_ = false;
    return;
  }
  const Clause *clause = clause_at(k.arena, cref);
  std::vector<int> external;
  external.reserve(clause->size);
  for (const Lit lit : *clause)
    external.push_back(kernel_externalize(k, lit));
  std::sort(external.begin(), external.end());
  std::string payload = "ordinal=" + std::to_string(ordinal) +
                        "|ticks=" + std::to_string(ticks) +
                        "|limit=" + std::to_string(limit) +
                        "|size=" + std::to_string(clause->size) +
                        "|redundant=" + std::to_string(clause->redundant()) +
                        "|glue=" + std::to_string(clause->glue()) +
                        "|garbage=" + std::to_string(clause->garbage()) +
                        "|clause=";
  for (size_t i = 0; i != external.size(); ++i) {
    if (i)
      payload.push_back(',');
    payload += std::to_string(external[i]);
  }
  payload += "|pose=";
  for (uint32_t i = 0; i != clause->size; ++i) {
    if (i)
      payload.push_back(',');
    payload += std::to_string(kernel_externalize(k, clause->lits[i]));
  }
  payload += "|priority=" + std::to_string(clause->vivify()) +
             "|arena_order=" +
             std::to_string(kernel_clause_position(k, cref)) +
             "|ref=" + std::to_string(cref) + "|decisions=";
  for (uint32_t level = 1; level <= k.level; ++level) {
    if (level != 1)
      payload.push_back(',');
    payload +=
        std::to_string(kernel_externalize(k, k.trail[k.frames[level]]));
  }
  // Candidate-local propagation can mutate an unrelated watched clause before
  // the next macro snapshot. Reuse the selected semantic-clause observer at
  // each candidate boundary so that representation lineage remains visible.
  annotate_clause_set(k, payload);
  emit(k, event, payload.c_str(), false);
  if (watch_within_vivify_ && end)
    vivify_watch_active_ = false;
}

void EventTrace::vivify_counts(const Kernel &k, const uint32_t *counts,
                               size_t size) {
  const char *spec = std::getenv("SAT_EVENT_TRACE_VIVIFY_LITERALS");
  if (!spec || !*spec || !want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = "literals=";
  const char *cursor = spec;
  size_t selected = 0;
  while (*cursor && selected++ != 64) {
    char *end = nullptr;
    const long external = std::strtol(cursor, &end, 10);
    if (end == cursor || (*end && *end != ','))
      return;
    const Lit lit = kernel_internalize(k, static_cast<int>(external));
    if (payload.back() != '=')
      payload.push_back(',');
    payload += std::to_string(external) + ':';
    if (lit == INVALID_LIT || lit >= size) {
      payload += "absent";
    } else {
      payload += std::to_string(counts[lit]) + "@raw=" +
                 std::to_string(lit) + "@import=" +
                 std::to_string(kernel_import_lit_order(k, lit)) +
                 "@compact=" +
                 std::to_string(kernel_compact_import_lit_order(k, lit));
      if (std::getenv("SAT_EVENT_TRACE_VIVIFY_CLAUSES")) {
        for (Ref ref = 0; ref < k.arena.size();) {
          const Clause *clause = clause_at(k.arena, ref);
          ref += clause_actual_words(clause);
          if (clause->garbage() ||
              std::find(clause->begin(), clause->end(), lit) == clause->end())
            continue;
          std::vector<int> semantic;
          semantic.reserve(clause->size);
          std::string pose;
          for (const Lit member : *clause) {
            const int value = kernel_externalize(k, member);
            semantic.push_back(value);
            if (!pose.empty())
              pose.push_back(',');
            pose += std::to_string(value);
          }
          std::sort(semantic.begin(), semantic.end());
          uint64_t hash = UINT64_C(1469598103934665603);
          std::string set;
          for (const int value : semantic) {
            hash = mix(hash, static_cast<uint64_t>(static_cast<int64_t>(value)));
            if (!set.empty())
              set.push_back(',');
            set += std::to_string(value);
          }
          hash = mix(hash, clause->size);
          char hex[17];
          std::snprintf(hex, sizeof hex, "%016llx",
                        static_cast<unsigned long long>(hash));
          const std::string clause_payload =
              "literal=" + std::to_string(external) + "|hash=" + hex +
              "|size=" + std::to_string(clause->size) + "|redundant=" +
              std::to_string(clause->redundant()) + "|glue=" +
              std::to_string(clause->glue()) + "|clause=" + set +
              "|pose=" + pose;
          emit(k, "vivify_occurrence_clause", clause_payload.c_str(), false);
        }
      }
    }
    cursor = *end ? end + 1 : end;
  }
  if (*cursor)
    return;
  emit(k, "vivify_counts", payload.c_str(), false);
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
    ref += clause_actual_words(c);
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
    const Ref next = ref + clause_actual_words(c);
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
  if (formula_only_)
    return false;
  if (detail_window_)
    return k.stats.conflicts >= detail_conflict_from_ &&
           k.stats.conflicts <= detail_conflict_to_;
  return true;
}

void EventTrace::detail(const Kernel &k, const char *event, int64_t payload) {
  const bool analysis_end = !std::strcmp(event, "analysis_end");
  if (analysis_end)
    formula_checkpoint(k);
  const bool stop_here = analysis_end &&
                         stop_after_conflict_ != UINT64_MAX &&
                         k.stats.conflicts >= stop_after_conflict_;
  if (!stop_here && !want_detail(k))
    return;
  if (!detail_window_ && saw_reorder_) {
    if (!post_reorder_details_)
      return;
    post_reorder_details_--;
  }
  char buf[32];
  std::snprintf(buf, sizeof buf, "%lld", (long long)payload);
  // Analysis is the only detailed event that mutates the ranking.  Ordinary
  // detailed traces snapshot its post-state so a later decision mismatch can
  // be attributed to the exact conflict that changed VMTF/EVSIDS.  Compact
  // trajectory traces retain the boundary without repeatedly scanning and
  // sorting every variable on giant formulas; a localized follow-up can turn
  // the full snapshot back on by leaving trajectory-only mode.
  const bool snapshot = !trajectory_only_ && analysis_end;
  if (stop_here && k.h && k.h->stable)
    emit_heap_scores(k);
  emit(k, event, buf, snapshot);
  if (stop_here) {
    std::fflush(out_);
    std::raise(SIGTERM);
  }
}

static bool formula_checkpoint_selected(const char *spec, uint64_t conflict) {
  if (!spec || !*spec)
    return false;
  while (*spec) {
    char *end = nullptr;
    const uint64_t selected = std::strtoull(spec, &end, 10);
    if (end == spec)
      return false;
    if (selected == conflict)
      return true;
    spec = *end == ',' ? end + 1 : end;
    if (*end && *end != ',')
      return false;
  }
  return false;
}

void EventTrace::formula_checkpoint(const Kernel &k) {
  if (!out_ || emitted_ >= limit_ ||
      !formula_checkpoint_selected(
          std::getenv("TACTIX_EVENT_TRACE_FORMULA_CONFLICTS"),
          k.stats.conflicts))
    return;
  uint64_t irredundant = 0, redundant = 0;
  uint64_t irredundant_hash = 0, redundant_hash = 0;
  uint64_t heap_hash = 1469598103934665603ull;
  uint64_t heap_score_hash = 1469598103934665603ull;
  size_t heap_size = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage()) {
      const uint64_t hash = eliminate_cleanup_clause_hash(k, *clause);
      if (clause->redundant()) {
        redundant++;
        redundant_hash += hash;
      } else {
        irredundant++;
        irredundant_hash += hash;
      }
    }
    ref += words;
  }
  if (k.h) {
    heap_size = k.h->scores.stack.size();
    for (const Var v : k.h->scores.stack) {
      heap_hash = mix(heap_hash, k.internal_to_external[v] + 1ull);
      uint64_t bits;
      static_assert(sizeof bits == sizeof k.h->scores.score[v]);
      std::memcpy(&bits, &k.h->scores.score[v], sizeof bits);
      heap_score_hash = mix(heap_score_hash,
                            k.internal_to_external[v] + 1ull);
      heap_score_hash = mix(heap_score_hash, bits);
    }
  }
  char checkpoint_payload[360];
  std::snprintf(checkpoint_payload, sizeof checkpoint_payload,
                "irredundant=%llu|redundant=%llu|irredundant_hash=%016llx|"
                "redundant_hash=%016llx|heap=%zu|heap_hash=%016llx|"
                "heap_score_hash=%016llx",
                (unsigned long long)irredundant,
                (unsigned long long)redundant,
                (unsigned long long)irredundant_hash,
                (unsigned long long)redundant_hash, heap_size,
                (unsigned long long)heap_hash,
                (unsigned long long)heap_score_hash);
  emit(k, "formula_checkpoint", checkpoint_payload, true);
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
                              uint64_t remaining, uint64_t next,
                              uint64_t sequence) {
  if (!want_detail(k))
    return;
  const std::string payload =
      "stage=" + std::string(stage) +
      "|remaining=" + std::to_string(remaining) +
      "|next=" + std::to_string(next) +
      "|sequence=" + std::to_string(sequence);
  emit(k, "randec_state", payload.c_str(), false);
}

void EventTrace::watch_state(const Kernel &k, Lit propagated,
                             const char *event, bool emit_summary) {
  if ((watch_within_vivify_ && !vivify_watch_active_) || !want_detail(k) ||
      emitted_ >= limit_)
    return;
  if (const char *selected_scan =
          std::getenv("TACTIX_EVENT_TRACE_WATCH_SCAN_LITERAL")) {
    if (kernel_externalize(k, propagated) != std::atoi(selected_scan))
      return;
  }
  const Lit watched = negate(propagated);
  uint64_t selected_entries = 0;
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
    selected_entries++;
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
                 std::to_string(kernel_clause_position(k, entry.ref())) +
                 "|searched=" + std::to_string(large->searched) + "|pose=";
      for (uint32_t i = 0; i < large->size; i++) {
        if (i)
          payload += ',';
        payload += std::to_string(kernel_externalize(k, large->lits[i]));
      }
      if (std::getenv("TACTIX_EVENT_TRACE_WATCH_VALUES")) {
        payload += "|values=";
        for (uint32_t i = 0; i < large->size; i++) {
          if (i)
            payload += ',';
          payload += std::to_string(int(k.values[large->lits[i]]));
        }
      }
    }
    emit(k, event, payload.c_str(), false);
  }
  if (emit_summary) {
    const std::string payload =
        "scan=" + std::to_string(kernel_externalize(k, propagated)) +
        "|selected=" + std::to_string(selected_entries) +
        "|words=" + std::to_string(k.watches.heads[watched].size);
    emit(k, "watch_post_summary", payload.c_str(), false);
  }
}

void EventTrace::watch_scan(const Kernel &k, Lit propagated) {
  watch_state(k, propagated, "watch", false);
}

void EventTrace::watch_scan_post(const Kernel &k, Lit propagated) {
  if (!std::getenv("TACTIX_EVENT_TRACE_WATCH_POST"))
    return;
  watch_state(k, propagated, "watch_post", true);
}

void EventTrace::watch_cost(const Kernel &k, Lit propagated,
                            uint64_t words_before, uint64_t words_after,
                            uint64_t base, uint64_t binary_assignments,
                            uint64_t large_dereferences,
                            uint64_t relocations,
                            uint64_t large_assignments, uint64_t total) {
  if (!watch_cost_ ||
      (watch_within_vivify_ && !vivify_watch_active_) || !want_detail(k))
    return;
  const std::string payload =
      "round=" + std::to_string(k.stats.vivify_rounds) +
      "|scope=" + std::to_string(k.vivify_scope) +
      "|ordinal=" + std::to_string(vivify_watch_ordinal_) +
      "|scan=" + std::to_string(kernel_externalize(k, propagated)) +
      "|words_before=" + std::to_string(words_before) +
      "|words_after=" + std::to_string(words_after) +
      "|base=" + std::to_string(base) +
      "|binary_assign=" + std::to_string(binary_assignments) +
      "|large_deref=" + std::to_string(large_dereferences) +
      "|relocate=" + std::to_string(relocations) +
      "|large_assign=" + std::to_string(large_assignments) +
      "|total=" + std::to_string(total);
  emit(k, "watch_cost", payload.c_str(), false);
}

void event_trace_watch_scan(const Kernel &k, Lit propagated) {
  if (active_trace)
    active_trace->watch_scan(k, propagated);
}

void event_trace_watch_scan_post(const Kernel &k, Lit propagated) {
  if (active_trace)
    active_trace->watch_scan_post(k, propagated);
}

void event_trace_watch_cost(const Kernel &k, Lit propagated,
                            uint64_t words_before, uint64_t words_after,
                            uint64_t base, uint64_t binary_assignments,
                            uint64_t large_dereferences,
                            uint64_t relocations,
                            uint64_t large_assignments, uint64_t total) {
  if (active_trace)
    active_trace->watch_cost(k, propagated, words_before, words_after, base,
                             binary_assignments, large_dereferences,
                             relocations, large_assignments, total);
}

void event_trace_detail(const Kernel &k, const char *event, int64_t payload) {
  if (active_trace)
    active_trace->detail(k, event, payload);
}

void event_trace_vivify_candidate(const Kernel &k, const char *event,
                                  uint64_t ordinal, Ref cref, uint64_t ticks,
                                  uint64_t limit) {
  if (active_trace)
    active_trace->vivify_candidate(k, event, ordinal, cref, ticks, limit);
}

void event_trace_vivify_counts(const Kernel &k, const uint32_t *counts,
                               size_t size) {
  if (active_trace)
    active_trace->vivify_counts(k, counts, size);
}

void event_trace_learned_stage(const Kernel &k, const char *event,
                               const std::vector<Lit> &clause) {
  if (active_trace)
    active_trace->learned_stage(k, event, clause);
}

void event_trace_clause_glue(const Kernel &k, const char *source,
                             const Clause *clause, uint32_t old_glue,
                             uint32_t new_glue) {
  if (active_trace)
    active_trace->clause_glue(k, source, clause, old_glue, new_glue);
}

void event_trace_clause_used(const Kernel &k, const Clause *clause,
                             uint32_t mode) {
  if (active_trace)
    active_trace->clause_used(k, clause, mode);
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
                              uint64_t remaining, uint64_t next,
                              uint64_t sequence) {
  if (active_trace)
    active_trace->randec_state(k, stage, remaining, next, sequence);
}

void EventTrace::phase_pick(const Kernel &k, Var var, int8_t target,
                            int8_t saved, int8_t result) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const int external =
      kernel_externalize(k, make_lit(var, false));
  if (const char *selected = std::getenv("SAT_EVENT_TRACE_PHASE_VAR"))
    if (*selected && std::abs(std::atoi(selected)) != std::abs(external))
      return;
  const std::string payload =
      "external=" + std::to_string(external) +
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

void EventTrace::target_state(const Kernel &k, const char *stage,
                              uint32_t assigned) {
  if (!phase_only_ || !out_ || emitted_ >= limit_ || !k.h)
    return;
  int watched = 0;
  if (const char *spec = std::getenv("SAT_EVENT_TRACE_PHASE_VAR"))
    watched = std::atoi(spec);
  int target = 0, saved = 0, value = 0, active = 0;
  if (watched > 0) {
    const Lit lit = kernel_internalize(k, watched);
    if (lit != INVALID_LIT) {
      const Var v = var_of(lit);
      const int sign = negated(lit) ? -1 : 1;
      target = sign * k.h->target[v];
      saved = sign * k.h->saved[v];
      value = sign * k.values[make_lit(v, false)];
      active = heuristics_var_active(k, v);
    }
  }
  const std::string payload =
      "stage=" + std::string(stage) + "|assigned=" +
      std::to_string(assigned) + "|target_assigned=" +
      std::to_string(k.h->target_assigned) + "|best_assigned=" +
      std::to_string(k.h->best_assigned) + "|compact_vars=" +
      std::to_string(k.compact_import_order.size()) + "|physical_vars=" +
      std::to_string(k.nvars) + "|watch=" + std::to_string(watched) +
      "|watch_target=" + std::to_string(target) + "|watch_saved=" +
      std::to_string(saved) + "|watch_value=" + std::to_string(value) +
      "|watch_active=" + std::to_string(active);
  emit(k, "target_state", payload.c_str(), false);
}

void event_trace_target_state(const Kernel &k, const char *stage,
                              uint32_t assigned) {
  if (active_trace)
    active_trace->target_state(k, stage, assigned);
}

void EventTrace::phase_save(const Kernel &k, Lit lit) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const int external = kernel_externalize(k, lit);
  if (const char *selected = std::getenv("SAT_EVENT_TRACE_PHASE_VAR"))
    if (*selected && std::abs(std::atoi(selected)) != std::abs(external))
      return;
  const std::string payload =
      "lit=" + std::to_string(external);
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
  trace_slot_memory(k, name, stage);
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
                              uint64_t ticks_before, uint64_t ticks_after,
                              uint64_t solves) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" + std::to_string(external) +
      "|before=" + std::to_string(ticks_before) +
      "|after=" + std::to_string(ticks_after) +
      "|delta=" + std::to_string(ticks_after - ticks_before) +
      "|solves=" + std::to_string(solves);
  emit(k, "sweep_effort", payload.c_str(), false);
}

void event_trace_sweep_effort(const Kernel &k, uint32_t external,
                              uint64_t ticks_before, uint64_t ticks_after,
                              uint64_t solves) {
  if (active_trace)
    active_trace->sweep_effort(k, external, ticks_before, ticks_after, solves);
}

void EventTrace::sweep_environment_var(const Kernel &k, Var seed,
                                       uint32_t ordinal, Var var,
                                       uint32_t depth) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_VARS");
  if (!selected || std::strtol(selected, nullptr, 10) !=
                       kernel_externalize(k, make_lit(seed, false)))
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

void EventTrace::sweep_binary_attempt(const Kernel &k, Var seed,
                                      uint32_t ordinal, Lit lit, Lit other,
                                      int32_t lit_depth, int32_t other_depth,
                                      const char *disposition) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_BINARIES");
  if (!selected || std::strtol(selected, nullptr, 10) !=
                       kernel_externalize(k, make_lit(seed, false)))
    return;
  const std::string payload =
      "seed=" +
      std::to_string(kernel_externalize(k, make_lit(seed, false))) +
      "|ordinal=" + std::to_string(ordinal) +
      "|lit=" + std::to_string(kernel_externalize(k, lit)) +
      "|other=" + std::to_string(kernel_externalize(k, other)) +
      "|lit_depth=" + std::to_string(lit_depth) +
      "|other_depth=" + std::to_string(other_depth) +
      "|disposition=" + disposition;
  emit(k, "sweep_binary_attempt", payload.c_str(), false);
}

void event_trace_sweep_binary_attempt(const Kernel &k, Var seed,
                                      uint32_t ordinal, Lit lit, Lit other,
                                      int32_t lit_depth, int32_t other_depth,
                                      const char *disposition) {
  if (active_trace)
    active_trace->sweep_binary_attempt(k, seed, ordinal, lit, other,
                                       lit_depth, other_depth, disposition);
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

void EventTrace::sweep_commit(const Kernel &k, Lit a, Lit b, int8_t av,
                              int8_t bv, const char *stage) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_COMMIT");
  if (!selected || !*selected)
    return;
  char *end = nullptr;
  const long selected_a = std::strtol(selected, &end, 10);
  if (end == selected || *end != ',')
    return;
  const char *second = end + 1;
  const long selected_b = std::strtol(second, &end, 10);
  if (end == second || *end)
    return;
  const int external_a = kernel_externalize(k, a);
  const int external_b = kernel_externalize(k, b);
  if (!((external_a == selected_a && external_b == selected_b) ||
        (external_a == selected_b && external_b == selected_a)))
    return;
  const std::string payload =
      "a=" + std::to_string(external_a) +
      "|b=" + std::to_string(external_b) + "|av=" +
      std::to_string(static_cast<int>(av)) + "|bv=" +
      std::to_string(static_cast<int>(bv)) +
      "|proof=" + std::to_string(k.proof ? 1 : 0) + "|stage=" + stage;
  emit(k, "sweep_commit", payload.c_str(), false);
}

void event_trace_sweep_commit(const Kernel &k, Lit a, Lit b, int8_t av,
                              int8_t bv, const char *stage) {
  if (active_trace)
    active_trace->sweep_commit(k, a, b, av, bv, stage);
}

void EventTrace::sweep_rewrite_binary(const Kernel &k, Lit source,
                                      Lit representative, uint32_t position,
                                      Lit other) {
  if (!out_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_REWRITE");
  if (!selected || !*selected)
    return;
  const int external_representative =
      kernel_externalize(k, representative);
  if (external_representative != std::atoi(selected))
    return;
  char payload[160];
  std::snprintf(payload, sizeof payload,
                "source=%d|repr=%d|position=%u|other=%d",
                kernel_externalize(k, source), external_representative,
                position, kernel_externalize(k, other));
  emit(k, "sweep_rewrite_binary", payload, false);
}

void event_trace_sweep_rewrite_binary(const Kernel &k, Lit source,
                                      Lit representative, uint32_t position,
                                      Lit other) {
  if (active_trace)
    active_trace->sweep_rewrite_binary(k, source, representative, position,
                                       other);
}

void EventTrace::sweep_rewrite_occurrence(const Kernel &k, Lit source,
                                          Lit representative,
                                          uint32_t position, bool binary,
                                          uint32_t payload) {
  if (!out_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_REWRITE");
  if (!selected || !*selected)
    return;
  const int external_representative =
      kernel_externalize(k, representative);
  if (external_representative != std::atoi(selected))
    return;
  std::string detail =
      "source=" + std::to_string(kernel_externalize(k, source)) +
      "|repr=" + std::to_string(external_representative) +
      "|position=" + std::to_string(position);
  if (binary) {
    detail += "|kind=binary|other=" +
              std::to_string(kernel_externalize(k, static_cast<Lit>(payload)));
  } else {
    const Clause *const clause = clause_at(k.arena, static_cast<Ref>(payload));
    detail += "|kind=large|clause=";
    bool first = true;
    for (const Lit lit : *clause) {
      detail += first ? "" : ",";
      detail += std::to_string(kernel_externalize(k, lit));
      first = false;
    }
  }
  emit(k, "sweep_rewrite_occurrence", detail.c_str(), false);
}

void event_trace_sweep_rewrite_occurrence(const Kernel &k, Lit source,
                                          Lit representative,
                                          uint32_t position, bool binary,
                                          uint32_t payload) {
  if (active_trace)
    active_trace->sweep_rewrite_occurrence(k, source, representative, position,
                                           binary, payload);
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
  const char *selected_representative =
      std::getenv("SAT_EVENT_TRACE_SUBSTITUTE_REPRESENTATIVE");
  if (selected) {
    if (std::atoi(selected) != kernel_externalize(k, lit))
      return;
  } else if (selected_representative) {
    if (representative == INVALID_LIT ||
        std::atoi(selected_representative) !=
            kernel_externalize(k, representative))
      return;
  } else {
    const char *all = std::getenv("SAT_EVENT_TRACE_SUBSTITUTE_ALL");
    if (!all || !std::atoi(all) || representative == INVALID_LIT ||
        representative == lit)
      return;
  }
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

void EventTrace::bve_candidate(const Kernel &k, Var var, const char *stage,
                               uint64_t ticks, uint64_t limit, bool applied) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" + std::to_string(k.internal_to_external[var] + 1) +
      "|stage=" + stage +
      "|pivot_value=" +
      std::to_string(static_cast<int>(k.value(make_lit(var, false)))) +
      "|ticks=" + std::to_string(ticks) +
      "|limit=" + std::to_string(limit) +
      "|applied=" + (applied ? "1" : "0");
  // A trace-only checkpoint used to distinguish a stale dense occurrence
  // image from an already-diverged live formula.  Keeping this opt-in avoids
  // the full arena walk and ranking snapshot on ordinary event traces.
  const bool formula_checkpoint =
      std::getenv("SAT_EVENT_TRACE_BVE_FORMULA") != nullptr;
  emit(k, "bve_candidate", payload.c_str(), formula_checkpoint);
}

void event_trace_bve_candidate(const Kernel &k, Var var, const char *stage,
                               uint64_t ticks, uint64_t limit, bool applied) {
  if (active_trace)
    active_trace->bve_candidate(k, var, stage, ticks, limit, applied);
}

void event_trace_bve_definition(const Kernel &k, Var var, uint64_t attempt,
                                uint32_t core, const char *outcome,
                                uint64_t ticks, uint64_t budget,
                                uint64_t generator, uint32_t variables,
                                uint32_t side0, uint32_t side1,
                                const uint32_t *ids, uint32_t size) {
  if (!active_trace)
    return;
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_BVE_EXTERNAL");
  if (selected && std::atoi(selected) !=
                      static_cast<int>(k.internal_to_external[var] + 1))
    return;
  uint64_t hash = 1469598103934665603ull;
  uint32_t core0 = 0, core1 = 0;
  for (uint32_t i = 0; i < size; ++i) {
    hash ^= ids[i];
    hash *= 1099511628211ull;
    core0 += ids[i] <= side0;
    core1 += ids[i] > side0;
  }
  char payload[512];
  std::snprintf(payload, sizeof payload,
                "external=%u|attempt=%llu|core=%u|outcome=%s|ticks=%llu|"
                "budget=%llu|generator=%016llx|variables=%u|side0=%u|"
                "side1=%u|core_size=%u|core0=%u|core1=%u|core_hash=%016llx",
                k.internal_to_external[var] + 1,
                (unsigned long long)attempt, core, outcome,
                (unsigned long long)ticks, (unsigned long long)budget,
                (unsigned long long)generator, variables, side0, side1, size,
                core0, core1, (unsigned long long)hash);
  active_trace->detail_text(k, "bve_definition", payload);
}

void EventTrace::bve_plan(const Kernel &k, Var var, uint32_t side0,
                          uint32_t side1, uint32_t gate_kind, uint32_t gate0,
                          uint32_t gate1, uint64_t charged,
                          bool resolve_gate) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload =
      "external=" + std::to_string(k.internal_to_external[var] + 1) +
      "|side0=" + std::to_string(side0) +
      "|side1=" + std::to_string(side1) +
      "|gate_kind=" + std::to_string(gate_kind) +
      "|gate0=" + std::to_string(gate0) +
      "|gate1=" + std::to_string(gate1) +
      "|charged=" + std::to_string(charged) +
      "|resolve_gate=" + (resolve_gate ? "1" : "0");
  emit(k, "bve_plan", payload.c_str(), false);
}

void event_trace_bve_plan(const Kernel &k, Var var, uint32_t side0,
                          uint32_t side1, uint32_t gate_kind, uint32_t gate0,
                          uint32_t gate1, uint64_t charged,
                          bool resolve_gate) {
  if (active_trace)
    active_trace->bve_plan(k, var, side0, side1, gate_kind, gate0, gate1,
                           charged, resolve_gate);
}

static uint64_t bve_clause_hash(const Kernel &k, const Lit *lits,
                                uint32_t size) {
  std::vector<int32_t> external;
  external.reserve(size);
  for (uint32_t i = 0; i < size; i++)
    external.push_back(kernel_externalize(k, lits[i]));
  std::sort(external.begin(), external.end());
  uint64_t hash = 1469598103934665603ull;
  for (const int32_t lit : external) {
    hash ^= static_cast<uint32_t>(lit);
    hash *= 1099511628211ull;
  }
  return hash;
}

void EventTrace::bve_pair(const Kernel &k, Var var, uint32_t ordinal,
                          const Lit *side0, uint32_t size0, Ref ref0,
                          uint64_t id0, const Lit *side1, uint32_t size1,
                          Ref ref1, uint64_t id1, const char *outcome) {
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_BVE_EXTERNAL");
  if (!selected || std::atoi(selected) !=
                       static_cast<int>(k.internal_to_external[var] + 1) ||
      !want_detail(k) || emitted_ >= limit_)
    return;
  const uint64_t order0 =
      ref0 == INVALID_REF ? INVALID_REF : kernel_clause_position(k, ref0);
  const uint64_t order1 =
      ref1 == INVALID_REF ? INVALID_REF : kernel_clause_position(k, ref1);
  char header[512];
  std::snprintf(header, sizeof header,
                "external=%u|ordinal=%u|side0_size=%u|side0_hash=%016llx|"
                "side0_ref=%s%u|side0_id=%llu|side0_order=%llu|"
                "side1_size=%u|side1_hash=%016llx|side1_ref=%s%u|"
                "side1_id=%llu|side1_order=%llu|outcome=%s",
                k.internal_to_external[var] + 1, ordinal, size0,
                (unsigned long long)bve_clause_hash(k, side0, size0),
                ref0 == INVALID_REF ? "-" : "", ref0 == INVALID_REF ? 0 : ref0,
                (unsigned long long)id0, (unsigned long long)order0, size1,
                (unsigned long long)bve_clause_hash(k, side1, size1),
                ref1 == INVALID_REF ? "-" : "", ref1 == INVALID_REF ? 0 : ref1,
                (unsigned long long)id1, (unsigned long long)order1, outcome);
  std::string payload(header);
  const auto append_pose = [&](const char *name, const Lit *side,
                               uint32_t size) {
    payload += '|';
    payload += name;
    payload += '=';
    for (uint32_t i = 0; i < size; ++i) {
      if (i)
        payload += ',';
      payload += std::to_string(kernel_externalize(k, side[i]));
    }
  };
  append_pose("side0_pose", side0, size0);
  append_pose("side1_pose", side1, size1);
  emit(k, "bve_pair", payload.c_str(), false);
}

void event_trace_bve_pair(const Kernel &k, Var var, uint32_t ordinal,
                          const Lit *side0, uint32_t size0, Ref ref0,
                          uint64_t id0, const Lit *side1, uint32_t size1,
                          Ref ref1, uint64_t id1, const char *outcome) {
  if (active_trace)
    active_trace->bve_pair(k, var, ordinal, side0, size0, ref0, id0, side1,
                           size1, ref1, id1, outcome);
}

void EventTrace::bve_heap(const Kernel &k, const char *stage, Var trigger,
                          Var subject, double score, uint32_t position,
                          Var root, uint32_t size) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  char payload[512];
  const int trigger_external =
      trigger == INVALID_VAR ? 0 : k.internal_to_external[trigger] + 1;
  const int root_external =
      root == INVALID_VAR ? 0 : k.internal_to_external[root] + 1;
  const double relevance =
      !k.h ? 0.0
           : k.h->stable ? k.h->scores.score[subject]
                         : static_cast<double>(k.h->links[subject].stamp);
  const uint32_t positive =
      bve_schedule_occurrence_count(k, make_lit(subject, false));
  const uint32_t negative =
      bve_schedule_occurrence_count(k, make_lit(subject, true));
  if (position == UINT32_MAX)
    std::snprintf(payload, sizeof payload,
                  "stage=%s|trigger=%d|subject=%u|score=%.17g|score_hex=%a|"
                  "relevance=%.17g|relevance_hex=%a|positive=%u|negative=%u|"
                  "pos=-|root=%d|size=%u",
                  stage, trigger_external, k.internal_to_external[subject] + 1,
                  score, score, relevance, relevance, positive, negative,
                  root_external, size);
  else
    std::snprintf(payload, sizeof payload,
                  "stage=%s|trigger=%d|subject=%u|score=%.17g|score_hex=%a|"
                  "relevance=%.17g|relevance_hex=%a|positive=%u|negative=%u|"
                  "pos=%u|root=%d|size=%u",
                  stage, trigger_external, k.internal_to_external[subject] + 1,
                  score, score, relevance, relevance, positive, negative,
                  position, root_external, size);
  emit(k, "bve_heap", payload, false);
}

void event_trace_bve_heap(const Kernel &k, const char *stage, Var trigger,
                          Var subject, double score, uint32_t position,
                          Var root, uint32_t size) {
  if (active_trace)
    active_trace->bve_heap(k, stage, trigger, subject, score, position, root,
                           size);
}

static uint64_t eliminate_cleanup_clause_hash(const Kernel &k,
                                              const Clause &clause) {
  std::vector<int32_t> external;
  external.reserve(clause.size);
  for (const Lit lit : clause)
    external.push_back(kernel_externalize(k, lit));
  std::sort(external.begin(), external.end());
  uint64_t hash = 1469598103934665603ull;
  for (const int32_t lit : external) {
    hash ^= static_cast<uint32_t>(lit);
    hash *= 1099511628211ull;
  }
  return hash;
}

void EventTrace::eliminate_cleanup(const Kernel &k, const char *stage) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  uint64_t live_irredundant = 0, live_redundant = 0;
  uint64_t root_irredundant = 0, root_redundant = 0;
  uint64_t retired_irredundant = 0, retired_redundant = 0;
  uint64_t both_irredundant = 0, both_redundant = 0;
  uint64_t root_hash = 0, retired_hash = 0, collect_hash = 0;
  uint64_t live_redundant_hash = 0;
  std::vector<uint32_t> retired_external;
  for (Var var = 0; var < k.nvars; ++var)
    if (kernel_var_retired(k, var))
      retired_external.push_back(k.internal_to_external[var] + 1);
  std::sort(retired_external.begin(), retired_external.end());
  uint64_t retired_variables_hash = 1469598103934665603ull;
  for (const uint32_t external : retired_external) {
    retired_variables_hash ^= external;
    retired_variables_hash *= 1099511628211ull;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage()) {
      const bool redundant = clause->redundant();
      (redundant ? live_redundant : live_irredundant)++;
      const uint64_t clause_hash =
          redundant ? eliminate_cleanup_clause_hash(k, *clause) : 0;
      live_redundant_hash += clause_hash;
      bool root = false, retired = false;
      for (const Lit lit : *clause) {
        root |= k.values[lit] > 0;
        retired |= kernel_var_retired(k, var_of(lit));
      }
      if (root || retired) {
        const uint64_t hash = clause_hash ? clause_hash
                                          : eliminate_cleanup_clause_hash(
                                                k, *clause);
        collect_hash += hash;
        if (root) {
          root_hash += hash;
          (redundant ? root_redundant : root_irredundant)++;
        }
        if (retired) {
          retired_hash += hash;
          (redundant ? retired_redundant : retired_irredundant)++;
        }
        if (root && retired)
          (redundant ? both_redundant : both_irredundant)++;
      }
    }
    ref += words;
  }
  char payload[640];
  std::snprintf(
      payload, sizeof payload,
      "stage=%s|live_irredundant=%llu|live_redundant=%llu|"
      "root_irredundant=%llu|root_redundant=%llu|"
      "retired_irredundant=%llu|retired_redundant=%llu|"
      "both_irredundant=%llu|both_redundant=%llu|root_hash=%016llx|"
      "retired_hash=%016llx|collect_hash=%016llx|"
      "live_redundant_hash=%016llx|retired_variables=%zu|"
      "retired_variables_hash=%016llx",
      stage, (unsigned long long)live_irredundant,
      (unsigned long long)live_redundant,
      (unsigned long long)root_irredundant,
      (unsigned long long)root_redundant,
      (unsigned long long)retired_irredundant,
      (unsigned long long)retired_redundant,
      (unsigned long long)both_irredundant,
      (unsigned long long)both_redundant, (unsigned long long)root_hash,
      (unsigned long long)retired_hash, (unsigned long long)collect_hash,
      (unsigned long long)live_redundant_hash, retired_external.size(),
      (unsigned long long)retired_variables_hash);
  emit(k, "eliminate_cleanup", payload, false);

  const char *clauses = std::getenv("TACTIX_EVENT_TRACE_ELIMINATE_CLAUSES");
  if (!clauses || *clauses != '1')
    return;
  for (Ref ref = 0; ref < k.arena.size() && emitted_ < limit_;) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && clause->redundant()) {
      bool root = false, retired = false;
      for (const Lit lit : *clause) {
        root |= k.values[lit] > 0;
        retired |= kernel_var_retired(k, var_of(lit));
      }
      char prefix[192];
      std::snprintf(prefix, sizeof prefix,
                    "stage=%s|hash=%016llx|size=%u|root=%u|retired=%u|clause=",
                    stage,
                    (unsigned long long)eliminate_cleanup_clause_hash(
                        k, *clause),
                    clause->size, root, retired);
      std::vector<int32_t> external;
      external.reserve(clause->size);
      for (const Lit lit : *clause)
        external.push_back(kernel_externalize(k, lit));
      std::sort(external.begin(), external.end());
      std::string clause_payload(prefix);
      for (size_t i = 0; i != external.size(); ++i) {
        if (i)
          clause_payload.push_back(',');
        clause_payload += std::to_string(external[i]);
      }
      emit(k, "eliminate_cleanup_clause", clause_payload.c_str(), false);
    }
    ref += words;
  }
}

void event_trace_eliminate_cleanup(const Kernel &k, const char *stage) {
  if (active_trace)
    active_trace->eliminate_cleanup(k, stage);
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

void EventTrace::emit_heap_scores(const Kernel &k) {
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

void EventTrace::heap_scores(const Kernel &k) {
  // Trajectory-only and endpoint-stop modes deliberately omit intermediate
  // heap-score records.  Return before hashing the full stack: on
  // multi-million-variable instances discarded diagnostic work otherwise
  // dominates the solver run.
  if (trajectory_only_ || !k.h || !k.h->stable)
    return;
  if (stop_after_conflict_ != UINT64_MAX) {
    if (heap_each_bump_ &&
        (k.stats.conflicts >= stop_after_conflict_ ||
         k.stats.decisions >= heap_from_decision_))
      emit_heap_scores(k);
    return;
  }
  if (!want_detail(k))
    return;
  emit_heap_scores(k);
}

void EventTrace::heap_insert(const Kernel &k, Var v, uint32_t from,
                             uint32_t to) {
  if (!out_ || heap_pop_only_ || !k.h || !k.h->stable || !want_detail(k))
    return;
  uint64_t hash = 1469598103934665603ull;
  for (const Var member : k.h->scores.stack)
    hash = mix(hash, k.internal_to_external[member] + 1ull);
  char payload[224];
  std::snprintf(payload, sizeof payload,
                "var=%d|score=%.17g|from=%u|to=%u|hash=%016llx",
                kernel_externalize(k, make_lit(v, false)),
                k.h->scores.score[v], from, to,
                (unsigned long long)hash);
  emit(k, "heap_insert", payload, false);
}

void EventTrace::heap_remove(const Kernel &k, Var removed, Var last,
                             uint32_t removed_pos, uint32_t from,
                             uint32_t to) {
  if (!out_ || !k.h || !k.h->stable || !want_detail(k) ||
      k.stats.decisions != heap_remove_decision_)
    return;
  char payload[256];
  if (to == UINT32_MAX)
    std::snprintf(payload, sizeof payload,
                  "removed=%d:%.17g|removed_pos=%u|last=%d:%.17g|from=%u|to=-",
                  kernel_externalize(k, make_lit(removed, false)),
                  k.h->scores.score[removed], removed_pos,
                  kernel_externalize(k, make_lit(last, false)),
                  k.h->scores.score[last], from);
  else
    std::snprintf(payload, sizeof payload,
                  "removed=%d:%.17g|removed_pos=%u|last=%d:%.17g|from=%u|to=%u",
                  kernel_externalize(k, make_lit(removed, false)),
                  k.h->scores.score[removed], removed_pos,
                  kernel_externalize(k, make_lit(last, false)),
                  k.h->scores.score[last], from, to);
  emit(k, "heap_remove", payload, false);
}

void EventTrace::heap_pop(const Kernel &k, const char *stage, Var held,
                          Var other, uint32_t from, uint32_t to) {
  if (!out_ || !k.h || !k.h->stable || k.stats.decisions != heap_pop_decision_)
    return;
  char payload[256];
  std::snprintf(payload, sizeof payload,
                "stage=%s|held=%d:%.17g|other=%d:%.17g|from=%u|to=%u", stage,
                kernel_externalize(k, make_lit(held, false)),
                k.h->scores.score[held],
                kernel_externalize(k, make_lit(other, false)),
                k.h->scores.score[other], from, to);
  emit(k, "heap_pop", payload, false);
}

void EventTrace::binary_assignment(const Kernel &k, Lit assigned,
                                   Lit immediate_reason, Lit stored_reason,
                                   bool jumped) {
  if (!out_)
    return;
  const char *selected =
      std::getenv("TACTIX_EVENT_TRACE_BINARY_ASSIGNMENT");
  if (!selected || !*selected)
    return;
  const int external = kernel_externalize(k, assigned);
  if (std::abs(external) != std::abs(std::atoi(selected)))
    return;
  char payload[256];
  std::snprintf(payload, sizeof payload,
                "assigned=%d|immediate=%d|jumped=%u|stored=%d", external,
                kernel_externalize(k, immediate_reason), jumped,
                kernel_externalize(k, stored_reason));
  emit(k, "binary_assignment", payload, false);
}

void EventTrace::binary_add(const Kernel &k, Lit a, Lit b) {
  if (!out_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_BINARY_ADD");
  if (!selected || !*selected)
    return;
  const int wanted = std::atoi(selected);
  const int external_a = kernel_externalize(k, a);
  const int external_b = kernel_externalize(k, b);
  if (external_a != wanted && external_b != wanted)
    return;
  char payload[96];
  std::snprintf(payload, sizeof payload, "a=%d|b=%d", external_a,
                external_b);
  emit(k, "binary_add", payload, false);
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

void EventTrace::restart_clock(const Kernel &k, uint64_t limit,
                               bool initialized, double fast, double slow,
                               double margin, bool eligible) {
  if (!std::getenv("TACTIX_EVENT_TRACE_RESTART_CLOCK") || !want_detail(k))
    return;
  if (k.stats.conflicts < limit)
    return;
  char payload[320];
  std::snprintf(payload, sizeof payload,
                "restarts=%llu|limit=%llu|initialized=%u|fast=%a|slow=%a|margin=%a|"
                "threshold=%a|eligible=%u",
                (unsigned long long)k.stats.restarts,
                (unsigned long long)limit, unsigned(initialized), fast, slow,
                margin, margin * slow, unsigned(eligible));
  emit(k, "restart_clock", payload, false);
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

void event_trace_heap_insert(const Kernel &k, Var v, uint32_t from,
                             uint32_t to) {
  if (active_trace)
    active_trace->heap_insert(k, v, from, to);
}

void event_trace_heap_remove(const Kernel &k, Var removed, Var last,
                             uint32_t removed_pos, uint32_t from,
                             uint32_t to) {
  if (active_trace)
    active_trace->heap_remove(k, removed, last, removed_pos, from, to);
}

void event_trace_heap_pop(const Kernel &k, const char *stage, Var held,
                          Var other, uint32_t from, uint32_t to) {
  if (active_trace)
    active_trace->heap_pop(k, stage, held, other, from, to);
}

void event_trace_binary_assignment(const Kernel &k, Lit assigned,
                                   Lit immediate_reason, Lit stored_reason,
                                   bool jumped) {
  if (active_trace)
    active_trace->binary_assignment(k, assigned, immediate_reason,
                                    stored_reason, jumped);
}

void event_trace_binary_add(const Kernel &k, Lit a, Lit b) {
  if (active_trace)
    active_trace->binary_add(k, a, b);
}

void event_trace_binary_lifecycle(const Kernel &k, const char *event, Lit a,
                                  Lit b, uint64_t id, uint32_t copies) {
  open_binary_lifecycle();
  if (!binary_lifecycle.out)
    return;
  int external_a = kernel_externalize(k, a);
  int external_b = kernel_externalize(k, b);
  if (external_b < external_a) {
    std::swap(external_a, external_b);
    std::swap(a, b);
  }
  if (std::find(binary_lifecycle.selected.begin(),
                binary_lifecycle.selected.end(),
                std::make_pair(external_a, external_b)) ==
      binary_lifecycle.selected.end())
    return;
  std::fprintf(binary_lifecycle.out,
               "tactix\t%llu\t%s\t%s\t%llu\t%llu\t%llu\t%d\t%d\t%d\t%d\t"
               "%llu\t%u\t-\n",
               static_cast<unsigned long long>(binary_lifecycle.sequence++),
               event, binary_lifecycle_rule,
               static_cast<unsigned long long>(k.stats.conflicts),
               static_cast<unsigned long long>(k.stats.search_ticks),
               static_cast<unsigned long long>(k.stats.probe_ticks),
               external_a, external_b, static_cast<int>(k.value(a)),
               static_cast<int>(k.value(b)),
               static_cast<unsigned long long>(id), copies);
}

void event_trace_binary_collapse(const Kernel &k, Ref source, Lit a, Lit b) {
  open_binary_lifecycle();
  if (!binary_lifecycle.out)
    return;
  int external_a = kernel_externalize(k, a);
  int external_b = kernel_externalize(k, b);
  if (external_b < external_a) {
    std::swap(external_a, external_b);
    std::swap(a, b);
  }
  if (std::find(binary_lifecycle.selected.begin(),
                binary_lifecycle.selected.end(),
                std::make_pair(external_a, external_b)) ==
      binary_lifecycle.selected.end())
    return;
  const Clause *const clause = clause_at(k.arena, source);
  std::fprintf(binary_lifecycle.out,
               "tactix\t%llu\tsweep-large-collapse-source\t%s\t%llu\t%llu\t"
               "%llu\t%d\t%d\t%d\t%d\t%llu\t%u\t"
               "source_ref=%u|source_id=%llu|source=",
               static_cast<unsigned long long>(binary_lifecycle.sequence++),
               binary_lifecycle_rule,
               static_cast<unsigned long long>(k.stats.conflicts),
               static_cast<unsigned long long>(k.stats.search_ticks),
               static_cast<unsigned long long>(k.stats.probe_ticks), external_a,
               external_b, static_cast<int>(k.value(a)),
               static_cast<int>(k.value(b)),
               static_cast<unsigned long long>(kernel_binary_id(k, a, b)),
               kernel_binary_copies(k, a, b), source,
               static_cast<unsigned long long>(clause->id()));
  bool first = true;
  for (const Lit lit : *clause) {
    std::fprintf(binary_lifecycle.out, "%s%d", first ? "" : ",",
                 kernel_externalize(k, lit));
    first = false;
  }
  std::fputc('\n', binary_lifecycle.out);
}

const char *event_trace_rule_enter(const char *rule) {
  const char *previous = binary_lifecycle_rule;
  binary_lifecycle_rule = rule ? rule : "-";
  return previous;
}

void event_trace_rule_leave(const char *previous) {
  binary_lifecycle_rule = previous ? previous : "-";
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
  if (k.conflict_ref != INVALID_REF) {
    const Clause *clause = clause_at(k.arena, k.conflict_ref);
    payload += "|ref=" + std::to_string(k.conflict_ref) +
               "|id=" + std::to_string(clause->id()) +
               "|glue=" + std::to_string(clause->glue()) +
               "|used=" + std::to_string(clause->used()) +
               "|redundant=" + std::to_string(clause->redundant());
  } else {
    payload += "|ref=binary";
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
  if (trajectory_only_) {
    uint64_t order_hash = 1469598103934665603ull;
    std::vector<int> external;
    external.reserve(clause.size());
    for (const Lit lit : clause) {
      const int value = kernel_externalize(k, lit);
      external.push_back(value);
      order_hash = mix(
          order_hash,
          static_cast<uint64_t>(static_cast<int64_t>(value)));
    }
    std::sort(external.begin(), external.end());
    uint64_t set_hash = 1469598103934665603ull;
    for (const int value : external)
      set_hash =
          mix(set_hash, static_cast<uint64_t>(static_cast<int64_t>(value)));
    char payload[128];
    std::snprintf(payload, sizeof payload,
                  "size=%zu|order_hash=%016llx|set_hash=%016llx",
                  clause.size(), (unsigned long long)order_hash,
                  (unsigned long long)set_hash);
    emit(k, event, payload, false);
    return;
  }
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
  payload += "|levels=";
  std::vector<uint32_t> nonasserting_levels;
  for (size_t i = 0; i < clause.size(); i++) {
    if (i)
      payload += ',';
    const uint32_t level = k.var_level(var_of(clause[i]));
    payload += std::to_string(level);
    if (i && std::find(nonasserting_levels.begin(), nonasserting_levels.end(),
                       level) == nonasserting_levels.end())
      nonasserting_levels.push_back(level);
  }
  payload += "|nonasserting_glue=" +
             std::to_string(nonasserting_levels.size());
  emit(k, event, payload.c_str(), false);
}

void EventTrace::clause_glue(const Kernel &k, const char *source,
                             const Clause *clause, uint32_t old_glue,
                             uint32_t new_glue) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = "source=" + std::string(source) +
                        "|id=" + std::to_string(clause->id()) +
                        "|old=" + std::to_string(old_glue) +
                        "|new=" + std::to_string(new_glue) + "|pose=";
  std::vector<int> external;
  external.reserve(clause->size);
  for (const Lit lit : *clause) {
    if (!external.empty())
      payload.push_back(',');
    const int value = kernel_externalize(k, lit);
    payload += std::to_string(value);
    external.push_back(value);
  }
  std::sort(external.begin(), external.end());
  payload += "|set=";
  for (size_t i = 0; i != external.size(); ++i) {
    if (i)
      payload.push_back(',');
    payload += std::to_string(external[i]);
  }
  emit(k, "reason_glue", payload.c_str(), false);
}

void EventTrace::clause_used(const Kernel &k, const Clause *clause,
                             uint32_t mode) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = "mode=" + std::to_string(mode) +
                        "|glue=" + std::to_string(clause->glue()) +
                        "|pose=";
  std::vector<int> external;
  external.reserve(clause->size);
  for (const Lit lit : *clause) {
    if (!external.empty())
      payload.push_back(',');
    const int value = kernel_externalize(k, lit);
    payload += std::to_string(value);
    external.push_back(value);
  }
  std::sort(external.begin(), external.end());
  payload += "|set=";
  for (size_t i = 0; i != external.size(); ++i) {
    if (i)
      payload.push_back(',');
    payload += std::to_string(external[i]);
  }
  emit(k, "reason_used", payload.c_str(), false);
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

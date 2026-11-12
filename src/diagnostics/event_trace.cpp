#include "diagnostics/event_trace.hpp"

#ifdef TACTIX_ENABLE_EVENT_TRACE

#include "kernel/splitmix64.hpp"
#include "policies/heuristics.hpp"
#include "rules/bve/bve.hpp"

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <sys/resource.h>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace tactix {
namespace {

// Opens a diagnostic trace for writing; a trace the user requested but that
// cannot be created reports its environment variable and exits with status 2.
FILE *open_trace_or_exit(const char *path, const char *environment) {
  FILE *out = std::fopen(path, "w");
  if (!out) {
    std::perror(environment);
    std::exit(2);
  }
  return out;
}

// Opens a diagnostic trace as above, line-buffered, and writes its header.
FILE *open_line_trace(const char *path, const char *environment,
                      const char *header) {
  FILE *out = open_trace_or_exit(path, environment);
  std::setvbuf(out, nullptr, _IOLBF, 0);
  std::fputs(header, out);
  return out;
}

// Whether `environment` names, in decimal, a conflict other than the current
// one; an unset or empty selector names every conflict.
bool other_conflict(const Kernel &k, const char *environment) {
  const char *conflict = std::getenv(environment);
  return conflict && *conflict &&
         std::strtoull(conflict, nullptr, 10) != k.stats.conflicts;
}

// Stores the decimal value of `environment` in `value` when it is set, and
// reports whether it was.
bool read_decimal(const char *environment, uint64_t &value) {
  const char *text = std::getenv(environment);
  if (text)
    value = std::strtoull(text, nullptr, 10);
  return text != nullptr;
}

// Parses all of `text` as a decimal; false when it is empty or has trailing
// characters.
bool parse_decimal(const char *text, uint64_t &value) {
  char *end = nullptr;
  value = std::strtoull(text, &end, 10);
  return end != text && !*end;
}

// How a scan of a comma-separated decimal list ended.
enum class DecimalList { Complete, Stopped, Malformed };

// Visits each decimal of the comma-separated list `spec` in order, parsed as
// strtol, strtoul or strtoull for a long, unsigned long or unsigned long long
// `Value`. A value is visited before its separator is checked; the scan stops
// when `visit` returns false, and is malformed at a token that is not a
// decimal or is followed by neither a comma nor the end.
template <class Value, class Visit>
DecimalList visit_decimal_list(const char *spec, Visit visit) {
  while (*spec) {
    char *end = nullptr;
    Value value;
    if constexpr (std::is_same_v<Value, long>)
      value = std::strtol(spec, &end, 10);
    else if constexpr (std::is_same_v<Value, unsigned long>)
      value = std::strtoul(spec, &end, 10);
    else
      value = std::strtoull(spec, &end, 10);
    if (end == spec)
      return DecimalList::Malformed;
    if (!visit(value))
      return DecimalList::Stopped;
    spec = end;
    if (*spec == ',')
      ++spec;
    else if (*spec)
      return DecimalList::Malformed;
  }
  return DecimalList::Complete;
}

EventTrace *active_trace;
// Conflicts between post-Reorder macro checkpoints.
constexpr uint64_t CHECKPOINT_INTERVAL = 1000;
bool bve_retry_trace_armed;
uint32_t bve_retry_trace_candidate;

[[noreturn]] void terminate_trace_endpoint() {
  std::raise(SIGTERM);
  // Tactix installs a cooperative SIGTERM handler. If it returns here, the
  // diagnostic endpoint has already flushed every selected artifact and must
  // terminate cleanly rather than falling through into more solver work.
  std::exit(0);
}

// Whether `event` is one of `names`.
bool event_in(const char *event, std::initializer_list<const char *> names) {
  for (const char *name : names)
    if (!std::strcmp(event, name))
      return true;
  return false;
}

// Whether `event` begins with one of `prefixes`.
bool event_prefixed(const char *event,
                    std::initializer_list<const char *> prefixes) {
  for (const char *prefix : prefixes)
    if (!std::strncmp(event, prefix, std::strlen(prefix)))
      return true;
  return false;
}

bool exit_selector_matches(const char *event, const char *payload,
                           const char *match) {
  if (!match || !*match)
    return false;
  if (std::strstr(event, match) || (payload && std::strstr(payload, match)))
    return true;
  // Preserve the existing `event|payload` selector syntax without building a
  // temporary string for every high-volume event. Only a selector beginning
  // with this exact event can match across the separator.
  const size_t event_size = std::strlen(event);
  const size_t match_size = std::strlen(match);
  if (match_size <= event_size || std::strncmp(match, event, event_size) ||
      match[event_size] != '|')
    return false;
  const std::string selector =
      std::string(event) + "|" + (payload ? payload : "-");
  return selector.find(match) != std::string::npos;
}

// The TACTIX_EVENT_TRACE_LATE_PATH destination when the exit selector names
// this event/payload and, if one is selected, the current exit conflict.
const char *late_trace_path(const Kernel &k, const char *event,
                            const char *payload) {
  const char *const path = std::getenv("TACTIX_EVENT_TRACE_LATE_PATH");
  const char *const match = std::getenv("SAT_EVENT_TRACE_EXIT_AFTER_MATCH");
  if (!path || !*path || !match || !*match ||
      !exit_selector_matches(event, payload, match) ||
      other_conflict(k, "SAT_EVENT_TRACE_EXIT_CONFLICT"))
    return nullptr;
  return path;
}

void append_absolute_integers(const char *spec, std::vector<int> &values) {
  visit_decimal_list<long>(spec, [&](long value) {
    values.push_back(std::abs(static_cast<int>(value)));
    return true;
  });
}

// The external literals of a clause, or of any internal literal range, sorted.
template <class Integer = int, class Lits>
std::vector<Integer> sorted_external_clause(const Kernel &k,
                                            const Lits &clause) {
  std::vector<Integer> result;
  result.reserve(static_cast<size_t>(clause.end() - clause.begin()));
  for (const Lit lit : clause)
    result.push_back(kernel_externalize(k, lit));
  std::sort(result.begin(), result.end());
  return result;
}

// Maps an internal literal to its external DIMACS literal.
struct ExternalLiteral {
  const Kernel &k;
  int operator()(Lit lit) const { return kernel_externalize(k, lit); }
};

// Leaves a list element unchanged.
struct SameValue {
  template <class T> T operator()(T value) const { return value; }
};

// Appends `values`, each mapped by `map`, as a comma-separated decimal list.
template <class Range, class Map = SameValue>
void append_list(std::string &out, const Range &values, Map map = {}) {
  bool first = true;
  for (const auto &value : values) {
    if (!first)
      out += ',';
    first = false;
    out += std::to_string(map(value));
  }
}

// Prints `values`, each mapped by `map` to an int, comma-separated.
template <class Range, class Map = SameValue>
void print_list(FILE *out, const Range &values, Map map = {}) {
  const char *separator = "";
  for (const auto &value : values) {
    std::fprintf(out, "%s%d", separator, map(value));
    separator = ",";
  }
}

// Appends a payload part: text verbatim and a number as std::to_string
// spells it.
void append_part(std::string &out, const char *text) { out += text; }
void append_part(std::string &out, const std::string &text) { out += text; }
template <class Number> void append_part(std::string &out, Number number) {
  static_assert(std::is_arithmetic_v<Number> && !std::is_same_v<Number, char>);
  out += std::to_string(number);
}

// Appends `parts`, each as append_part spells it.
template <class... Parts>
void append_parts(std::string &out, const Parts &...parts) {
  (append_part(out, parts), ...);
}

// The concatenation of `parts`, each as append_part spells it.
template <class... Parts> std::string concat(const Parts &...parts) {
  std::string out;
  append_parts(out, parts...);
  return out;
}

// The FNV-style offset every digest starts from, and its fold step.
constexpr uint64_t FNV_OFFSET = 1469598103934665603ull;
uint64_t mix(uint64_t h, uint64_t x) { return (h ^ x) * 1099511628211ull; }

// Mixes a signed value as its sign-extended 64-bit image.
uint64_t mix_signed(uint64_t h, int64_t x) {
  return mix(h, static_cast<uint64_t>(x));
}

// Folds each value in as `uint32_t`.
template <class Range> uint64_t fnv_hash(const Range &values) {
  uint64_t hash = FNV_OFFSET;
  for (const auto value : values)
    hash = mix(hash, static_cast<uint32_t>(value));
  return hash;
}

// Folds each signed value in as its sign-extended 64-bit image.
template <class Range> uint64_t fnv_signed(const Range &values) {
  uint64_t hash = FNV_OFFSET;
  for (const auto value : values)
    hash = mix_signed(hash, value);
  return hash;
}

// Order-free hash of a clause's external literal set.
uint64_t eliminate_cleanup_clause_hash(const Kernel &k, const Clause &clause) {
  return fnv_hash(sorted_external_clause<int32_t>(k, clause));
}

// Live irredundant and redundant clause counts and their summed set hashes.
struct FormulaDigest {
  uint64_t irredundant = 0, redundant = 0;
  uint64_t irredundant_hash = 0, redundant_hash = 0;
};

FormulaDigest formula_digest(const Kernel &k) {
  FormulaDigest digest;
  for (const auto [ref, clause] : arena_records(k.arena)) {
    if (clause->garbage())
      continue;
    const uint64_t hash = eliminate_cleanup_clause_hash(k, *clause);
    if (clause->redundant()) {
      digest.redundant++;
      digest.redundant_hash += hash;
    } else {
      digest.irredundant++;
      digest.irredundant_hash += hash;
    }
  }
  return digest;
}

void append_factor_entry_literals(std::string &payload, const Kernel &k,
                                  Lit selected, Lit other,
                                  const Clause *clause) {
  if (!clause) {
    append_parts(payload, kernel_externalize(k, selected), ",",
                 kernel_externalize(k, other));
    return;
  }
  append_list(payload, sorted_external_clause(k, *clause));
}

void append_clause_pose(std::string &payload, const Kernel &k,
                        const Clause &clause) {
  append_list(payload, clause, ExternalLiteral{k});
  payload += "|levels=";
  append_list(payload, clause,
              [&](Lit lit) { return k.var_level(var_of(lit)); });
  payload += "|set=";
  append_list(payload, sorted_external_clause(k, clause));
}

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
  const auto account = [&result](const void *block, size_t capacity) {
    result.capacity_bytes += capacity;
    if (!block)
      return;
    result.allocations++;
#if defined(__GLIBC__)
    result.usable_bytes +=
        malloc_usable_size(const_cast<void *>(block));
#else
    result.usable_bytes += capacity;
#endif
  };
  account(dense->large.arena_data(), dense->large.arena_capacity_bytes());
  account(dense->large.heads_data(), dense->large.heads_capacity_bytes());
  account(dense->ordered.arena_data(), dense->ordered.arena_capacity_bytes());
  account(dense->ordered.heads_data(), dense->ordered.heads_capacity_bytes());
  account(dense->ordered.counts_data(), dense->ordered.count_capacity_bytes());
  return result;
}

void trace_slot_memory(const Kernel &k, const char *name, const char *stage,
                       size_t temporary_bytes = 0, uint32_t subject = 0) {
  static FILE *const out = [] {
    const char *path = std::getenv("TACTIX_SLOT_MEMORY_TRACE");
    FILE *file = path && *path ? std::fopen(path, "w") : nullptr;
    if (file)
      std::setvbuf(file, nullptr, _IOLBF, 0);
    return file;
  }();
  if (!out)
    return;
  struct rusage usage {};
  getrusage(RUSAGE_SELF, &usage);
  const long peak_rss_kib = usage.ru_maxrss;
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
      capacity_bytes(k.radix_scratch) + capacity_bytes(k.marked_levels) +
      capacity_bytes(k.touched_levels) + capacity_bytes(k.eager_marks) +
      capacity_bytes(k.sweep_tail) + capacity_bytes(k.sweep_front) +
      capacity_bytes(k.sweep_slot) + capacity_bytes(k.sweep_marked);
  const size_t input_bytes =
      capacity_bytes(k.input_activation_order) +
      capacity_bytes(k.input_activated) +
      capacity_bytes(k.input_marks);
  const size_t touch_bytes =
      capacity_bytes(k.bve_retry) + capacity_bytes(k.factor_marks) +
      capacity_bytes(k.subsume_material) +
      capacity_bytes(k.backbone_priority) + capacity_bytes(k.touched) +
      capacity_bytes(k.removed) + capacity_bytes(k.touched_kept);
  const size_t event_bytes =
      capacity_bytes(k.retired_events) + capacity_bytes(k.substituted_events) +
      capacity_bytes(k.fresh_var_events);
  const size_t probing_bytes =
      capacity_bytes(k.probing.lists) + capacity_bytes(k.probing.words) +
      capacity_bytes(k.probing.stamp) + capacity_bytes(k.probing.clauses);
  const size_t proof_index_bytes =
      capacity_bytes(k.unit_ids) + capacity_bytes(k.dsr_root_values) +
      k.binary_ids.diagnostic_storage_bytes();
  size_t heuristics_bytes = 0;
  if (k.h) {
    heuristics_bytes =
        capacity_bytes(k.h->saved) + capacity_bytes(k.h->links) +
        capacity_bytes(k.h->scores.stack) + capacity_bytes(k.h->scores.pos) +
        capacity_bytes(k.h->scores.score) + capacity_bytes(k.h->target) +
        capacity_bytes(k.h->best);
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
               "heap_used_kib=%zu\theap_free_kib=%zu\tmmap_kib=%zu\t"
               "collector_order_capacity_bytes=%zu\t"
               "collector_temporary_capacity_bytes=%zu\t"
               "dense_large_arena_capacity_bytes=%zu\t"
               "dense_ordered_arena_capacity_bytes=%zu\t"
               "memory_conflicts=%llu\tmemory_decisions=%llu\t"
               "memory_subject_internal=%u\n",
               name, stage, peak_rss_kib, current_rss_kib, vm_kib, k.nvars,
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
               heap_used_kib, heap_free_kib, mmap_kib,
               capacity_bytes(k.collect_order_scratch), temporary_bytes,
               k.dense ? k.dense->large.arena_capacity_bytes() : 0,
               k.dense ? k.dense->ordered.arena_capacity_bytes() : 0,
               (unsigned long long)k.stats.conflicts,
               (unsigned long long)k.stats.decisions, subject);
}

void trace_bve_capacity_change(const Kernel &k, Var var, const char *stage) {
  static const bool enabled = [] {
    const char *path = std::getenv("TACTIX_SLOT_MEMORY_TRACE");
    return path && *path;
  }();
  if (!enabled)
    return;
  // Constant space: only a capacity transition samples /proc and the
  // allocator; a row brackets an interval that may span several growths.
  const std::array<size_t, 9> capacities = {
      k.arena.capacity(), k.watches.arena.capacity(),
      k.reconstruction.capacity(), k.watches.heads.capacity(),
      k.dense ? k.dense->large.arena_capacity_bytes() : 0,
      k.dense ? k.dense->ordered.arena_capacity_bytes() : 0,
      k.dense ? k.dense->large.heads_capacity_bytes() : 0,
      k.dense ? k.dense->ordered.heads_capacity_bytes() : 0,
      k.dense ? k.dense->ordered.count_capacity_bytes() : 0};
  static std::array<size_t, 9> previous{};
  static const Kernel *owner = nullptr;
  if (owner == &k && capacities == previous)
    return;
  owner = &k;
  previous = capacities;
  trace_slot_memory(k, "BVECapacity", stage, 0, var + 1);
}

struct BinarySetSnapshotState {
  bool initialized = false;
  FILE *out = nullptr;
  uint64_t sequence = 0;
};
BinarySetSnapshotState binary_set_snapshot;

// The bit image of a score, as the digests fold it.
uint64_t double_bits(double value) {
  uint64_t bits = 0;
  static_assert(sizeof bits == sizeof value);
  std::memcpy(&bits, &value, sizeof bits);
  return bits;
}

// The score heap's size, the hash of its members' external variables in heap
// order, and the hash of each such member followed by its score bits.
struct HeapDigest {
  size_t size = 0;
  uint64_t hash = FNV_OFFSET;
  uint64_t score_hash = FNV_OFFSET;
};

HeapDigest heap_digest(const Kernel &k) {
  HeapDigest digest;
  if (!k.h)
    return digest;
  digest.size = k.h->scores.stack.size();
  for (const Var v : k.h->scores.stack) {
    const uint64_t external = k.internal_to_external[v] + 1ull;
    digest.hash = mix(digest.hash, external);
    digest.score_hash = mix(mix(digest.score_hash, external),
                            double_bits(k.h->scores.score[v]));
  }
  return digest;
}

// Order-free hash of a binary clause's external literal pair.
uint64_t binary_pair_hash(const Kernel &k, Lit lit, Lit other) {
  int a = kernel_externalize(k, lit);
  int b = kernel_externalize(k, other);
  if (b < a)
    std::swap(a, b);
  return mix_signed(mix_signed(FNV_OFFSET, a), b);
}

// Adds the count and the summed per-literal hashes of root-fixed literals;
// `skip_retired` leaves retired variables out.
void root_fixed_digest(const Kernel &k, bool skip_retired, uint64_t &count,
                       uint64_t &hash_sum) {
  for (Var v = 0; v < k.nvars; ++v) {
    if (skip_retired && kernel_var_retired(k, v))
      continue;
    const Lit positive = make_lit(v, false);
    const int8_t value = k.values[positive];
    if (!value)
      continue;
    const Lit fixed = value > 0 ? positive : negate(positive);
    hash_sum += mix_signed(FNV_OFFSET, kernel_externalize(k, fixed));
    ++count;
  }
}

uint64_t state_digest(const Kernel &k) {
  uint64_t h = mix(FNV_OFFSET, k.level);
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

  // SKIP_HEAP keeps the scalar populations without an O(nvars) top32 copy.
  if (!include_top)
    return res;

  // Focused VMTF is ordered from queue_last backwards; walk only the printed
  // prefix, as Kissat's focused_top does.
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
    // Ties break by first-import order, Kissat's internal index order.
    return kernel_import_var_order(k, a) < kernel_import_var_order(k, b);
  });
  std::string top;
  const size_t n = std::min<size_t>(32, ranked.size());
  append_list(top, std::span(ranked).first(n),
              [&](Var v) { return k.internal_to_external[v] + 1; });
  if (!top.empty())
    res.top.swap(top);
  return res;
}

void live_clause_counts(const Kernel &k, uint64_t &irredundant,
                        uint64_t &redundant) {
  irredundant = redundant = 0;
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (c->garbage())
      continue;
    (c->redundant() ? redundant : irredundant)++;
  }
}

void annotate_clause_set(const Kernel &k, std::string &payload) {
  const char *spec = std::getenv("SAT_EVENT_TRACE_CLAUSE_SET");
  if (!spec || !*spec)
    return;
  uint64_t limit = 0;
  const char *conflict_from =
      std::getenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_FROM");
  if (conflict_from && parse_decimal(conflict_from, limit) &&
      k.stats.conflicts < limit)
    return;
  const char *conflict_to =
      std::getenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_TO");
  if (conflict_to && parse_decimal(conflict_to, limit) &&
      k.stats.conflicts > limit)
    return;
  // At most 64 values; a 65th, or a malformed list, selects nothing.
  std::vector<int> selected;
  if (visit_decimal_list<long>(spec, [&](long value) {
        if (selected.size() == 64)
          return false;
        selected.push_back(static_cast<int>(value));
        return true;
      }) != DecimalList::Complete)
    return;
  std::sort(selected.begin(), selected.end());
  payload += "|clause_set=" + std::string(spec) + "|clause_poses=";
  size_t matches = 0;
  std::vector<uint8_t> matched(selected.size());
  for (const auto [clause_ref, clause] : arena_records(k.arena)) {
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
    append_parts(payload, "id=", clause->id(), ",ref=", clause_ref,
                 ",order=", kernel_clause_position(k, clause_ref),
                 ",searched=", clause->searched, ",redundant=",
                 clause->redundant(), ",glue=", clause->glue(), ",used=",
                 clause->used(), ",priority=", clause->vivify(), ",pose=");
    append_list(payload, *clause, ExternalLiteral{k});
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

// Prints the conflict, decision and propagation counts, the active and
// unassigned variables, the trail size and the live irredundant, redundant and
// binary clause counts, tab-separated.
void print_formula_state(FILE *out, const Kernel &k) {
  uint64_t irredundant = 0, redundant = 0;
  live_clause_counts(k, irredundant, redundant);
  std::fprintf(out, "%llu\t%llu\t%llu\t%llu\t%u\t%zu\t%llu\t%llu\t%llu",
               (unsigned long long)k.stats.conflicts,
               (unsigned long long)k.stats.decisions,
               (unsigned long long)k.stats.propagations,
               (unsigned long long)kernel_active_variables(k),
               k.unassigned_active, k.trail.size(),
               (unsigned long long)irredundant,
               (unsigned long long)redundant,
               (unsigned long long)live_binary_clause_count(k));
}

static void emit_standalone_inprocess_score(const Kernel &k,
                                            const char *label) {
  const char *path = std::getenv("TACTIX_INPROCESS_SCORE_TRACE");
  const char *selected = std::getenv("SAT_INPROCESS_SCORE_EXTERNAL");
  if (!path || !*path || !selected || !*selected || !k.h)
    return;
  if (other_conflict(k, "SAT_INPROCESS_SCORE_CONFLICT"))
    return;
  const int external = std::abs(std::atoi(selected));
  const Lit lit = kernel_internalize(k, external);
  if (lit == INVALID_LIT)
    return;
  static FILE *out = nullptr;
  if (!out)
    out = open_line_trace(path, "TACTIX_INPROCESS_SCORE_TRACE",
                          "# tactix-inprocess-score\t1\n"
                          "# solver\tlabel\tconflicts\tdecisions\texternal\t"
                          "score\tposition\tvalue\tactive\n");
  const Var v = var_of(lit);
  std::fprintf(out, "tactix\t%s\t%llu\t%llu\t%d\t%.17g\t", label,
               (unsigned long long)k.stats.conflicts,
               (unsigned long long)k.stats.decisions, external,
               k.h->scores.score[v]);
  if (heuristics_heap_contains(k, v))
    std::fprintf(out, "%u", k.h->scores.pos[v]);
  else
    std::fputc('-', out);
  std::fprintf(out, "\t%d\t%u\n", k.values[make_lit(v, false)],
               !kernel_var_retired(k, v));
}

static void emit_standalone_macro_active_set(const Kernel &k,
                                             const char *event) {
  static bool dumped = false;
  if (dumped)
    return;
  const char *path = std::getenv("TACTIX_MACRO_ACTIVE_SET");
  const char *selected = std::getenv("SAT_MACRO_ACTIVE_SET_EVENT");
  if (!path || !*path || !selected || std::strcmp(event, selected) ||
      other_conflict(k, "SAT_MACRO_ACTIVE_SET_CONFLICT"))
    return;
  FILE *out = open_trace_or_exit(path, "TACTIX_MACRO_ACTIVE_SET");
  std::fprintf(out, "# tactix-macro-active-set\t1\n");
  std::fprintf(out, "# solver\tevent\tconflicts\tactive\n");
  std::fprintf(out, "# tactix\t%s\t%llu\t%llu\n", event,
               (unsigned long long)k.stats.conflicts,
               (unsigned long long)kernel_active_variables(k));
  std::fputs("# external\n", out);
  for (Var v = 0; v < k.nvars; ++v)
    if (!kernel_var_retired(k, v) &&
        (!k.values[make_lit(v, false)] || k.assigned[v].level != 0))
      std::fprintf(out, "%d\n",
                   kernel_externalize(k, make_lit(v, false)));
  std::fclose(out);
  dumped = true;
}

static void emit_standalone_macro_boundary(const Kernel &k,
                                           const char *event) {
  // Called only at DefaultPolicy's outer macro sites; each row is O(1).
  emit_standalone_inprocess_score(k, event);
  emit_standalone_macro_active_set(k, event);
  static FILE *out = nullptr;
  static uint64_t conflict_from = 0;
  static uint64_t sequence = 0;
  if (!out) {
    const char *path = std::getenv("TACTIX_MACRO_BOUNDARY_TRACE");
    if (!path || !*path)
      return;
    read_decimal("SAT_MACRO_BOUNDARY_CONFLICT_FROM", conflict_from);
    out = open_line_trace(path, "TACTIX_MACRO_BOUNDARY_TRACE",
                          "# tactix-macro-boundary\t1\n"
                          "# solver\tseq\tevent\tconflicts\tdecisions\t"
                          "propagations\tlevel\tmode\tunassigned\ttrail\t"
                          "restarts\treductions\tswitched\trephased\tstamp\n");
  }
  if (k.stats.conflicts < conflict_from)
    return;
  std::fprintf(out,
               "tactix\t%llu\t%s\t%llu\t%llu\t%llu\t%u\t%s\t%u\t"
               "%zu\t%llu\t%llu\t%llu\t%llu\t%u\n",
               (unsigned long long)sequence++, event,
               (unsigned long long)k.stats.conflicts,
               (unsigned long long)k.stats.decisions,
               (unsigned long long)k.stats.propagations, k.level,
               !k.h ? "none" : k.h->stable ? "stable" : "focused",
               k.unassigned_active, k.trail.size(),
               (unsigned long long)k.stats.restarts,
               (unsigned long long)k.stats.reductions,
               (unsigned long long)k.stats.switched,
               (unsigned long long)k.stats.rephased,
               !k.h ? 0u : k.h->queue_stamp);
  const char *state_event = std::getenv("SAT_MACRO_BOUNDARY_STATE_EVENT");
  const char *state_path = std::getenv("TACTIX_MACRO_BOUNDARY_STATE");
  if (state_path && *state_path && state_event && *state_event &&
      !std::strcmp(event, state_event)) {
    static FILE *state = nullptr;
    if (!state)
      state = open_line_trace(state_path, "TACTIX_MACRO_BOUNDARY_STATE",
                              "# tactix-macro-state\t1\n"
                              "# solver\tevent\tconflicts\tdecisions\t"
                              "propagations\tactive\tunassigned\ttrail\t"
                              "irred\tred\tbinary\n");
    std::fprintf(state, "tactix\t%s\t", event);
    print_formula_state(state, k);
    std::fputc('\n', state);
  }
  const char *exit_event = std::getenv("SAT_MACRO_BOUNDARY_EXIT_EVENT");
  if (exit_event && *exit_event && !std::strcmp(event, exit_event)) {
    std::fflush(out);
    std::exit(0);
  }
}

// The set-snapshot path named by `path_environment` when the
// `match_environment` text occurs in `event|payload` at exactly the
// `conflict_environment` conflict; null otherwise.
const char *selected_set_snapshot(const Kernel &k, const char *event,
                                  const char *payload,
                                  const char *path_environment,
                                  const char *match_environment,
                                  const char *conflict_environment) {
  const char *path = std::getenv(path_environment);
  const char *match = std::getenv(match_environment);
  const char *conflict = std::getenv(conflict_environment);
  uint64_t selected = 0;
  if (!path || !*path || !match || !*match || !conflict ||
      !parse_decimal(conflict, selected) || selected != k.stats.conflicts)
    return nullptr;
  const std::string selector =
      std::string(event) + "|" + (payload ? payload : "-");
  return selector.find(match) == std::string::npos ? nullptr : path;
}

void emit_binary_set_snapshot(const Kernel &k, const char *event,
                              const char *payload) {
  const char *path = selected_set_snapshot(
      k, event, payload, "SAT_EVENT_TRACE_BINARY_SET_PATH",
      "SAT_EVENT_TRACE_BINARY_SET_MATCH",
      "SAT_EVENT_TRACE_BINARY_SET_CONFLICT");
  if (!path)
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

// The free event functions that only forward to the active trace.
#define TACTIX_TRACE_FORWARD(name, parameters, arguments)                     \
  void event_trace_##name parameters {                                        \
    if (active_trace)                                                         \
      active_trace->name arguments;                                           \
  }
#include "diagnostics/event_trace_forwarders.inc"

void emit_formula_set_snapshot(const Kernel &k, const char *event,
                               const char *payload) {
  // Only the first selected boundary is dumped, even if it cannot be opened.
  static bool selected = false;
  if (selected)
    return;
  const char *path = selected_set_snapshot(
      k, event, payload, "SAT_EVENT_TRACE_FORMULA_SET_PATH",
      "SAT_EVENT_TRACE_FORMULA_SET_MATCH",
      "SAT_EVENT_TRACE_FORMULA_SET_CONFLICT");
  if (!path)
    return;
  selected = true;
  FILE *out = std::fopen(path, "w");
  if (!out)
    return;
  std::fprintf(out, "# tactix-formula-set\t2\n");
  std::fprintf(out, "# redundant\tsize\thash\tliterals\n");
  const bool include_garbage =
      std::getenv("SAT_EVENT_TRACE_FORMULA_SET_INCLUDE_GARBAGE") != nullptr;
  for (const auto [ref, clause] : arena_records(k.arena)) {
    if (!clause->garbage() || include_garbage) {
      const auto external = sorted_external_clause<int32_t>(k, *clause);
      std::fprintf(out, "%u\t%u\t%016llx\t", clause->redundant(),
                   clause->size, (unsigned long long)fnv_hash(external));
      print_list(out, external);
      if (std::getenv("SAT_EVENT_TRACE_FORMULA_SET_META"))
        std::fprintf(out,
                     "\tglue=%u\tpriority=%u\tsearched=%u\tused=%u"
                     "\treason=%u\tshrunken=%u",
                     clause->glue(), static_cast<unsigned>(clause->vivify()),
                     clause->searched, clause->used(),
                     static_cast<unsigned>(clause->reason()),
                     static_cast<unsigned>(clause->shrunken()));
      if (include_garbage)
        std::fprintf(out, "\tgarbage=%u", clause->garbage());
      std::fputc('\n', out);
    }
  }
  std::fclose(out);
}

void emit_heap_set_snapshot(const Kernel &k) {
  static bool dumped = false;
  if (dumped || !k.h || !k.h->stable)
    return;
  const char *path = std::getenv("SAT_EVENT_TRACE_HEAP_SET_PATH");
  const char *conflict = std::getenv("SAT_EVENT_TRACE_HEAP_SET_CONFLICT");
  const char *decision = std::getenv("SAT_EVENT_TRACE_HEAP_SET_DECISION");
  if (!path || !*path || !conflict || !*conflict || !decision || !*decision)
    return;
  uint64_t selected_conflict = 0, selected_decision = 0;
  if (!parse_decimal(conflict, selected_conflict) ||
      !parse_decimal(decision, selected_decision) ||
      selected_conflict != k.stats.conflicts ||
      selected_decision != k.stats.decisions)
    return;
  FILE *out = std::fopen(path, "w");
  if (!out)
    return;
  std::fputs("# tactix-heap-set\t1\n", out);
  std::fputs("# external\tposition\tscore\tassigned\tactive\n", out);
  for (size_t position = 0; position != k.h->scores.stack.size(); ++position) {
    const Var v = k.h->scores.stack[position];
    std::fprintf(out, "%d\t%zu\t%.17g\t%u\t%u\n",
                 kernel_externalize(k, make_lit(v, false)), position,
                 k.h->scores.score[v],
                 k.values[make_lit(v, false)] != 0,
                 !kernel_var_retired(k, v));
  }
  std::fclose(out);
  dumped = true;
}

EventTrace::~EventTrace() { close(); }

bool EventTrace::open_from_env(const Kernel &k) {
  const char *path = std::getenv("TACTIX_EVENT_TRACE");
  if (!path || !*path)
    return false;
  const auto flag = [](const char *environment) {
    return std::getenv(environment) != nullptr;
  };
  read_decimal("TACTIX_EVENT_TRACE_LIMIT", limit_);
  detail_window_ |=
      read_decimal("TACTIX_EVENT_TRACE_CONFLICT_FROM", detail_conflict_from_);
  detail_window_ |=
      read_decimal("TACTIX_EVENT_TRACE_CONFLICT_TO", detail_conflict_to_);
  read_decimal("TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT", stop_after_conflict_);
  read_decimal("TACTIX_EVENT_TRACE_HEAP_REMOVE_DECISION",
               heap_remove_decision_);
  compact_probe_ = flag("TACTIX_EVENT_TRACE_COMPACT_PROBE");
  boundary_only_ = flag("TACTIX_EVENT_TRACE_BOUNDARY_ONLY");
  if (const char *match =
          std::getenv("SAT_EVENT_TRACE_EXIT_AFTER_MATCH"))
    exit_match_ = match;
  read_decimal("SAT_EVENT_TRACE_EXIT_CONFLICT", exit_conflict_);
  target_only_ = !exit_match_.empty() && flag("SAT_EVENT_TRACE_TARGET_ONLY");
  skip_heap_ = flag("TACTIX_EVENT_TRACE_SKIP_HEAP");
  heap_vars_only_ = flag("TACTIX_EVENT_TRACE_HEAP_VARS_ONLY");
  if (const char *spec = std::getenv("SAT_EVENT_TRACE_HEAP_VARS"))
    append_absolute_integers(spec, heap_vars_);
  skip_randec_draws_ = flag("TACTIX_EVENT_TRACE_SKIP_RANDEC_DRAWS");
  skip_analysis_ticks_ = flag("TACTIX_EVENT_TRACE_SKIP_ANALYSIS_TICKS");
  keep_snapshots_ = flag("TACTIX_EVENT_TRACE_KEEP_SNAPSHOTS");
  search_only_ = flag("TACTIX_EVENT_TRACE_SEARCH_ONLY");
  glue_only_ = flag("TACTIX_EVENT_TRACE_GLUE_ONLY");
  formula_only_ = flag("TACTIX_EVENT_TRACE_FORMULA_ONLY");
  trajectory_only_ = flag("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY");
  learned_only_ = flag("TACTIX_EVENT_TRACE_LEARNED_ONLY");
  imply_only_ = flag("TACTIX_EVENT_TRACE_IMPLY_ONLY");
  factor_fresh_only_ = flag("SAT_EVENT_TRACE_FACTOR_FRESH_ONLY");
  factor_update_only_ = flag("SAT_EVENT_TRACE_FACTOR_UPDATE_ONLY");
  factor_literal_only_ = flag("SAT_EVENT_TRACE_FACTOR_LITERAL_ONLY");
  factor_seed_only_ = flag("SAT_EVENT_TRACE_FACTOR_SEED_ONLY");
  clause_lifecycle_only_ = flag("SAT_EVENT_TRACE_CLAUSE_LIFECYCLE_ONLY");
  if (const char *spec = std::getenv("SAT_EVENT_TRACE_IMPLY_LITERALS"))
    append_absolute_integers(spec, imply_literals_);
  bve_pop_only_ = flag("TACTIX_EVENT_TRACE_BVE_POP_ONLY");
  conflict_only_ = flag("TACTIX_EVENT_TRACE_CONFLICT_ONLY");
  sweep_only_ = flag("TACTIX_EVENT_TRACE_SWEEP_ONLY");
  watch_only_ = flag("TACTIX_EVENT_TRACE_WATCH_ONLY");
  watch_only_strict_ = flag("TACTIX_EVENT_TRACE_WATCH_ONLY_STRICT");
  watch_cost_ = flag("TACTIX_EVENT_TRACE_WATCH_COST");
  search_watch_cost_only_ = flag("TACTIX_EVENT_TRACE_SEARCH_WATCH_COST_ONLY");
  watch_within_vivify_ = flag("TACTIX_EVENT_TRACE_WATCH_WITHIN_VIVIFY");
  out_ = std::fopen(path, "w");
  if (!out_)
    return false;
  // Line buffering keeps the last completed boundary visible while running.
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
  const bool exit_after_match =
      exit_selector_matches(event, payload, exit_match_.c_str()) &&
      (exit_conflict_ == UINT64_MAX || exit_conflict_ == k.stats.conflicts);
  if (target_only_ && !exit_after_match)
    return;
  // Strict watch capture rejects even macro snapshots; WATCH_ONLY keeps them.
  if (watch_only_strict_ && std::strcmp(event, "watch"))
    return;
  if (factor_fresh_only_ && !event_in(event, {"trace_begin", "factor_fresh"}) &&
      !exit_after_match)
    return;
  if (factor_update_only_ &&
      !event_in(event, {"trace_begin", "factor_update", "factor_fresh"}) &&
      !exit_after_match)
    return;
  const bool selected_sweep_commit =
      std::getenv("SAT_EVENT_TRACE_SWEEP_COMMIT") != nullptr;
  if (factor_literal_only_ &&
      !event_in(event, {"trace_begin", "factor_literal_state",
                        "factor_literal_entry", "factor_mark_transition",
                        "binary_add", "factor_fresh"}) &&
      (!selected_sweep_commit ||
       !event_in(event, {"sweep_equivalence", "sweep_commit"})))
    return;
  if (factor_seed_only_ &&
      !event_in(event, {"trace_begin", "factor_seed", "factor_remainder",
                        "factor_candidate", "factor_candidate_source",
                        "factor_candidate_binary_source", "factor_fresh"}) &&
      !exit_after_match)
    return;
  // Selected-clause lifecycle traces retain macro snapshots to follow arena
  // relocation, but reject ordinary search detail before it can consume the
  // row cap or dominate solver time.
  if (clause_lifecycle_only_ && !snapshot &&
      !event_in(event, {"trace_begin", "eliminate_cleanup",
                        "factor_literal_state", "terminate"}))
    return;
  if (clause_lifecycle_only_ && snapshot && !in_detail_window(k))
    return;
  // Sweep-only snapshots honor the detail window.
  if (sweep_only_ && snapshot && !in_detail_window(k))
    return;
  if (conflict_only_ &&
      !event_in(event, {"trace_begin", "conflict", "terminate"}))
    return;
  if (snapshot)
    emit_binary_set_snapshot(k, event, payload);
  if (snapshot)
    emit_formula_set_snapshot(k, event, payload);
  if (!out_)
    return;
  if (learned_only_ &&
      !event_in(event, {"trace_begin", "learned", "terminate"}))
    return;
  if (imply_only_ && !event_in(event, {"trace_begin", "imply", "terminate"}))
    return;
  // Cumulative BVE-pop checkpoints locate a dynamic heap split.
  if (bve_pop_only_ &&
      !event_in(event, {"trace_begin", "bve_schedule", "bve_pop_checkpoint",
                        "terminate"}))
    return;
  if (heap_vars_only_ &&
      !event_in(event, {"trace_begin", "heap_insert", "heap_remove", "heap_pop",
                        "root_assign", "substitute_map", "analysis_end",
                        "terminate"}))
    return;
  if (formula_only_ &&
      !event_in(event, {"trace_begin", "formula_checkpoint", "terminate"}))
    return;
  if (snapshot && !keep_snapshots_ && stop_after_conflict_ != UINT64_MAX &&
      k.stats.conflicts < stop_after_conflict_ &&
      !event_in(event, {"analysis_end", "formula_checkpoint"}))
    return;
  if (glue_only_ &&
      !event_in(event, {"reason_glue", "reason_used", "retier",
                        "analysis_end"}))
    return;
  if (skip_heap_ && !std::strncmp(event, "heap_", 5) &&
      std::strcmp(event, "heap_reinsert"))
    return;
  if (skip_randec_draws_ && !std::strcmp(event, "randec_draw"))
    return;
  if (skip_analysis_ticks_ && !std::strncmp(event, "analysis_tick", 13))
    return;
  if (watch_only_ && !snapshot &&
      !event_in(event, {"watch", "watch_move", "watch_cost", "reason_glue",
                        "vivify_candidate", "vivify_candidate_end",
                        "vivify_budget_stop"}))
    return;
  if (search_only_ && !snapshot &&
      !event_in(event, {"decision", "randec_draw", "randec_state", "phase_pick",
                        "score_bump", "heap_insert", "heap_remove",
                        "heap_reinsert", "heap_scores", "binary_add",
                        "sweep_seed", "sweep_environment",
                        "sweep_environment_clause", "sweep_query",
                        "sweep_query_model", "sweep_equivalence",
                        "sweep_commit", "sweep_rewrite_binary", "watch",
                        "imply", "conflict"}) &&
      !event_prefixed(event, {"analyzed", "analysis_", "reason_", "learned"}))
    return;
  // Minimal portable search trajectory.  Keep macro/snapshot boundaries and
  // only the choices plus learned-clause transaction needed to locate the
  // first semantic divergence before enabling the high-volume internals.
  if (sweep_only_ && !snapshot &&
      !event_in(event, {"sweep_schedule", "sweep_seed", "sweep_environment",
                        "sweep_environment_clause", "sweep_environment_var",
                        "sweep_binary_attempt", "sweep_effort", "sweep_query",
                        "sweep_query_model", "sweep_equivalence",
                        "sweep_commit", "sweep_flip", "sweep_rewrite_binary",
                        "sweep_rewrite_occurrence", "sweep_unit"}))
    return;
  if (trajectory_only_ && !snapshot &&
      !(sweep_only_ && !std::strncmp(event, "sweep_", 6)) &&
      !event_in(event, {"decision", "randec_draw", "randec_state", "phase_pick",
                        "conflict", "mode_clock", "analysis_begin",
                        "analysis_end",
                        "analysis_eager_subsumed", "analysis_otfs_strengthened",
                        "analysis_otfs_subsumed"}) &&
      !event_prefixed(event, {"backbone_", "learned"}))
    return;
  if (compact_probe_ &&
      !event_in(event, {"trace_begin", "probe_begin", "probe_end",
                        "preprocess_begin", "preprocess_end",
                        "slot_boundary"}) &&
      !event_prefixed(event, {"factor_", "bve_", "eliminate_", "substitute_"}))
    return;
  if (boundary_only_ &&
      !event_in(event, {"trace_begin", "probe_begin", "probe_end",
                        "preprocess_begin", "preprocess_end", "slot_boundary"}))
    return;
  // A requested terminal observation is stronger than the ordinary row cap.
  // Otherwise a noisy retained family can exhaust the cap and make an
  // explicitly matched observer silently run to solver completion.
  if (emitted_ >= limit_ && !exit_after_match) {
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
    active = kernel_active_variables(k);
  }
  int search_external = 0;
  if (k.h && k.h->search_idx != INVALID_VAR &&
      k.h->search_idx < k.internal_to_external.size())
    search_external = static_cast<int>(k.internal_to_external[k.h->search_idx]) + 1;
  std::string annotated_payload = payload ? payload : "-";
  if (snapshot) {
    if (std::getenv("TACTIX_EVENT_TRACE_FORMULA_MACRO")) {
      const FormulaDigest live = formula_digest(k);
      uint64_t formula_binary_hash = 0, root_fixed_hash = 0;
      uint64_t root_fixed = 0;
      // Sum of canonical per-copy pair hashes: independent of watch direction
      // and traversal order, keeping duplicate multiplicity.
      for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
        for (const WatchEntry watch : k.watches.entries(lit))
          if (watch.binary() && lit < watch.other())
            formula_binary_hash += binary_pair_hash(k, lit, watch.other());
      root_fixed_digest(k, false, root_fixed, root_fixed_hash);
      char formula[384];
      std::snprintf(
          formula, sizeof formula,
          "|formula_irredundant=%llu|formula_redundant=%llu|"
          "formula_irredundant_hash=%016llx|formula_redundant_hash=%016llx|"
          "formula_binary_hash=%016llx|root_fixed=%llu|"
          "root_fixed_hash=%016llx",
          (unsigned long long)live.irredundant,
          (unsigned long long)live.redundant,
          (unsigned long long)live.irredundant_hash,
          (unsigned long long)live.redundant_hash,
          (unsigned long long)formula_binary_hash,
          (unsigned long long)root_fixed,
          (unsigned long long)root_fixed_hash);
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
      if (compact_targets) {
        uint64_t hash = FNV_OFFSET;
        size_t count = 0, pos_a = 0, pos_b = 0;
        int lit_a = 0, lit_b = 0;
        if (watched != INVALID_LIT)
          for (const WatchEntry entry : k.watches.entries(watched)) {
            if (!entry.binary())
              continue;
            const int other = kernel_externalize(k, entry.other());
            hash = mix_signed(hash, other);
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
  if (exit_after_match) {
    std::fflush(out_);
    std::exit(0);
  }
}

template <class... Parts>
void EventTrace::emit_parts(const Kernel &k, const char *event,
                            const Parts &...parts) {
  emit(k, event, concat(parts...).c_str(), false);
}

void EventTrace::macro(const Kernel &k, const char *event, const char *payload) {
  emit_standalone_macro_boundary(k, event);
  // A late trace opens only at its exact event/conflict selector, so the
  // preceding search keeps the dormant fast path. `open_from_env` installs
  // `out_` before trace_begin, so its nested macro call cannot recurse here.
  if (!out_)
    if (const char *path = late_trace_path(k, event, payload)) {
      setenv("TACTIX_EVENT_TRACE", path, 1);
      if (!open_from_env(k))
        std::abort();
    }
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
  const auto external = sorted_external_clause(k, *clause);
  std::string payload = concat(
      "ordinal=", ordinal, "|ticks=", ticks, "|limit=", limit, "|size=",
      clause->size, "|redundant=", clause->redundant(), "|glue=",
      clause->glue(), "|garbage=", clause->garbage(), "|clause=");
  append_list(payload, external);
  payload += "|pose=";
  append_list(payload, *clause, ExternalLiteral{k});
  append_parts(payload, "|priority=", clause->vivify(), "|arena_order=",
               kernel_clause_position(k, cref), "|ref=", cref, "|decisions=");
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
      append_parts(payload, counts[lit], "@raw=", lit, "@import=",
                   kernel_import_lit_order(k, lit), "@compact=",
                   kernel_compact_import_lit_order(k, lit));
      if (std::getenv("SAT_EVENT_TRACE_VIVIFY_CLAUSES")) {
        for (const auto [clause_ref, clause] : arena_records(k.arena)) {
          if (clause->garbage() ||
              std::find(clause->begin(), clause->end(), lit) == clause->end())
            continue;
          const auto semantic = sorted_external_clause(k, *clause);
          std::string pose, set;
          append_list(pose, *clause, ExternalLiteral{k});
          append_list(set, semantic);
          const uint64_t hash = mix(fnv_signed(semantic), clause->size);
          char hex[17];
          std::snprintf(hex, sizeof hex, "%016llx",
                        static_cast<unsigned long long>(hash));
          emit_parts(k, "vivify_occurrence_clause", "literal=", external,
                     "|hash=", hex, "|size=", clause->size, "|redundant=",
                     clause->redundant(), "|glue=", clause->glue(),
                     "|priority=", clause->vivify(), "|id=", clause->id(),
                     "|ref=", clause_ref, "|arena_order=",
                     kernel_clause_position(k, clause_ref), "|clause=", set,
                     "|pose=", pose);
        }
      }
    }
    cursor = *end ? end + 1 : end;
  }
  if (*cursor)
    return;
  const FormulaDigest live = formula_digest(k);
  char summary[160];
  std::snprintf(summary, sizeof summary,
                "|irredundant=%llu|redundant=%llu|"
                "irredundant_hash=%016llx|redundant_hash=%016llx",
                (unsigned long long)live.irredundant,
                (unsigned long long)live.redundant,
                (unsigned long long)live.irredundant_hash,
                (unsigned long long)live.redundant_hash);
  payload += summary;
  emit(k, "vivify_counts", payload.c_str(), false);
}

void EventTrace::retier(const Kernel &k, const RetierSchedule &tiers,
                        uint32_t mode, const uint64_t *used_glue) {
  assert(mode < 2);
  std::string payload =
      concat("mode=", mode ? "stable" : "focused", "|focused=",
             tiers.tier1_by_mode[0], ",", tiers.tier2_by_mode[0], "|stable=",
             tiers.tier1_by_mode[1], ",", tiers.tier2_by_mode[1], "|posed=",
             k.tier1, ",", k.tier2, "|used=");
  uint64_t total = 0;
  for (uint32_t glue = 0; glue <= Stats::MAX_GLUE_USED; ++glue) {
    if (glue)
      payload += ',';
    const uint64_t used = used_glue[glue];
    total += used;
    append_parts(payload, glue, ":", used);
  }
  append_parts(payload, "|total=", total);
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
    if (!a.affine && !a.binary && a.reason != DECISION_REASON &&
        a.reason != UNIT_REASON)
      reason[a.reason] = 1;
  }
  uint64_t irredundant = 0, live = 0;
  live_clause_counts(k, irredundant, live);
  const uint64_t reduction = k.stats.reductions + 1;
  std::fprintf(file,
               "tactix\tsummary\t%llu\t%llu\t%s\t%u\t%u\t%llu\t"
               "%zu\t%zu\t-\t-\t-\t-\t-\t-\t-\t-\t-\n",
               (unsigned long long)reduction,
               (unsigned long long)k.stats.conflicts,
               k.h && k.h->stable ? "stable" : "focused", k.tier1, k.tier2,
               (unsigned long long)live, candidates.size(), victims);
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (!c->redundant() || c->garbage())
      continue;
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
    const auto literals = sorted_external_clause(k, *c);
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
    print_list(file, literals);
    std::fputc('\n', file);
  }
  std::fflush(file);
}

bool EventTrace::want_detail(const Kernel &k) {
  if (!out_)
    return false;
  // Learned-only is a low-perturbation trajectory observer.  Reject other
  // detailed helpers before they construct payloads; learned_stage applies
  // the same conflict window directly below.
  if (imply_only_)
    return false;
  if (learned_only_ || heap_vars_only_ || factor_fresh_only_ ||
      factor_update_only_ || factor_literal_only_ || clause_lifecycle_only_)
    return false;
  // detail() invokes formula_checkpoint before consulting this gate, so
  // formula-only can reject every expensive ordinary detail payload here.
  if (formula_only_ || conflict_only_)
    return false;
  return in_detail_window(k);
}

void EventTrace::detail(const Kernel &k, const char *event, int64_t payload) {
  const bool analysis_end = !std::strcmp(event, "analysis_end");
  if (analysis_end && !learned_only_)
    formula_checkpoint(k);
  const bool stop_here = analysis_end &&
                         stop_after_conflict_ != UINT64_MAX &&
                         k.stats.conflicts >= stop_after_conflict_;
  if (heap_vars_only_ && !stop_here)
    return;
  if (!stop_here && !want_detail(k))
    return;
  // The post-Reorder detail budget bounds internals; trajectory-only traces
  // keep every analysis boundary.
  if (!trajectory_only_ && !detail_window_ && saw_reorder_) {
    if (!post_reorder_details_)
      return;
    post_reorder_details_--;
  }
  char buf[32];
  std::snprintf(buf, sizeof buf, "%lld", (long long)payload);
  // Analysis is the only detailed event that changes the ranking, so ordinary
  // traces snapshot its post-state; trajectory-only traces skip the scan.
  const bool snapshot = !trajectory_only_ && analysis_end;
  if (stop_here && k.h && k.h->stable && !heap_vars_only_)
    emit_heap_scores(k);
  emit(k, event, buf, snapshot);
  if (stop_here) {
    std::fflush(out_);
    terminate_trace_endpoint();
  }
}

static bool formula_checkpoint_threshold_reached(const char *spec,
                                                 uint64_t conflict,
                                                 bool &have_emitted,
                                                 uint64_t &last_emitted) {
  if (!spec || !*spec)
    return false;
  bool found = false;
  uint64_t reached = 0;
  if (visit_decimal_list<unsigned long long>(spec, [&](uint64_t threshold) {
        if (threshold <= conflict &&
            (!have_emitted || threshold > last_emitted) &&
            (!found || threshold > reached)) {
          reached = threshold;
          found = true;
        }
        return true;
      }) == DecimalList::Malformed)
    return false;
  if (found) {
    have_emitted = true;
    last_emitted = reached;
  }
  return found;
}

void EventTrace::formula_checkpoint(const Kernel &k) {
  if (!out_ || emitted_ >= limit_)
    return;
  if (!formula_checkpoint_threshold_reached(
          std::getenv("TACTIX_EVENT_TRACE_FORMULA_THRESHOLDS"),
          k.stats.conflicts, formula_threshold_emitted_,
          formula_threshold_last_))
    return;
  const FormulaDigest live = formula_digest(k);
  const HeapDigest heap = heap_digest(k);
  if (k.h) {
    const char *heap_dump_path =
        std::getenv("TACTIX_EVENT_TRACE_HEAP_DUMP");
    if (heap_dump_path && *heap_dump_path) {
      if (FILE *heap_dump = std::fopen(heap_dump_path, "a")) {
        for (size_t pos = 0; pos != k.h->scores.stack.size(); ++pos) {
          const Var v = k.h->scores.stack[pos];
          const uint64_t compact =
              kernel_compact_import_lit_order(k, make_lit(v, false)) >> 1;
          const uint64_t imported = kernel_import_var_order(k, v);
          std::fprintf(heap_dump,
                       "tactix\t%llu\t%zu\t%u\t%u\t%llu\t%llu\t%.17g\n",
                       (unsigned long long)k.stats.conflicts, pos,
                       k.internal_to_external[v] + 1,
                       v, (unsigned long long)compact,
                       (unsigned long long)imported,
                       k.h->scores.score[v]);
        }
        std::fclose(heap_dump);
      }
    }
  }
  char checkpoint_payload[360];
  std::snprintf(checkpoint_payload, sizeof checkpoint_payload,
                "irredundant=%llu|redundant=%llu|irredundant_hash=%016llx|"
                "redundant_hash=%016llx|heap=%zu|heap_hash=%016llx|"
                "heap_score_hash=%016llx",
                (unsigned long long)live.irredundant,
                (unsigned long long)live.redundant,
                (unsigned long long)live.irredundant_hash,
                (unsigned long long)live.redundant_hash, heap.size,
                (unsigned long long)heap.hash,
                (unsigned long long)heap.score_hash);
  emit(k, "formula_checkpoint", checkpoint_payload, true);
}

void EventTrace::randec_draw(const Kernel &k, uint32_t raw, uint32_t slot,
                             Var var, bool accepted) {
  if (!want_detail(k))
    return;
  emit_parts(k, "randec_draw", "raw=", raw, "|slot=", slot, "|external=",
             k.internal_to_external[var] + 1, "|accepted=", accepted);
}

void EventTrace::randec_state(const Kernel &k, const char *stage,
                              uint64_t remaining, uint64_t next,
                              uint64_t sequence, uint64_t rng) {
  if (!want_detail(k))
    return;
  emit_parts(k, "randec_state", "stage=", stage, "|remaining=", remaining,
             "|next=", next, "|sequence=", sequence, "|rng=", rng);
}

void EventTrace::watch_scan(const Kernel &k, Lit propagated) {
  // A selected ordinary-search cost trace needs only the O(1) aggregate row
  // emitted after each scan.  Do not construct per-entry watch payloads, and
  // do not admit startup or inprocessing scans that share this hook.
  if (search_watch_cost_only_ ||
      (watch_within_vivify_ && !vivify_watch_active_) || !want_detail(k) ||
      emitted_ >= limit_)
    return;
  const Lit watched = negate(propagated);
  for (const WatchEntry entry : k.watches.entries(watched)) {
    if (emitted_ >= limit_)
      return;
    const Clause *large =
        entry.binary() ? nullptr : clause_at(k.arena, entry.ref());
    const std::vector<int> clause =
        large ? sorted_external_clause(k, *large)
              : sorted_external_clause(k, std::array{watched, entry.other()});
    if (const char *selected_clause =
            std::getenv("TACTIX_EVENT_TRACE_WATCH_CLAUSE_SET")) {
      bool matched = false;
      for (const char *alternative = selected_clause; *alternative;) {
        const char *separator = std::strchr(alternative, ';');
        const char *const alternative_end =
            separator ? separator : alternative + std::strlen(alternative);
        const char *cursor = alternative;
        bool same = true;
        for (size_t i = 0; i != clause.size(); ++i) {
          char *end = nullptr;
          const long wanted = std::strtol(cursor, &end, 10);
          if (end == cursor || end > alternative_end ||
              wanted != clause[i]) {
            same = false;
            break;
          }
          cursor = end;
          if (i + 1 != clause.size()) {
            if (cursor == alternative_end || *cursor != ',') {
              same = false;
              break;
            }
            cursor++;
          }
        }
        if (same && cursor == alternative_end) {
          matched = true;
          break;
        }
        alternative = separator ? separator + 1 : alternative_end;
      }
      if (!matched)
        continue;
    }
    std::string payload =
        concat("scan=", kernel_externalize(k, propagated), "|block=",
               kernel_externalize(k, entry.other()), "|clause=");
    append_list(payload, clause);
    if (large && std::getenv("TACTIX_EVENT_TRACE_WATCH_POSE")) {
      append_parts(payload, "|id=", large->id(), "|order=",
                   kernel_clause_position(k, entry.ref()), "|garbage=",
                   large->garbage(), "|searched=", large->searched, "|pose=");
      append_list(payload, *large, ExternalLiteral{k});
    }
    emit(k, "watch", payload.c_str(), false);
  }
}

void EventTrace::watch_move(const Kernel &k, Lit propagated, Lit target,
                            Lit blocking, const Clause *clause) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected =
      std::getenv("TACTIX_EVENT_TRACE_WATCH_MOVE_CLAUSE_SET");
  if (!selected || !*selected)
    return;
  std::string key;
  append_list(key, sorted_external_clause(k, *clause));
  if (key != selected)
    return;
  std::string payload = concat(
      "scan=", kernel_externalize(k, propagated), "|target=",
      kernel_externalize(k, target), "|block=", kernel_externalize(k, blocking),
      "|searched=", clause->searched, "|pose=");
  append_list(payload, *clause, ExternalLiteral{k});
  emit(k, "watch_move", payload.c_str(), false);
}

void EventTrace::watch_cost(const Kernel &k, Lit propagated,
                            uint64_t words_before, uint64_t words_after,
                            uint64_t base, uint64_t binary_assignments,
                            uint64_t large_dereferences,
                            uint64_t relocations,
                            uint64_t large_assignments, uint64_t total) {
  if (!watch_cost_ ||
      (search_watch_cost_only_ && (k.inprocessing || k.warming)) ||
      (watch_within_vivify_ && !vivify_watch_active_) || !want_detail(k))
    return;
  emit_parts(k, "watch_cost", "round=", k.stats.vivify_rounds, "|scope=",
             k.vivify_scope, "|ordinal=", vivify_watch_ordinal_, "|scan=",
             kernel_externalize(k, propagated), "|words_before=", words_before,
             "|words_after=", words_after, "|base=", base, "|binary_assign=",
             binary_assignments, "|large_deref=", large_dereferences,
             "|relocate=", relocations, "|large_assign=", large_assignments,
             "|total=", total);
}

void event_trace_detail(const Kernel &k, const char *event, int64_t payload) {
  if (active_trace)
    active_trace->detail(k, event, payload);
}

void event_trace_randec_draw(const Kernel &k, uint32_t raw, uint32_t slot,
                             Var var, bool accepted) {
  if (active_trace) {
    active_trace->randec_draw(k, raw, slot, var, accepted);
    return;
  }
  // Random-index localization needs the same dormant fast path as late slot
  // boundaries: reaching a selected search decision with an ordinary active
  // trace is prohibitively expensive on propagation-heavy formulas.
  const char *const path = late_trace_path(k, "randec_draw", "");
  if (!path)
    return;
  if (const char *decision = std::getenv("SAT_EVENT_TRACE_EXIT_DECISION"))
    if (*decision &&
        std::strtoull(decision, nullptr, 10) != k.stats.decisions)
      return;
  setenv("TACTIX_EVENT_TRACE", path, 1);
  EventTrace late;
  if (!late.open_from_env(k))
    std::abort();
  late.randec_draw(k, raw, slot, var, accepted);
}

void EventTrace::phase_pick(const Kernel &k, Var var, int8_t target,
                            int8_t saved, int8_t result) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  emit_parts(k, "phase_pick", "external=",
             kernel_externalize(k, make_lit(var, false)), "|target=", target,
             "|saved=", saved, "|result=", result);
}

void EventTrace::target_save(const Kernel &k) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::vector<int> assigned = sorted_external_clause(k, k.trail);
  std::string payload = concat("assigned=", assigned.size(), "|set=");
  append_list(payload, assigned);
  emit(k, "target_save", payload.c_str(), false);
}

void EventTrace::phase_save(const Kernel &k, Lit lit) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  emit_parts(k, "phase_save", "lit=", kernel_externalize(k, lit));
}

void EventTrace::slot_boundary(const Kernel &k, const char *name,
                               const char *stage) {
  const std::string payload = concat("name=", name, "|stage=", stage);
  const bool exit_after_match = exit_selector_matches(
                                    "slot_boundary", payload.c_str(),
                                    exit_match_.c_str()) &&
                                (exit_conflict_ == UINT64_MAX ||
                                 exit_conflict_ == k.stats.conflicts);
  // Selected-variable mode rejects macro snapshots, but an explicitly selected
  // binary/formula-set boundary still fires.
  if (heap_vars_only_) {
    emit_binary_set_snapshot(k, "slot_boundary", payload.c_str());
    emit_formula_set_snapshot(k, "slot_boundary", payload.c_str());
  }
  // Clause-lifecycle mode keeps slot boundaries to locate the first rule that
  // rewrites or deletes a selected clause.
  if (clause_lifecycle_only_) {
    emit(k, "slot_boundary", payload.c_str(), true);
    return;
  }
  if (!want_detail(k) && !exit_after_match)
    return;
  if (emitted_ >= limit_ && !exit_after_match)
    return;
  emit(k, "slot_boundary", payload.c_str(), true);
}

void event_trace_memory_checkpoint(const Kernel &k, const char *name,
                                   const char *stage, size_t temporary_bytes) {
  trace_slot_memory(k, name, stage, temporary_bytes);
}

void event_trace_slot_boundary(const Kernel &k, const char *name,
                               const char *stage) {
  const std::string score_label = concat(name, "_", stage);
  emit_standalone_inprocess_score(k, score_label.c_str());
  trace_slot_memory(k, name, stage);
  // Set snapshots fire without a global trace; an active trace routes this
  // boundary through EventTrace::emit instead.
  if (!active_trace &&
      (std::getenv("SAT_EVENT_TRACE_BINARY_SET_PATH") ||
       std::getenv("SAT_EVENT_TRACE_FORMULA_SET_PATH"))) {
    const std::string payload = concat("name=", name, "|stage=", stage);
    emit_binary_set_snapshot(k, "slot_boundary", payload.c_str());
    emit_formula_set_snapshot(k, "slot_boundary", payload.c_str());
  }
  if (active_trace) {
    active_trace->slot_boundary(k, name, stage);
    return;
  }
  // A late trace opens only at the selected slot/conflict; the selectors are
  // checked before any payload is built.
  const char *const late_path = std::getenv("TACTIX_EVENT_TRACE_LATE_PATH");
  const char *const match = std::getenv("SAT_EVENT_TRACE_EXIT_AFTER_MATCH");
  if (!late_path || !*late_path || !match || !*match)
    return;
  const std::string payload = concat("name=", name, "|stage=", stage);
  const char *const path = late_trace_path(k, "slot_boundary", payload.c_str());
  if (!path)
    return;
  setenv("TACTIX_EVENT_TRACE", path, 1);
  EventTrace late;
  if (!late.open_from_env(k))
    std::abort();
  late.slot_boundary(k, name, stage);
}

// The external Sweep seed of an admitted detail row when `environment` names
// it (by strtol), otherwise 0, which is never an external literal.
int EventTrace::selected_sweep_seed(const Kernel &k, Var seed,
                                    const char *environment) {
  if (!want_detail(k) || emitted_ >= limit_)
    return 0;
  const char *selected = std::getenv(environment);
  if (!selected)
    return 0;
  const int external = kernel_externalize(k, make_lit(seed, false));
  return std::strtol(selected, nullptr, 10) == external ? external : 0;
}

// Whether the `a,b` pair in `selected` names the external pair, in either
// order or with both literals negated.
static bool sweep_pair_selected(const char *selected, int external_a,
                                int external_b) {
  char *end = nullptr;
  const long selected_a = std::strtol(selected, &end, 10);
  if (end == selected || *end != ',')
    return false;
  const char *second = end + 1;
  const long selected_b = std::strtol(second, &end, 10);
  if (end == second || *end)
    return false;
  const bool direct =
      (external_a == selected_a && external_b == selected_b) ||
      (external_a == selected_b && external_b == selected_a);
  const bool mirrored =
      (external_a == -selected_a && external_b == -selected_b) ||
      (external_a == -selected_b && external_b == -selected_a);
  return direct || mirrored;
}

// Whether `environment` is set and names (by atoi) `lit`'s external literal.
static bool selects_literal(const Kernel &k, const char *environment, Lit lit) {
  const char *selected = std::getenv(environment);
  return selected && std::atoi(selected) == kernel_externalize(k, lit);
}

void EventTrace::sweep_seed(const Kernel &k, Var seed) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  emit_parts(k, "sweep_seed", "external=",
             kernel_externalize(k, make_lit(seed, false)));
}

void EventTrace::sweep_schedule(const Kernel &k, Var seed, uint64_t pos,
                                uint64_t neg, uint64_t rank,
                                uint64_t import_order) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  emit_parts(k, "sweep_schedule", "external=",
             kernel_externalize(k, make_lit(seed, false)), "|internal=", seed,
             "|import=", import_order, "|pos=", pos, "|neg=", neg, "|rank=",
             rank);
}

void EventTrace::sweep_environment(const Kernel &k, Var seed, uint32_t vars,
                                   uint32_t clauses, uint64_t hash) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  char digest[17];
  std::snprintf(digest, sizeof digest, "%016llx",
                static_cast<unsigned long long>(hash));
  emit_parts(k, "sweep_environment", "external=",
             kernel_externalize(k, make_lit(seed, false)), "|vars=", vars,
             "|clauses=", clauses, "|hash=", digest);
}

void EventTrace::sweep_environment_clause(const Kernel &k, Var seed,
                                          uint32_t ordinal, const Lit *lits,
                                          uint32_t size) {
  const int external =
      selected_sweep_seed(k, seed, "SAT_EVENT_TRACE_SWEEP_CLAUSES");
  if (!external)
    return;
  std::string payload =
      concat("seed=", external, "|ordinal=", ordinal, "|order=");
  append_list(payload, std::span(lits, size), ExternalLiteral{k});
  payload += "|set=";
  append_list(payload, sorted_external_clause(k, std::span(lits, size)));
  emit(k, "sweep_environment_clause", payload.c_str(), false);
}

void EventTrace::sweep_query(const Kernel &k, Var seed, uint32_t ordinal,
                             const char *kind, Lit a, Lit b, int result,
                             uint64_t ticks, uint64_t conflicts) {
  const int external =
      selected_sweep_seed(k, seed, "SAT_EVENT_TRACE_SWEEP_QUERIES");
  if (!external)
    return;
  const int ea = a == INVALID_LIT ? 0 : kernel_externalize(k, a);
  const int eb = b == INVALID_LIT ? 0 : kernel_externalize(k, b);
  emit_parts(k, "sweep_query", "seed=", external, "|ordinal=", ordinal,
             "|kind=", kind, "|a=", ea, "|b=", eb, "|result=", result,
             "|ticks=", ticks, "|conflicts=", conflicts);
}

void EventTrace::sweep_model(const Kernel &k, Var seed, uint32_t ordinal,
                             const Var *vars, const int8_t *values,
                             uint32_t size) {
  const int external =
      selected_sweep_seed(k, seed, "SAT_EVENT_TRACE_SWEEP_QUERIES");
  if (!external)
    return;
  std::string payload =
      concat("seed=", external, "|ordinal=", ordinal, "|model=");
  for (uint32_t i = 0; i < size; i++) {
    if (i)
      payload += ',';
    payload += std::to_string(
        kernel_externalize(k, make_lit(vars[i], values[i] < 0)));
  }
  emit(k, "sweep_query_model", payload.c_str(), false);
}

void EventTrace::sweep_flip(const Kernel &k, Var seed, const char *stage,
                            Lit lit, bool result, uint64_t ticks) {
  const int external =
      selected_sweep_seed(k, seed, "SAT_EVENT_TRACE_SWEEP_QUERIES");
  if (!external)
    return;
  emit_parts(k, "sweep_flip", "seed=", external, "|stage=", stage, "|lit=",
             kernel_externalize(k, lit), "|result=", result ? 1 : 0, "|ticks=",
             ticks);
}

void EventTrace::sweep_effort(const Kernel &k, uint32_t external,
                              uint64_t ticks_before, uint64_t ticks_after,
                              uint64_t solves) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  emit_parts(k, "sweep_effort", "external=", external, "|before=", ticks_before,
             "|after=", ticks_after, "|delta=", ticks_after - ticks_before,
             "|solves=", solves);
}

void EventTrace::sweep_environment_var(const Kernel &k, Var seed,
                                       uint32_t ordinal, Var var,
                                       uint32_t depth) {
  const int external =
      selected_sweep_seed(k, seed, "SAT_EVENT_TRACE_SWEEP_VARS");
  if (!external)
    return;
  emit_parts(k, "sweep_environment_var", "seed=", external, "|ordinal=",
             ordinal, "|external=", kernel_externalize(k, make_lit(var, false)),
             "|depth=", depth);
}

void EventTrace::sweep_binary_attempt(const Kernel &k, Var seed,
                                      uint32_t ordinal, Lit lit, Lit other,
                                      int32_t lit_depth, int32_t other_depth,
                                      const char *disposition) {
  const int external =
      selected_sweep_seed(k, seed, "SAT_EVENT_TRACE_SWEEP_BINARIES");
  if (!external)
    return;
  emit_parts(k, "sweep_binary_attempt", "seed=", external, "|ordinal=", ordinal,
             "|lit=", kernel_externalize(k, lit), "|other=",
             kernel_externalize(k, other), "|lit_depth=", lit_depth,
             "|other_depth=", other_depth, "|disposition=", disposition);
}

void EventTrace::sweep_equivalence(const Kernel &k, Lit a, Lit b) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  if (const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_COMMIT"))
    if (!sweep_pair_selected(selected, kernel_externalize(k, a),
                             kernel_externalize(k, b)))
      return;
  emit_parts(k, "sweep_equivalence", "a=", kernel_externalize(k, a), "|b=",
             kernel_externalize(k, b));
}

void EventTrace::sweep_commit(const Kernel &k, Lit a, Lit b, int8_t av,
                              int8_t bv, const char *stage) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const char *selected = std::getenv("SAT_EVENT_TRACE_SWEEP_COMMIT");
  if (!selected || !*selected)
    return;
  const int external_a = kernel_externalize(k, a);
  const int external_b = kernel_externalize(k, b);
  if (!sweep_pair_selected(selected, external_a, external_b))
    return;
  emit_parts(k, "sweep_commit", "a=", external_a, "|b=", external_b, "|av=",
             static_cast<int>(av), "|bv=", static_cast<int>(bv), "|proof=",
             k.proof ? 1 : 0, "|stage=", stage);
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
      concat("source=", kernel_externalize(k, source), "|repr=",
             external_representative, "|position=", position);
  if (binary) {
    append_parts(detail, "|kind=binary|other=",
                 kernel_externalize(k, static_cast<Lit>(payload)));
  } else {
    const Clause *const clause = clause_at(k.arena, static_cast<Ref>(payload));
    detail += "|kind=large|clause=";
    append_list(detail, *clause, ExternalLiteral{k});
  }
  emit(k, "sweep_rewrite_occurrence", detail.c_str(), false);
}

void EventTrace::sweep_unit(const Kernel &k, Lit lit) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  emit_parts(k, "sweep_unit", "lit=", kernel_externalize(k, lit));
}

void EventTrace::factor_seed(const Kernel &k, Lit seed, uint32_t score,
                             uint32_t remainders) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  if (const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_SEED"))
    if (*selected &&
        std::atoi(selected) != kernel_externalize(k, seed))
      return;
  emit_parts(k, "factor_seed", "external=", kernel_externalize(k, seed),
             "|score=", score, "|remainders=", remainders);
}

void EventTrace::factor_remainder(const Kernel &k, Lit seed,
                                  uint32_t ordinal, bool large,
                                  const Lit *lits, uint32_t size) {
  if (!want_detail(k) || emitted_ >= limit_ ||
      !selects_literal(k, "TACTIX_EVENT_TRACE_FACTOR_SEED", seed))
    return;
  std::string payload =
      concat("seed=", kernel_externalize(k, seed), "|ordinal=", ordinal,
             "|source=", large ? "large" : "binary", "|lits=");
  append_list(payload, std::span(lits, size), ExternalLiteral{k});
  emit(k, "factor_remainder", payload.c_str(), false);
}

void EventTrace::factor_candidate(const Kernel &k, Lit seed, uint32_t depth,
                                  uint32_t ordinal, Lit candidate,
                                  uint32_t count, uint32_t score) {
  if (!want_detail(k) || emitted_ >= limit_ ||
      !selects_literal(k, "TACTIX_EVENT_TRACE_FACTOR_SEED", seed))
    return;
  emit_parts(k, "factor_candidate", "seed=", kernel_externalize(k, seed),
             "|depth=", depth, "|ordinal=", ordinal, "|candidate=",
             kernel_externalize(k, candidate), "|count=", count, "|score=",
             score);
}

void EventTrace::factor_literal_state(const Kernel &k, Lit lit, bool marked,
                                      bool retired, int value,
                                      uint32_t score) {
  if ((!factor_literal_only_ && !clause_lifecycle_only_ && !want_detail(k)) ||
      emitted_ >= limit_ ||
      !selects_literal(k, "TACTIX_EVENT_TRACE_FACTOR_LITERAL", lit))
    return;
  emit_parts(k, "factor_literal_state", "external=", kernel_externalize(k, lit),
             "|active=", retired ? "0" : "1", "|marked=", marked ? "1" : "0",
             "|value=", value, "|score=", score);
}

void EventTrace::substitute_map(const Kernel &k, Lit lit, Lit representative) {
  if (!out_ || emitted_ >= limit_)
    return;
  if (heap_vars_only_) {
    if (!heap_var_selected(k, var_of(lit)))
      return;
  } else if (!want_detail(k)) {
    return;
  }
  // Selected-variable mode checked the common list above.
  if (!heap_vars_only_) {
    const char *all = std::getenv("SAT_EVENT_TRACE_SUBSTITUTE_ALL");
    if (!all || !std::atoi(all) || representative == INVALID_LIT ||
        representative == lit)
      return;
  }
  const int external_representative =
      representative == INVALID_LIT ? 0 : kernel_externalize(k, representative);
  emit_parts(k, "substitute_map", "external=", kernel_externalize(k, lit),
             "|representative=", external_representative, "|active=",
             kernel_var_retired(k, var_of(lit)) ? "0" : "1", "|value=",
             k.value(lit));
}

void EventTrace::factor_literal_entry(const Kernel &k, Lit selected_lit,
                                      uint32_t ordinal, bool live, Lit other,
                                      uint64_t id, const Clause *clause) {
  if ((!factor_literal_only_ && !want_detail(k)) || emitted_ >= limit_ ||
      !selects_literal(k, "TACTIX_EVENT_TRACE_FACTOR_LITERAL", selected_lit))
    return;
  std::string payload =
      concat("external=", kernel_externalize(k, selected_lit), "|ordinal=",
             ordinal, "|kind=", clause ? "large" : "binary", "|live=",
             live ? "1" : "0", "|id=", id, "|lits=");
  append_factor_entry_literals(payload, k, selected_lit, other, clause);
  emit(k, "factor_literal_entry", payload.c_str(), false);
}

void EventTrace::factor_candidate_source(const Kernel &k, Lit seed,
                                         uint32_t depth, uint32_t column,
                                         Lit scan, Lit candidate,
                                         const Clause *current,
                                         const Clause *matched) {
  if (!want_detail(k) || emitted_ >= limit_ ||
      !selects_literal(k, "TACTIX_EVENT_TRACE_FACTOR_SEED", seed))
    return;
  std::string payload = concat(
      "seed=", kernel_externalize(k, seed), "|depth=", depth, "|column=",
      column, "|scan=", kernel_externalize(k, scan), "|candidate=",
      kernel_externalize(k, candidate), "|current=");
  append_list(payload, sorted_external_clause(k, *current));
  payload += "|matched=";
  append_list(payload, sorted_external_clause(k, *matched));
  emit(k, "factor_candidate_source", payload.c_str(), false);
}

void EventTrace::factor_candidate_binary_source(const Kernel &k, Lit seed,
                                                uint32_t depth,
                                                uint32_t column, Lit q,
                                                Lit candidate) {
  if (!want_detail(k) || emitted_ >= limit_ ||
      !selects_literal(k, "TACTIX_EVENT_TRACE_FACTOR_SEED", seed))
    return;
  emit_parts(k, "factor_candidate_binary_source", "seed=",
             kernel_externalize(k, seed), "|depth=", depth, "|column=", column,
             "|q=", kernel_externalize(k, q), "|candidate=",
             kernel_externalize(k, candidate));
}

void EventTrace::factor_mark_transition(const Kernel &k, Lit lit, bool marked,
                                        const char *source) {
  if ((!factor_literal_only_ && !want_detail(k)) || emitted_ >= limit_ ||
      !selects_literal(k, "TACTIX_EVENT_TRACE_FACTOR_MARK", lit))
    return;
  emit_parts(k, "factor_mark_transition", "external=",
             kernel_externalize(k, lit), "|marked=", marked ? "1" : "0",
             "|source=", source);
}

void EventTrace::factor_update(const Kernel &k, Lit grid_seed,
                               uint32_t ordinal, Lit lit, uint32_t score) {
  const char *selected_ordinal =
      std::getenv("SAT_EVENT_TRACE_FACTOR_UPDATE_ORDINAL");
  if (!selected_ordinal ||
      std::strtoull(selected_ordinal, nullptr, 10) != k.stats.factorings)
    return;
  if ((!factor_update_only_ && !want_detail(k)) || emitted_ >= limit_)
    return;
  emit_parts(k, "factor_update", "factor=", k.stats.factorings, "|grid=",
             kernel_externalize(k, grid_seed), "|ordinal=", ordinal,
             "|external=", kernel_externalize(k, lit), "|score=", score);
}

void EventTrace::factor_grid(const Kernel &k, Lit seed, uint32_t factors,
                             uint32_t remainders, int delta) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  emit_parts(k, "factor_grid", "external=", kernel_externalize(k, seed),
             "|factors=", factors, "|remainders=", remainders, "|delta=",
             delta);
}

void EventTrace::factor_grid_member(const Kernel &k, uint32_t ordinal,
                                    Lit factor) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  emit_parts(k, "factor_grid_member", "ordinal=", ordinal, "|external=",
             kernel_externalize(k, factor));
}

void EventTrace::factor_fresh(const Kernel &k, Lit seed, Var fresh,
                              const Lit *factors, uint32_t factor_count,
                              uint32_t remainders, int delta,
                              uint64_t ticks, uint64_t pops,
                              uint64_t first_ticks, uint64_t next_ticks,
                              uint64_t match_ticks) {
  if (factor_update_only_) {
    const char *selected =
        std::getenv("SAT_EVENT_TRACE_FACTOR_UPDATE_ORDINAL");
    if (!selected || std::strtoull(selected, nullptr, 10) + 1 !=
                         k.stats.factorings)
      return;
  }
  if (factor_seed_only_) {
    const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
    if (!selected || !*selected ||
        std::atoi(selected) != kernel_externalize(k, seed))
      return;
  }
  if ((!factor_fresh_only_ && !factor_update_only_ &&
       !factor_literal_only_ && !factor_seed_only_ && !want_detail(k)) ||
      emitted_ >= limit_)
    return;
  std::string payload = concat(
      "fresh=", kernel_externalize(k, make_lit(fresh, false)), "|ordinal=",
      k.stats.factorings, "|seed=", kernel_externalize(k, seed), "|factors=",
      factor_count, "|remainders=", remainders, "|delta=", delta, "|members=");
  append_list(payload, std::span(factors, factor_count), ExternalLiteral{k});
  if (std::getenv("SAT_EVENT_TRACE_FACTOR_METER"))
    append_parts(payload, "|ticks=", ticks);
  if (std::getenv("SAT_EVENT_TRACE_FACTOR_METER_COMPONENTS"))
    append_parts(payload, "|pops=", pops, "|first_ticks=", first_ticks,
                 "|next_ticks=", next_ticks, "|match_ticks=", match_ticks);
  if (factor_literal_only_ && !exit_match_.empty() &&
      !exit_selector_matches("factor_fresh", payload.c_str(),
                             exit_match_.c_str()))
    return;
  emit(k, "factor_fresh", payload.c_str(), false);
}

void EventTrace::bve_candidate(const Kernel &k, Var var, const char *stage,
                               uint64_t ticks, uint64_t limit, bool applied) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  const std::string payload = concat(
      "external=", k.internal_to_external[var] + 1, "|stage=", stage,
      "|pivot_value=", static_cast<int>(k.value(make_lit(var, false))),
      "|ticks=", ticks, "|limit=", limit, "|applied=", applied ? "1" : "0");
  emit(k, "bve_candidate", payload.c_str(), false);
}

// Whether the `conflict_environment` and `external_environment` selectors,
// each when set and nonempty, name the current conflict and `var`.
static bool selects_conflict_and_var(const Kernel &k,
                                     const char *conflict_environment,
                                     const char *external_environment,
                                     Var var) {
  const bool conflict = !other_conflict(k, conflict_environment);
  const char *external = std::getenv(external_environment);
  return conflict && (!external || !*external ||
                      std::strtoul(external, nullptr, 10) ==
                          k.internal_to_external[var] + 1);
}

void event_trace_bve_candidate(const Kernel &k, Var var, const char *stage,
                               uint64_t ticks, uint64_t limit, bool applied) {
  trace_bve_capacity_change(k, var, stage);
  if (std::getenv("TACTIX_BVE_RETRY_TRACE") &&
      !other_conflict(k, "SAT_BVE_RETRY_CONFLICT")) {
    // The candidate owns the ensuing policy-level root propagation and
    // connected-unit flush too.  Keep it current across the rule's `end`
    // callback; the next candidate `begin` replaces it.
    bve_retry_trace_candidate = k.internal_to_external[var] + 1;
  }
  if (const char *path = std::getenv("TACTIX_BVE_CANDIDATE_TRACE")) {
    const bool selected = selects_conflict_and_var(
        k, "SAT_BVE_CANDIDATE_CONFLICT", "SAT_BVE_CANDIDATE_EXTERNAL", var);
    const uint32_t actual = k.internal_to_external[var] + 1;
    if (*path && selected) {
      static std::FILE *out = nullptr;
      if (!out)
        out = open_line_trace(path, "TACTIX_BVE_CANDIDATE_TRACE",
                              "# bve-candidate-observer\t1\n");
      const bool retry = var < k.bve_retry.size() && k.bve_retry[var];
      std::fprintf(out,
                   "tactix\texternal=%u\tstage=%s\tconflicts=%llu\t"
                   "ticks=%llu\tlimit=%llu\tapplied=%u\tretry=%u\t"
                   "value=%d\tretired=%u\troots=%llu\ttrail=%zu\t"
                   "propagate=%u\n",
                   actual, stage, (unsigned long long)k.stats.conflicts,
                   (unsigned long long)ticks, (unsigned long long)limit,
                   applied, retry,
                   static_cast<int>(k.value(make_lit(var, false))),
                   kernel_var_retired(k, var),
                   (unsigned long long)k.stats.root_assignments,
                   k.trail.size(), k.propagate_head);
      if (!std::strcmp(stage, "end") &&
          std::getenv("SAT_BVE_CANDIDATE_EXIT_AFTER_END")) {
        std::fflush(out);
        std::exit(0);
      }
    }
  }
  if (!std::strcmp(stage, "end") && std::getenv("TACTIX_BVE_RETRY_TRACE") &&
      selects_conflict_and_var(k, "SAT_BVE_RETRY_CONFLICT",
                               "SAT_BVE_RETRY_EXTERNAL", var))
    bve_retry_trace_armed = true;
  if (active_trace)
    active_trace->bve_candidate(k, var, stage, ticks, limit, applied);
}

void event_trace_bve_retry_mark(const Kernel &k, Var var, const char *source,
                                bool before, bool after,
                                const Clause *clause) {
  if (!bve_retry_trace_armed || before || !after)
    return;
  const char *path = std::getenv("TACTIX_BVE_RETRY_TRACE");
  if (!selects_conflict_and_var(k, "SAT_BVE_RETRY_CONFLICT",
                                "SAT_BVE_RETRY_EXTERNAL", var) ||
      !path || !*path)
    return;
  const uint32_t actual = k.internal_to_external[var] + 1;
  std::FILE *out = open_trace_or_exit(path, "TACTIX_BVE_RETRY_TRACE");
  uint64_t clause_hash = 0;
  uint64_t clause_id = 0;
  uint32_t clause_size = 0;
  std::string clause_lits = "-";
  if (clause) {
    const auto external_clause =
        sorted_external_clause<int32_t>(k, *clause);
    clause_hash = fnv_hash(external_clause);
    clause_id = clause->id();
    clause_size = clause->size;
    clause_lits.clear();
    append_list(clause_lits, external_clause);
  }
  std::fprintf(out,
               "# bve-retry-observer\t1\n"
               "tactix\texternal=%u\tconflicts=%llu\tsource=%s\t"
               "trigger=%u\tbefore=%u\tafter=%u\tvalue=%d\tretired=%u\t"
               "clause_id=%llu\tclause_size=%u\tclause_hash=%016llx\t"
               "clause_lits=%s\n",
               actual, (unsigned long long)k.stats.conflicts,
               source ? source : "unspecified", bve_retry_trace_candidate,
               before, after,
               static_cast<int>(k.value(make_lit(var, false))),
               kernel_var_retired(k, var), (unsigned long long)clause_id,
               clause_size, (unsigned long long)clause_hash,
               clause_lits.c_str());
  std::fclose(out);
  if (std::getenv("SAT_BVE_RETRY_EXIT_AFTER_MARK"))
    std::exit(0);
  bve_retry_trace_armed = false;
}

void event_trace_bve_definition(const Kernel &k, Var var, uint64_t attempt,
                                uint32_t core, const char *outcome,
                                uint64_t ticks, uint64_t budget,
                                uint64_t generator, uint32_t variables,
                                uint32_t side0, uint32_t side1,
                                const uint32_t *ids, uint32_t size) {
  if (!active_trace)
    return;
  uint64_t hash = FNV_OFFSET;
  uint32_t core0 = 0, core1 = 0;
  for (uint32_t i = 0; i < size; ++i) {
    hash = mix(hash, ids[i]);
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
  emit_parts(k, "bve_plan", "external=", k.internal_to_external[var] + 1,
             "|side0=", side0, "|side1=", side1, "|gate_kind=", gate_kind,
             "|gate0=", gate0, "|gate1=", gate1, "|charged=", charged,
             "|resolve_gate=", resolve_gate ? "1" : "0");
}

static std::string bve_schedule_payload(const Kernel &k, const Var *members,
                                        const double *scores, uint32_t size,
                                        uint32_t bound,
                                        uint64_t insertion_hash) {
  uint64_t xor_hash = 0, sum_hash = 0, external_sum = 0;
  uint64_t topology_hash = FNV_OFFSET;
  uint64_t score_xor = 0, score_sum = 0;
  for (uint32_t i = 0; i < size; ++i) {
    const Var member = members[i];
    const uint64_t external = k.internal_to_external[member] + 1ull;
    const uint64_t mixed = splitmix64_hash(external);
    xor_hash ^= mixed;
    sum_hash += mixed;
    external_sum += external;
    topology_hash = mix(topology_hash, external);
    const uint64_t score_mixed =
        splitmix64_hash(mixed ^ double_bits(scores[member]));
    score_xor ^= score_mixed;
    score_sum += score_mixed;
  }
  char payload[512];
  std::snprintf(payload, sizeof payload,
                "bound=%u|scheduled=%u|external_sum=%016llx|"
                "set_xor=%016llx|set_sum=%016llx|"
                "insertion_hash=%016llx|topology_hash=%016llx|"
                "score_xor=%016llx|score_sum=%016llx",
                bound, size, (unsigned long long)external_sum,
                (unsigned long long)xor_hash, (unsigned long long)sum_hash,
                (unsigned long long)insertion_hash,
                (unsigned long long)topology_hash,
                (unsigned long long)score_xor,
                (unsigned long long)score_sum);
  return payload;
}

void EventTrace::bve_schedule(const Kernel &k, const Var *members,
                              const double *scores, uint32_t size,
                              uint32_t bound, uint64_t insertion_hash) {
  if (emitted_ >= limit_)
    return;
  const std::string payload = bve_schedule_payload(
      k, members, scores, size, bound, insertion_hash);
  // This one-row-per-round observer deliberately bypasses the detail window:
  // its purpose is to find the first schedule-set divergence before a later
  // selected conflict without tracing every intervening clause mutation.
  emit(k, "bve_schedule", payload.c_str(), false);
}

void event_trace_bve_schedule(const Kernel &k, const Var *members,
                              const double *scores, uint32_t size,
                              uint32_t bound, uint64_t insertion_hash) {
  // Standalone observer: dumps the selected BVE schedule and exits before its
  // first candidate, without a global trace.
  if (const char *path = std::getenv("TACTIX_BVE_SCHEDULE_TRACE")) {
    if (*path) {
      if (other_conflict(k, "SAT_BVE_SCHEDULE_CONFLICT"))
        return;
      static uint64_t matching_ordinal = 0;
      if (const char *selected = std::getenv("SAT_BVE_SCHEDULE_ORDINAL")) {
        const uint64_t wanted = std::strtoull(selected, nullptr, 10);
        if (*selected && matching_ordinal++ != wanted)
          return;
      }
      std::FILE *out = open_trace_or_exit(path, "TACTIX_BVE_SCHEDULE_TRACE");
      const std::string payload = bve_schedule_payload(
          k, members, scores, size, bound, insertion_hash);
      uint64_t irredundant = 0, redundant = 0;
      live_clause_counts(k, irredundant, redundant);
      std::fprintf(out, "# bve-schedule-observer\t1\n");
      std::fprintf(
          out,
          "tactix\t%s|conflicts=%llu|decisions=%llu|propagations=%llu|"
          "active=%llu|vars=%u|trail=%zu|irred=%llu|red=%llu|binary=%zu\n",
          payload.c_str(), (unsigned long long)k.stats.conflicts,
          (unsigned long long)k.stats.decisions,
          (unsigned long long)k.stats.propagations,
          (unsigned long long)kernel_active_variables(k), k.nvars,
          k.trail.size(), (unsigned long long)irredundant,
          (unsigned long long)redundant, live_binary_clause_count(k));
      if (std::getenv("SAT_BVE_SCHEDULE_MEMBERS")) {
        assert(k.bve_occurrence_count_image);
        for (uint32_t position = 0; position != size; ++position) {
          const Var member = members[position];
          const uint64_t score_bits = double_bits(scores[member]);
          const uint32_t positive =
              (*k.bve_occurrence_count_image)[make_lit(member, false)];
          const uint32_t negative =
              (*k.bve_occurrence_count_image)[make_lit(member, true)];
          std::fprintf(out,
                       "member\texternal=%u\tposition=%u\tpositive=%u\t"
                       "negative=%u\tscore=%016llx\n",
                       k.internal_to_external[member] + 1, position, positive,
                       negative, (unsigned long long)score_bits);
        }
      }
      if (const char *spec = std::getenv("SAT_BVE_SCHEDULE_SELECTED")) {
        std::vector<uint32_t> selected;
        visit_decimal_list<unsigned long>(spec, [&](unsigned long external) {
          if (!external || external > UINT32_MAX)
            return false;
          selected.push_back(static_cast<uint32_t>(external));
          return true;
        });
        for (Var var = 0; var != k.nvars; ++var) {
          const int base = kernel_externalize(k, make_lit(var, false));
          const uint32_t external = static_cast<uint32_t>(std::abs(base));
          if (std::find(selected.begin(), selected.end(), external) ==
              selected.end())
            continue;
          uint32_t position = UINT32_MAX;
          for (uint32_t i = 0; i != size; ++i)
            if (members[i] == var) {
              position = i;
              break;
            }
          uint32_t positive = (*k.bve_occurrence_count_image)[make_lit(var, false)];
          uint32_t negative = (*k.bve_occurrence_count_image)[make_lit(var, true)];
          int value = k.value(make_lit(var, false));
          if (base < 0) {
            std::swap(positive, negative);
            value = -value;
          }
          std::fprintf(out,
                       "selected\texternal=%u\tposition=%s\tpositive=%u\t"
                       "negative=%u\tvalue=%d\tlevel=%u\tretired=%u\n",
                       external, position == UINT32_MAX ? "-" :
                           std::to_string(position).c_str(),
                       positive, negative, value, k.assigned[var].level,
                       kernel_var_retired(k, var));
        }
      }
      std::fclose(out);
      std::exit(0);
    }
  }
  if (active_trace)
    active_trace->bve_schedule(k, members, scores, size, bound,
                               insertion_hash);
}

void EventTrace::bve_pop(const Kernel &k, uint64_t ordinal,
                         uint64_t cumulative_hash, Var popped, double score,
                         Var root, uint32_t size) {
  if (emitted_ >= limit_ || ordinal % 4096)
    return;
  char payload[256];
  const uint32_t external = k.internal_to_external[popped] + 1;
  const uint32_t root_external =
      root == INVALID_VAR ? 0 : k.internal_to_external[root] + 1;
  std::snprintf(payload, sizeof payload,
                "ordinal=%llu|external=%u|score=%.17g|score_hex=%a|"
                "root=%u|size=%u|cumulative_hash=%016llx",
                (unsigned long long)ordinal, external, score, score,
                root_external, size, (unsigned long long)cumulative_hash);
  emit(k, "bve_pop_checkpoint", payload, false);
}

static uint64_t eliminate_cleanup_clause_pose_hash(const Kernel &k,
                                                   const Clause &clause) {
  uint64_t hash = FNV_OFFSET;
  for (const Lit lit : clause)
    hash = mix_signed(hash, kernel_externalize(k, lit));
  hash = mix(hash, clause.redundant());
  hash = mix(hash, clause.glue());
  hash = mix(hash, clause.searched);
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
  uint64_t live_irredundant_hash = 0, live_redundant_hash = 0;
  uint64_t binary_count = 0, binary_hash = 0;
  uint64_t binary_watch_order_hash = 0, large_watch_order_hash = 0;
  uint64_t root_fixed = 0, root_fixed_hash = 0;
  std::vector<uint32_t> retired_external;
  for (Var var = 0; var < k.nvars; ++var)
    if (kernel_var_retired(k, var))
      retired_external.push_back(k.internal_to_external[var] + 1);
  std::sort(retired_external.begin(), retired_external.end());
  const uint64_t retired_variables_hash = fnv_hash(retired_external);
  for (const auto [ref, clause] : arena_records(k.arena)) {
    if (!clause->garbage()) {
      const bool redundant = clause->redundant();
      (redundant ? live_redundant : live_irredundant)++;
      const uint64_t clause_hash = eliminate_cleanup_clause_hash(k, *clause);
      (redundant ? live_redundant_hash : live_irredundant_hash) +=
          clause_hash;
      bool root = false, retired = false;
      for (const Lit lit : *clause) {
        root |= k.values[lit] > 0;
        retired |= kernel_var_retired(k, var_of(lit));
      }
      if (root || retired) {
        collect_hash += clause_hash;
        if (root) {
          root_hash += clause_hash;
          (redundant ? root_redundant : root_irredundant)++;
        }
        if (retired) {
          retired_hash += clause_hash;
          (redundant ? retired_redundant : retired_irredundant)++;
        }
        if (root && retired)
          (redundant ? both_redundant : both_irredundant)++;
      }
    }
  }
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
  {
    const uint64_t list_seed =
        mix_signed(FNV_OFFSET, kernel_externalize(k, lit));
    uint64_t binary_list_hash = list_seed, large_list_hash = list_seed;
    uint64_t binary_list_count = 0, large_list_count = 0;
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary()) {
        const Clause *clause = clause_at(k.arena, watch.ref());
        large_list_hash = mix_signed(large_list_hash,
                                     kernel_externalize(k, watch.blocking()));
        large_list_hash = mix(
            large_list_hash,
            eliminate_cleanup_clause_pose_hash(k, *clause));
        ++large_list_count;
        continue;
      }
      binary_list_hash =
          mix_signed(binary_list_hash, kernel_externalize(k, watch.other()));
      ++binary_list_count;
      if (lit >= watch.other())
        continue;
      binary_hash += binary_pair_hash(k, lit, watch.other());
      ++binary_count;
    }
    if (binary_list_count) {
      binary_list_hash = mix(binary_list_hash, binary_list_count);
      binary_watch_order_hash += binary_list_hash;
    }
    if (large_list_count) {
      large_list_hash = mix(large_list_hash, large_list_count);
      large_watch_order_hash += large_list_hash;
    }
  }
  root_fixed_digest(k, true, root_fixed, root_fixed_hash);
  const HeapDigest heap = heap_digest(k);
  char payload[1280];
  std::snprintf(
      payload, sizeof payload,
      "stage=%s|live_irredundant=%llu|live_redundant=%llu|"
      "root_irredundant=%llu|root_redundant=%llu|"
      "retired_irredundant=%llu|retired_redundant=%llu|"
      "both_irredundant=%llu|both_redundant=%llu|root_hash=%016llx|"
      "retired_hash=%016llx|collect_hash=%016llx|"
      "live_irredundant_hash=%016llx|live_redundant_hash=%016llx|"
      "binary_count=%llu|binary_hash=%016llx|root_fixed=%llu|"
      "root_fixed_hash=%016llx|binary_watch_order_hash=%016llx|"
      "large_watch_order_hash=%016llx|retired_variables=%zu|"
      "retired_variables_hash=%016llx|heap=%zu|heap_hash=%016llx|"
      "heap_score_hash=%016llx",
      stage, (unsigned long long)live_irredundant,
      (unsigned long long)live_redundant,
      (unsigned long long)root_irredundant,
      (unsigned long long)root_redundant,
      (unsigned long long)retired_irredundant,
      (unsigned long long)retired_redundant,
      (unsigned long long)both_irredundant,
      (unsigned long long)both_redundant, (unsigned long long)root_hash,
      (unsigned long long)retired_hash, (unsigned long long)collect_hash,
      (unsigned long long)live_irredundant_hash,
      (unsigned long long)live_redundant_hash,
      (unsigned long long)binary_count, (unsigned long long)binary_hash,
      (unsigned long long)root_fixed, (unsigned long long)root_fixed_hash,
      (unsigned long long)binary_watch_order_hash,
      (unsigned long long)large_watch_order_hash,
      retired_external.size(),
      (unsigned long long)retired_variables_hash, heap.size,
      (unsigned long long)heap.hash,
      (unsigned long long)heap.score_hash);


  emit(k, "eliminate_cleanup", payload, false);
}

// Appends `external:score` for variable `v`, then `:position` (`:-` outside
// the heap) when `with_position`.
static void append_scored_var(std::string &payload, const Kernel &k, Var v,
                              bool with_position) {
  char score[64];
  std::snprintf(score, sizeof score, "%.17g", k.h->scores.score[v]);
  append_parts(payload, kernel_externalize(k, make_lit(v, false)), ":", score);
  if (with_position)
    append_parts(payload, ":",
                 heuristics_heap_contains(k, v)
                     ? std::to_string(k.h->scores.pos[v])
                     : "-");
}

void EventTrace::analyzed(const Kernel &k, const std::vector<Var> &analyzed,
                          const std::vector<uint8_t> &seen,
                          uint8_t bump_mask) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = "vars=";
  bool first = true;
  std::vector<uint8_t> emitted(k.nvars);
  for (const Var v : analyzed) {
    if (!(seen[v] & bump_mask) || emitted[v])
      continue;
    emitted[v] = 1;
    if (!first)
      payload += ',';
    first = false;
    append_scored_var(payload, k, v, true);
  }
  emit(k, "analyzed", payload.c_str(), false);
}

void EventTrace::analyzed_post(const Kernel &k,
                               const std::vector<Var> &analyzed) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = "vars=";
  for (size_t i = 0; i < analyzed.size(); i++) {
    if (i) payload += ',';
    append_scored_var(payload, k, analyzed[i], true);
  }
  emit(k, "analyzed_post", payload.c_str(), false);
}

void EventTrace::emit_heap_scores(const Kernel &k) {
  emit_heap_set_snapshot(k);
  char hash[32];
  std::snprintf(hash, sizeof hash, "hash=%016llx|",
                (unsigned long long)heap_digest(k).hash);
  std::string payload(hash);
  const size_t size = std::min<size_t>(32, k.h->scores.stack.size());
  for (size_t i = 0; i < size; i++) {
    if (i)
      payload += ',';
    append_scored_var(payload, k, k.h->scores.stack[i], false);
  }
  emit(k, "heap_scores", payload.c_str(), false);
}

void EventTrace::heap_after_decision(const Kernel &k) {
  emit_heap_set_snapshot(k);
}

void EventTrace::heap_scores(const Kernel &k) {
  // Trajectory-only and endpoint-stop modes omit intermediate heap scores but
  // keep the one-shot heap-set dump reachable, in Kissat's observer order.
  if (!k.h || !k.h->stable)
    return;
  if (trajectory_only_) {
    emit_heap_set_snapshot(k);
    return;
  }
  if (heap_vars_only_)
    return;
  if (stop_after_conflict_ != UINT64_MAX || !want_detail(k))
    return;
  emit_heap_scores(k);
}

bool EventTrace::heap_var_selected(const Kernel &k, Var v) const {
  if (v >= k.nvars)
    return false;
  const int external =
      std::abs(kernel_externalize(k, make_lit(v, false)));
  return std::find(heap_vars_.begin(), heap_vars_.end(), external) !=
         heap_vars_.end();
}

bool EventTrace::heap_event_admitted(const Kernel &k) const {
  return !imply_only_ && out_ && k.h && k.h->stable &&
         (!heap_vars_only_ || in_detail_window(k));
}

void EventTrace::heap_insert(const Kernel &k, Var v, uint32_t from,
                             uint32_t to) {
  if (!heap_event_admitted(k))
    return;
  const bool selected = heap_var_selected(k, v);
  if (heap_vars_only_ ? !selected : !want_detail(k))
    return;
  char payload[224];
  std::snprintf(payload, sizeof payload,
                "var=%d|score=%.17g|from=%u|to=%u|hash=%016llx",
                kernel_externalize(k, make_lit(v, false)),
                k.h->scores.score[v], from, to,
                (unsigned long long)heap_digest(k).hash);
  emit(k, "heap_insert", payload, false);
}

void EventTrace::heap_remove(const Kernel &k, Var removed, Var last,
                             uint32_t removed_pos, uint32_t from,
                             uint32_t to) {
  if (!heap_event_admitted(k))
    return;
  const bool selected =
      heap_var_selected(k, removed) || heap_var_selected(k, last);
  if (heap_vars_only_ ? !selected
                      : (!want_detail(k) ||
                         k.stats.decisions != heap_remove_decision_))
    return;
  const std::string to_text = to == UINT32_MAX ? "-" : std::to_string(to);
  char payload[256];
  std::snprintf(payload, sizeof payload,
                "removed=%d:%.17g|removed_pos=%u|last=%d:%.17g|from=%u|to=%s",
                kernel_externalize(k, make_lit(removed, false)),
                k.h->scores.score[removed], removed_pos,
                kernel_externalize(k, make_lit(last, false)),
                k.h->scores.score[last], from, to_text.c_str());
  emit(k, "heap_remove", payload, false);
}

void EventTrace::heap_pop(const Kernel &k, const char *stage, Var held,
                          Var other, uint32_t from, uint32_t to) {
  // Pops are traced only for selected variables.
  if (!heap_event_admitted(k) || !heap_vars_only_ ||
      (!heap_var_selected(k, held) && !heap_var_selected(k, other)))
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

void EventTrace::root_assign(const Kernel &k, Lit lit) {
  const Var v = var_of(lit);
  if (!out_ || v >= k.nvars)
    return;
  if (heap_vars_only_ && !in_detail_window(k))
    return;
  if (heap_vars_only_ && !heap_var_selected(k, v))
    return;
  if (!heap_vars_only_) {
    detail(k, "root_assign", kernel_externalize(k, lit));
    return;
  }
  const Assigned &assignment = k.assigned[v];
  std::string reason;
  if (assignment.affine) {
    reason = "affine";
  } else if (assignment.reason == UNIT_REASON) {
    reason = "unit";
  } else if (assignment.reason == DECISION_REASON) {
    reason = "decision";
  } else if (assignment.binary) {
    reason = "binary:";
    append_list(reason,
                sorted_external_clause(k, std::array{lit, assignment.reason}));
  } else {
    reason = "clause:";
    append_list(reason, sorted_external_clause(
                            k, *clause_at(k.arena, assignment.reason)));
  }
  emit_parts(k, "root_assign", "lit=", kernel_externalize(k, lit), "|warming=",
             unsigned(k.warming), "|trail_after=", k.trail.size(),
             "|unflushed_after=", k.unflushed_root, "|reason=", reason);
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
  emit_parts(k, "mode_clock", "stage=", stage, "|ticks=", k.stats.search_ticks,
             "|propagate_ticks=", k.diagnostic_search_propagation_ticks,
             "|analysis_ticks=",
             k.stats.search_ticks - k.diagnostic_search_propagation_ticks,
             "|limit=", limit, "|anchor=", anchor, "|count=", count);
}

void EventTrace::implications(const Kernel &k, size_t trail_begin) {
  // Implication-only tracing rejects unrelated helpers before constructing
  // their payloads. Admit this owner directly at the same conflict window.
  const bool selected =
      imply_only_ ? out_ && in_detail_window(k) : want_detail(k);
  if (!selected || emitted_ >= limit_)
    return;
  for (size_t pos = trail_begin; pos < k.trail.size(); pos++) {
    if (emitted_ >= limit_)
      return;
    const Lit assigned_lit = k.trail[pos];
    const int external_assigned = kernel_externalize(k, assigned_lit);
    if (!imply_literals_.empty() &&
        std::find(imply_literals_.begin(), imply_literals_.end(),
                  std::abs(external_assigned)) == imply_literals_.end())
      continue;
    const Assigned &assignment = k.assigned[var_of(assigned_lit)];
    if (assignment.reason == DECISION_REASON || assignment.reason == UNIT_REASON)
      continue;
    if (assignment.affine) {
      emit_parts(k, "imply", "lit=", external_assigned, "|level=",
                 assignment.level, "|reason=affine");
      continue;
    }
    const std::array binary_reason{assigned_lit, assignment.reason};
    const std::vector<int> reason =
        assignment.binary
            ? sorted_external_clause(k, binary_reason)
            : sorted_external_clause(k, *clause_at(k.arena, assignment.reason));
    std::string payload = concat("lit=", external_assigned, "|level=",
                                 assignment.level, "|reason=");
    append_list(payload, reason);
    emit(k, "imply", payload.c_str(), false);
  }
}

void EventTrace::conflict(const Kernel &k) {
  const bool in_conflict_window = out_ && in_detail_window(k);
  if (!(conflict_only_ ? in_conflict_window : want_detail(k)) ||
      !k.conflict_valid)
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
  append_list(payload, std::span(lits, size), ExternalLiteral{k});
  payload += "|set=";
  append_list(payload, sorted_external_clause(k, std::span(lits, size)));
  payload += "|levels=";
  for (uint32_t i = 0; i < size; i++) {
    if (i)
      payload += ',';
    append_parts(payload, kernel_externalize(k, lits[i]), ":",
                 k.var_level(var_of(lits[i])));
  }
  if (k.conflict_ref != INVALID_REF) {
    const Clause *clause = clause_at(k.arena, k.conflict_ref);
    append_parts(payload, "|ref=", k.conflict_ref, "|id=", clause->id(),
                 "|glue=", clause->glue(), "|used=", clause->used(),
                 "|redundant=", clause->redundant());
  } else {
    payload += "|ref=binary";
  }


  emit(k, "conflict", payload.c_str(), false);
}

void EventTrace::learned(const Kernel &k, const std::vector<Lit> &clause) {
  learned_stage(k, "learned", clause);
}

void EventTrace::learned_stage(const Kernel &k, const char *event,
                               const std::vector<Lit> &clause) {
  if (learned_only_ && std::strcmp(event, "learned"))
    return;
  const bool in_window = in_detail_window(k);
  if ((learned_only_ ? !out_ || !in_window : !want_detail(k)) ||
      emitted_ >= limit_)
    return;
  if (trajectory_only_) {
    std::vector<int> external;
    external.reserve(clause.size());
    for (const Lit lit : clause)
      external.push_back(kernel_externalize(k, lit));
    const uint64_t order_hash = fnv_signed(external);
    std::sort(external.begin(), external.end());
    const uint64_t set_hash = fnv_signed(external);
    char payload[128];
    std::snprintf(payload, sizeof payload,
                  "size=%zu|order_hash=%016llx|set_hash=%016llx",
                  clause.size(), (unsigned long long)order_hash,
                  (unsigned long long)set_hash);
    emit(k, event, payload, false);
    return;
  }
  std::string payload = "order=";
  append_list(payload, clause, ExternalLiteral{k});
  payload += "|set=";
  append_list(payload, sorted_external_clause(k, clause));
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
  append_parts(payload, "|nonasserting_glue=", nonasserting_levels.size());
  emit(k, event, payload.c_str(), false);
}

void EventTrace::clause_glue(const Kernel &k, const char *source,
                             const Clause *clause, uint32_t old_glue,
                             uint32_t new_glue) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload = concat("source=", source, "|id=", clause->id(), "|old=",
                               old_glue, "|new=", new_glue, "|pose=");
  append_clause_pose(payload, k, *clause);
  emit(k, "reason_glue", payload.c_str(), false);
}

void EventTrace::clause_used(const Kernel &k, const Clause *clause,
                             uint32_t mode) {
  if (!want_detail(k) || emitted_ >= limit_)
    return;
  std::string payload =
      concat("mode=", mode, "|glue=", clause->glue(), "|pose=");
  append_clause_pose(payload, k, *clause);
  emit(k, "reason_used", payload.c_str(), false);
}

void EventTrace::checkpoint(const Kernel &k) {
  // A coarse search-window observer can be armed independently of the global
  // event trace.  It reuses the O(1) macro journal and therefore records only
  // scalar search counters; no arena, watch, heap, or variable scan is added.
  static bool standalone_initialized = false;
  static bool standalone_enabled = false;
  static uint64_t standalone_from = 0;
  static uint64_t standalone_interval = 1;
  static uint64_t standalone_bucket = UINT64_MAX;
  if (!standalone_initialized) {
    const char *interval =
        std::getenv("SAT_SEARCH_CHECKPOINT_INTERVAL");
    if (interval && *interval) {
      standalone_initialized = true;
      standalone_enabled = true;
      standalone_interval =
          std::max<uint64_t>(1, std::strtoull(interval, nullptr, 10));
      read_decimal("SAT_SEARCH_CHECKPOINT_CONFLICT_FROM", standalone_from);
    }
  }
  if (standalone_enabled && k.stats.conflicts >= standalone_from) {
    const uint64_t bucket =
        (k.stats.conflicts - standalone_from) / standalone_interval;
    if (bucket != standalone_bucket) {
      standalone_bucket = bucket;
      emit_standalone_macro_boundary(k, "search_checkpoint");
    }
  }
  if (!out_ || !saw_reorder_)
    return;
  const uint64_t bucket = k.stats.conflicts / CHECKPOINT_INTERVAL;
  if (bucket <= checkpoint_bucket_)
    return;
  checkpoint_bucket_ = bucket;
  macro(k, "checkpoint");
}

void EventTrace::reorder_begin(const Kernel &k) {
  macro(k, "reorder_begin");
  saw_reorder_ = true;
  post_reorder_details_ = 1024;
  checkpoint_bucket_ = k.stats.conflicts / CHECKPOINT_INTERVAL;
}

void EventTrace::reorder_end(const Kernel &k) { macro(k, "reorder_end"); }

} // namespace tactix
#endif

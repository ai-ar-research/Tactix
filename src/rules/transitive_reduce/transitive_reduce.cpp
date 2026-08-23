// See transitive_reduce.hpp for the rule's contract. The walk is read-only and
// every mutation goes through kernel_delete_binary, which owns the proof delete
// and the root-reason retirement.
#include "rules/transitive_reduce/transitive_reduce.hpp"

#include "rules/propagate/propagate.hpp"

#include "diagnostics/event_trace.hpp"
#include "kernel/watch.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

namespace tactix {

RuleEligibility transitive_reduce_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/calculus.md` §7).
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

namespace {

#ifdef TACTIX_ENABLE_EVENT_TRACE
struct TransitiveTraceState {
  bool initialized = false;
  FILE *out = nullptr;
  std::vector<std::pair<int, int>> selected;
  uint64_t source_sequence = 0;
  uint64_t endpoint_conflicts = UINT64_MAX;
  bool all_sources = false;
};

TransitiveTraceState transitive_trace;

void open_transitive_trace() {
  if (transitive_trace.initialized)
    return;
  transitive_trace.initialized = true;
  const char *path = std::getenv("TACTIX_TRANSITIVE_TRACE");
  const char *spec = std::getenv("TACTIX_TRANSITIVE_TRACE_PAIRS");
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
    transitive_trace.selected.emplace_back(a, b);
    cursor = *end ? end + 1 : end;
  }
  transitive_trace.out = std::fopen(path, "w");
  if (!transitive_trace.out)
    return;
  std::setvbuf(transitive_trace.out, nullptr, _IOLBF, 0);
  transitive_trace.all_sources =
      std::getenv("TACTIX_TRANSITIVE_TRACE_ALL") != nullptr;
  std::fputs("# tactix-transitive-trace\t2\n", transitive_trace.out);
  std::fputs("# solver\tevent\tsource_sequence\tconflicts\tsource\t"
             "candidate_ordinal\tdestination\tticks_before\tticks_after\t"
             "redundant\tfailed\tcandidate_count\tout_degree\t"
             "source_internal\tdestination_internal\tsource_import\t"
             "destination_import\tsource_reference\tdestination_reference\t"
             "source_active\tdestination_active\tsource_value\t"
             "destination_value\tcopies\tskip_reason\n",
             transitive_trace.out);
}

bool transitive_trace_pair(const Kernel &k, Lit a, Lit b) {
  open_transitive_trace();
  if (!transitive_trace.out)
    return false;
  int external_a = kernel_externalize(k, a);
  int external_b = kernel_externalize(k, b);
  if (external_b < external_a)
    std::swap(external_a, external_b);
  return std::find(transitive_trace.selected.begin(),
                   transitive_trace.selected.end(),
                   std::make_pair(external_a, external_b)) !=
         transitive_trace.selected.end();
}

bool transitive_trace_all_sources() { return transitive_trace.all_sources; }

void transitive_trace_row(const Kernel &k, const char *event,
                          uint64_t source_sequence, Lit source,
                          size_t candidate_ordinal, Lit destination,
                          uint64_t ticks_before, uint64_t ticks_after,
                          bool redundant, bool failed, size_t candidate_count,
                          uint32_t out_degree) {
  std::fprintf(
      transitive_trace.out,
      "tactix\t%s\t%llu\t%llu\t%d\t%zu\t%d\t%llu\t%llu\t%d\t%d\t%zu\t%u"
      "\t%u\t%u\t%llu\t%llu\t%llu\t%llu\t%d\t%d\t%d\t%d\t0\t-\n",
      event, static_cast<unsigned long long>(source_sequence),
      static_cast<unsigned long long>(k.stats.conflicts),
      kernel_externalize(k, source), candidate_ordinal,
      destination == INVALID_LIT ? 0 : kernel_externalize(k, destination),
      static_cast<unsigned long long>(ticks_before),
      static_cast<unsigned long long>(ticks_after), redundant, failed,
      candidate_count, out_degree, source,
      destination == INVALID_LIT ? INVALID_LIT : destination,
      static_cast<unsigned long long>(kernel_import_lit_order(k, source)),
      static_cast<unsigned long long>(
          destination == INVALID_LIT ? UINT64_MAX
                                     : kernel_import_lit_order(k, destination)),
      static_cast<unsigned long long>(
          kernel_compact_import_lit_order(k, source)),
      static_cast<unsigned long long>(
          destination == INVALID_LIT
              ? UINT64_MAX
              : kernel_compact_import_lit_order(k, destination)),
      !kernel_var_retired(k, var_of(source)),
      destination != INVALID_LIT &&
          !kernel_var_retired(k, var_of(destination)),
      k.values[source],
      destination == INVALID_LIT ? 0 : k.values[destination]);
}

bool transitive_trace_selected_endpoint(int external) {
  for (const auto &pair : transitive_trace.selected)
    if (external == pair.first || external == pair.second)
      return true;
  return false;
}

void transitive_trace_endpoints(const Kernel &k) {
  open_transitive_trace();
  if (!transitive_trace.out ||
      transitive_trace.endpoint_conflicts == k.stats.conflicts)
    return;
  transitive_trace.endpoint_conflicts = k.stats.conflicts;
  for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
    const int external = kernel_externalize(k, lit);
    if (!transitive_trace_selected_endpoint(external))
      continue;
    std::fprintf(
        transitive_trace.out,
        "tactix\tendpoint\t0\t%llu\t%d\t0\t0\t0\t0\t0\t0\t0\t0\t%u\t%u"
        "\t%llu\t%llu\t%llu\t%llu\t%d\t0\t%d\t0\t0\t-\n",
        static_cast<unsigned long long>(k.stats.conflicts), external, lit,
        INVALID_LIT,
        static_cast<unsigned long long>(kernel_import_lit_order(k, lit)),
        static_cast<unsigned long long>(UINT64_MAX),
        static_cast<unsigned long long>(
            kernel_compact_import_lit_order(k, lit)),
        static_cast<unsigned long long>(UINT64_MAX),
        !kernel_var_retired(k, var_of(lit)), k.values[lit]);
  }
}

void transitive_trace_admission(const Kernel &k, Lit src, Lit dst,
                                uint32_t copies, const char *skip_reason) {
  if (!transitive_trace_pair(k, src, dst))
    return;
  std::fprintf(
      transitive_trace.out,
      "tactix\tadmission\t0\t%llu\t%d\t0\t%d\t0\t0\t0\t0\t0\t0\t%u\t%u"
      "\t%llu\t%llu\t%llu\t%llu\t%d\t%d\t%d\t%d\t%u\t%s\n",
      static_cast<unsigned long long>(k.stats.conflicts),
      kernel_externalize(k, src), kernel_externalize(k, dst), src, dst,
      static_cast<unsigned long long>(kernel_import_lit_order(k, src)),
      static_cast<unsigned long long>(kernel_import_lit_order(k, dst)),
      static_cast<unsigned long long>(
          kernel_compact_import_lit_order(k, src)),
      static_cast<unsigned long long>(
          kernel_compact_import_lit_order(k, dst)),
      !kernel_var_retired(k, var_of(src)),
      !kernel_var_retired(k, var_of(dst)), k.values[src], k.values[dst], copies,
      skip_reason);
}

void transitive_trace_walk_node(const Kernel &k, Lit src, Lit dst, Lit node,
                                size_t ordinal, uint64_t ticks_before,
                                uint64_t ticks_after, uint32_t watch_entries) {
  if (!transitive_trace_pair(k, src, dst))
    return;
  char node_payload[48];
  std::snprintf(node_payload, sizeof node_payload, "node=%d",
                kernel_externalize(k, node));
  std::fprintf(
      transitive_trace.out,
      "tactix\twalk-node\t0\t%llu\t%d\t%zu\t%d\t%llu\t%llu\t0\t0\t0\t0\t%u\t%u"
      "\t%llu\t%llu\t%llu\t%llu\t%d\t%d\t%d\t%d\t%u\t%s\n",
      static_cast<unsigned long long>(k.stats.conflicts),
      kernel_externalize(k, src), ordinal, kernel_externalize(k, dst),
      static_cast<unsigned long long>(ticks_before),
      static_cast<unsigned long long>(ticks_after), node, dst,
      static_cast<unsigned long long>(kernel_import_lit_order(k, node)),
      static_cast<unsigned long long>(kernel_import_lit_order(k, dst)),
      static_cast<unsigned long long>(
          kernel_compact_import_lit_order(k, node)),
      static_cast<unsigned long long>(
          kernel_compact_import_lit_order(k, dst)),
      !kernel_var_retired(k, var_of(node)),
      !kernel_var_retired(k, var_of(dst)), k.values[node], k.values[dst],
      watch_entries, node_payload);
}

void transitive_trace_walk_entries(const Kernel &k, Lit src, Lit dst,
                                   Lit node, Lit watched) {
  if (!transitive_trace_pair(k, src, dst))
    return;
  const char *selected =
      std::getenv("TACTIX_TRANSITIVE_TRACE_WATCH_LITERAL");
  if (!selected || std::atoi(selected) != kernel_externalize(k, watched))
    return;
  size_t ordinal = 0;
  for (const WatchEntry entry : k.watches.entries(watched)) {
    std::vector<int> clause;
    Lit other = INVALID_LIT;
    uint64_t id = 0;
    std::string payload =
        "node=" + std::to_string(kernel_externalize(k, node)) +
        ";watch=" + std::to_string(kernel_externalize(k, watched));
    if (entry.binary()) {
      other = entry.other();
      payload += ";kind=binary;clause=" +
                 std::to_string(kernel_externalize(k, watched)) + "," +
                 std::to_string(kernel_externalize(k, other));
    } else {
      const Clause *large = clause_at(k.arena, entry.ref());
      id = large->id();
      for (const Lit lit : *large)
        clause.push_back(kernel_externalize(k, lit));
      std::sort(clause.begin(), clause.end());
      payload += ";kind=large;blocker=" +
                 std::to_string(kernel_externalize(k, entry.blocking())) +
                 ";clause=";
      for (size_t i = 0; i < clause.size(); i++) {
        if (i)
          payload += ',';
        payload += std::to_string(clause[i]);
      }
    }
    std::fprintf(
        transitive_trace.out,
        "tactix\twalk-watch\t0\t%llu\t%d\t%zu\t%d\t0\t0\t%d\t0\t0\t0"
        "\t%u\t%u\t%llu\t%llu\t%llu\t%llu\t%d\t%d\t%d\t%d\t%llu\t%s\n",
        static_cast<unsigned long long>(k.stats.conflicts),
        kernel_externalize(k, watched), ordinal,
        other == INVALID_LIT ? 0 : kernel_externalize(k, other),
        entry.binary() ? 0 : 1, watched,
        other == INVALID_LIT ? INVALID_LIT : other,
        static_cast<unsigned long long>(kernel_import_lit_order(k, watched)),
        static_cast<unsigned long long>(
            other == INVALID_LIT ? UINT64_MAX
                                 : kernel_import_lit_order(k, other)),
        static_cast<unsigned long long>(
            kernel_compact_import_lit_order(k, watched)),
        static_cast<unsigned long long>(
            other == INVALID_LIT
                ? UINT64_MAX
                : kernel_compact_import_lit_order(k, other)),
        !kernel_var_retired(k, var_of(watched)),
        other != INVALID_LIT && !kernel_var_retired(k, var_of(other)),
        k.values[watched], other == INVALID_LIT ? 0 : k.values[other],
        static_cast<unsigned long long>(id), payload.c_str());
    ordinal++;
  }
}
#else
inline bool transitive_trace_pair(const Kernel &, Lit, Lit) { return false; }
inline bool transitive_trace_all_sources() { return false; }
inline void transitive_trace_endpoints(const Kernel &) {}
inline void transitive_trace_admission(const Kernel &, Lit, Lit, uint32_t,
                                       const char *) {}
inline void transitive_trace_walk_node(const Kernel &, Lit, Lit, Lit, size_t,
                                       uint64_t, uint64_t, uint32_t) {}
inline void transitive_trace_walk_entries(const Kernel &, Lit, Lit, Lit,
                                          Lit) {}
inline void transitive_trace_row(const Kernel &, const char *, uint64_t, Lit,
                                 size_t, Lit, uint64_t, uint64_t, bool, bool,
                                 size_t, uint32_t) {}
#endif

// Out-edges of p are the binary watches of negate(p): (negate(p) ∨ q) is
// exactly p → q. Nothing pushes to the watch arena during a walk, so an entries
// range stays valid throughout — kernel_delete_binary only ever runs between
// walks, and compacts in place.

// Per-round scratch; nothing rule-scoped leaks into the kernel. `dead` is
// indexed by the other endpoint x and means "the pair (src ∨ x) is excluded
// from every walk": one flag serving both as the candidate's self-exclusion and
// as the round-local tombstone.
struct Walk {
  std::vector<uint8_t> reached; // by lit
  std::vector<Lit> touched;     // the marked lits, for O(marked) clearing
  std::vector<Lit> queue;       // BFS frontier
  std::vector<uint32_t> dead;   // by lit x: excluded copies of (src ∨ x)
  std::vector<Lit> dead_list;   // one entry per tombstoned physical copy
  std::vector<uint32_t> seen;   // per-walk source-copy ordinal by destination
  std::vector<Lit> seen_list;   // touched entries of `seen`
  std::vector<Lit> cands;       // physical source-watch candidate order
  uint64_t ticks = 0;           // cache lines scanned; reduce_source flushes
                                // into stats.transitive_ticks (const-walk
                                // helpers charge here instead of the kernel)

  void init(const Kernel &k) {
    const size_t nlits = 2 * static_cast<size_t>(k.nvars);
    reached.assign(nlits, 0);
    dead.assign(nlits, 0);
    seen.assign(nlits, 0);
  }
};

// Kissat charges `1 + ceil(number-of-raw-watch-words / 32)` for every source
// or propagated watch list it opens (`kissat_cache_lines(n,sizeof(watch))`,
// with 4-byte watch words and its assumed 128-byte load line).  In watching
// mode both engines use exactly one word for a binary and two for a large watch
// (blocker plus reference), so the raw vector/arena size is the portable unit.
uint64_t transitive_watch_ticks(const Kernel &k, Lit lit,
                                uint32_t *word_count = nullptr) {
  const uint32_t words = k.watches.size(lit);
  if (word_count)
    *word_count = words;
  return 1 + ((static_cast<uint64_t>(words) + 31) >> 5);
}

// Reachability walk from ¬src over the binary implication graph, skipping every
// edge whose clause is one of this source's excluded (src ∨ x) pairs, in either
// direction — that is what keeps a candidate out of its own justifying path.
// Read-only. Returns true iff dst is reached; sets *failed when some literal is
// reached with its negation, i.e. ¬src is a failed literal, deferred to V1.
bool walk_reaches(const Kernel &k, Walk &w, Lit src, Lit dst, bool *failed) {
  *failed = false;
  for (const Lit l : w.touched)
    w.reached[l] = 0;
  w.touched.clear();
  w.queue.clear();
  for (const Lit lit : w.seen_list)
    w.seen[lit] = 0;
  w.seen_list.clear();

  const Lit root = negate(src);
  w.reached[root] = 1;
  w.touched.push_back(root);
  w.queue.push_back(root);
  bool found = false;
  for (size_t qi = 0; qi < w.queue.size(); qi++) {
    const Lit l = w.queue[qi];
    const Lit nl = negate(l); // edges l → q are the binary watches of ¬l
    // the walk's real memory traffic, in propagate's cache-line unit: this
    // scan is the whole cost of visiting `l`
    const uint64_t node_ticks_before = k.stats.transitive_ticks + w.ticks;
    uint32_t watch_words = 0;
    w.ticks += transitive_watch_ticks(k, nl, &watch_words);
    transitive_trace_walk_node(k, src, dst, l, qi, node_ticks_before,
                               k.stats.transitive_ticks + w.ticks,
                               watch_words);
    transitive_trace_walk_entries(k, src, dst, l, nl);
    uint32_t seen_mirror_copies = 0;
    for (const WatchEntry wt : k.watches.entries(nl)) {
      if (!wt.binary())
        continue;
      const Lit q = wt.other();
      // Kissat excludes the current physical source-watch slot, then removes
      // each redundant copy before the next candidate. Reproduce that without
      // mutating a list being scanned: `dead` counts prior tombstones plus the
      // current occurrence. On the source direction all of them are absent;
      // on the mirror direction the current copy is still present during its
      // own walk, so only the prior tombstones are absent.
      if (nl == src) {
        uint32_t &seen = w.seen[q];
        if (!seen)
          w.seen_list.push_back(q);
        if (seen++ < w.dead[q])
          continue;
      }
      if (q == src) {
        const uint32_t current = nl == dst ? 1 : 0;
        assert(w.dead[nl] >= current);
        if (seen_mirror_copies++ < w.dead[nl] - current)
          continue;
      }
      // root-fixed literals are not nodes. At an engine fixpoint an active node
      // has no edge to a root-false literal, and every successor of a root-true
      // literal is itself root-true, so no path to an active dst is lost;
      // skipping is conservative anyway — fewer paths found, never more
      if (k.values[q] != 0)
        continue;
      if (w.reached[q])
        continue;
      w.reached[q] = 1;
      w.touched.push_back(q);
      if (w.reached[negate(q)]) {
        *failed = true; // ¬src implies both polarities of q
        return false;
      }
      if (q == dst) {
        found = true; // normally no early exit: the walk runs to exhaustion so
                      // failed-literal detection sees the whole reachable set
        if (k.transitive_early_exit)
          return true; // kissat transitive.c:130; forfeits this walk's units
      }
      w.queue.push_back(q);
    }
  }
  return found;
}

// Collect one source's candidates in physical binary-watch order and return
// ¬src's binary out-degree. Kissat processes duplicate copies independently.
uint32_t collect_candidates(const Kernel &k, Walk &w, Lit src) {
  w.cands.clear();
  uint32_t out_degree = 0;
  // watches[src] holds (src ∨ q) for every out-edge ¬src → q, so its binary
  // count IS ¬src's out-degree
  w.ticks += transitive_watch_ticks(k, src); // src-list scan traffic
  for (const WatchEntry wt : k.watches.entries(src)) {
    if (!wt.binary())
      continue;
    const Lit dst = wt.other();
    out_degree++;
    // Kissat's compact literal comparison tests each undirected clause from
    // one endpoint only. Stable Tactix identities are not compact identities,
    // so use the policy-owned live compact-position bridge for the same test.
    // A root-fixed dst means (src ∨ dst) is satisfied: C2's flush owns it.
    const bool owned = kernel_compact_import_lit_order(k, dst) >
                       kernel_compact_import_lit_order(k, src);
    uint32_t copies = 0;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (transitive_trace_pair(k, src, dst))
      for (const WatchEntry copy : k.watches.entries(src))
        copies += copy.binary() && copy.other() == dst;
#endif
    const char *skip_reason = !owned             ? "ownership"
                              : k.values[dst] != 0 ? "destination-fixed"
                              : copies > 1         ? "duplicate"
                                                   : "admitted";
    transitive_trace_admission(k, src, dst, copies, skip_reason);
    if (!owned || k.values[dst] != 0)
      continue;
    w.cands.push_back(dst);
  }
  return out_degree;
}

// one source: walk each candidate, tombstone the redundant ones, then delete.
// Preconditions: guard holds, src is in range and active. Leaves w clean.
ApplyResult reduce_source(Kernel &k, Lit src, Walk &w) {
  const uint64_t source_ticks_before = k.stats.transitive_ticks + w.ticks;
  const uint64_t source_checked_before = k.stats.transitive_checked;
  const uint32_t out_degree = collect_candidates(k, w, src);
  bool trace_source = transitive_trace_all_sources();
  for (const Lit dst : w.cands)
    trace_source = trace_source || transitive_trace_pair(k, src, dst);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  const uint64_t trace_sequence =
      trace_source ? transitive_trace.source_sequence++ : UINT64_MAX;
#else
  const uint64_t trace_sequence = UINT64_MAX;
#endif
  if (trace_source)
    transitive_trace_row(k, "source-begin", trace_sequence, src, 0,
                         INVALID_LIT, source_ticks_before,
                         k.stats.transitive_ticks + w.ticks, false, false,
                         w.cands.size(), out_degree);
  if (w.cands.empty()) {
    k.stats.transitive_ticks += w.ticks;
    w.ticks = 0;
    return ApplyResult::NoOp;
  }

  size_t candidate_ordinal = 0;
  for (const Lit dst : w.cands) {
    // Poll per dst walk, as kissat does (transitive.c:181-183): a source's
    // candidate set is unbounded, so a poll only between sources leaves both
    // the dose and an interrupt unobservable for the length of one source.
    // Breaking here is sound — nothing is deleted until the loop below, and
    // each tombstone was established against the graph minus the earlier ones,
    // so a prefix of `dead_list` is exactly the result of a shorter round.
    k.stats.transitive_polls++;
    if (kernel_terminating(k)) {
      k.stats.transitive_terminated++;
      break;
    }
    k.stats.transitive_checked++;
    w.dead[dst]++; // exclude this physical candidate occurrence
    const uint64_t ticks_before = k.stats.transitive_ticks + w.ticks;
    bool failed = false;
    const bool redundant = walk_reaches(k, w, src, dst, &failed);
    // Credit in flight so the policy's between-instance meter check sees this
    // source's spend as it happens rather than after it is over.
    if (k.transitive_live_meter) {
      k.stats.transitive_ticks += w.ticks;
      w.ticks = 0;
    }
    const uint64_t ticks_after = k.stats.transitive_ticks + w.ticks;
    if (trace_source)
      transitive_trace_row(k, "candidate", trace_sequence, src,
                           candidate_ordinal, dst, ticks_before, ticks_after,
                           redundant, failed, w.cands.size(), out_degree);
    candidate_ordinal++;
    if (failed) {
      // ¬src is a failed literal: V1 Probe owns the unit and its chain, and
      // once src is fixed C2's flush removes these binaries anyway. Abort the
      // source — nothing has been deleted yet, only tombstoned
      k.stats.transitive_failed++;
      w.dead[dst]--;
      for (const Lit d : w.dead_list)
        w.dead[d]--;
      w.dead_list.clear();
      k.stats.transitive_ticks += w.ticks;
      w.ticks = 0;
      return ApplyResult::NoOp;
    }
    if (redundant)
      w.dead_list.push_back(dst); // this copy stays tombstoned for later walks
    else
      w.dead[dst]--; // this copy survives: restore it for later walks
    // Kissat tests the absolute TransitiveReduce effort limit after each fully
    // charged candidate (`transitive.c`), so a candidate that crosses the
    // boundary is retained and the next one is not started.  DefaultPolicy
    // hands this source its residual allowance through `work_budget`.
    const uint64_t budget_spent =
        k.transitive_budget_checked
            ? k.stats.transitive_checked - source_checked_before
            : k.stats.transitive_ticks + w.ticks - source_ticks_before;
    if (k.work_budget && budget_spent > k.work_budget)
      break;
  }

  // mutate only now: kernel_delete_binary compacts watches[src], the very
  // list the walks above read
  ApplyResult res = ApplyResult::NoOp;
  for (const Lit dst : w.dead_list) {
    if (trace_source)
      transitive_trace_row(k, "delete", trace_sequence, src, 0, dst,
                           k.stats.transitive_ticks + w.ticks,
                           k.stats.transitive_ticks + w.ticks, true, false,
                           w.cands.size(), out_degree);
    if (kernel_delete_binary_one_copy(k, src, dst)) {
      k.stats.transitive_deleted++;
      res = ApplyResult::Ok;
    }
    w.dead[dst]--;
  }
  w.dead_list.clear();
  k.stats.transitive_ticks += w.ticks;
  w.ticks = 0;
  if (trace_source)
    transitive_trace_row(k, "source-end", trace_sequence, src,
                         candidate_ordinal, INVALID_LIT, source_ticks_before,
                         k.stats.transitive_ticks, res == ApplyResult::Ok,
                         false, w.cands.size(), out_degree);
  return res;
}

} // namespace

void transitive_reduce_sources(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  transitive_trace_endpoints(k);
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  for (Lit src = 0; src < nlits; src++) {
    const Var v = var_of(src);
    if (k.values[src] == 0 && !kernel_var_retired(k, v))
      out.push_back(src);
  }
}

ApplyResult transitive_reduce_try(Kernel &k, Lit src) {
  assert(transitive_reduce_guard(k));
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (var_of(src) >= k.nvars || k.values[src] != 0)
    return ApplyResult::NoOp; // out of range / root-fixed
  // Persistent per-instance scratch: a fresh Walk costs three O(2·nvars)
  // zero-fills per dosed candidate, the per-call linear-scratch hazard. Reuse
  // is behaviour-identical because every Walk array is invariant-clean between
  // calls (`dead`/`copies` restored on exit, `reached` cleared at walk entry);
  // re-init only when nvars grew. thread_local keeps the scratch rule-scoped
  // and is safe under the plan's sequential-solving contract.
  static thread_local Walk w;
  if (w.reached.size() != 2 * static_cast<size_t>(k.nvars))
    w.init(k);
  return reduce_source(k, src, w);
}

// canonical instance: one full round over every source literal
ApplyResult transitive_reduce_apply(Kernel &k) {
  assert(transitive_reduce_guard(k));
  // D-LEVEL: drain the root closure U (no-op at a fixpoint); a conflict
  // refutes. Every inprocessing entry point drains — the rule is sound either
  // way, since an undrained root finds fewer paths, but a policy author reasons
  // from the blanket claim.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.transitive_rounds++;
  std::vector<Lit> sources;
  transitive_reduce_sources(k, sources);
  if (sources.empty())
    return ApplyResult::NoOp;
  Walk w;
  w.init(k);
  ApplyResult res = ApplyResult::NoOp;
  for (const Lit src : sources) {
    // a source can go stale as earlier sources delete edges (never assigned:
    // V4 changes no values); reduce_source re-derives everything it needs
    if (reduce_source(k, src, w) == ApplyResult::Ok)
      res = ApplyResult::Ok;
  }
  return res;
}

// instance surface: payload = source literal, the shared lit_instance encoding;
// INSTANCE_CANONICAL = the full round
static uint32_t transitive_reduce_instances(Kernel &k, uint64_t *out,
                                            uint32_t cap) {
  std::vector<Lit> srcs;
  transitive_reduce_sources(k, srcs);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(srcs.size()));
  // The set, in the rule's own order. Where a caller starts in it is that
  // caller's business: the resumption cursor lives in the policy
  // (DefaultPolicy::SlotEntry::cursor), where a scheduling decision belongs.
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(k, srcs[i]);
  return n;
}

static ApplyResult transitive_reduce_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return transitive_reduce_apply(k);
  Lit src;
  if (!lit_of_instance(k, inst, &src))
    return ApplyResult::NoOp;
  return transitive_reduce_try(k, src);
}

TACTIX_REGISTER_RULE(TransitiveReduce, RuleTier::Inprocess,
                     transitive_reduce_guard, transitive_reduce_apply,
                     transitive_reduce_instances,
                     transitive_reduce_apply_instance);

} // namespace tactix

#include "rules/factor/factor.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp" // inprocess_reach_root

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tactix {

namespace {

// Factor work currency (kissat factor.c:274, utilities.h kissat_cache_lines):
// 1 + one tick per 128 bytes touched, so a walk over `words` uint32_t watch
// entries costs 1 + (4·words)/128. The apply-local stream retains unique
// binary partners and large-clause occurrences, like Kissat's dense watches
// (`docs/scheduling-parity-ledger.md` §13).
inline uint64_t factor_cache_lines(uint64_t watches) {
  return watches ? (watches + 31) >> 5 : 0;
}

inline void charge_factor_watch_scan(Kernel &k, uint64_t watches) {
  k.stats.factor_ticks += 1 + factor_cache_lines(watches);
}

// Factor's null-stack dense entry leaves every pre-existing watch slice
// binary-only.  Traverse that packed four-byte image directly, as Kissat's
// `all_binary_large_watches` does over its dense stack, instead of paying the
// sparse reader's tag-dependent stride on every binary.  A composite factoring
// can append a large quotient later in the same application; those mixed lists
// deliberately fall back to the ordinary entry-aware iterator.
template <class Visit>
inline void for_each_factor_binary(const Kernel &k, Lit lit, Visit visit) {
  const WatchHead &head = k.watches.heads[lit];
  if (head.size == head.binary_count()) {
    const uint32_t *word = k.watches.arena.data() + head.offset;
    const uint32_t *const end = word + head.size;
    for (; word != end; ++word) {
      assert(Watch{*word}.binary());
      visit(Watch{*word}.other());
    }
    return;
  }
  for (const WatchEntry watch : k.watches.entries(lit))
    if (watch.binary())
      visit(watch.other());
}

// --- remainder interning (`docs/design/e2-factorsize-design.md`) -------------
// A remainder is `C \ {f}` for a grid clause C.  Content is interned so equal
// remainders compare cheaply, but quotient columns are clause OCCURRENCES and
// may therefore carry the same id more than once.  This distinction is
// load-bearing: Kissat's quotient stacks retain one watch per clause and match
// them one-to-one as factors are added.  `unit_id` grows on demand, never
// fill-initialized per call, which would make memset dominate.
struct RemPool {
  std::vector<Lit> pool;            // flat literal storage
  std::vector<uint32_t> off, len;   // per id
  std::vector<int32_t> unit_id;     // literal -> id for |r| == 1, -1 = unseen

  uint32_t intern_unit(Lit l) {
    if (unit_id.size() <= static_cast<size_t>(l))
      unit_id.resize(static_cast<size_t>(l) + 1, -1);
    if (unit_id[l] >= 0)
      return static_cast<uint32_t>(unit_id[l]);
    const uint32_t id = static_cast<uint32_t>(off.size());
    off.push_back(static_cast<uint32_t>(pool.size()));
    len.push_back(1);
    pool.push_back(l);
    hashes.push_back(0); // keeps `hashes` indexed by id; units never probe
    unit_id[l] = static_cast<int32_t>(id);
    return id;
  }
  // Multi-literal intern; `b[0..n)` must be sorted ascending. Identity is by
  // content.  Interning never implies occurrence deduplication.
  //
  // The index is an open-addressed table of ids rather than a map of buckets:
  // one application interns tens of millions of remainders, and a node plus a
  // bucket vector per distinct hash made allocation, and then the pool's own
  // destruction, the rule's largest cost.  Ids are still handed out in
  // first-intern order and still compared by content, so the table changes
  // only how a match is found.
  std::vector<uint32_t> table;  // id + 1, 0 = empty
  std::vector<uint64_t> hashes; // by id; 0 for the unit ids, which never index
  uint32_t occupied = 0;

  void rehash_table(size_t capacity) {
    table.assign(capacity, 0);
    const size_t mask = capacity - 1;
    for (uint32_t id = 0; id < static_cast<uint32_t>(off.size()); id++) {
      if (len[id] == 1)
        continue;
      size_t pos = hashes[id] & mask;
      while (table[pos])
        pos = (pos + 1) & mask;
      table[pos] = id + 1;
    }
  }

  uint32_t intern(const Lit *b, uint32_t n) {
    if (n == 1)
      return intern_unit(b[0]);
    uint64_t h = 1469598103934665603ull; // FNV-1a over the sorted literals
    for (uint32_t i = 0; i < n; i++) {
      h ^= b[i];
      h *= 1099511628211ull;
    }
    if (table.empty() || (occupied + 1) * 4 > table.size() * 3)
      rehash_table(table.empty() ? size_t{1} << 12 : table.size() * 2);
    const size_t mask = table.size() - 1;
    size_t pos = h & mask;
    while (table[pos]) {
      const uint32_t id = table[pos] - 1;
      if (hashes[id] == h && len[id] == n &&
          !std::memcmp(pool.data() + off[id], b, n * sizeof(Lit)))
        return id;
      pos = (pos + 1) & mask;
    }
    const uint32_t id = static_cast<uint32_t>(off.size());
    off.push_back(static_cast<uint32_t>(pool.size()));
    len.push_back(n);
    pool.insert(pool.end(), b, b + n);
    hashes.push_back(h);
    table[pos] = id + 1;
    occupied++;
    return id;
  }
  uint32_t count() const { return static_cast<uint32_t>(off.size()); }
  uint32_t size_of(uint32_t id) const { return len[id]; }
  const Lit *lits_of(uint32_t id) const { return pool.data() + off[id]; }
  Lit lit0(uint32_t id) const { return pool[off[id]]; }
};

// --- the clause index for factor_size >= 3 -----------------------------------
// Occurrence lists over irredundant clauses of size 3..factor_size, one arena
// pass per apply — the N1-safe shape BCE uses, since `Kernel::dense` is null
// outside an inprocessing phase. Binaries are not indexed: routing them here
// would change the order `factor_size == 2` bit-identity depends on. Redundant
// clauses are excluded because S2 Reduce could delete a grid member between
// selection and application.
struct FactorWatch {
  Ref ref = INVALID_REF;
  // One apply-local key beside the reference, so the record is a pair of
  // words and a scan streams the same width kissat's dense watch does.  For a
  // large entry it is the lazily interned remainder id: a large clause and
  // the factor literal naming this dense-watch entry are immutable for the
  // lifetime of one Factor application, so caching it lets repeated seed
  // visits skip copying, sorting, hashing and comparing the same remainder.
  // UINT32_MAX also covers a remainder that was not yet materialized;
  // failures caused by the moving fresh-literal boundary are deliberately not
  // cached.  For a binary entry it is the partner literal; those are borrowed
  // from Kernel::watches and never copied here.  The occurrence id this used
  // to carry existed only to index Factor's temporary marks, which now live
  // in the clause header as kissat's do.
  uint32_t aux = UINT32_MAX;
  uint32_t remainder_id() const {
    assert(!binary());
    return aux;
  }
  void set_remainder_id(uint32_t id) {
    assert(!binary());
    aux = id;
  }
  bool binary() const { return ref == INVALID_REF; }
  Lit other() const {
    assert(binary());
    return static_cast<Lit>(aux);
  }
  static FactorWatch binary_watch(Lit other) {
    FactorWatch result;
    result.aux = other;
    return result;
  }
  static FactorWatch large_watch(Ref ref) {
    FactorWatch result;
    result.ref = ref;
    return result;
  }
};
static_assert(sizeof(FactorWatch) == 8,
              "Factor's dense scan requires a compact watch record");

struct FactorIndex {
  // The initial large-clause suffix followed by every binary or large entry
  // appended during this Factor application.  Initial binary partners stay in
  // the kernel's compact watch slices: copying every 4-byte kernel watch into
  // a 16-byte FactorWatch dominated binary-heavy SAT instances and has no
  // counterpart in Kissat, whose dense mode reuses its watch stacks.
  // `initial_binary_live` identifies the still-live borrowed prefix.  Together
  // that prefix and this vector are Kissat's one mixed dense stream.
  std::vector<std::vector<FactorWatch>> order;
  std::vector<uint32_t> initial_binary_live;
  // Exact live size of each dense Factor watch list.  Kissat obtains this in
  // O(1) from its stack header for heap scores and next-factor tie breaks.
  // Re-walking every dense list here makes a tied m-by-n grid cubic.
  std::vector<uint32_t> live_degree;
  uint32_t large_count = 0;
  bool built = false;
};

// Kissat's Factor schedule is a mutable max-heap, not a lazy priority queue.
// Equal scores preserve heap topology: bubble-up stops at equality and
// bubble-down prefers the left child.  This matters after a factoring updates
// only the literals in the removed/added clauses; globally re-scoring a stale
// entry changes which otherwise valid grid consumes the shared clauses first.
class FactorHeap {
public:
  bool empty() const { return stack_.empty(); }
  uint32_t size() const { return static_cast<uint32_t>(stack_.size()); }
  const std::vector<Lit> &members() const { return stack_; }
  uint32_t position(Lit lit) const {
    return contains(lit) ? pos_[lit] : UINT32_MAX;
  }
  uint32_t score(Lit lit) const {
    return lit < score_.size() ? score_[lit] : 0;
  }

  void update(Lit lit, uint32_t score) {
    ensure(lit);
    const uint32_t old = score_[lit];
    score_[lit] = score;
    if (contains(lit)) {
      if (score <= 1) {
        erase(lit);
      } else if (score > old) {
        bubble_up(lit);
      } else if (score < old) {
        bubble_down(lit);
      }
    } else if (score > 1) {
      pos_[lit] = static_cast<uint32_t>(stack_.size());
      stack_.push_back(lit);
      bubble_up(lit);
    }
  }

  Lit pop_max() {
    assert(!stack_.empty());
    const Lit result = stack_[0];
    const Lit last = stack_.back();
    stack_.pop_back();
    pos_[result] = UINT32_MAX;
    if (!stack_.empty()) {
      stack_[0] = last;
      pos_[last] = 0;
      bubble_down(last);
    }
    return result;
  }

private:
  bool contains(Lit lit) const {
    return lit < pos_.size() && pos_[lit] != UINT32_MAX;
  }
  void ensure(Lit lit) {
    const size_t need = static_cast<size_t>(lit) + 1;
    if (pos_.size() >= need)
      return;
    pos_.resize(need, UINT32_MAX);
    score_.resize(need, 0);
  }
  void bubble_up(Lit lit) {
    uint32_t at = pos_[lit];
    while (at) {
      const uint32_t parent_at = (at - 1) / 2;
      const Lit parent = stack_[parent_at];
      if (score_[parent] >= score_[lit])
        break;
      stack_[at] = parent;
      pos_[parent] = at;
      at = parent_at;
    }
    stack_[at] = lit;
    pos_[lit] = at;
  }
  void bubble_down(Lit lit) {
    uint32_t at = pos_[lit];
    for (;;) {
      uint32_t child_at = 2 * at + 1;
      if (child_at >= stack_.size())
        break;
      Lit child = stack_[child_at];
      const uint32_t sibling_at = child_at + 1;
      if (sibling_at < stack_.size() &&
          score_[stack_[sibling_at]] > score_[child]) {
        child_at = sibling_at;
        child = stack_[child_at];
      }
      if (score_[child] <= score_[lit])
        break;
      stack_[at] = child;
      pos_[child] = at;
      at = child_at;
    }
    stack_[at] = lit;
    pos_[lit] = at;
  }
  void erase(Lit lit) {
    const uint32_t at = pos_[lit];
    const Lit last = stack_.back();
    stack_.pop_back();
    pos_[lit] = UINT32_MAX;
    if (at == stack_.size())
      return;
    stack_[at] = last;
    pos_[last] = at;
    bubble_up(last);
    bubble_down(last);
  }

  std::vector<Lit> stack_;
  std::vector<uint32_t> pos_, score_;
};

#ifdef TACTIX_ENABLE_EVENT_TRACE
void trace_factor_heap(const Kernel &k, const FactorHeap &heap, Lit grid_seed,
                       const char *stage) {
  static bool heap_set_dumped = false;
  const char *heap_set_path =
      std::getenv("SAT_EVENT_TRACE_FACTOR_HEAP_SET_PATH");
  const char *selected_ordinal =
      std::getenv("SAT_EVENT_TRACE_FACTOR_HEAP_ORDINAL");
  if (selected_ordinal &&
      std::strtoull(selected_ordinal, nullptr, 10) != k.stats.factorings)
    return;
  const char *selected_stage =
      std::getenv("SAT_EVENT_TRACE_FACTOR_HEAP_SET_STAGE");
  if (!selected_stage || !*selected_stage)
    selected_stage = "updated";
  if (!heap_set_dumped && heap_set_path && *heap_set_path &&
      selected_ordinal && !std::strcmp(stage, selected_stage)) {
    if (FILE *out = std::fopen(heap_set_path, "w")) {
      std::fputs("# solver\tposition\texternal\tscore\n", out);
      const auto &members = heap.members();
      for (size_t position = 0; position != members.size(); ++position) {
        const Lit lit = members[position];
        std::fprintf(out, "tactix\t%zu\t%d\t%u\n", position,
                     kernel_externalize(k, lit), heap.score(lit));
      }
      std::fclose(out);
      heap_set_dumped = true;
    }
  }
  if (std::getenv("SAT_EVENT_TRACE_FACTOR_HEAP_DIGEST") &&
      !std::strcmp(stage, "updated")) {
    uint64_t hash = 1469598103934665603ull;
    const auto &members = heap.members();
    for (const Lit lit : members) {
      hash ^= static_cast<uint32_t>(kernel_externalize(k, lit));
      hash *= 1099511628211ull;
      hash ^= heap.score(lit);
      hash *= 1099511628211ull;
    }
    const Lit top = members.empty() ? INVALID_LIT : members[0];
    const uint32_t top_score = members.empty() ? 0 : heap.score(top);
    event_trace_factor_heap_digest(k, grid_seed,
                                   static_cast<uint32_t>(members.size()),
                                   hash, top, top_score);
  }
  const char *selected = std::getenv("TACTIX_EVENT_TRACE_FACTOR_HEAP");
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
  if (a == INVALID_LIT || b == INVALID_LIT)
    return;
  event_trace_factor_heap(k, grid_seed, stage, a, heap.position(a),
                          heap.score(a), b, heap.position(b), heap.score(b),
                          heap.size());
}

bool factor_match_trace_selected(const Kernel &k, Lit seed, Lit next) {
  const char *path = std::getenv("TACTIX_FACTOR_MATCH_TRACE");
  const char *pair = std::getenv("SAT_FACTOR_MATCH_PAIR");
  if (!path || !*path || !pair || !*pair)
    return false;
  if (const char *conflict = std::getenv("SAT_FACTOR_MATCH_CONFLICT"))
    if (*conflict && std::strtoull(conflict, nullptr, 10) != k.stats.conflicts)
      return false;
  int selected_seed = 0, selected_next = 0;
  char tail = 0;
  if (std::sscanf(pair, "%d,%d%c", &selected_seed, &selected_next, &tail) != 2)
    return false;
  return selected_seed == kernel_externalize(k, seed) &&
         selected_next == kernel_externalize(k, next);
}

void trace_factor_match_large(const Kernel &k, Lit seed, Lit next,
                              uint32_t factors, size_t column,
                              const Lit *remainder, uint32_t remainder_size,
                              Lit scan, uint32_t degree, uint32_t visits,
                              bool matched) {
  static FILE *out = nullptr;
  if (!out) {
    const char *path = std::getenv("TACTIX_FACTOR_MATCH_TRACE");
    if (!path || !*path)
      return;
    out = std::fopen(path, "w");
    if (!out) {
      std::perror("TACTIX_FACTOR_MATCH_TRACE");
      std::exit(2);
    }
    std::setvbuf(out, nullptr, _IOLBF, 0);
    std::fputs("# factor-match\t1\n", out);
    std::fputs("# solver\tseed\tnext\tfactors\tcolumn\tremainder\tscan\t"
               "degree\tvisits\tmatched\n",
               out);
  }
  std::vector<int> external;
  external.reserve(remainder_size);
  for (uint32_t i = 0; i != remainder_size; ++i)
    external.push_back(kernel_externalize(k, remainder[i]));
  std::sort(external.begin(), external.end());
  std::fprintf(out, "tactix\t%d\t%d\t%u\t%zu\t",
               kernel_externalize(k, seed), kernel_externalize(k, next),
               factors, column);
  for (size_t i = 0; i != external.size(); ++i)
    std::fprintf(out, "%s%d", i ? "," : "", external[i]);
  std::fprintf(out, "\t%d\t%u\t%u\t%u\n", kernel_externalize(k, scan),
               degree, visits, matched);
}
#endif

// Count binary clauses per literal — the `bincount` half of kissat's candidate
// reduction (factor.c:1019). Binaries are never pruned, being the grid's other
// side; they only decide whether a large clause survives.
static void count_binaries(const Kernel &k, std::vector<uint32_t> &bincount) {
  bincount.assign(2 * k.nvars, 0);
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++) {
    // Native counts a pair once, from its smaller imported endpoint, only
    // while that endpoint is ACTIVE. Root-fixed identities remain allocated
    // in Tactix, so retirement alone is not the corresponding predicate.
    // This is the candidate-peeling census, not the physical matching degree:
    // the null-stack dense transition still retains satisfied binaries.
    if (k.value(l) || kernel_var_retired(k, var_of(l)))
      continue;
    const uint64_t order = kernel_import_lit_order(k, l);
    for_each_factor_binary(k, l, [&](Lit other) {
      if (order > kernel_import_lit_order(k, other))
        return;
      bincount[l]++;
      bincount[other]++;
    });
  }
}

void build_factor_index(const Kernel &k, FactorIndex &idx, uint32_t max_size) {
  idx.order.assign(2 * k.nvars, {});
  idx.initial_binary_live.assign(2 * k.nvars, 0);
  idx.live_degree.assign(2 * k.nvars, 0);
  idx.large_count = 0;
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++) {
    idx.initial_binary_live[lit] = k.watches.binary_size(lit);
    idx.live_degree[lit] = idx.initial_binary_live[lit];
    // Factor enters Kissat's dense mode with a null binary-save stack.  That
    // flushes large watches but deliberately keeps the current sparse binary
    // sequence; unlike Congruence it does not sort/reconnect the pairs.  This
    // order is both the quotient-column order and a tie-break during greedy
    // growth.  Re-sorting by import identity here reversed sted5's two
    // quotients (-3016,-198)/(-3016,-199), producing the first search
    // divergence at conflict 146 despite an identical clause set.
  }
  // --- candidate reduction (kissat `factorcandrounds`, factor.c:1000) --------
  // A large clause is grid material only if every literal occurs in at least
  // two candidate clauses. Dropping one can drop its neighbours below the bar,
  // so the prune iterates to fixpoint, bounded by `k.factor_cand_rounds`; 0
  // restores the unfiltered candidate set exactly.
  std::vector<uint8_t> alive;
  if (k.factor_cand_rounds && max_size >= 3) {
    std::vector<uint32_t> bincount, largecount, newcount;
    count_binaries(k, bincount);
    // seed: every in-scope irredundant large clause is a candidate
    largecount.assign(2 * k.nvars, 0);
    std::vector<Ref> cand;
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(c);
      if (!c->garbage() && !c->redundant() && c->size >= 3 &&
          c->size <= max_size) {
        cand.push_back(ref);
        for (const Lit l : *c)
          largecount[l]++;
      }
      ref += words;
    }
    alive.assign(cand.size(), 1);
    size_t live = cand.size();
    for (uint32_t round = 0; round < k.factor_cand_rounds; round++) {
      newcount.assign(2 * k.nvars, 0);
      size_t now = 0;
      for (size_t i = 0; i < cand.size(); i++) {
        if (!alive[i])
          continue;
        const Clause *c = clause_at(k.arena, cand[i]);
        bool keep = true;
        for (const Lit l : *c)
          if (bincount[l] + largecount[l] < 2) {
            keep = false;
            break;
          }
        alive[i] = keep;
        if (!keep)
          continue;
        for (const Lit l : *c)
          newcount[l]++;
        now++;
      }
      largecount.swap(newcount);
      if (now == live) // fixpoint: nothing dropped this round
        break;
      live = now;
    }
    // Kissat does not connect the last round's survivor bitmap directly.
    // Its separate connection pass tests every survivor against the counts
    // produced by that last round (factor.c:1098-1119).  This is one more
    // eligibility test, without another count rebuild.  Omitting it retains
    // the final shell of a cascading peel and can expose a physical clause to
    // Factor that the reference deliberately leaves disconnected.
    for (size_t i = 0; i < cand.size(); i++) {
      if (!alive[i])
        continue;
      const Clause *c = clause_at(k.arena, cand[i]);
      bool connect = true;
      for (const Lit l : *c)
        if (bincount[l] + largecount[l] < 2) {
          connect = false;
          break;
        }
      if (!connect)
        continue;
      idx.large_count++;
      for (const Lit l : *c) {
        idx.order[l].push_back(FactorWatch::large_watch(cand[i]));
        idx.live_degree[l]++;
      }
    }
    idx.built = true;
    return;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    if (!c->garbage() && !c->redundant() && c->size >= 3 && c->size <= max_size) {
      idx.large_count++;
      for (const Lit l : *c) {
        idx.order[l].push_back(FactorWatch::large_watch(ref));
        idx.live_degree[l]++;
      }
    }
    ref += words;
  }
  idx.built = true;
}

// Iterate the surviving binary prefix borrowed at Factor entry.  Binary
// deletion is stable and newly created binaries append to the kernel slice, so
// the first `initial_binary_live[l]` binary entries remain exactly that prefix.
template <class F>
void for_each_initial_factor_binary(const Kernel &k, const FactorIndex &idx,
                                    Lit l, F &&f) {
  if (l >= idx.initial_binary_live.size())
    return;
  uint32_t left = idx.initial_binary_live[l];
  for (const WatchEntry watch : k.watches.entries(l)) {
    if (!watch.binary())
      continue;
    if (!left)
      break;
    f(watch.other());
    left--;
  }
  assert(!left);
}

// kissat's per-literal Factor marks (`factor.c`): the literal is a factor of
// the current grid, a member of the current column's remainder, or already
// counted as a next-factor candidate for the current quotient clause.
static constexpr uint8_t MARK_FACTOR = 1;
static constexpr uint8_t MARK_QUOTIENT = 2;
static constexpr uint8_t MARK_NOUNTED = 4;

// Is clause `c` live and usable as a grid member?  Root assignments do not
// narrow Factor's clause-occurrence grid: Kissat connects every surviving
// irredundant clause and its first quotient retains assigned remainders.
[[maybe_unused]] bool grid_clause_live(const Kernel &k, const Clause *c) {
  (void)k;
  return !c->garbage() && !c->redundant();
}

// The index only ever holds live irredundant clauses: `build_factor_index`
// admits no others, and `apply_factoring` removes a grid clause's occurrence
// from every literal's list before marking it garbage, the way kissat's
// `delete_unfactored` does.  Nothing else runs inside one application, so the
// scans below assert the predicate instead of testing it, which lets a filter
// that rejects on a side array run before the clause header is loaded at all.
// On Rank 22 the test held 489,549,033 times and failed none.

// The remainder `C \ {l}`, written sorted into `out`. Returns false if any other
// literal crosses the extension boundary.  Assigned literals remain valid
// remainder members; the abstract E2 grid is over live clauses, not only the
// currently unassigned projection.
bool remainder_of(const Kernel &k, const Clause *c, Lit l, Lit limit,
                  std::vector<Lit> &out) {
  (void)k;
  out.clear();
  for (const Lit x : *c) {
    if (x == l)
      continue;
    if (x >= limit)
      return false; // extension-var boundary
    out.push_back(x);
  }
  std::sort(out.begin(), out.end());
  return !out.empty();
}

struct RemOccurrence {
  uint32_t id;
  Ref ref; // INVALID_REF denotes the proof-addressable binary pair.
};

// All remainder OCCURRENCES of `l` in Kissat's mixed dense-watch order: the
// borrowed binary prefix, the initially connected large clauses, then every
// binary or large clause appended during this Factor application. Equal large
// clauses deliberately yield repeated ids.
void remainder_occurrences_of(Kernel &k, FactorIndex &idx, RemPool &pool,
                              Lit l, Lit limit, std::vector<Lit> &rem_scratch,
                              std::vector<RemOccurrence> &out) {
  out.clear();
  if (!idx.built || l >= idx.order.size())
    return;
  for_each_initial_factor_binary(k, idx, l, [&](Lit other) {
    if (other < limit) {
      k.stats.factor_ticks++;
      out.push_back({pool.intern_unit(other), INVALID_REF});
    }
  });
  for (FactorWatch &watch : idx.order[l]) {
    if (watch.binary()) {
      const Lit other = watch.other();
      if (other < limit) {
        k.stats.factor_ticks++;
        out.push_back({pool.intern_unit(other), INVALID_REF});
      }
      continue;
    }
    if (k.factor_size < 3)
      continue;
    const Ref ref = watch.ref;
    k.stats.factor_ticks++;
    k.stats.factor_large_remainders++;
    if (watch.remainder_id() != UINT32_MAX) {
      out.push_back({watch.remainder_id(), ref});
      k.stats.factor_remainders_reused++;
      continue;
    }
    const Clause *c = clause_at(k.arena, ref);
    assert(grid_clause_live(k, c));
    if (!remainder_of(k, c, l, limit, rem_scratch))
      continue;
    watch.set_remainder_id(
        pool.intern(rem_scratch.data(), static_cast<uint32_t>(rem_scratch.size())));
    out.push_back({watch.remainder_id(), ref});
    k.stats.factor_remainders_materialized++;
  }
}

// Match C ∖ {factor} against an already-canonical remainder without
// materializing C's remainder. Clause literals are unique, and |C|=|R|+1,
// so membership plus the factor occurrence proves exact multiset equality.
// Membership is the caller's stamped mark, the way kissat reads one
// `marks[other] & QUOTIENT` byte per literal (`factor.c` quotient scan).
bool matches_interned_remainder(const Clause *c, Lit factor, Lit limit,
                                const std::vector<uint8_t> &marks,
                                uint8_t quotient_bit) {
  bool found_factor = false;
  for (const Lit lit : *c) {
    if (lit == factor) {
      found_factor = true;
      continue;
    }
    if (lit >= limit || lit >= marks.size() || !(marks[lit] & quotient_bit))
      return false;
  }
  return found_factor;
}

void apply_factoring(Kernel &k, FactorIndex &idx, const std::vector<Lit> &F,
                     const std::vector<uint32_t> &R, const RemPool &pool,
                     const std::vector<std::vector<Ref>> &grid_refs) {
#ifndef NDEBUG
  assert(grid_refs.size() == F.size());
  for (const auto &row : grid_refs)
    assert(row.size() == R.size());
#endif
  const Var xv = kernel_new_extension_vars(k, 1);
  const Lit px = make_lit(xv, false); // x
  const Lit nx = make_lit(xv, true);  // ¬x
  const bool lrat = k.proof && k.proof->lrat();
  const auto append_binary_watch = [&](Lit a, Lit b, uint64_t id) {
    (void)id;
    if (!idx.built)
      return;
    const size_t need = static_cast<size_t>(std::max(a, b)) + 1;
    if (idx.order.size() < need)
      idx.order.resize(need);
    if (idx.live_degree.size() < need)
      idx.live_degree.resize(need, 0);
    if (idx.initial_binary_live.size() < need)
      idx.initial_binary_live.resize(need, 0);
    idx.order[a].push_back(FactorWatch::binary_watch(b));
    idx.order[b].push_back(FactorWatch::binary_watch(a));
    idx.live_degree[a]++;
    idx.live_degree[b]++;
  };

  // Dividers (x ∨ fᵢ): RAT on the fresh literal x. At add time no clause
  // contains ¬x (the quotients come after), so the RAT condition is vacuous —
  // pivot-first, empty hints.
  std::vector<uint64_t> divider_ids(F.size());
  // `best_F` is the first-factor -> last-factor quotient chain. Kissat applies
  // that chain from the best quotient back through `prev`, so its dividers
  // enter every binary watch list last-factor first. The formula is unchanged
  // by this order, but propagation and learned-reason selection are not.
  for (size_t rev = F.size(); rev-- > 0;) {
    const Lit f = F[rev];
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {px, f}; // pivot (x) first
    if (k.proof)
      k.proof->add_rat(id, lits, 2, nullptr, 0);
    kernel_add_binary_clause(k, px, f, id);
    append_binary_watch(px, f, id);
    divider_ids[rev] = id;
  }

  // Quotients (¬x ∨ rⱼ): RAT on ¬x. The only clauses containing x are the m
  // dividers, so there is one RAT group per divider, each resolvent (fᵢ∨rⱼ) a
  // grid clause still live because the deletes come last. Hints are the flat
  // stream [-dividerᵢ, orig(fᵢ,rⱼ)]ᵢ, read before deletion.
  std::vector<int64_t> hints;
  std::vector<Lit> qlits;
  for (size_t column = 0; column < R.size(); column++) {
    const uint32_t rid = R[column];
    const uint32_t rn = pool.size_of(rid);
    const Lit *rl = pool.lits_of(rid);
    // Quotient {¬x} ∪ r, pivot first.  `rid` is only the canonical
    // comparison key.  Kissat constructs the actual quotient from the chosen
    // last-factor clause (`q->clauses`) and preserves that clause's literal
    // pose after removing the factor.  Reusing the sorted key here changes the
    // quotient's initial watched pair and therefore later propagation order.
    // The RAT-on-¬x argument is indifferent to |r|, so the hint stream stays
    // per-divider at any factor size.
    qlits.assign(1, nx);
    if (rn == 1) {
      qlits.push_back(rl[0]);
    } else {
      const Ref pose_ref = grid_refs.back()[column];
      assert(pose_ref != INVALID_REF);
      const Clause *pose = clause_at(k.arena, pose_ref);
      const Lit pose_factor = F.back();
      for (const Lit other : *pose)
        if (other != pose_factor)
          qlits.push_back(other);
      assert(qlits.size() == static_cast<size_t>(rn) + 1);
      assert(std::is_permutation(qlits.begin() + 1, qlits.end(), rl,
                                 rl + rn));
    }
    const uint64_t id = ++k.next_clause_id;
    if (lrat) {
      hints.clear();
      for (size_t i = F.size(); i-- > 0;) {
        const Ref gref = grid_refs[i][column];
        const uint64_t gid = (gref == INVALID_REF)
                                 ? kernel_binary_id(k, F[i], rl[0])
                                 : clause_at(k.arena, gref)->id();
        hints.push_back(-static_cast<int64_t>(divider_ids[i]));
        hints.push_back(static_cast<int64_t>(gid));
      }
    }
    if (k.proof)
      k.proof->add_rat(id, qlits.data(), static_cast<uint32_t>(qlits.size()),
                       hints.data(), static_cast<uint32_t>(hints.size()));
    if (rn == 1) {
      kernel_add_binary_clause(k, nx, rl[0], id);
      append_binary_watch(nx, rl[0], id);
    } else {
      const Ref qref =
          kernel_add_large_clause(k, qlits.data(),
                                  static_cast<uint32_t>(qlits.size()),
                                  /*redundant=*/false, /*glue=*/0, id);
      k.watches.defrag_if_needed();
      // Kissat inserts every quotient into its dense Factor watch lists before
      // continuing the same pass.  Keep this apply-local index live too: the
      // fresh clause can change later seed scores and can complete a composite
      // grid.  Stale original refs need no removal because grid_clause_live
      // filters their garbage marks.
      if (idx.built && qlits.size() <= k.factor_size) {
        if (idx.order.size() < static_cast<size_t>(2 * k.nvars))
          idx.order.resize(static_cast<size_t>(2 * k.nvars));
        if (idx.live_degree.size() < static_cast<size_t>(2 * k.nvars))
          idx.live_degree.resize(static_cast<size_t>(2 * k.nvars), 0);
        idx.large_count++;
        for (const Lit l : qlits) {
          idx.order[l].push_back(FactorWatch::large_watch(qref));
          idx.live_degree[l]++;
        }
      }
    }
  }

  // Delete the originals last (proof `del`), all copies: a duplicated grid
  // binary is fully replaced by the aux-var definition, while every large
  // occurrence is deleted through the exact Ref carried by the aligned grid.
  const auto delete_binary_watch = [&](Lit a, Lit b) {
    const uint32_t copies = kernel_binary_copies(k, a, b);
    assert(copies);
    if (!idx.built)
      return kernel_delete_binary_all_copies_known(k, a, b, copies);
    else {
      uint32_t dynamic_a = 0, dynamic_b = 0;
      for (const FactorWatch watch : idx.order[a])
        dynamic_a += watch.binary() && watch.other() == b;
      for (const FactorWatch watch : idx.order[b])
        dynamic_b += watch.binary() && watch.other() == a;
      assert(dynamic_a == dynamic_b && dynamic_a <= copies);
      const auto retire_dynamic = [](std::vector<FactorWatch> &entries,
                                     Lit other) {
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                           [other](const FactorWatch &watch) {
                             return watch.binary() && watch.other() == other;
                           }),
            entries.end());
      };
      retire_dynamic(idx.order[a], b);
      retire_dynamic(idx.order[b], a);
      const uint32_t initial = copies - dynamic_a;
      assert(idx.initial_binary_live[a] >= initial);
      assert(idx.initial_binary_live[b] >= initial);
      idx.initial_binary_live[a] -= initial;
      idx.initial_binary_live[b] -= initial;
      assert(idx.live_degree[a] >= copies);
      assert(idx.live_degree[b] >= copies);
      idx.live_degree[a] -= copies;
      idx.live_degree[b] -= copies;
      return kernel_delete_binary_all_copies_known(k, a, b, copies);
    }
  };
  for (size_t i = F.size(); i-- > 0;)
    for (size_t column = 0; column < R.size(); column++) {
      const uint32_t rid = R[column];
      if (pool.size_of(rid) == 1) {
        const bool deleted = delete_binary_watch(F[i], pool.lit0(rid));
        assert(deleted);
        (void)deleted;
        continue;
      }
      const Ref ref = grid_refs[i][column];
      if (ref == INVALID_REF)
        continue;
      Clause *c = clause_at(k.arena, ref);
      if (c->garbage())
        continue;
      if (idx.built)
        for (const Lit l : *c) {
          assert(l < idx.live_degree.size() && idx.live_degree[l]);
          idx.live_degree[l]--;
          // kissat's `delete_unfactored` removes the clause's occurrence from
          // every literal's dense list before marking it garbage (`factor.c`),
          // so no later scan reaches it. Keeping the entry and filtering it on
          // arrival costs one clause-header load per visit for the rest of the
          // application; that was 47.5% of this rule's dense visits on
          // mod4block. `live_degree` already excludes it, so the scan charge,
          // the per-visit tick and the surviving order are unchanged.
          if (l < idx.order.size()) {
            std::vector<FactorWatch> &entries = idx.order[l];
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                                         [ref](const FactorWatch w) {
                                           return !w.binary() && w.ref == ref;
                                         }),
                          entries.end());
          }
        }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      std::string transition = "factor-grid-delete(factors=";
      for (size_t fi = 0; fi < F.size(); fi++) {
        if (fi)
          transition.push_back(',');
        transition += std::to_string(kernel_externalize(k, F[fi]));
      }
      transition += ";remainder=";
      const Lit *remainder = pool.lits_of(rid);
      for (uint32_t ri = 0; ri < pool.size_of(rid); ri++) {
        if (ri)
          transition.push_back(',');
        transition += std::to_string(kernel_externalize(k, remainder[ri]));
      }
      transition.push_back(')');
      rule_journal_note_clause_transition(k, transition.c_str(), c->lits,
                                          c->size, c->id());
#endif
      kernel_delete_clause(k, c);
    }
}

// Diagnostic: dump the clause DB as DIMACS and exit, so the factored formula
// can be fed to another solver — separating grid quality from how well this
// engine leverages the aux vars. Env-gated.
void dump_factored_and_exit(const Kernel &k, const char *path) {
  std::vector<std::vector<int>> cls;
  auto dl = [](Lit l) { const int e = static_cast<int>(var_of(l)) + 1;
                        return negated(l) ? -e : e; };
  for (Var v = 0; v < k.nvars; v++) {
    const int8_t val = k.values[make_lit(v, false)];
    if (val > 0) cls.push_back({static_cast<int>(v) + 1});
    else if (val < 0) cls.push_back({-(static_cast<int>(v) + 1)});
  }
  for (Lit l = 0; l < 2 * k.nvars; l++) {
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && l < w.other())
        cls.push_back({dl(l), dl(w.other())});
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage()) { std::vector<int> cl; for (const Lit l : *c) cl.push_back(dl(l)); cls.push_back(cl); }
    ref += clause_actual_words(c);
  }
  FILE *f = std::fopen(path, "w");
  std::fprintf(f, "p cnf %u %zu\n", k.nvars, cls.size());
  for (const auto &cl : cls) { for (int x : cl) std::fprintf(f, "%d ", x); std::fprintf(f, "0\n"); }
  std::fclose(f);
  std::fprintf(stderr, "[factor] dumped %zu clauses (%u vars) to %s\n", cls.size(), k.nvars, path);
  std::exit(0);
}

} // namespace

RuleEligibility factor_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

namespace {

bool exact_grid_clause_matches(const Clause *clause, Lit factor,
                               const std::vector<Lit> &remainder) {
  if (clause->garbage() || clause->redundant() ||
      clause->size != remainder.size() + 1)
    return false;
  bool found_factor = false;
  for (const Lit literal : *clause) {
    if (literal == factor) {
      if (found_factor)
        return false;
      found_factor = true;
    } else if (!std::binary_search(remainder.begin(), remainder.end(), literal)) {
      return false;
    }
  }
  return found_factor;
}

bool prepare_exact_grid(Kernel &k, const FactorGrid &request,
                        std::vector<Lit> &factors,
                        std::vector<std::vector<Lit>> &remainders,
                        std::vector<uint32_t> &remainder_ids, RemPool &pool,
                        std::vector<std::vector<Ref>> &grid_refs) {
  factors = request.factors;
  remainders = request.remainders;
  if (factors.size() < 2 || remainders.size() < 2)
    return false;
  if (remainders.size() >
      std::numeric_limits<uint64_t>::max() / factors.size())
    return false;
  const uint64_t cells = static_cast<uint64_t>(factors.size()) *
                         static_cast<uint64_t>(remainders.size());
  if (!request.members.empty() && request.members.size() != cells)
    return false;

  std::unordered_set<Lit> distinct_factors;
  for (const Lit factor : factors)
    if (factor == INVALID_LIT || var_of(factor) >= k.nvars ||
        k.value(factor) != 0 || kernel_var_retired(k, var_of(factor)) ||
        !distinct_factors.insert(factor).second)
      return false;

  remainder_ids.clear();
  remainder_ids.reserve(remainders.size());
  for (std::vector<Lit> &remainder : remainders) {
    if (remainder.empty())
      return false;
    std::sort(remainder.begin(), remainder.end());
    if (std::adjacent_find(remainder.begin(), remainder.end()) !=
        remainder.end())
      return false;
    for (const Lit literal : remainder)
      if (literal == INVALID_LIT || var_of(literal) >= k.nvars)
        return false;
    remainder_ids.push_back(pool.intern(
        remainder.data(), static_cast<uint32_t>(remainder.size())));
  }

  grid_refs.assign(factors.size(),
                   std::vector<Ref>(remainders.size(), INVALID_REF));
  std::unordered_set<uint64_t> used_binaries;
  std::unordered_set<Ref> used_large;
  for (size_t row = 0; row < factors.size(); ++row) {
    const Lit factor = factors[row];
    for (size_t column = 0; column < remainders.size(); ++column) {
      const std::vector<Lit> &remainder = remainders[column];
      if (std::binary_search(remainder.begin(), remainder.end(), factor))
        return false;
      if (remainder.size() == 1) {
        const Lit other = remainder.front();
        if (factor == other || factor == negate(other) ||
            !kernel_binary_live(k, factor, other))
          return false;
        const uint64_t key = kernel_binary_key(factor, other);
        if (!used_binaries.insert(key).second)
          return false;
        if (!request.members.empty() &&
            request.members[row * remainders.size() + column] != INVALID_REF)
          return false;
        continue;
      }

      Ref selected = INVALID_REF;
      if (!request.members.empty()) {
        selected = request.members[row * remainders.size() + column];
        if (selected == INVALID_REF || selected >= k.arena.size())
          return false;
        const Clause *clause = clause_at(k.arena, selected);
        if (!exact_grid_clause_matches(clause, factor, remainder))
          return false;
      } else {
        for (Ref ref = 0; ref < k.arena.size();) {
          const Clause *clause = clause_at(k.arena, ref);
          const uint32_t words = clause_actual_words(clause);
          if (exact_grid_clause_matches(clause, factor, remainder) &&
              !used_large.count(ref)) {
            selected = ref;
            break;
          }
          ref += words;
        }
        if (selected == INVALID_REF)
          return false;
      }
      if (!used_large.insert(selected).second)
        return false;
      grid_refs[row][column] = selected;
    }
  }
  return true;
}

} // namespace

ApplyResult factor_apply_grid(Kernel &k, const FactorGrid &request) {
  assert(factor_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;

  std::vector<Lit> factors;
  std::vector<std::vector<Lit>> remainders;
  std::vector<uint32_t> remainder_ids;
  std::vector<std::vector<Ref>> grid_refs;
  RemPool pool;
  if (!prepare_exact_grid(k, request, factors, remainders, remainder_ids, pool,
                          grid_refs))
    return ApplyResult::NoOp;

  const Var fresh_first = k.nvars;
  FactorIndex index; // sparse exact action; no matcher-owned dense image
  apply_factoring(k, index, factors, remainder_ids, pool, grid_refs);
  ++k.stats.factorings;
  k.factor_completed = false;
  k.fresh_var_events.push_back({fresh_first, k.nvars});
  return ApplyResult::Ok;
}

// `forced_seed == INVALID_LIT` is the canonical sweep: schedule every eligible
// first factor and keep factoring until the heap or the budget runs out. A
// literal payload instead applies at most one factoring, seeded at exactly that
// literal (`N1-FACTOR-INSTANCES`; instance surface at the bottom of this file).
static ApplyResult factor_apply_seeded(Kernel &k, Lit forced_seed) {
  assert(factor_guard(k));
  k.factor_completed = false;
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  // Kissat enters dense mode immediately before Factor through the null-stack
  // `kissat_flush_large_watches` path: keep the first binary for each partner
  // in encounter order and remove every sparse large watch.  Factor reconnects
  // the irredundant large occurrences in its own dense index below, and sparse
  // resume rebuilds the search watches on exit.  Keeping mixed sparse watches
  // here made every binary quotient scan walk unrelated large entries, a
  // representation cost absent from Kissat's dense Factor mode.
  if (k.large_watches_connected)
    kernel_flush_large_watches(k);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (const char *p = std::getenv("TACTIX_DUMP_PREFACTOR"))
    dump_factored_and_exit(k, p);
#endif

  // Kissat snapshots `factoring.initial = LITS` at the start of every Factor
  // application.  `LITS` includes extension variables created by earlier
  // applications; excluding those variables loses composite grids on later
  // inprocessing rounds.  Since literal ids are zero based, `next > initial`
  // deliberately admits the positive literal of this application's first
  // fresh variable (`next == LITS`), while excluding its negation and all later
  // fresh literals.  Keep this boundary fixed while the application runs.
  const Lit initial_lits = static_cast<Lit>(2 * k.nvars);
  const Lit next_factor_limit = initial_lits + 1;

  // Fresh-variable boundary for the post-application demotion: every var this
  // application adds lands in [fresh_first, k.nvars), contiguously, so the
  // O(#queue) repair is paid once per application and threads no list.
  const Var fresh_first = k.nvars;

  // Composite grids widen the remainder universe as the pass applies. Kissat
  // keeps `factoring.initial` fixed, so only the first positive fresh literal
  // can be a next factor (`next_factor_limit`). Clause addition marks both
  // fresh polarities without scheduling them directly; a later touched update
  // can therefore schedule one as a first factor in the same pass. See
  // `docs/design/s2b-composite-grids-design.md`.
  // Termination needs no var-class exclusion — every accepted grid has
  // delta >= 1, strictly decreasing the finite live-binary count. Default off:
  // composition over-applies in a preprocessing blast, and belongs to the
  // interleaved during-search doses where DefaultPolicy sets it.
  const bool composite = k.factor_composite;
  Lit live_lits = initial_lits;

  // Remainder content is interned, while the vectors below retain one entry
  // per clause occurrence.  Equal ids are valid distinct quotient columns.
  RemPool pool;
  // One arena pass per apply builds Kissat's unified dense Factor order.  At
  // `factor_size == 2` it contains only the existing binary stream; large
  // clauses are neither indexed nor considered.
  FactorIndex fidx;
  build_factor_index(k, fidx, k.factor_size);
  std::vector<Lit> rem_scratch; // one remainder, sorted
  // Per-candidate scratch spans the FACTOR admission boundary, not the
  // remainder universe: `next_factor_limit` admits one fresh positive literal
  // (see above), so `live_lits` entries are one short of the widest index a
  // candidate can carry. Kissat sizes `factoring->count` the same way by
  // growing it for every fresh literal in resize_factoring (factor.c:645).
  std::vector<uint32_t> cofactor(next_factor_limit, 0); // co-occurrence counts
  // Binary quotient filtering asks whether the selected next factor is paired
  // with each one-literal remainder.  Mark the selected factor's adjacency
  // list once per greedy step.  The old implementation rebuilt one
  // unordered_map (and one heap node per edge) for every remainder, turning a
  // dense m-by-n grid into millions of allocator operations although the
  // already-deduplicated Factor index contains exactly the required relation.
  std::vector<uint32_t> binary_partner_epoch(next_factor_limit, 0);
  uint32_t binary_epoch = 0;
  // Kissat uses temporary clause/literal marks while intersecting dense watch
  // lists.  Compact FactorWatch ids provide the same behavior without rebuilding
  // unordered sets for every quotient column.  Epoch rollover is vanishingly
  // rare but handled so the representation remains exact.
  // Kissat marks the clause header (`clause::quotient`) so a dense scan reads
  // the mark from the line it already loads for `size`.  These hold the
  // clauses marked since the last reset, so the marks clear without a sweep.
  std::vector<Ref> reserved_marked, used_marked;
  // kissat keeps all three per-literal Factor marks in ONE byte
  // (`factor.c`: FACTOR, QUOTIENT, NOUNTED) and reads it once per literal of
  // every candidate clause.  Separate arrays cost one bounds test and one
  // dependent load each on that path.
  std::vector<uint8_t> marks(next_factor_limit, 0);
  std::vector<Lit> quotient_marked, nounted;
  // Kissat tests a byte-sized `marks[lit] & FACTOR` bit while growing a
  // quotient.  Keep the same temporary bit and clear only the literals in F
  // after each seed.  A 32-bit epoch fixed the old quadratic search too, but
  // made the 733M-visit binary loop load four times the state and reload the
  // current epoch on every candidate.

  // Kissat's hot next-factor loop reads one cached `flags[idx].active` bit.
  // Tactix otherwise reconstructs that same predicate from the literal value
  // and variable-retirement arrays on every dense-watch visit.  Snapshot the
  // application-stable predicate per literal; Factor does not assign or retire
  // existing variables, and the one admitted fresh positive literal is marked
  // when its extension variable is created below.
  std::vector<uint8_t> factor_active(next_factor_limit, 0);
  for (Lit lit = 0; lit < initial_lits; lit++)
    factor_active[lit] =
        k.value(lit) == 0 && !kernel_var_retired(k, var_of(lit));
  const auto clear_marks = [&](std::vector<Ref> &marked, uint32_t bit) {
    for (const Ref ref : marked)
      clause_at(k.arena, ref)->flags &= ~bit;
    marked.clear();
  };
  // The marks mean nothing outside this application and every exit must leave
  // the arena clean, including the early returns below.
  struct MarkGuard {
    Kernel &k;
    std::vector<Ref> &reserved, &used;
    ~MarkGuard() {
      for (const Ref ref : reserved)
        clause_at(k.arena, ref)->flags &= ~Clause::FACTOR_RESERVED;
      for (const Ref ref : used)
        clause_at(k.arena, ref)->flags &= ~Clause::FACTOR_USED;
#ifndef NDEBUG
      for (Ref ref = 0; ref < k.arena.size();) {
        const Clause *c = clause_at(k.arena, ref);
        assert(!(c->flags & (Clause::FACTOR_RESERVED | Clause::FACTOR_USED)) &&
               "a Factor mark outlived its application");
        ref += clause_actual_words(c);
      }
#endif
    }
  } mark_guard{k, reserved_marked, used_marked};
  // Stamp one column's interned remainder QUOTIENT and clear the previous
  // column's, exactly as kissat sets the bit while scanning a quotient clause
  // and unmarks it afterwards.  Every remainder literal is below the interning
  // limit, so the array covers them and a literal outside it is certainly not
  // a member.
  const auto stamp_remainder = [&](const Lit *lits, uint32_t n, Lit limit) {
    for (const Lit l : quotient_marked)
      marks[l] &= static_cast<uint8_t>(~MARK_QUOTIENT);
    quotient_marked.clear();
    if (marks.size() < static_cast<size_t>(limit))
      marks.resize(limit, 0);
    for (uint32_t i = 0; i < n; i++) {
      marks[lits[i]] |= MARK_QUOTIENT;
      quotient_marked.push_back(lits[i]);
    }
  };
  const auto clear_nounted = [&]() {
    for (const Lit l : nounted)
      marks[l] &= static_cast<uint8_t>(~MARK_NOUNTED);
    nounted.clear();
  };
  std::vector<Lit> touched, F, best_F;
  std::vector<uint32_t> R, newR, best_R;
  std::vector<RemOccurrence> occurrences;
  std::vector<std::vector<Ref>> grid_refs, new_grid_refs, best_grid_refs;
  uint64_t applied = 0;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  uint64_t diagnostic_pops = 0;
  uint64_t diagnostic_first_ticks = 0;
  uint64_t diagnostic_next_ticks = 0;
  uint64_t diagnostic_match_ticks = 0;
#endif

  // Seed schedule: a max-heap keyed by occurrence count, re-scored on every
  // literal an applied factoring touches (kissat `update_factored`,
  // factor.c:800). A popped literal is eligible again only after a newly added
  // divider or quotient marks that same polarity, matching Kissat's per-literal
  // `flags->factor` bit. A static literal-index scan instead lets an early
  // low-degree seed spend binaries a high-degree seed needed. The
  // scoring/accept scalars are the policy's when a `Kernel::factor_selection`
  // is attached; the mechanism stays here. Watch sizes ≤ 2^24 keep the
  // double→uint32_t round-trip order-exact
  // (`docs/design/s2a-factor-matcher-design.md`).
  FactorSelection *const fm = k.factor_selection;
  if (fm)
    fm->begin_sweep(k);
  // Kissat scores the dense Factor watch lists: one entry per binary partner
  // plus one per surviving candidate large clause. Sparse search
  // watch size is not an approximation of this degree (a literal can watch
  // thousands of clauses which do not contain it as Factor material).
  const auto reference_score_of = [&](Lit l) -> uint32_t {
    if (!fidx.built || l >= fidx.live_degree.size())
      return 0;
    // Kissat's update_candidate/tied_next_factor_score use the complete live
    // dense-watch size.  A quotient containing this application's fresh
    // extension literal is not eligible as a grid row, but still contributes
    // to the heap and tie score.
    return fidx.live_degree[l];
  };
  const auto score_of = [&](Lit l) -> uint32_t {
    const uint32_t reference_score = reference_score_of(l);
    return fm ? static_cast<uint32_t>(fm->seed_score(
                    k, l, static_cast<double>(reference_score)))
              : reference_score;
  };
  // `k.work_budget`: stop the round after this many factor_ticks. A legacy
  // zero is unbounded; `work_budget_bounded` represents explicit zero. Two
  // quit points mirror Kissat's (factor.c:442, :914).
  const uint64_t tick_start = k.stats.factor_ticks;
  const uint64_t budget = k.work_budget;
  const auto over_budget = [&]() {
    return kernel_work_budget_exceeded(k,
                                       k.stats.factor_ticks - tick_start);
  };
  FactorHeap schedule;
  // Per-polarity analogue of Kissat's `flags->factor`: clear on pop, set again
  // only when a newly added divider or quotient contains the literal.
  std::vector<uint8_t> armed(next_factor_limit, 0);
  // Exact persistent per-literal seed marks (`flags->factor`, inline.h:60-66,
  // read at factor.c:922). Clause addition sets the actual polarity and a pop
  // clears it. The shared per-variable touched epoch is intentionally not used:
  // arming the opposite polarity changed mod4block's 13th Factor grid. N1: this
  // narrows only the canonical schedule; the rule's instance surface remains
  // complete. `TACTIX_TOUCH_MARKS=0` retains the all-literals diagnostic arm.
  // Pinned by tests/test_factor.cpp::test_touch_filter_is_policy_only.
  const bool touch_filter = k.touch_filter;
  const auto seed_is_new = [&](Lit l) {
    return !touch_filter || k.factor_marks[l];
  };
  // Kissat's `tumble=1` assigns internal variables in first-import order, and
  // Factor builds this heap by ascending internal index.  Tactix deliberately
  // keeps permanent external identities and therefore has a different dense
  // index permutation.  Iterate through the equivalent reference order so
  // equal-score heap topology has the same meaning without coupling a proof
  // rule's public literal identities to parse-time renumbering.
  std::vector<Lit> schedule_order(live_lits);
  std::iota(schedule_order.begin(), schedule_order.end(), Lit{0});
  std::stable_sort(schedule_order.begin(), schedule_order.end(),
                   [&](Lit a, Lit b) {
                     return kernel_import_lit_order(k, a) <
                            kernel_import_lit_order(k, b);
                   });
  uint32_t schedule_ordinal = 0;
  // Single-instance mode arms exactly the requested literal. The touch filter
  // (`seed_is_new`) is deliberately NOT applied: it is DefaultPolicy's
  // per-literal narrowing of the canonical schedule, and N1 forbids a policy
  // narrowing from reaching the public instance surface.
  if (forced_seed != INVALID_LIT) {
    if (forced_seed < live_lits && k.value(forced_seed) == 0 &&
        !kernel_var_retired(k, var_of(forced_seed))) {
      armed[forced_seed] = 1;
      schedule.update(forced_seed, score_of(forced_seed));
    }
  } else
  for (const Lit l : schedule_order) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_factor_literal_state(
        k, l, k.factor_marks[l], kernel_var_retired(k, var_of(l)), k.value(l),
        reference_score_of(l));
    const char *selected_literal =
        std::getenv("TACTIX_EVENT_TRACE_FACTOR_LITERAL");
    if (selected_literal &&
        std::atoi(selected_literal) == kernel_externalize(k, l)) {
      uint32_t ordinal = 0;
      for_each_initial_factor_binary(k, fidx, l, [&](Lit other) {
        event_trace_factor_literal_entry(
            k, l, ordinal++, other < live_lits, other,
            kernel_binary_id(k, l, other), nullptr);
      });
      for (const FactorWatch watch : fidx.order[l]) {
        if (watch.binary()) {
          const Lit other = watch.other();
          event_trace_factor_literal_entry(
              k, l, ordinal++, other < live_lits, other,
              kernel_binary_id(k, l, other), nullptr);
        } else {
          const Clause *clause = clause_at(k.arena, watch.ref);
          const bool live = grid_clause_live(k, clause);
          event_trace_factor_literal_entry(
              k, l, ordinal++, live, INVALID_LIT, clause->id(), clause);
        }
      }
    }
#endif
    if (k.value(l) != 0 || kernel_var_retired(k, var_of(l)))
      continue;
    if (!seed_is_new(l)) {
      k.stats.factor_mark_skipped++;
      continue;
    }
    armed[l] = 1;
    const uint32_t score = score_of(l);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_factor_schedule(k, schedule_ordinal, l, score);
#endif
    schedule_ordinal++;
    schedule.update(l, score);
  }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  trace_factor_heap(k, schedule, INVALID_LIT, "initial");
#endif

  while (!schedule.empty()) {
    if (over_budget()) // quit point 1: before each popped seed
      break;
    // One factoring per literal instance. An applied factoring re-scores its
    // factors back into the heap, which the canonical sweep is meant to
    // continue from and a single instance is not.
    if (forced_seed != INVALID_LIT && applied)
      break;
    const Lit seed = schedule.pop_max();
#ifdef TACTIX_ENABLE_EVENT_TRACE
    diagnostic_pops++;
    trace_factor_heap(k, schedule, seed, "popped");
    if (const char *selected =
            std::getenv("TACTIX_EVENT_TRACE_FACTOR_ADMISSION")) {
      if (std::atoi(selected) == kernel_externalize(k, seed))
        std::fprintf(
            stderr,
            "[factor-admission] external=%d internal=%u live_lits=%u "
            "armed=%u value=%d retired=%u heap_score=%u score=%u\n",
            kernel_externalize(k, seed), seed, live_lits,
            seed < armed.size() ? armed[seed] : 0, k.value(seed),
            kernel_var_retired(k, var_of(seed)), schedule.score(seed),
            score_of(seed));
    }
#endif
    if (forced_seed != INVALID_LIT && seed != forced_seed)
      break;
    // `live_lits`, not `armed.size()`: the first-factor boundary is the
    // dynamically grown pass universe. An update_candidate on a
    // divider/quotient literal can push a fresh literal into the heap after
    // some later factoring touches it.
    if (seed >= live_lits || !armed[seed] || k.value(seed) != 0 ||
        kernel_var_retired(k, var_of(seed)))
      continue;
    armed[seed] = 0;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_factor_mark_transition(k, seed, false, "factor_pop");
#endif
    k.factor_marks[seed] = 0;
    const uint32_t now = score_of(seed);
    if (now <= 1)
      continue;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const uint64_t diagnostic_first_before = k.stats.factor_ticks;
#endif
    remainder_occurrences_of(k, fidx, pool, seed, live_lits, rem_scratch,
                             occurrences);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    diagnostic_first_ticks += k.stats.factor_ticks - diagnostic_first_before;
#endif
    R.clear();
    grid_refs.assign(1, {});
    R.reserve(occurrences.size());
    grid_refs[0].reserve(occurrences.size());
    for (const RemOccurrence occurrence : occurrences) {
      R.push_back(occurrence.id);
      grid_refs[0].push_back(occurrence.ref);
    }
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_factor_seed(k, seed, now, static_cast<uint32_t>(R.size()));
    for (uint32_t ordinal = 0; ordinal < R.size(); ordinal++)
      event_trace_factor_remainder(
          k, seed, ordinal, grid_refs[0][ordinal] != INVALID_REF,
          pool.lits_of(R[ordinal]), pool.size_of(R[ordinal]));
#endif
    if (R.size() < 2)
      continue;
    F.assign(1, seed);
    marks[seed] |= MARK_FACTOR;
    // m,n >= 2 makes delta nonnegative. Starting below that legal floor lets
    // an external selection policy admit a zero-gain 2x2 grid, while the
    // shipped `delta > bound` policy (bound 0) retains its exact behavior.
    int best_delta = -1;
    best_F.clear();
    best_R.clear();
    best_grid_refs.clear();

    // greedily grow the factor set, tracking the max-delta prefix.
    while (R.size() >= 2) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
      const uint64_t diagnostic_next_before = k.stats.factor_ticks;
#endif
      // Count next factors in Kissat's actual clause-occurrence order.  A
      // content bucket is insufficient here: for each current large clause
      // Kissat chooses that clause's first least-watched remainder literal,
      // scans its dense watch list, and reserves each exact matching clause at
      // most once across the whole quotient.  That first-discovery order is
      // also the tie-break between equal count/score candidates.
      touched.clear();
      const auto count_candidate = [&](Lit candidate) -> bool {
        // Predicate order follows Kissat factor.c:350-357: boundary, FACTOR
        // mark, then ACTIVE.  `factor_active` is exactly Tactix's former
        // value-zero plus not-retired conjunction, cached above.
        if (candidate >= next_factor_limit ||
            (marks[candidate] & MARK_FACTOR) || !factor_active[candidate])
          return false;
        if (!cofactor[candidate])
          touched.push_back(candidate);
        cofactor[candidate]++;
        return true;
      };
      charge_factor_watch_scan(k, R.size());
      bool next_factor_budget_hit = false;
      clear_marks(reserved_marked, Clause::FACTOR_RESERVED);
      for (size_t column = 0; column < R.size(); column++) {
        const uint32_t rid = R[column];
        const uint32_t rn = pool.size_of(rid);
        const Lit *remainder = pool.lits_of(rid);
        stamp_remainder(remainder, rn, live_lits);
        if (rn == 1) {
          const Lit q = remainder[0];
          if (q >= static_cast<Lit>(2 * k.nvars))
            continue;
          charge_factor_watch_scan(k, reference_score_of(q));
          // Only the binary subsequence affects this count. The kernel slice
          // has that same initial-then-appended order without walking and
          // decoding unrelated large entries from the mixed ledger.
          for_each_factor_binary(k, q, [&](Lit other) {
            if (count_candidate(other)) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
              event_trace_factor_candidate_binary_source(
                  k, seed, static_cast<uint32_t>(F.size()),
                  static_cast<uint32_t>(column), q, other);
#endif
            }
          });
          if (over_budget()) {
            next_factor_budget_hit = true;
            break;
          }
          continue;
        }

        const Ref current_ref = grid_refs.back()[column];
        if (current_ref == INVALID_REF)
          continue;
        const Clause *current = clause_at(k.arena, current_ref);
        k.stats.factor_ticks++;
        Lit scan = INVALID_LIT;
        uint32_t scan_size = 0;
        uint32_t factors_in_clause = 0;
        for (const Lit lit : *current) {
          if (lit < marks.size() && (marks[lit] & MARK_FACTOR)) {
            factors_in_clause++;
            continue;
          }
          const uint32_t dense_size = reference_score_of(lit);
          if (scan == INVALID_LIT || dense_size < scan_size) {
            scan = lit;
            scan_size = dense_size;
          }
        }
        if (factors_in_clause != 1 || scan == INVALID_LIT ||
            scan >= fidx.order.size())
          continue;

        charge_factor_watch_scan(k, reference_score_of(scan));
        clear_nounted();
        for (const FactorWatch watch : fidx.order[scan]) {
          if (watch.binary())
            continue;
          const Ref ref = watch.ref;
          if (ref == current_ref)
            continue;
          // Kissat charges every live large entry reached by the dense scan,
          // including occurrences already reserved by an earlier column.
          k.stats.factor_ticks++;
          Clause *const candidate_clause = clause_at(k.arena, ref);
          assert(grid_clause_live(k, candidate_clause));
          if (candidate_clause->factor_reserved())
            continue;
          if (candidate_clause->size != current->size)
            continue;
          Lit candidate = INVALID_LIT;
          bool matches = true;
          for (const Lit lit : *candidate_clause) {
            const uint8_t mark = lit < marks.size() ? marks[lit] : uint8_t{0};
            if (mark & MARK_QUOTIENT)
              continue;
            if ((mark & (MARK_FACTOR | MARK_NOUNTED)) ||
                candidate != INVALID_LIT) {
              matches = false;
              break;
            }
            candidate = lit;
          }
          if (!matches || candidate == INVALID_LIT ||
              candidate >= next_factor_limit || !factor_active[candidate])
            continue;
          marks[candidate] |= MARK_NOUNTED;
          nounted.push_back(candidate);
          candidate_clause->flags |= Clause::FACTOR_RESERVED;
          reserved_marked.push_back(ref);
          if (count_candidate(candidate)) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
            event_trace_factor_candidate_source(
                k, seed, static_cast<uint32_t>(F.size()),
                static_cast<uint32_t>(column), scan, candidate, current,
                candidate_clause);
#endif
          }
        }
        if (over_budget()) {
          next_factor_budget_hit = true;
          break;
        }
      }

      if (next_factor_budget_hit) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
        diagnostic_next_ticks += k.stats.factor_ticks - diagnostic_next_before;
#endif
        for (const Lit h : touched)
          cofactor[h] = 0;
        break;
      }

      // Highest co-occurrence count, ties broken by watch-list size (kissat's
      // `tied_next_factor_score`, factor.c:318). On a dense binary graph the max
      // count is tied among many candidates, so the tie-break selects the grid.
      Lit g = INVALID_LIT;
      uint32_t bestc = 1, best_score = 0;
#ifdef TACTIX_ENABLE_EVENT_TRACE
      uint32_t candidate_ordinal = 0;
#endif
      for (const Lit h : touched) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
        const uint32_t ordinal = candidate_ordinal++;
#endif
        const uint32_t c = cofactor[h];
        if (c <= 1 || c < bestc)
          continue;
        const uint32_t reference_score = reference_score_of(h);
        const uint32_t score = fm ? static_cast<uint32_t>(
                                        fm->next_factor_score(
                                            k, h, c, seed,
                                            static_cast<double>(reference_score)))
                                  : reference_score;
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_factor_candidate(k, seed, static_cast<uint32_t>(F.size()),
                                     ordinal, h, c, score);
        uint32_t score_entry_ordinal = 0;
        for_each_initial_factor_binary(k, fidx, h, [&](Lit other) {
          event_trace_factor_score_entry(
              k, seed, static_cast<uint32_t>(F.size()), h,
              score_entry_ordinal++, other < live_lits, other,
              kernel_binary_id(k, h, other), nullptr);
        });
        for (const FactorWatch watch : fidx.order[h]) {
          if (watch.binary()) {
            const Lit other = watch.other();
            event_trace_factor_score_entry(
                k, seed, static_cast<uint32_t>(F.size()), h,
                score_entry_ordinal++, other < live_lits, other,
                kernel_binary_id(k, h, other), nullptr);
          } else {
            const Clause *clause = clause_at(k.arena, watch.ref);
            const bool live = grid_clause_live(k, clause);
            event_trace_factor_score_entry(
                k, seed, static_cast<uint32_t>(F.size()), h,
                score_entry_ordinal++, live, INVALID_LIT, clause->id(), clause);
          }
        }
#endif
        if (c > bestc) {
          bestc = c;
          best_score = score;
          g = h;
        } else if (score > best_score) { // c == bestc
          best_score = score;
          g = h;
        }
      }
      for (const Lit h : touched)
        cofactor[h] = 0;
#ifdef TACTIX_ENABLE_EVENT_TRACE
      diagnostic_next_ticks += k.stats.factor_ticks - diagnostic_next_before;
#endif
      if (g == INVALID_LIT) // no factor shares ≥ 2 remainders
        break;

      // Kissat's `new_quotient(g)` installs g in the dense FACTOR mark before
      // `factorize_next` scans the current quotient.  A current clause that
      // already contains g therefore has two factor literals and cannot be a
      // match row.  Marking g only after this scan preserves the chosen grid
      // but wastes work on such impossible rows (an exact match would require
      // a duplicate g literal), consuming the shared Factor effort early.
      assert(g < marks.size() && !(marks[g] & MARK_FACTOR));
      marks[g] |= MARK_FACTOR;
#ifdef TACTIX_ENABLE_EVENT_TRACE
      const uint64_t diagnostic_match_before = k.stats.factor_ticks;
      const bool diagnostic_match_selected =
          factor_match_trace_selected(k, seed, g);
#endif
      if (++binary_epoch == 0) {
        std::fill(binary_partner_epoch.begin(), binary_partner_epoch.end(), 0);
        binary_epoch = 1;
      }
      if (g < static_cast<Lit>(2 * k.nvars))
        for_each_factor_binary(k, g, [&](Lit other) {
          if (other < binary_partner_epoch.size())
            binary_partner_epoch[other] = binary_epoch;
        });

      // Filter every existing row through the same match indices, then append
      // g's exact clause Ref as the new row.  Iterating R in its current order
      // is Kissat's factorize_next + flush_unmatched_clauses lifecycle.
      newR.clear();
      std::vector<size_t> matched_columns;
      std::vector<Ref> next_refs;
      clear_marks(used_marked, Clause::FACTOR_USED);
      charge_factor_watch_scan(k, R.size());
      for (size_t column = 0; column < R.size(); column++) {
        const uint32_t rid = R[column];
        const uint32_t rn = pool.size_of(rid);
        Ref matched_ref = INVALID_REF;
        if (rn == 1) {
          const Lit remainder = pool.lit0(rid);
          charge_factor_watch_scan(k, reference_score_of(remainder));
          if (remainder >= binary_partner_epoch.size() ||
              binary_partner_epoch[remainder] != binary_epoch)
            continue;
        } else {
          // Kissat does not consume an arbitrary clause from a content bucket.
          // For each surviving clause occurrence it chooses that clause's
          // first least-watched remainder literal, scans that dense watch list
          // in arena order, and takes the first unused exact match.  Equal
          // clauses therefore have occurrence identity and order; preserving
          // only their multiplicity changes the later mutable-heap topology.
          const Ref current_ref = grid_refs.back()[column];
          if (current_ref == INVALID_REF)
            continue;
          const Clause *current = clause_at(k.arena, current_ref);
          k.stats.factor_ticks++;
          Lit scan = INVALID_LIT;
          uint32_t scan_size = 0;
          uint32_t factors_in_clause = 0;
          for (const Lit lit : *current) {
            if (lit < marks.size() && (marks[lit] & MARK_FACTOR)) {
              factors_in_clause++;
              continue;
            }
            const uint32_t dense_size = reference_score_of(lit);
            if (scan == INVALID_LIT || dense_size < scan_size) {
              scan = lit;
              scan_size = dense_size;
            }
          }
          if (factors_in_clause != 1 || scan == INVALID_LIT ||
              scan >= fidx.order.size())
            continue;
          charge_factor_watch_scan(k, reference_score_of(scan));
          const Lit *expected = pool.lits_of(rid);
          stamp_remainder(expected, rn, live_lits);
          uint32_t visited_large = 0;
          for (const FactorWatch watch : fidx.order[scan]) {
            if (watch.binary())
              continue;
            const Ref ref = watch.ref;
            if (ref == current_ref)
              continue;
            k.stats.factor_ticks++;
            visited_large++;
            Clause *const candidate = clause_at(k.arena, ref);
            assert(grid_clause_live(k, candidate));
            if (candidate->factor_used())
              continue;
            if (candidate->size != rn + 1)
              continue;
            if (!matches_interned_remainder(candidate, g, live_lits,
                                            marks, MARK_QUOTIENT))
              continue;
            matched_ref = ref;
            candidate->flags |= Clause::FACTOR_USED;
            used_marked.push_back(ref);
            break;
          }
#ifdef TACTIX_ENABLE_EVENT_TRACE
          if (diagnostic_match_selected)
            trace_factor_match_large(
                k, seed, g, static_cast<uint32_t>(F.size()), column, expected,
                rn, scan, reference_score_of(scan), visited_large,
                matched_ref != INVALID_REF);
#endif
          if (matched_ref == INVALID_REF)
            continue;
        }
        matched_columns.push_back(column);
        newR.push_back(rid);
        next_refs.push_back(matched_ref);
      }
      // `newR.size()` need not equal `bestc`: the counting pass reserves each
      // live large occurrence once across the whole quotient, so a clause an
      // earlier column spent on another candidate is invisible to g's count,
      // while this pass re-scans and reserves only against g. Grid-choice
      // fidelity only — the grid and its delta are built from the matched
      // columns, and only g's selection reads `bestc`. Deviation and repair:
      // docs/implementation-notes.md §5, Factor next-factor matching.
      new_grid_refs.assign(grid_refs.size() + 1, {});
      for (size_t row = 0; row < grid_refs.size(); row++) {
        new_grid_refs[row].reserve(matched_columns.size());
        for (const size_t column : matched_columns)
          new_grid_refs[row].push_back(grid_refs[row][column]);
      }
      new_grid_refs.back() = std::move(next_refs);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      diagnostic_match_ticks +=
          k.stats.factor_ticks - diagnostic_match_before;
#endif
      const uint32_t m = static_cast<uint32_t>(F.size()) + 1;
      const uint32_t n = static_cast<uint32_t>(newR.size());
      const int delta = static_cast<int>(m * n) - static_cast<int>(m + n);
      if (delta > best_delta) {
        best_delta = delta;
        best_F = F;
        best_F.push_back(g);
        best_R = newR;
        best_grid_refs = new_grid_refs;
      }
      // commit g and shrink R to newR to keep growing.
      F.push_back(g);
      assert(marks[g] & MARK_FACTOR);
      R.swap(newR);
      grid_refs.swap(new_grid_refs);
    }

    // Acceptance: an attached FactorSelection takes precedence, otherwise
    // `k.factor_bound` (kissat factor.c:939). Grid existence supplies the only
    // rule-side floor (m,n >= 2, hence delta >= 0); profitability is policy.
    const bool bound_ok =
        best_delta >= 0 &&
        (fm ? fm->accept(static_cast<uint32_t>(best_F.size()),
                         static_cast<uint32_t>(best_R.size()), best_delta)
            : best_delta > static_cast<int>(k.factor_bound));
    if (!bound_ok && best_delta >= 0)
      k.stats.factor_rejected_by_bound++; // the grid existed; the bound said no
    if (bound_ok) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_factor_grid(k, seed, static_cast<uint32_t>(best_F.size()),
                              static_cast<uint32_t>(best_R.size()), best_delta);
      for (uint32_t i = 0; i < best_F.size(); i++)
        event_trace_factor_grid_member(k, i, best_F[i]);
#endif
      if (std::getenv("TACTIX_GRID_TRACE")) // diagnostic: grid dimensions
        std::fprintf(stderr, "[grid] seed=%d %zux%zu delta=%d\n",
                     kernel_externalize(k, seed), best_F.size(),
                     best_R.size(), best_delta);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_factor_pair(k, seed, "before");
#endif
      const Var added_fresh = k.nvars;
      apply_factoring(k, fidx, best_F, best_R, pool, best_grid_refs);
      assert(k.nvars == added_fresh + 1);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_factor_pair(k, seed, "after");
#endif
      applied++;
      k.stats.factorings++;
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_factor_fresh(k, seed, added_fresh, best_F.data(),
                               static_cast<uint32_t>(best_F.size()),
                               static_cast<uint32_t>(best_R.size()),
                               best_delta, k.stats.factor_ticks - tick_start,
                               diagnostic_pops, diagnostic_first_ticks,
                               diagnostic_next_ticks, diagnostic_match_ticks);
#endif
      // Grow the pass universe over the fresh var (kissat resize_factoring).
      // Clause insertion has marked both its divider and quotient polarities,
      // but, like Kissat, do not schedule either one merely because it was
      // created. A later update_candidate can push an armed fresh polarity.
      if (composite && static_cast<Lit>(2 * k.nvars) > live_lits) {
        live_lits = static_cast<Lit>(2 * k.nvars);
        cofactor.resize(std::max(live_lits, next_factor_limit), 0);
        armed.resize(std::max(live_lits, next_factor_limit), 0);
        binary_partner_epoch.resize(std::max(live_lits, next_factor_limit), 0);
        marks.resize(std::max(live_lits, next_factor_limit), 0);
        const Lit fresh = make_lit(added_fresh, false);
        const Lit not_fresh = negate(fresh);
        assert(k.factor_marks[fresh] && k.factor_marks[not_fresh]);
        // The fixed `next_factor_limit` admits only the first fresh positive
        // literal of this application.  Later composite extension variables
        // can be first-factor seeds, but can never pass next_factor's boundary
        // and therefore have no slot in this fixed ACTIVE image.
        if (fresh < factor_active.size())
          factor_active[fresh] =
              k.value(fresh) == 0 && !kernel_var_retired(k, added_fresh);
        armed[fresh] = armed[not_fresh] = 1;
      }
      // Clause addition re-arms exactly the polarities present in the new
      // definition: each factor occurs in a divider, and every remainder
      // literal in a quotient.  Kissat clears a candidate's factor bit when it
      // is popped and kissat_mark_added_literal sets it again here; retaining a
      // once-per-pass `tried` bit loses later profitable grids (mod4block's
      // second -145 factoring is the first concrete witness).
      for (const Lit f : best_F)
        armed[f] = 1;
      for (const uint32_t rid : best_R) {
        const Lit *rl = pool.lits_of(rid);
        for (uint32_t i = 0; i < pool.size_of(rid); i++)
          if (rl[i] < live_lits)
            armed[rl[i]] = 1;
      }
      // Mutable decrease/increase-key updates in Kissat's exact order.  The
      // selected factors are visited from the final quotient backwards.  Each
      // factor and its opposite are updated, then every other literal of each
      // removed grid clause in clause order.  Updating only this set (rather
      // than lazily refreshing every eventual heap pop) preserves the reference
      // precedence among grids that compete for clauses.
      uint32_t update_ordinal = 0;
      const auto update_candidate = [&](Lit lit) {
        const uint32_t score = score_of(lit);
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_factor_update(k, seed, update_ordinal, lit, score);
#endif
        update_ordinal++;
        schedule.update(lit, score);
      };
      for (size_t rev = best_F.size(); rev-- > 0;) {
        const Lit f = best_F[rev];
        update_candidate(f);
        update_candidate(negate(f));
        for (size_t column = 0; column < best_R.size(); column++) {
          const uint32_t rid = best_R[column];
          const uint32_t rn = pool.size_of(rid);
          const Lit *rl = pool.lits_of(rid);
          if (rn == 1) {
            update_candidate(rl[0]);
            continue;
          }
          const Ref ref = best_grid_refs[rev][column];
          if (ref == INVALID_REF)
            continue;
          const Clause *c = clause_at(k.arena, ref);
          for (const Lit other : *c)
            if (other != f)
              update_candidate(other);
        }
      }
#ifdef TACTIX_ENABLE_EVENT_TRACE
      trace_factor_heap(k, schedule, seed, "updated");
#endif
    }
    for (const Lit factor : F) {
      assert(factor < marks.size() && (marks[factor] & MARK_FACTOR));
      marks[factor] &= static_cast<uint8_t>(~MARK_FACTOR);
    }
  }

  // Kissat advances its call-level material watermark only when the mutable
  // schedule is empty.  Keep this result distinct from the per-literal bits:
  // Factor's own divider/quotient additions can leave bits armed even though
  // their cumulative events belong to the just-completed call.
  k.factor_completed = schedule.empty();

  // A budget cut needs no tail repair: unpopped literal bits remain set in
  // `factor_marks`, exactly as Kissat leaves them for the next occasion.

  // The executed pass owns an advice boundary even if its fresh interval is
  // empty: Kissat restamps the queue after zero-yield and budget-cut passes.
  // Only the policy consumer mutates H; NoOp still describes formula yield.
  k.fresh_var_events.push_back({fresh_first, k.nvars});

  if (fm)
    fm->end_sweep();

  // Kissat leaves Factor's dense occurrence mode through
  // `kissat_resume_sparse_mode`: it drops every connected large occurrence,
  // sorts each live clause's watched pair for the current root assignment,
  // and reconnects the clauses in arena order.  Keeping the pre-Factor sparse
  // watches instead is observably different even when every accepted grid is
  // identical: quotient implications can enter the trail in a different
  // order and select a different learned reason.  Binary watches deliberately
  // stay in their first-encounter order across this boundary.
  kernel_rebuild_large_watches(k);

  if (std::getenv("TACTIX_FACTOR_TRACE"))
    std::fprintf(stderr,
                 "[factor] entered (%u initial lits), %llu factorings, "
                 "%llu ticks (budget %llu%s)\n",
                 initial_lits, (unsigned long long)applied,
                 (unsigned long long)(k.stats.factor_ticks - tick_start),
                 (unsigned long long)budget, over_budget() ? ", CUT" : "");
  if (const char *p = std::getenv("TACTIX_DUMP_FACTORED"))
    if (applied)
      dump_factored_and_exit(k, p);
  return applied ? ApplyResult::Ok : ApplyResult::NoOp;
}

ApplyResult factor_apply(Kernel &k) {
  return factor_apply_seeded(k, INVALID_LIT);
}

// `N1-FACTOR-INSTANCES` (disposition: docs/implementation-notes.md §5 Factor).
// E2's atom is one factoring, and the choice a policy owns is the seed literal,
// so that is the payload: every unassigned, unretired literal is offered,
// whatever DefaultPolicy's touch filter or heap score would schedule. The grid
// a seed grows into is not separately addressable — that space is exponential
// and is the rule's construction, swappable through `factor_selection`.
uint32_t factor_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  if (cap == 0)
    return 0;
  uint32_t n = 0;
  out[n++] = INSTANCE_CANONICAL; // ranked canonical-first, as everywhere
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars) && n < cap; l++)
    if (k.value(l) == 0 && !kernel_var_retired(k, var_of(l)))
      out[n++] = lit_instance(k, l);
  return n;
}

ApplyResult factor_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return factor_apply(k);
  Lit l;
  if (!lit_of_instance(k, inst, &l))
    return ApplyResult::NoOp;
  return factor_apply_seeded(k, l);
}

TACTIX_REGISTER_RULE(Factor, RuleTier::Inprocess, factor_guard, factor_apply,
                     factor_instances, factor_apply_instance);

} // namespace tactix

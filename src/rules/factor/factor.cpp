#include "rules/factor/factor.hpp"

#include "diagnostics/event_trace.hpp"
#include "kernel/indexed_max_heap.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/instance_helpers.hpp"
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

// Factor work currency (kissat factor.c:274, kissat_cache_lines): 1 + one tick
// per 128 bytes, i.e. 1 + (4·words)/128 (parity-ledger §13).
inline uint64_t factor_cache_lines(uint64_t watches) {
  return watches ? (watches + 31) >> 5 : 0;
}

inline void charge_factor_watch_scan(Kernel &k, uint64_t watches) {
  k.stats.factor_ticks += 1 + factor_cache_lines(watches);
}

// After the null-stack dense entry, pre-existing slices are binary-only: walk
// the packed image like Kissat's `all_binary_large_watches`. Lists with an
// appended large quotient use the entry-aware iterator.
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

// --- remainder interning (`docs/design/e2-factorsize-design.md`) ---
// A remainder is `C \ {f}`, interned by content; quotient columns are clause
// occurrences and may repeat an id, matching Kissat's one watch per clause.
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
  // Multi-literal intern; `b[0..n)` sorted ascending. Identity is by content
  // and never deduplicates occurrences. Open-addressed id table; ids are
  // assigned in first-intern order.
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

// --- the clause index for factor_size >= 3 ---
// Occurrence lists over irredundant clauses of size 3..factor_size, one arena
// pass per apply. Binaries are not indexed (`factor_size == 2` order) and
// redundant clauses are excluded (Reduce could delete a grid member).
struct FactorWatch {
  Ref ref = INVALID_REF;
  // Apply-local key: for a large entry the lazily interned remainder id (the
  // clause and factor are immutable during one application; UINT32_MAX also
  // means not yet materialized, and boundary failures are not cached); for a
  // binary entry the partner literal.
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
  // Initial large suffix plus every entry appended during this application;
  // initial binary partners stay in the kernel slices (`initial_binary_live`
  // bounds the borrowed prefix). Together they are Kissat's mixed dense stream.
  std::vector<std::vector<FactorWatch>> order;
  std::vector<uint32_t> initial_binary_live;
  // Exact live size of each dense list (Kissat's O(1) stack size).
  std::vector<uint32_t> live_degree;
  uint32_t large_count = 0;
  bool built = false;
};

// Kissat's mutable max-heap: bubble-up stops at equality and bubble-down
// prefers the left child, so equal scores keep heap topology.
class FactorHeap {
  IndexedMaxHeap<Lit, uint32_t> heap_;

public:
  bool empty() const { return heap_.empty(); }
  uint32_t size() const { return heap_.size(); }
  const std::vector<Lit> &members() const { return heap_.members(); }
  uint32_t position(Lit lit) const { return heap_.position(lit); }
  uint32_t score(Lit lit) const { return heap_.score(lit); }

  void update(Lit lit, uint32_t score) {
    heap_.ensure(lit);
    if (heap_.contains(lit)) {
      if (score <= 1) {
        heap_.assign_score(lit, score);
        heap_.erase(lit);
      } else {
        heap_.update_score(lit, score);
      }
    } else if (score > 1) {
      heap_.insert(lit, score);
    } else
      heap_.assign_score(lit, score);
  }

  Lit pop_max() { return heap_.pop_max(); }
};

// Binary count per literal, kissat's `bincount` (factor.c:1019).
static void count_binaries(const Kernel &k, std::vector<uint32_t> &bincount) {
  bincount.assign(2 * k.nvars, 0);
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++) {
    // Count a pair once from its smaller imported endpoint while it is ACTIVE
    // (not root-fixed); this is the peeling census, not the matching degree.
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
    // Factor's null-stack dense entry keeps the sparse binary sequence (no
    // sort); it is the quotient-column order and a growth tie-break.
  }
  // --- candidate reduction (kissat `factorcandrounds`, factor.c:1000) ---
  // A large clause is grid material only if every literal occurs in two candidate
  // clauses; iterate to fixpoint, bounded by `k.factor_cand_rounds` (0 = none).
  // Connect one large clause to every one of its literals' streams.
  const auto connect = [&](Ref ref, const Clause *c) {
    idx.large_count++;
    for (const Lit l : *c) {
      idx.order[l].push_back(FactorWatch::large_watch(ref));
      idx.live_degree[l]++;
    }
  };
  std::vector<uint8_t> alive;
  if (k.factor_cand_rounds && max_size >= 3) {
    std::vector<uint32_t> bincount, largecount, newcount;
    count_binaries(k, bincount);
    // Every literal of `c` occurs in at least two candidate clauses.
    const auto qualifies = [&](const Clause *c) {
      for (const Lit l : *c)
        if (bincount[l] + largecount[l] < 2)
          return false;
      return true;
    };
    // seed: every in-scope irredundant large clause is a candidate
    largecount.assign(2 * k.nvars, 0);
    std::vector<Ref> cand;
    for (const auto [ref, c] : arena_records(k.arena)) {
      if (!c->garbage() && !c->redundant() && c->size >= 3 &&
          c->size <= max_size) {
        cand.push_back(ref);
        for (const Lit l : *c)
          largecount[l]++;
      }
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
        alive[i] = qualifies(c);
        if (!alive[i])
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
    // Kissat's connection pass re-tests survivors against the last round's
    // counts (factor.c:1098-1119).
    for (size_t i = 0; i < cand.size(); i++) {
      if (!alive[i])
        continue;
      const Clause *c = clause_at(k.arena, cand[i]);
      if (qualifies(c))
        connect(cand[i], c);
    }
    idx.built = true;
    return;
  }
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (!c->garbage() && !c->redundant() && c->size >= 3 && c->size <= max_size)
      connect(ref, c);
  }
  idx.built = true;
}

// The first `initial_binary_live[l]` entries are the borrowed prefix (deletion
// is stable, new binaries append).
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

// kissat's per-literal Factor marks (`factor.c`): factor, quotient member, or
// counted next-factor candidate.
static constexpr uint8_t MARK_FACTOR = 1;
static constexpr uint8_t MARK_QUOTIENT = 2;
static constexpr uint8_t MARK_NOUNTED = 4;

// Live grid member? Root assignments do not narrow the grid (Kissat).
[[maybe_unused]] bool grid_clause_live(const Kernel &k, const Clause *c) {
  (void)k;
  return !c->garbage() && !c->redundant();
}

#ifdef TACTIX_ENABLE_EVENT_TRACE
// A traced dump of `lit`'s Factor stream: the borrowed initial binary prefix,
// then the ordered watches, each as `visit(live, other, id, clause)`.
template <class Visit>
void for_each_traced_factor_entry(const Kernel &k, const FactorIndex &idx,
                                  Lit lit, Lit live_lits, Visit visit) {
  for_each_initial_factor_binary(k, idx, lit, [&](Lit other) {
    visit(other < live_lits, other, kernel_binary_id(k, lit, other), nullptr);
  });
  for (const FactorWatch watch : idx.order[lit]) {
    if (watch.binary()) {
      const Lit other = watch.other();
      visit(other < live_lits, other, kernel_binary_id(k, lit, other),
            nullptr);
    } else {
      const Clause *clause = clause_at(k.arena, watch.ref);
      visit(grid_clause_live(k, clause), INVALID_LIT, clause->id(), clause);
    }
  }
}
#endif

// The index holds only live irredundant clauses (`apply_factoring` removes
// occurrences before marking garbage, as `delete_unfactored`), so scans assert
// the predicate.

// `C \ {l}` sorted into `out`; false if another literal crosses the extension
// boundary. Assigned literals remain members.
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

// All remainder occurrences of `l` in Kissat's mixed dense order: binary
// prefix, initial large clauses, then appended clauses. Equal clauses repeat
// ids.
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

// Match C ∖ {factor} against a canonical remainder via the caller's stamped
// mark (kissat `marks[other] & QUOTIENT`); |C|=|R|+1 proves equality.
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

  // Dividers (x ∨ fᵢ): RAT on fresh x, vacuous before the quotients. Kissat
  // applies `best_F` from the best quotient back through `prev`, so dividers
  // enter the watch lists last-factor first (propagation order).
  for (size_t rev = F.size(); rev-- > 0;) {
    const Lit f = F[rev];
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {px, f}; // pivot (x) first
    if (k.proof)
      k.proof->add_rat(id, lits, 2);
    kernel_add_binary_clause(k, px, f, id);
    append_binary_watch(px, f, id);
  }

  // Quotients (¬x ∨ rⱼ): RAT on ¬x, one group per divider; each resolvent is a
  // grid clause still live.
  std::vector<Lit> qlits;
  for (size_t column = 0; column < R.size(); column++) {
    const uint32_t rid = R[column];
    const uint32_t rn = pool.size_of(rid);
    const Lit *rl = pool.lits_of(rid);
    // Quotient {¬x} ∪ r, pivot first, built from the chosen last-factor clause
    // in its literal pose (Kissat `q->clauses`); `rid` is only the comparison
    // key.
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
    if (k.proof)
      k.proof->add_rat(id, qlits.data(), static_cast<uint32_t>(qlits.size()));
    if (rn == 1) {
      kernel_add_binary_clause(k, nx, rl[0], id);
      append_binary_watch(nx, rl[0], id);
    } else {
      const Ref qref =
          kernel_add_large_clause(k, qlits.data(),
                                  static_cast<uint32_t>(qlits.size()),
                                  /*redundant=*/false, /*glue=*/0, id);
      k.watches.defrag_if_needed();
      // Kissat inserts each quotient into the dense lists during the pass;
      // stale refs are filtered by grid_clause_live.
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

  // Delete the originals last, all copies: binaries via the definition, large
  // occurrences through their exact Refs.
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
          // kissat's `delete_unfactored` removes the occurrence from every
          // dense list before marking garbage; `live_degree` already excludes
          // it.
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

// Diagnostic: dump the clause DB as DIMACS and exit. Env-gated.
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
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (!c->garbage()) { std::vector<int> cl; for (const Lit l : *c) cl.push_back(dl(l)); cls.push_back(cl); }
  }
  FILE *f = std::fopen(path, "w");
  std::fprintf(f, "p cnf %u %zu\n", k.nvars, cls.size());
  for (const auto &cl : cls) { for (int x : cl) std::fprintf(f, "%d ", x); std::fprintf(f, "0\n"); }
  std::fclose(f);
  std::fprintf(stderr, "[factor] dumped %zu clauses (%u vars) to %s\n", cls.size(), k.nvars, path);
  std::exit(0);
}

} // namespace

RuleEligibility factor_guard(const Kernel &k) { return root_level_guard(k); }

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
        for (const auto [ref, clause] : arena_records(k.arena)) {
          if (exact_grid_clause_matches(clause, factor, remainder) &&
              !used_large.count(ref)) {
            selected = ref;
            break;
          }
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

// `forced_seed == INVALID_LIT` is the canonical sweep; a literal payload
// applies at most one factoring seeded there (`N1-FACTOR-INSTANCES`).
static ApplyResult factor_apply_seeded(Kernel &k, Lit forced_seed) {
  assert(factor_guard(k));
  k.factor_completed = false;
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  // Kissat's null-stack `kissat_flush_large_watches` before Factor: keep the
  // first binary per partner, drop sparse large watches; the dense index
  // reconnects large occurrences and sparse resume rebuilds on exit.
  if (k.large_watches_connected)
    kernel_flush_large_watches(k);

  // Kissat's `factoring.initial = LITS` includes earlier extension variables;
  // `next > initial` admits only this application's first fresh positive
  // literal. Fixed during the run.
  const Lit initial_lits = static_cast<Lit>(2 * k.nvars);
  const Lit next_factor_limit = initial_lits + 1;

  // Fresh vars of this application are [fresh_first, k.nvars).
  const Var fresh_first = k.nvars;

  // Composite grids: with `factoring.initial` fixed only the first fresh
  // positive literal can be a next factor; fresh polarities are marked, not
  // scheduled (`docs/design/s2b-composite-grids-design.md`). Every grid has
  // delta >= 1, so it terminates. Off by default; DefaultPolicy sets it during
  // search.
  const bool composite = k.factor_composite;
  Lit live_lits = initial_lits;

  // Remainder content is interned, while the vectors below retain one entry
  // per clause occurrence.  Equal ids are valid distinct quotient columns.
  RemPool pool;
  // Kissat's unified dense Factor order; binaries only at `factor_size == 2`.
  FactorIndex fidx;
  build_factor_index(k, fidx, k.factor_size);
  std::vector<Lit> rem_scratch; // one remainder, sorted
  // Scratch spans `next_factor_limit` (kissat resize_factoring, factor.c:645).
  std::vector<uint32_t> cofactor(next_factor_limit, 0); // co-occurrence counts
  // Mark the selected next factor's adjacency once per greedy step.
  std::vector<uint32_t> binary_partner_epoch(next_factor_limit, 0);
  uint32_t binary_epoch = 0;
  // Kissat clause marks (`clause::quotient`) are kept in the clause header;
  // these lists clear them without a sweep.
  std::vector<Ref> reserved_marked, used_marked;
  // kissat keeps FACTOR, QUOTIENT and NOUNTED in one byte per literal.
  std::vector<uint8_t> marks(next_factor_limit, 0);
  std::vector<Lit> quotient_marked, nounted;
  // Byte-sized `marks[lit] & FACTOR`, cleared for F after each seed.

  // Snapshot of Kissat's `flags[idx].active` per literal (stable during Factor;
  // the admitted fresh literal is marked on creation).
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
      for (const auto [ref, c] : arena_records(k.arena)) {
        assert(!(c->flags & (Clause::FACTOR_RESERVED | Clause::FACTOR_USED)) &&
               "a Factor mark outlived its application");
      }
#endif
    }
  } mark_guard{k, reserved_marked, used_marked};
  // Stamp the column's remainder QUOTIENT and clear the previous one (kissat).
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

  // Seed schedule: max-heap by occurrence count, re-scored on touched literals
  // (kissat `update_factored`, factor.c:800); a popped literal is re-armed by a
  // new clause with that polarity (`flags->factor`). Scalars come from
  // `Kernel::factor_selection` when attached. Watch sizes ≤ 2^24 keep the
  // double round-trip exact (`docs/design/s2a-factor-matcher-design.md`).
  FactorSelection *const fm = k.factor_selection;
  if (fm)
    fm->begin_sweep(k);
  // Kissat scores dense Factor lists: one entry per binary partner plus one per
  // candidate large clause.
  const auto reference_score_of = [&](Lit l) -> uint32_t {
    if (!fidx.built || l >= fidx.live_degree.size())
      return 0;
    // update_candidate/tied_next_factor_score use the complete live dense size.
    return fidx.live_degree[l];
  };
  // Kissat's match scan: exactly one factor literal, the first least-watched
  // remainder literal; INVALID_LIT rejects.
  const auto grid_clause_scan_literal = [&](Ref current_ref,
                                            const Clause *&current) -> Lit {
    if (current_ref == INVALID_REF)
      return INVALID_LIT;
    current = clause_at(k.arena, current_ref);
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
      return INVALID_LIT;
    charge_factor_watch_scan(k, reference_score_of(scan));
    return scan;
  };
  const auto score_of = [&](Lit l) -> uint32_t {
    const uint32_t reference_score = reference_score_of(l);
    return fm ? static_cast<uint32_t>(fm->seed_score(
                    k, l, static_cast<double>(reference_score)))
              : reference_score;
  };
  // `k.work_budget` in factor_ticks; zero is unbounded unless
  // `work_budget_bounded`. Quit points as Kissat (factor.c:442, :914).
  const uint64_t tick_start = k.stats.factor_ticks;
  const auto over_budget = [&]() {
    return kernel_work_budget_exceeded(k,
                                       k.stats.factor_ticks - tick_start);
  };
  FactorHeap schedule;
  // Per-polarity analogue of Kissat's `flags->factor`: clear on pop, set again
  // only when a newly added divider or quotient contains the literal.
  std::vector<uint8_t> armed(next_factor_limit, 0);
  // Persistent per-literal seed marks (`flags->factor`, inline.h:60-66,
  // factor.c:922). N1: only the canonical schedule is narrowed.
  // `TACTIX_TOUCH_MARKS=0` arms all literals
  // (test_touch_filter_is_policy_only).
  const bool touch_filter = k.touch_filter;
  const auto seed_is_new = [&](Lit l) {
    return !touch_filter || k.factor_marks[l];
  };
  // Build the heap in Kissat's first-import order (`tumble=1`) through the
  // reference order.
  std::vector<Lit> schedule_order(live_lits);
  std::iota(schedule_order.begin(), schedule_order.end(), Lit{0});
  std::stable_sort(schedule_order.begin(), schedule_order.end(),
                   [&](Lit a, Lit b) {
                     return kernel_import_lit_order(k, a) <
                            kernel_import_lit_order(k, b);
                   });
  // A single instance arms the requested literal; the touch filter (a policy
  // narrowing) does not apply (N1).
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
      for_each_traced_factor_entry(
          k, fidx, l, live_lits,
          [&](bool live, Lit other, uint64_t id, const Clause *clause) {
            event_trace_factor_literal_entry(k, l, ordinal++, live, other, id,
                                             clause);
          });
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
    schedule.update(l, score);
  }

  while (!schedule.empty()) {
    if (over_budget()) // quit point 1: before each popped seed
      break;
    // One factoring per literal instance.
    if (forced_seed != INVALID_LIT && applied)
      break;
    const Lit seed = schedule.pop_max();
#ifdef TACTIX_ENABLE_EVENT_TRACE
    diagnostic_pops++;
#endif
    if (forced_seed != INVALID_LIT && seed != forced_seed)
      break;
    // Bound by `live_lits`, the dynamically grown universe.
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
    // The matcher records only improving grids; zero-gain 2x2 actions remain
    // available through exact-grid enumeration.
    int best_delta = 0;
    best_F.clear();
    best_R.clear();
    best_grid_refs.clear();

    // greedily grow the factor set, tracking the max-delta prefix.
    while (R.size() >= 2) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
      const uint64_t diagnostic_next_before = k.stats.factor_ticks;
#endif
      // Count next factors in Kissat's clause-occurrence order: per clause, its
      // first least-watched remainder literal's list, reserving each match
      // once. First discovery breaks ties.
      touched.clear();
      const auto count_candidate = [&](Lit candidate) -> bool {
        // Predicate order as factor.c:350-357: boundary, FACTOR mark, then
        // ACTIVE.
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
          // Only binaries count; the kernel slice has the same order.
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
        const Clause *current = nullptr;
        const Lit scan = grid_clause_scan_literal(current_ref, current);
        if (scan == INVALID_LIT)
          continue;
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

      // Highest count, ties by watch size (kissat `tied_next_factor_score`,
      // factor.c:318).
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

      // Kissat's `new_quotient(g)` marks g before `factorize_next` scans, so
      // rows containing g are skipped.
      assert(g < marks.size() && !(marks[g] & MARK_FACTOR));
      marks[g] |= MARK_FACTOR;
#ifdef TACTIX_ENABLE_EVENT_TRACE
      const uint64_t diagnostic_match_before = k.stats.factor_ticks;
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

      // Filter rows through the match indices, then append g's Ref (Kissat
      // factorize_next + flush_unmatched_clauses).
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
          // Kissat takes, per surviving occurrence, the first unused exact
          // match in its first least-watched remainder literal's list; equal
          // clauses keep identity.
          const Ref current_ref = grid_refs.back()[column];
          const Clause *current = nullptr;
          const Lit scan = grid_clause_scan_literal(current_ref, current);
          if (scan == INVALID_LIT)
            continue;
          const Lit *expected = pool.lits_of(rid);
          stamp_remainder(expected, rn, live_lits);
          for (const FactorWatch watch : fidx.order[scan]) {
            if (watch.binary())
              continue;
            const Ref ref = watch.ref;
            if (ref == current_ref)
              continue;
            k.stats.factor_ticks++;
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
          if (matched_ref == INVALID_REF)
            continue;
        }
        matched_columns.push_back(column);
        newR.push_back(rid);
        next_refs.push_back(matched_ref);
      }
      // `newR.size()` can differ from `bestc` (the count reserves across the
      // whole quotient); only g's selection reads `bestc` (implementation-notes
      // §5).
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

    // Acceptance: an attached FactorSelection, else `k.factor_bound` (kissat
    // factor.c:939).
    const bool bound_ok =
        !best_F.empty() &&
        (fm ? fm->accept(static_cast<uint32_t>(best_F.size()),
                         static_cast<uint32_t>(best_R.size()), best_delta)
            : best_delta > static_cast<int>(k.factor_bound));
    if (!bound_ok && !best_F.empty())
      k.stats.factor_rejected_by_bound++; // the grid existed; the bound said no
    if (bound_ok) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_factor_grid(k, seed, static_cast<uint32_t>(best_F.size()),
                              static_cast<uint32_t>(best_R.size()), best_delta);
      for (uint32_t i = 0; i < best_F.size(); i++)
        event_trace_factor_grid_member(k, i, best_F[i]);
#endif
      const Var added_fresh = k.nvars;
      apply_factoring(k, fidx, best_F, best_R, pool, best_grid_refs);
      assert(k.nvars == added_fresh + 1);
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
      // Grow the universe over the fresh var (kissat resize_factoring) without
      // scheduling it.
      if (composite && static_cast<Lit>(2 * k.nvars) > live_lits) {
        live_lits = static_cast<Lit>(2 * k.nvars);
        cofactor.resize(std::max(live_lits, next_factor_limit), 0);
        armed.resize(std::max(live_lits, next_factor_limit), 0);
        binary_partner_epoch.resize(std::max(live_lits, next_factor_limit), 0);
        marks.resize(std::max(live_lits, next_factor_limit), 0);
        const Lit fresh = make_lit(added_fresh, false);
        const Lit not_fresh = negate(fresh);
        assert(k.factor_marks[fresh] && k.factor_marks[not_fresh]);
        // Later composite extension variables never pass next_factor's
        // boundary.
        if (fresh < factor_active.size())
          factor_active[fresh] =
              k.value(fresh) == 0 && !kernel_var_retired(k, added_fresh);
        armed[fresh] = armed[not_fresh] = 1;
      }
      // Re-arm exactly the polarities in the new definition
      // (kissat_mark_added_literal).
      for (const Lit f : best_F)
        armed[f] = 1;
      for (const uint32_t rid : best_R) {
        const Lit *rl = pool.lits_of(rid);
        for (uint32_t i = 0; i < pool.size_of(rid); i++)
          if (rl[i] < live_lits)
            armed[rl[i]] = 1;
      }
      // Heap updates in Kissat's order: factors from the final quotient
      // backwards, each with its opposite, then the other literals of each
      // removed clause.
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
    }
    for (const Lit factor : F) {
      assert(factor < marks.size() && (marks[factor] & MARK_FACTOR));
      marks[factor] &= static_cast<uint8_t>(~MARK_FACTOR);
    }
  }

  // Kissat advances the material watermark only on an empty schedule.
  k.factor_completed = schedule.empty();

  // A budget cut needs no tail repair: unpopped literal bits remain set in
  // `factor_marks`, exactly as Kissat leaves them for the next occasion.

  // Kissat restamps the queue after every pass, including zero-yield ones.
  k.fresh_var_events.push_back({fresh_first, k.nvars});

  if (fm)
    fm->end_sweep();

  // `kissat_resume_sparse_mode`: drop large occurrences, sort each live
  // clause's watched pair for the root assignment and reconnect in arena order.
  // Binary watches keep first-encounter order.
  kernel_rebuild_large_watches(k);

  if (const char *p = std::getenv("TACTIX_DUMP_FACTORED"))
    if (applied)
      dump_factored_and_exit(k, p);
  return applied ? ApplyResult::Ok : ApplyResult::NoOp;
}

ApplyResult factor_apply(Kernel &k) {
  return factor_apply_seeded(k, INVALID_LIT);
}

// `N1-FACTOR-INSTANCES` (implementation-notes §5 Factor): payload is the seed
// literal; every unassigned, unretired literal is offered. Grids are not
// addressable; they are swappable through `factor_selection`.
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

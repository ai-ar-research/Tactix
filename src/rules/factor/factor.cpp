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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix {

namespace {

// A factoring is worth applying only if it removes at least this many clauses
// (delta = m·n − (m+n)). delta ≥ 1 needs m,n ≥ 2 with one ≥ 3.
constexpr int FACTOR_MIN_GAIN = 1;

// Factor work currency (kissat factor.c:274, utilities.h kissat_cache_lines):
// 1 + one tick per 128 bytes touched, so a walk over `words` uint32_t watch
// entries costs 1 + (4·words)/128. The apply-local stream retains unique
// binary partners and large-clause occurrences, like Kissat's dense watches
// (`docs/implementation-notes.md` §13).
inline void charge_factor_ticks(Kernel &k, uint32_t words) {
  k.stats.factor_ticks += 1 + (words >> 5);
}

// --- remainder interning (`docs/design/e2-factorsize-design.md` §3) ----------
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
    unit_id[l] = static_cast<int32_t>(id);
    return id;
  }
  // Multi-literal intern; `b[0..n)` must be sorted ascending. Identity is by
  // content.  Interning never implies occurrence deduplication.
  std::unordered_map<uint64_t, std::vector<uint32_t>> buckets;
  uint32_t intern(const Lit *b, uint32_t n) {
    if (n == 1)
      return intern_unit(b[0]);
    uint64_t h = 1469598103934665603ull; // FNV-1a over the sorted literals
    for (uint32_t i = 0; i < n; i++) {
      h ^= b[i];
      h *= 1099511628211ull;
    }
    auto &bucket = buckets[h];
    for (const uint32_t id : bucket)
      if (len[id] == n && !std::memcmp(pool.data() + off[id], b, n * sizeof(Lit)))
        return id;
    const uint32_t id = static_cast<uint32_t>(off.size());
    off.push_back(static_cast<uint32_t>(pool.size()));
    len.push_back(n);
    pool.insert(pool.end(), b, b + n);
    bucket.push_back(id);
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
  Lit other = INVALID_LIT;
  Ref ref = INVALID_REF;
  uint64_t id = 0;
  bool live = true;
  bool binary() const { return ref == INVALID_REF; }
};

struct FactorIndex {
  std::vector<std::vector<Ref>> occ; // by literal
  // Kissat's dense Factor watches are one ordered stream: each binary partner
  // surviving dense-entry deduplication, then candidate large clauses in arena
  // order, then every divider/quotient appended during the pass.
  std::vector<std::vector<FactorWatch>> order;
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
#endif

// Count binary clauses per literal — the `bincount` half of kissat's candidate
// reduction (factor.c:1019). Binaries are never pruned, being the grid's other
// side; they only decide whether a large clause survives.
static void count_binaries(const Kernel &k, std::vector<uint32_t> &bincount) {
  bincount.assign(2 * k.nvars, 0);
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++) {
    uint32_t n = 0;
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary())
        n++;
    bincount[l] = n;
  }
}

void build_factor_index(const Kernel &k, FactorIndex &idx, uint32_t max_size) {
  idx.occ.assign(2 * k.nvars, {});
  idx.order.assign(2 * k.nvars, {});
  for (Lit lit = 0; lit < static_cast<Lit>(2 * k.nvars); lit++)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary())
        idx.order[lit].push_back(
            {watch.other(), INVALID_REF,
             kernel_binary_id(k, lit, watch.other()), true});
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
      const uint32_t words = clause_words(c->size);
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
    for (size_t i = 0; i < cand.size(); i++)
      if (alive[i])
        for (const Lit l : *clause_at(k.arena, cand[i])) {
          idx.occ[l].push_back(cand[i]);
          idx.order[l].push_back({INVALID_LIT, cand[i], 0, true});
        }
    idx.built = true;
    return;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage() && !c->redundant() && c->size >= 3 && c->size <= max_size)
      for (const Lit l : *c) {
        idx.occ[l].push_back(ref);
        idx.order[l].push_back({INVALID_LIT, ref, 0, true});
      }
    ref += words;
  }
  idx.built = true;
}

// Is clause `c` live and usable as a grid member?  Root assignments do not
// narrow Factor's clause-occurrence grid: Kissat connects every surviving
// irredundant clause and its first quotient retains assigned remainders.
bool grid_clause_live(const Kernel &k, const Clause *c) {
  (void)k;
  return !c->garbage() && !c->redundant();
}

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

bool factor_binary_live(const Kernel &k, Lit lit, const FactorWatch &watch) {
  (void)k;
  (void)lit;
  return watch.live;
}

// All remainder OCCURRENCES of `l`: its unique binary partners in encounter
// order, followed by one entry per indexed large clause containing `l`. Equal
// large clauses deliberately yield repeated ids.
void remainder_occurrences_of(Kernel &k, const FactorIndex &idx, RemPool &pool,
                              Lit l, Lit limit, std::vector<Lit> &rem_scratch,
                              std::vector<RemOccurrence> &out) {
  out.clear();
  if (!idx.built || l >= idx.order.size())
    return;
  for (const FactorWatch watch : idx.order[l]) {
    if (watch.binary()) {
      if (watch.other < limit && factor_binary_live(k, l, watch))
        out.push_back({pool.intern_unit(watch.other), INVALID_REF});
      continue;
    }
    if (k.factor_size < 3)
      continue;
    const Ref ref = watch.ref;
    const Clause *c = clause_at(k.arena, ref);
    if (!grid_clause_live(k, c))
      continue;
    charge_factor_ticks(k, c->size);
    if (!remainder_of(k, c, l, limit, rem_scratch))
      continue;
    const uint32_t rid =
        pool.intern(rem_scratch.data(), static_cast<uint32_t>(rem_scratch.size()));
    out.push_back({rid, ref});
    k.stats.factor_large_remainders++;
  }
}

bool contains(const std::vector<Lit> &v, Lit l) {
  for (const Lit x : v)
    if (x == l)
      return true;
  return false;
}

// All clause occurrences of the form {factor} ∪ remainder, grouped by factor
// while preserving first-factor and clause encounter order.  The vectors of
// refs retain occurrence capacity for multiset intersection; content identity
// alone is insufficient when equal clauses occur more than once.
struct RemainderFactorBucket {
  std::vector<Lit> order;
  std::unordered_map<Lit, std::vector<Ref>> refs;
};

void build_remainder_factor_bucket(Kernel &k, const FactorIndex &idx,
                                   const RemPool &pool, uint32_t rid, Lit limit,
                                   RemainderFactorBucket &bucket) {
  bucket.order.clear();
  bucket.refs.clear();
  const uint32_t rn = pool.size_of(rid);
  const Lit *rl = pool.lits_of(rid);
  if (rn == 1) {
    if (rl[0] >= idx.order.size())
      return;
    for (const FactorWatch watch : idx.order[rl[0]]) {
      if (!watch.binary() || watch.other >= limit ||
          k.value(watch.other) != 0 ||
          !factor_binary_live(k, rl[0], watch))
        continue;
      const Lit factor = watch.other;
      auto [it, inserted] = bucket.refs.try_emplace(factor);
      if (inserted)
        bucket.order.push_back(factor);
      it->second.push_back(INVALID_REF);
    }
    return;
  }

  Lit scan = INVALID_LIT;
  size_t scan_occ = SIZE_MAX;
  for (uint32_t i = 0; i < rn; i++) {
    if (rl[i] >= idx.occ.size())
      continue;
    if (idx.occ[rl[i]].size() < scan_occ) {
      scan_occ = idx.occ[rl[i]].size();
      scan = rl[i];
    }
  }
  if (scan == INVALID_LIT)
    return;
  for (const FactorWatch watch : idx.order[scan]) {
    if (watch.binary())
      continue;
    const Ref ref = watch.ref;
    const Clause *c = clause_at(k.arena, ref);
    if (c->size != rn + 1 || !grid_clause_live(k, c))
      continue;
    charge_factor_ticks(k, c->size);
    Lit factor = INVALID_LIT;
    bool ok = true;
    for (const Lit x : *c) {
      if (std::binary_search(rl, rl + rn, x))
        continue;
      if (factor != INVALID_LIT) {
        ok = false;
        break;
      }
      factor = x;
    }
    if (!ok || factor == INVALID_LIT || factor >= limit ||
        k.value(factor) != 0)
      continue;
    auto [it, inserted] = bucket.refs.try_emplace(factor);
    if (inserted)
      bucket.order.push_back(factor);
    it->second.push_back(ref);
  }
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
    if (!idx.built)
      return;
    const size_t need = static_cast<size_t>(std::max(a, b)) + 1;
    if (idx.order.size() < need)
      idx.order.resize(need);
    idx.order[a].push_back({b, INVALID_REF, id, true});
    idx.order[b].push_back({a, INVALID_REF, id, true});
  };

  // Dividers (x ∨ fᵢ): RAT on the fresh literal x. At add time no clause
  // contains ¬x (the quotients come after), so the RAT condition is vacuous —
  // pivot-first, empty hints.
  std::vector<uint64_t> divider_ids;
  divider_ids.reserve(F.size());
  for (const Lit f : F) {
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {px, f}; // pivot (x) first
    if (k.proof)
      k.proof->add_rat(id, lits, 2, nullptr, 0);
    kernel_add_binary_clause(k, px, f, id);
    append_binary_watch(px, f, id);
    divider_ids.push_back(id);
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
    // Quotient {¬x} ∪ r, pivot first. The RAT-on-¬x argument is indifferent to
    // |r|, so the hint stream stays per-divider at any factor size.
    qlits.assign(1, nx);
    qlits.insert(qlits.end(), rl, rl + rn);
    const uint64_t id = ++k.next_clause_id;
    if (lrat) {
      hints.clear();
      for (size_t i = 0; i < F.size(); i++) {
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
      // Kissat inserts every quotient into its dense Factor watch lists before
      // continuing the same pass.  Keep this apply-local index live too: the
      // fresh clause can change later seed scores and can complete a composite
      // grid.  Stale original refs need no removal because grid_clause_live
      // filters their garbage marks.
      if (idx.built && qlits.size() <= k.factor_size) {
        if (idx.occ.size() < static_cast<size_t>(2 * k.nvars))
          idx.occ.resize(static_cast<size_t>(2 * k.nvars));
        if (idx.order.size() < static_cast<size_t>(2 * k.nvars))
          idx.order.resize(static_cast<size_t>(2 * k.nvars));
        for (const Lit l : qlits) {
          idx.occ[l].push_back(qref);
          idx.order[l].push_back({INVALID_LIT, qref, 0, true});
        }
      }
    }
  }

  // Delete the originals last (proof `del`), all copies: a duplicated grid
  // binary is fully replaced by the aux-var definition, while every large
  // occurrence is deleted through the exact Ref carried by the aligned grid.
  const auto invalidate_binary_watch = [&](Lit a, Lit b) {
    if (!idx.built)
      return;
    for (FactorWatch &watch : idx.order[a])
      if (watch.binary() && watch.other == b)
        watch.live = false;
    for (FactorWatch &watch : idx.order[b])
      if (watch.binary() && watch.other == a)
        watch.live = false;
  };
  for (size_t i = 0; i < F.size(); i++)
    for (size_t column = 0; column < R.size(); column++) {
      const uint32_t rid = R[column];
      if (pool.size_of(rid) == 1) {
        invalidate_binary_watch(F[i], pool.lit0(rid));
        kernel_delete_binary_all_copies(k, F[i], pool.lit0(rid));
        continue;
      }
      const Ref ref = grid_refs[i][column];
      if (ref == INVALID_REF)
        continue;
      Clause *c = clause_at(k.arena, ref);
      if (c->garbage())
        continue;
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
    ref += clause_words(c->size);
  }
  FILE *f = std::fopen(path, "w");
  std::fprintf(f, "p cnf %u %zu\n", k.nvars, cls.size());
  for (const auto &cl : cls) { for (int x : cl) std::fprintf(f, "%d ", x); std::fprintf(f, "0\n"); }
  std::fclose(f);
  std::fprintf(stderr, "[factor] dumped %zu clauses (%u vars) to %s\n", cls.size(), k.nvars, path);
  std::exit(0);
}

} // namespace

bool factor_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

ApplyResult factor_apply(Kernel &k) {
  assert(factor_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  // Kissat enters dense mode immediately before Factor. That transition
  // flushes duplicate binary watches while preserving their first encounter
  // order; Tactix's longer-lived phase occurrence index does not otherwise
  // reproduce this boundary.
  kernel_deduplicate_binary_watches(k);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  if (const char *p = std::getenv("TACTIX_DUMP_PREFACTOR"))
    dump_factored_and_exit(k, p);
#endif

  // Admission is the policy's decision, not the rule's: a selectivity heuristic
  // here as a NoOp would let the guard report applicable while apply did
  // nothing, so no API caller could apply a legal factoring — an N1 breach,
  // whose carve-out covers the policy declining, never the rule refusing
  // everyone. `Kernel::factor_admit` defaults true; `TACTIX_FACTOR_FORCE`
  // overrides for fuzz on tiny formulas.
  if (!k.factor_admit && !std::getenv("TACTIX_FACTOR_FORCE"))
    return ApplyResult::NoOp;

  // Anti-loop boundary: only original literals may be factors/remainders, never
  // the extension vars Factor adds. It must be `first_extension_var`, fixed
  // across applies — `2*k.nvars` would let a second apply factor over the
  // first's extension vars, unbounded self-factoring.
  const Lit orig_lits =
      static_cast<Lit>(2 * kernel_num_original_internal_vars(k));

  // Fresh-variable boundary for the post-application demotion: every var this
  // application adds lands in [fresh_first, k.nvars), contiguously, so the
  // O(#queue) repair is paid once per application and threads no list.
  const Var fresh_first = k.nvars;

  // Composite grids (`docs/design/s2b-composite-grids-design.md`):
  // `k.factor_composite` widens the remainder universe to all live literals,
  // growing as the pass applies (kissat resize_factoring). Fresh literals can
  // participate as remainders, but never as factors in the same pass: Kissat
  // keeps `factoring.initial` fixed and rejects `next > initial` in addition
  // to not pushing fresh literals into the first-factor heap.
  // Termination needs no var-class exclusion — every accepted grid has
  // delta >= 1, strictly decreasing the finite live-binary count. Default off:
  // composition over-applies in a preprocessing blast, and belongs to the
  // interleaved during-search doses where DefaultPolicy sets it.
  const bool composite = k.factor_composite;
  Lit live_lits = composite ? static_cast<Lit>(2 * k.nvars) : orig_lits;

  // Remainder content is interned, while the vectors below retain one entry
  // per clause occurrence.  Equal ids are valid distinct quotient columns.
  RemPool pool;
  // One arena pass per apply builds Kissat's unified dense Factor order.  At
  // `factor_size == 2` it contains only the existing binary stream; large
  // clauses are neither indexed nor considered.
  FactorIndex fidx;
  build_factor_index(k, fidx, k.factor_size);
  std::vector<Lit> rem_scratch;                 // one remainder, sorted
  std::vector<uint32_t> cofactor(live_lits, 0); // co-occurrence counts
  std::vector<Lit> touched, F, best_F;
  std::vector<uint32_t> R, newR, best_R;
  std::vector<RemOccurrence> occurrences;
  std::vector<std::vector<Ref>> grid_refs, new_grid_refs, best_grid_refs;
  uint64_t applied = 0;

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
    uint32_t score = 0;
    if (!fidx.built || l >= fidx.order.size())
      return score;
    for (const FactorWatch watch : fidx.order[l]) {
      if (watch.binary()) {
        score += watch.other < live_lits && factor_binary_live(k, l, watch);
        continue;
      }
      const Ref ref = watch.ref;
      const Clause *c = clause_at(k.arena, ref);
      if (!grid_clause_live(k, c))
        continue;
      bool usable = true;
      for (const Lit other : *c)
        if (other != l && other >= live_lits) {
          usable = false;
          break;
        }
      score += usable;
    }
    return score;
  };
  const auto score_of = [&](Lit l) -> uint32_t {
    const uint32_t reference_score = reference_score_of(l);
    return fm ? static_cast<uint32_t>(fm->seed_score(
                    k, l, static_cast<double>(reference_score)))
              : reference_score;
  };
  // `k.work_budget`: stop the round after this many factor_ticks, 0 = unbounded.
  // Two quit points, mirroring kissat's (factor.c:442, :914).
  const uint64_t tick_start = k.stats.factor_ticks;
  const uint64_t budget = k.work_budget;
  const auto over_budget = [&]() {
    return budget && k.stats.factor_ticks - tick_start > budget;
  };
  FactorHeap schedule;
  // Per-polarity analogue of Kissat's `flags->factor`: clear on pop, set again
  // only when a newly added divider or quotient contains the literal.
  std::vector<uint8_t> armed(live_lits, 0);
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
  for (const Lit l : schedule_order) {
    if (k.value(l) != 0)
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
    const Lit seed = schedule.pop_max();
    if (seed >= armed.size() || !armed[seed] || k.value(seed) != 0)
      continue;
    armed[seed] = 0;
    k.factor_marks[seed] = 0;
    const uint32_t now = score_of(seed);
    if (now <= 1)
      continue;
    remainder_occurrences_of(k, fidx, pool, seed, live_lits, rem_scratch,
                             occurrences);
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
    int best_delta = 0;
    best_F.clear();
    best_R.clear();
    best_grid_refs.clear();

    // greedily grow the factor set, tracking the max-delta prefix.
    while (R.size() >= 2 && !over_budget()) {
      // Count candidate next factors by multiset intersection with the current
      // quotient.  Equal remainder contents retain their clause-occurrence
      // capacity, so a candidate can consume no more exact Refs than exist.
      touched.clear();
      std::vector<uint32_t> remainder_count(pool.count(), 0), distinct_rids;
      for (const uint32_t rid : R)
        if (!remainder_count[rid]++)
          distinct_rids.push_back(rid);
      std::vector<RemainderFactorBucket> buckets(pool.count());
      for (const uint32_t rid : distinct_rids) {
        const Lit *remainder = pool.lits_of(rid);
        const uint32_t remainder_size = pool.size_of(rid);
        bool contains_existing_factor = false;
        for (uint32_t i = 0; i < remainder_size; i++)
          if (contains(F, remainder[i])) {
            contains_existing_factor = true;
            break;
        }
        if (contains_existing_factor)
          continue;
        RemainderFactorBucket &bucket = buckets[rid];
        build_remainder_factor_bucket(k, fidx, pool, rid, orig_lits, bucket);
        for (const Lit h : bucket.order) {
          if (var_of(h) == var_of(seed) || contains(F, h))
            continue;
          if (!cofactor[h])
            touched.push_back(h);
          const size_t available = bucket.refs.find(h)->second.size();
          cofactor[h] += static_cast<uint32_t>(
              std::min<size_t>(remainder_count[rid], available));
        }
      }
      // Highest co-occurrence count, ties broken by watch-list size (kissat's
      // `tied_next_factor_score`, factor.c:318). On a dense binary graph the max
      // count is tied among many candidates, so the tie-break selects the grid.
      Lit g = INVALID_LIT;
      uint32_t bestc = 1, best_score = 0;
      for (const Lit h : touched) {
        const uint32_t c = cofactor[h];
        if (c <= 1 || c < bestc)
          continue;
        const uint32_t reference_score = reference_score_of(h);
        const uint32_t score = fm ? static_cast<uint32_t>(
                                        fm->next_factor_score(
                                            k, h, c, seed,
                                            static_cast<double>(reference_score)))
                                  : reference_score;
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
      if (g == INVALID_LIT) // no factor shares ≥ 2 remainders
        break;

      // Filter every existing row through the same match indices, then append
      // g's exact clause Ref as the new row.  Iterating R in its current order
      // is Kissat's factorize_next + flush_unmatched_clauses lifecycle.
      newR.clear();
      std::vector<size_t> matched_columns;
      std::vector<Ref> next_refs;
      std::vector<uint32_t> used(pool.count(), 0);
      std::unordered_set<Ref> used_large_refs;
      for (size_t column = 0; column < R.size(); column++) {
        const uint32_t rid = R[column];
        const uint32_t rn = pool.size_of(rid);
        Ref matched_ref = INVALID_REF;
        if (rn == 1) {
          const auto it = buckets[rid].refs.find(g);
          if (it == buckets[rid].refs.end() ||
              used[rid] >= it->second.size())
            continue;
          matched_ref = it->second[used[rid]++];
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
          Lit scan = INVALID_LIT;
          uint32_t scan_size = 0;
          uint32_t factors_in_clause = 0;
          for (const Lit lit : *current) {
            if (contains(F, lit)) {
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
          const Lit *expected = pool.lits_of(rid);
          for (const FactorWatch watch : fidx.order[scan]) {
            if (watch.binary())
              continue;
            const Ref ref = watch.ref;
            if (ref == current_ref || used_large_refs.count(ref))
              continue;
            const Clause *candidate = clause_at(k.arena, ref);
            if (candidate->size != rn + 1 || !grid_clause_live(k, candidate))
              continue;
            if (!remainder_of(k, candidate, g, live_lits, rem_scratch) ||
                rem_scratch.size() != rn ||
                !std::equal(rem_scratch.begin(), rem_scratch.end(), expected))
              continue;
            matched_ref = ref;
            used_large_refs.insert(ref);
            break;
          }
          if (matched_ref == INVALID_REF)
            continue;
        }
        matched_columns.push_back(column);
        newR.push_back(rid);
        next_refs.push_back(matched_ref);
      }
      assert(newR.size() == bestc);
      new_grid_refs.assign(grid_refs.size() + 1, {});
      for (size_t row = 0; row < grid_refs.size(); row++) {
        new_grid_refs[row].reserve(matched_columns.size());
        for (const size_t column : matched_columns)
          new_grid_refs[row].push_back(grid_refs[row][column]);
      }
      new_grid_refs.back() = std::move(next_refs);
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
      R.swap(newR);
      grid_refs.swap(new_grid_refs);
    }

    // Acceptance: an attached FactorSelection takes precedence, otherwise
    // `k.factor_bound` (kissat factor.c:939). The delta ≥ FACTOR_MIN_GAIN floor
    // stays unconditional — termination is delta-monotone, and an empty best_F
    // must never reach `apply_factoring`.
    const bool bound_ok =
        best_delta >= FACTOR_MIN_GAIN &&
        (fm ? fm->accept(static_cast<uint32_t>(best_F.size()),
                         static_cast<uint32_t>(best_R.size()), best_delta)
            : best_delta > static_cast<int>(k.factor_bound));
    if (!bound_ok && best_delta >= FACTOR_MIN_GAIN)
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
      apply_factoring(k, fidx, best_F, best_R, pool, best_grid_refs);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_factor_pair(k, seed, "after");
#endif
      applied++;
      k.stats.factorings++;
      // Grow the pass universe over the fresh var (kissat resize_factoring).
      // Its divider/quotient clauses can participate in later chains through
      // re-armed original literals, but resize_factoring does not schedule the
      // fresh literal itself as a first factor.
      if (composite && static_cast<Lit>(2 * k.nvars) > live_lits) {
        live_lits = static_cast<Lit>(2 * k.nvars);
        cofactor.resize(live_lits, 0);
        armed.resize(live_lits, 0);
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
  }

  // A budget cut needs no tail repair: unpopped literal bits remain set in
  // `factor_marks`, exactly as Kissat leaves them for the next occasion.

  if (applied)
    k.fresh_var_events.push_back({fresh_first, k.nvars});

  if (fm)
    fm->end_sweep();
  if (std::getenv("TACTIX_FACTOR_TRACE"))
    std::fprintf(stderr,
                 "[factor] entered (%u orig lits), %llu factorings, "
                 "%llu ticks (budget %llu%s)\n",
                 orig_lits, (unsigned long long)applied,
                 (unsigned long long)(k.stats.factor_ticks - tick_start),
                 (unsigned long long)budget, over_budget() ? ", CUT" : "");
  if (const char *p = std::getenv("TACTIX_DUMP_FACTORED"))
    if (applied)
      dump_factored_and_exit(k, p);
  return applied ? ApplyResult::Ok : ApplyResult::NoOp;
}

uint32_t factor_instances(Kernel &, uint64_t *out, uint32_t cap) {
  if (cap >= 1)
    out[0] = INSTANCE_CANONICAL;
  return 1;
}

ApplyResult factor_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return factor_apply(k);
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Factor, RuleTier::Inprocess, factor_guard, factor_apply,
                     factor_instances, factor_apply_instance);

} // namespace tactix

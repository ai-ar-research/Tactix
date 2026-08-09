#include "rules/factor/factor.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp" // inprocess_reach_root

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tactix {

namespace {

// A factoring is worth applying only if it removes at least this many clauses
// (delta = m·n − (m+n)). delta ≥ 1 needs m,n ≥ 2 with one ≥ 3.
constexpr int FACTOR_MIN_GAIN = 1;
// safety cap on factorings per apply.
constexpr uint64_t FACTOR_MAX_PER_APPLY = 1u << 20;
// cap the grid dimensions (bounds the proof + detection cost).
constexpr uint32_t FACTOR_DIM_LIMIT = 4096;

// Factor work currency (kissat factor.c:274, utilities.h kissat_cache_lines):
// 1 + one tick per 128 bytes touched, so a walk over `words` uint32_t watch
// entries costs 1 + (4·words)/128. Approximate — kissat meters dense deduped
// watches, these keep duplicates (`docs/design/e2-factor-design.md` §5.4).
inline void charge_factor_ticks(Kernel &k, uint32_t words) {
  k.stats.factor_ticks += 1 + (words >> 5);
}

// Distinct binary partners of `l`: each unassigned `p` below the extension
// boundary `limit` with a live binary (l ∨ p). Deduplicated via the caller's
// `seen` marker, since duplicate binaries would inflate the grid dimensions and
// the gain, and produce repeated (undeletable) grid pairs.
void binary_partners(Kernel &k, Lit l, Lit limit, std::vector<Lit> &out,
                     std::vector<uint8_t> &seen) {
  out.clear();
  charge_factor_ticks(k, k.watches.size(l));
  for (const WatchEntry w : k.watches.entries(l)) {
    if (!w.binary())
      continue;
    const Lit p = w.other();
    if (p < limit && k.value(p) == 0 && !seen[p]) {
      seen[p] = 1;
      out.push_back(p);
    }
  }
  for (const Lit p : out) // clear the marker for reuse
    seen[p] = 0;
}

// --- remainder interning (`docs/design/e2-factorsize-design.md` §3) ----------
// A remainder is `C \ {f}` for a grid clause C. The set operations the grid
// search runs on R need identity, which a sub-clause does not have, so
// remainders are interned into dense ids for one apply; `factor_size == 2` must
// stay bit-identical to the pre-interning binary path. `unit_id` grows on
// demand, never fill-initialized per call, which would make memset dominate.
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
  // content, which is what makes `inR`, intersection and dedup work at all.
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
struct FactorIndex {
  std::vector<std::vector<Ref>> occ; // by literal
  bool built = false;
};

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
        for (const Lit l : *clause_at(k.arena, cand[i]))
          idx.occ[l].push_back(cand[i]);
    idx.built = true;
    return;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage() && !c->redundant() && c->size >= 3 && c->size <= max_size)
      for (const Lit l : *c)
        idx.occ[l].push_back(ref);
    ref += words;
  }
  idx.built = true;
}

// Is clause `c` live and usable as a grid member? Root-satisfied clauses are
// excluded: a grid whose member is already true is not a real m×n grid.
bool grid_clause_live(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant())
    return false;
  for (const Lit l : *c)
    if (k.value(l) > 0)
      return false;
  return true;
}

// The remainder `C \ {l}`, written sorted into `out`. Returns false if any other
// literal is assigned (an unassigned remainder is what the grid needs).
bool remainder_of(const Kernel &k, const Clause *c, Lit l, Lit limit,
                  std::vector<Lit> &out) {
  out.clear();
  for (const Lit x : *c) {
    if (x == l)
      continue;
    if (x >= limit || k.value(x) != 0)
      return false; // extension-var boundary, or not free
    out.push_back(x);
  }
  std::sort(out.begin(), out.end());
  return !out.empty();
}

// All remainders of `l`: its binary partners (interned as |r| == 1, in watch
// order) followed by, when factor_size >= 3, one per indexed clause containing
// `l`. Dedup is by interned id.
void remainders_of(Kernel &k, const FactorIndex &idx, RemPool &pool, Lit l,
                   Lit limit, std::vector<Lit> &partner_scratch,
                   std::vector<Lit> &rem_scratch, std::vector<uint8_t> &seen,
                   std::vector<uint32_t> &out, std::vector<uint8_t> &dedup,
                   std::vector<Ref> *out_refs = nullptr) {
  out.clear();
  if (out_refs)
    out_refs->clear();
  binary_partners(k, l, limit, partner_scratch, seen);
  for (const Lit p : partner_scratch) {
    out.push_back(pool.intern_unit(p));
    if (out_refs)
      out_refs->push_back(INVALID_REF); // binary: id via kernel_binary_id
  }
  // The index is a snapshot sized at apply entry, while `live_lits` and
  // `k.nvars` grow during a composite pass. A literal past it has no indexed
  // large clauses, only this pass's dividers/quotients, which are not grid
  // material — and without the bound the widened path reads past `occ`.
  if (!idx.built || k.factor_size < 3 || l >= idx.occ.size())
    return;
  for (const Ref ref : idx.occ[l]) {
    const Clause *c = clause_at(k.arena, ref);
    if (!grid_clause_live(k, c))
      continue;
    charge_factor_ticks(k, c->size);
    if (!remainder_of(k, c, l, limit, rem_scratch))
      continue;
    const uint32_t rid =
        pool.intern(rem_scratch.data(), static_cast<uint32_t>(rem_scratch.size()));
    if (dedup.size() <= rid)
      dedup.resize(rid + 1, 0);
    if (dedup[rid])
      continue;
    dedup[rid] = 1;
    out.push_back(rid);
    k.stats.factor_large_remainders++;
    if (out_refs)
      out_refs->push_back(ref);
  }
  for (const uint32_t rid : out) // restore the dedup marks set above
    if (rid < dedup.size())
      dedup[rid] = 0;
}

// Seed/tie-break score — kissat's `watches_score` (factor.c:310), the raw
// watch-list size. Kissat reads it in dense mode, so there it counts distinct
// binary neighbours; here duplicates count too. The faithful port is measured
// worse (`docs/design/s2a-factor-matcher-design.md`).
uint32_t watch_score(const Kernel &k, Lit l) { return k.watches.heads[l].size; }

bool contains(const std::vector<Lit> &v, Lit l) {
  for (const Lit x : v)
    if (x == l)
      return true;
  return false;
}

// The grid's clause ids/Refs, resolved once per factoring. Invariant: the grid
// is enumerated per factor, never per (factor, remainder) pair — the latter
// costs O(occ(f))·m·n·2, since the hint and the delete each need the clause
// (`docs/design/e2-factor-design.md`).
struct GridMap {
  // per factor index: remainder id -> Ref (INVALID_REF = binary)
  std::vector<std::unordered_map<uint32_t, Ref>> by_factor;
};

void build_grid_map(Kernel &k, const FactorIndex &idx, const std::vector<Lit> &F,
                    RemPool &pool, Lit limit, std::vector<Lit> &partner_scratch,
                    std::vector<Lit> &rem_scratch, std::vector<uint8_t> &seen,
                    std::vector<uint8_t> &dedup, GridMap &gm) {
  gm.by_factor.assign(F.size(), {});
  std::vector<uint32_t> ids;
  std::vector<Ref> refs;
  for (size_t i = 0; i < F.size(); i++) {
    remainders_of(k, idx, pool, F[i], limit, partner_scratch, rem_scratch, seen,
                  ids, dedup, &refs);
    for (size_t j = 0; j < ids.size(); j++)
      gm.by_factor[i].emplace(ids[j], refs[j]);
  }
}

void apply_factoring(Kernel &k, const std::vector<Lit> &F,
                     const std::vector<uint32_t> &R, const RemPool &pool,
                     const GridMap &gm) {
  const Var xv = kernel_new_extension_vars(k, 1);
  const Lit px = make_lit(xv, false); // x
  const Lit nx = make_lit(xv, true);  // ¬x
  const bool lrat = k.proof && k.proof->lrat();

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
    divider_ids.push_back(id);
  }

  // Quotients (¬x ∨ rⱼ): RAT on ¬x. The only clauses containing x are the m
  // dividers, so there is one RAT group per divider, each resolvent (fᵢ∨rⱼ) a
  // grid clause still live because the deletes come last. Hints are the flat
  // stream [-dividerᵢ, orig(fᵢ,rⱼ)]ᵢ, read before deletion.
  std::vector<int64_t> hints;
  std::vector<Lit> qlits;
  for (const uint32_t rid : R) {
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
        const auto it = gm.by_factor[i].find(rid);
        const Ref gref = (it == gm.by_factor[i].end()) ? INVALID_REF : it->second;
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
    if (rn == 1)
      kernel_add_binary_clause(k, nx, rl[0], id);
    else
      kernel_add_large_clause(k, qlits.data(),
                              static_cast<uint32_t>(qlits.size()),
                              /*redundant=*/false, /*glue=*/0, id);
  }

  // Delete the originals last (proof `del`), all copies: a duplicated grid
  // binary is fully replaced by the aux-var definition, and plain
  // `kernel_delete_binary` refuses copies > 1.
  for (size_t i = 0; i < F.size(); i++)
    for (const uint32_t rid : R) {
      if (pool.size_of(rid) == 1) {
        kernel_delete_binary_all_copies(k, F[i], pool.lit0(rid));
        continue;
      }
      const auto it = gm.by_factor[i].find(rid);
      if (it == gm.by_factor[i].end() || it->second == INVALID_REF)
        continue; // already gone; the definition still subsumes it
      Clause *c = clause_at(k.arena, it->second);
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
  const Lit orig_lits = static_cast<Lit>(2 * kernel_num_output_vars(k));

  // Fresh-variable boundary for the post-application demotion: every var this
  // application adds lands in [fresh_first, k.nvars), contiguously, so the
  // O(#queue) repair is paid once per application and threads no list.
  const Var fresh_first = k.nvars;

  // Composite grids (`docs/design/s2b-composite-grids-design.md`):
  // `k.factor_composite` widens the factor/remainder universe to all live
  // literals, growing as the pass applies (kissat resize_factoring).
  // Termination needs no var-class exclusion — every accepted grid has
  // delta >= 1, strictly decreasing the finite live-binary count. Default off:
  // composition over-applies in a preprocessing blast, and belongs to the
  // interleaved during-search doses where DefaultPolicy sets it.
  const bool composite = k.factor_composite;
  Lit live_lits = composite ? static_cast<Lit>(2 * k.nvars) : orig_lits;

  // R-side scratch is indexed by remainder id, not literal; `inR` grows with
  // the pool, which is bounded by material actually seen rather than 2*nvars.
  RemPool pool;
  // One arena pass per apply, only when the widened scope is enabled: at
  // `factor_size == 2` the index is never built and the binary path is
  // untouched, which is what the bit-identity gate checks.
  FactorIndex fidx;
  if (k.factor_size >= 3)
    build_factor_index(k, fidx, k.factor_size);
  std::vector<uint8_t> rem_dedup;               // remainders_of dedup, by id
  std::vector<Lit> rem_scratch;                 // one remainder, sorted
  std::vector<uint8_t> inR;                     // R membership, by remainder id
  const auto mark_R = [&](uint32_t rid, uint8_t v) {
    if (inR.size() <= rid)
      inR.resize(rid + 1, 0);
    inR[rid] = v;
  };
  const auto in_R = [&](uint32_t rid) {
    return rid < inR.size() && inR[rid];
  };
  std::vector<uint8_t> seen(live_lits, 0);      // binary_partners dedup scratch
  std::vector<uint32_t> cofactor(live_lits, 0); // co-occurrence counts
  std::vector<Lit> touched, partners, partner_lits, F, best_F;
  std::vector<uint32_t> R, newR, best_R, partner_rids;
  GridMap gmap;
  uint64_t applied = 0;

  // Seed schedule: a max-heap keyed by occurrence count, re-scored on every
  // literal an applied factoring touches (kissat `update_factored`,
  // factor.c:800), each literal still seeded at most once per pass. A static
  // literal-index scan instead lets an early low-degree seed spend binaries a
  // high-degree seed needed. The scoring/accept scalars are the policy's when a
  // `Kernel::factor_selection` is attached; the mechanism stays here. Watch sizes
  // ≤ 2^24 keep the double→uint32_t round-trip order-exact
  // (`docs/design/s2a-factor-matcher-design.md`).
  FactorSelection *const fm = k.factor_selection;
  if (fm)
    fm->begin_sweep(k);
  const auto score_of = [&](Lit l) -> uint32_t {
    charge_factor_ticks(k, k.watches.heads[l].size); // heap (re-)score
    return fm ? static_cast<uint32_t>(fm->seed_score(k, l))
              : watch_score(k, l);
  };
  // `k.work_budget`: stop the round after this many factor_ticks, 0 = unbounded.
  // Two quit points, mirroring kissat's (factor.c:442, :914).
  const uint64_t tick_start = k.stats.factor_ticks;
  const uint64_t budget = k.work_budget;
  const auto over_budget = [&]() {
    return budget && k.stats.factor_ticks - tick_start > budget;
  };
  std::priority_queue<std::pair<uint32_t, Lit>> pq;
  std::vector<uint8_t> tried(live_lits, 0);
  // Per-literal seed marks over the touched-epoch substrate; kissat keeps the
  // same as `flags->factor` (inline.h:60-66, read at factor.c:922). Without it
  // the schedule rebuilds the whole heap every round, charging factor_ticks per
  // watch list for contents that cannot have changed. Coarser than kissat on
  // polarity, the stamps being per variable. N1: it narrows which seeds the
  // round tries, exactly as the budget does, and the canonical apply still runs.
  // Pinned by tests/test_factor.cpp::test_touch_filter_is_policy_only.
  const bool touch_filter = k.touch_filter;
  const uint64_t touch_snapshot = k.touch_epoch;
  const uint64_t touch_wm = k.factor_touch_mark;
  const auto seed_is_new = [&](Lit l) {
    return !touch_filter || kernel_touched_since(k, var_of(l), touch_wm);
  };
  for (Lit l = 0; l < live_lits; l++) {
    if (k.value(l) != 0)
      continue;
    if (!seed_is_new(l)) {
      k.stats.factor_mark_skipped++;
      continue;
    }
    pq.push({score_of(l), l});
  }

  while (applied < FACTOR_MAX_PER_APPLY && !pq.empty()) {
    if (over_budget()) // quit point 1: before each popped seed
      break;
    const std::pair<uint32_t, Lit> top = pq.top();
    pq.pop();
    const Lit seed = top.second;
    if (tried[seed] || k.value(seed) != 0)
      continue;
    const uint32_t now = score_of(seed);
    if (now != top.first) { // stale key — re-insert at its current score
      pq.push({now, seed});
      continue;
    }
    tried[seed] = 1;
    remainders_of(k, fidx, pool, seed, live_lits, partner_lits, rem_scratch,
                  seen, R, rem_dedup);
    if (R.size() < 2 || R.size() > FACTOR_DIM_LIMIT)
      continue;
    for (const uint32_t rid : R)
      mark_R(rid, 1);
    F.assign(1, seed);
    int best_delta = 0;
    best_F.clear();
    best_R.clear();

    // greedily grow the factor set, tracking the max-delta prefix.
    while (F.size() < FACTOR_DIM_LIMIT && R.size() >= 2 && !over_budget()) {
      // count candidate next-factors by co-occurrence across the current R.
      touched.clear();
      for (const uint32_t rid : R) {
        // The next-factor candidates for remainder r are the literals h with
        // {h} ∪ r a live clause: for |r| == 1 exactly r's binary partners, and
        // above that the size-|r|+1 clauses in an occurrence list of r.
        if (pool.size_of(rid) == 1) {
          binary_partners(k, pool.lit0(rid), live_lits, partners, seen);
        } else {
          partners.clear();
          const uint32_t rn = pool.size_of(rid);
          const Lit *rl = pool.lits_of(rid);
          // Scan the rarest literal of the remainder: every clause containing
          // r contains all of them, so any one works, and the cheapest is
          // cadical's `min(|watches|)` direction search (transred.cpp:116-120).
          Lit scan = INVALID_LIT;
          size_t scan_occ = SIZE_MAX;
          for (uint32_t i = 0; i < rn; i++) {
            if (rl[i] >= fidx.occ.size())
              continue;
            if (fidx.occ[rl[i]].size() < scan_occ) {
              scan_occ = fidx.occ[rl[i]].size();
              scan = rl[i];
            }
          }
          if (scan == INVALID_LIT)
            continue; // index snapshot bound (see remainders_of)
          for (const Ref ref : fidx.occ[scan]) {
            const Clause *c = clause_at(k.arena, ref);
            if (c->size != rn + 1 || !grid_clause_live(k, c))
              continue;
            charge_factor_ticks(k, c->size);
            Lit extra = INVALID_LIT;
            bool ok = true;
            for (const Lit x : *c) {
              if (std::binary_search(rl, rl + rn, x))
                continue;
              if (extra != INVALID_LIT) { ok = false; break; }
              extra = x;
            }
            if (ok && extra != INVALID_LIT && extra < live_lits &&
                k.value(extra) == 0)
              partners.push_back(extra);
          }
        }
        for (const Lit h : partners) {
          if (var_of(h) == var_of(seed) || contains(F, h))
            continue;
          if (cofactor[h] == 0)
            touched.push_back(h);
          cofactor[h]++;
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
        const uint32_t score =
            fm ? static_cast<uint32_t>(fm->next_factor_score(k, h, c, seed))
               : score_of(h);
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
      // newR = R ∩ partners(g)  (the r in R with (g∨r) present)
      remainders_of(k, fidx, pool, g, live_lits, partner_lits, rem_scratch,
                    seen, partner_rids, rem_dedup);
      newR.clear();
      for (const uint32_t rid : partner_rids)
        if (in_R(rid))
          newR.push_back(rid);
      const uint32_t m = static_cast<uint32_t>(F.size()) + 1;
      const uint32_t n = static_cast<uint32_t>(newR.size());
      const int delta = static_cast<int>(m * n) - static_cast<int>(m + n);
      if (delta > best_delta) {
        best_delta = delta;
        best_F = F;
        best_F.push_back(g);
        best_R = newR;
      }
      // commit g and shrink R to newR to keep growing.
      F.push_back(g);
      for (const uint32_t rid : R)
        mark_R(rid, 0);
      for (const uint32_t rid : newR)
        mark_R(rid, 1);
      R.swap(newR);
    }
    for (const uint32_t rid : R) // unmark whatever R ended as
      mark_R(rid, 0);

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
      if (std::getenv("TACTIX_GRID_TRACE")) // diagnostic: grid dimensions
        std::fprintf(stderr, "[grid] %zux%zu delta=%d\n", best_F.size(),
                     best_R.size(), best_delta);
      build_grid_map(k, fidx, best_F, pool, live_lits, partner_lits,
                     rem_scratch, seen, rem_dedup, gmap);
      apply_factoring(k, best_F, best_R, pool, gmap);
      applied++;
      k.stats.factorings++;
      // Grow the pass universe over the fresh var (kissat resize_factoring) and
      // seed both polarities: its divider/quotient binaries are live, so later
      // chains this pass can compose over it. `tried` still caps each literal at
      // one seed attempt.
      if (composite && static_cast<Lit>(2 * k.nvars) > live_lits) {
        live_lits = static_cast<Lit>(2 * k.nvars);
        seen.resize(live_lits, 0); // inR is remainder-id indexed, grows in mark_R
        cofactor.resize(live_lits, 0);
        tried.resize(live_lits, 0);
        pq.push({score_of(live_lits - 2), static_cast<Lit>(live_lits - 2)});
        pq.push({score_of(live_lits - 1), static_cast<Lit>(live_lits - 1)});
      }
      // Re-score the literals whose binary degree just dropped, so a seed
      // stripped of its grid stops outranking an intact one (kissat
      // `update_factored`, factor.c:800). Untried literals only.
      for (const Lit f : best_F)
        if (!tried[f])
          pq.push({score_of(f), f});
      // best_R holds remainder ids, not literals — re-seed the literals inside
      // each remainder, and never index `tried` with an id: that is an
      // out-of-bounds a bit-identity check cannot see, ids staying small at
      // `factor_size == 2`.
      for (const uint32_t rid : best_R) {
        const Lit *rl = pool.lits_of(rid);
        for (uint32_t i = 0; i < pool.size_of(rid); i++)
          if (rl[i] < live_lits && !tried[rl[i]])
            pq.push({score_of(rl[i]), rl[i]});
      }
    }
  }

  // Resume: three early exits leave seeds unpopped, so re-arming the tail stops
  // the next round replaying the same head. Collected first and touched
  // afterwards, since touching inside the scan would move the very stamps the
  // `touch_wm` test reads.
  {
    std::vector<Var> rearm;
    for (Lit l = 0; touch_filter && l < live_lits; l++)
      if (!tried[l] && k.value(l) == 0 && seed_is_new(l))
        rearm.push_back(var_of(l));
    for (const Var v : rearm)
      kernel_touch_var(k, v);
    k.stats.factor_rearmed += rearm.size();
  }
  if (touch_filter)
    k.factor_touch_mark = touch_snapshot;

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

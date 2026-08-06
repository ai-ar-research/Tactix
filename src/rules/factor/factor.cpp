#include "rules/factor/factor.hpp"

#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp" // heuristics_demote_fresh_vars (fresh vars)
#include "policies/subpolicy.hpp"  // FactorMatchPolicy (S2a grid selection)
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

// Factor work currency (kissat factor.c:274 etc., utilities.h
// kissat_cache_lines): 1 + one tick per 128 bytes touched. Our watch entries
// are uint32_t words (binary = 1 word, large = 2), so a walk over `words`
// entries costs 1 + (4·words)/128. Approximate — kissat meters DENSE deduped
// watches, ours keep duplicates — but mp1's native spend (0.94 M of a 700 M
// allowance) leaves wide margin (design §5.4).
inline void charge_factor_ticks(Kernel &k, uint32_t words) {
  k.stats.factor_ticks += 1 + (words >> 5);
}

// DISTINCT binary partners of `l`: each `p` with a live binary (l ∨ p),
// all-unassigned and below the extension boundary `limit`. Deduplicated via the
// caller's `seen` marker (sized `limit`) — duplicate binaries in the watch list
// would otherwise inflate the grid dimensions and the gain, and produce repeated
// (undeletable) grid pairs. Each distinct partner is returned once.
void binary_partners(Kernel &k, Lit l, Lit limit, std::vector<Lit> &out,
                     std::vector<uint8_t> &seen) {
  out.clear();
  charge_factor_ticks(k, k.watches.heads[l].size);
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
  const uint32_t sz = k.watches.heads[l].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      const Lit p = w.other();
      if (p < limit && k.value(p) == 0 && !seen[p]) {
        seen[p] = 1;
        out.push_back(p);
      }
      i += 1;
    } else {
      i += 2;
    }
  }
  for (const Lit p : out) // clear the marker for reuse
    seen[p] = 0;
}

// --- remainder interning (R5, docs/design/e2-factorsize-design.md §3) --------
// A REMAINDER is `C \ {f}` for a grid clause C. Today every remainder is a
// single literal (the binary variant); raising Kernel::factor_size makes them
// sub-clauses, and the set operations the grid search runs on R — membership,
// intersection, dedup — then need identity, which a sub-clause does not have.
// So remainders are interned into dense ids for the duration of ONE apply.
//
// INCREMENT 1 (this commit) introduces the abstraction ONLY: `intern_unit` is
// the sole entry point and every remainder is still one literal, so the search
// is unchanged and `factor_size == 2` must be BIT-IDENTICAL — that is the gate,
// not a nicety (design §5). Increment 2 adds the multi-literal intern (hash over
// the sorted literals) and the occurrence scan that produces them.
//
// Scratch discipline: `unit_id` is indexed by literal and grows on demand
// rather than being fill-initialized per call. An O(nvars) fill-init in a hot
// per-call path is a recurring performance hazard in this codebase — memset
// ends up dominating the profile.
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
  // Multi-literal intern (R5 increment 2): `b[0..n)` must be SORTED ascending —
  // the caller builds it from a clause minus one literal, so the literals are
  // already distinct. Identity is by content, which is what makes `inR`,
  // intersection and dedup work over sub-clauses at all.
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

// --- the clause index for factor_size >= 3 (R5 increment 2b) -----------------
// Occurrence lists over IRREDUNDANT clauses with 3 <= size <= factor_size. Built
// with ONE arena pass per apply, the same N1-safe shape BCE uses (bce.cpp
// build_index) rather than depending on `Kernel::dense`, which is null outside
// an inprocessing phase — a rule must work standalone through the public API.
//
// Binaries are deliberately NOT indexed: they are already O(1)-enumerable from
// the watch lists, and routing them through here would change the enumeration
// ORDER that factor_size == 2 bit-identity depends on.
//
// Redundant clauses are excluded, as in kissat. A redundant grid member could be
// deleted by S2 Reduce between selection and application, which is a correctness
// hazard rather than a tuning choice (design §6).
struct FactorIndex {
  std::vector<std::vector<Ref>> occ; // by literal
  bool built = false;
};

// Count binary clauses per literal — the `bincount` half of kissat's candidate
// reduction (factor.c:1019). Binaries are never pruned (they are the grid's
// other side); they only contribute to whether a LARGE clause survives.
static void count_binaries(const Kernel &k, std::vector<uint32_t> &bincount) {
  bincount.assign(2 * k.nvars, 0);
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++) {
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
    const uint32_t sz = k.watches.heads[l].size;
    uint32_t n = 0;
    for (uint32_t i = 0; i < sz;) {
      if (Watch{ws[i]}.binary()) {
        n++;
        i += 1;
      } else {
        i += 2;
      }
    }
    bincount[l] = n;
  }
}

void build_factor_index(const Kernel &k, FactorIndex &idx, uint32_t max_size) {
  idx.occ.assign(2 * k.nvars, {});
  // --- candidate reduction (kissat `factorcandrounds`, factor.c:1000; cadical
  // `--factorcandrounds` default 2) -----------------------------------------
  // A large clause is grid material only if EVERY one of its literals occurs in
  // at least two candidate clauses — a literal seen once can never be a grid
  // column. Dropping such a clause can drop its neighbours below the bar too,
  // so the prune is iterated to fixpoint (bounded by k.factor_cand_rounds).
  //
  // This is a STRUCTURAL quality filter, not a per-instance separator — every
  // hand-designed admission gate that has been measured against NO gate at all
  // has lost. It asks the incidence structure whether repeated structure
  // exists, and answers "no" on random-ish formulas by itself, which is where
  // we over-apply factorings relative to cadical; on genuinely structured
  // instances it keeps the grids intact.
  //
  // `factor_cand_rounds = 0` restores the unfiltered candidate set exactly
  // (the loop body never runs and `alive` stays all-ones).
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

// Is clause `c` live and usable as a grid member? (root-satisfied clauses are
// excluded: a grid whose member is already true is not a real m×n grid)
bool grid_clause_live(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant())
    return false;
  for (const Lit l : *c)
    if (k.value(l) > 0)
      return false;
  return true;
}

// The remainder `C \ {l}`, written SORTED into `out`. Returns false if any other
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
// order — the factor_size == 2 path, unchanged) followed by, when factor_size
// >= 3, one remainder per indexed clause containing `l`. Dedup is by interned
// id, so a repeated remainder is offered once.
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
  // BOUNDS: the index is a SNAPSHOT sized at apply entry, while `live_lits` and
  // `k.nvars` GROW during a composite pass as Factor adds extension variables.
  // A literal past the snapshot simply has no indexed large clauses — its only
  // clauses are the dividers/quotients this pass just created, which are not
  // grid material. Without this bound the widened path reads past occ[].
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
  for (const uint32_t rid : out) // restore the dedup marks we set
    if (rid < dedup.size())
      dedup[rid] = 0;
}

// The grid clause {f} ∪ r, if it is live. Returns its Ref, or INVALID_REF for a
// binary (whose id comes from kernel_binary_id) — and sets `found` false when the
// pair is not a grid member at all. Both the LRAT hint and the delete need it.
Ref find_grid_clause(Kernel &k, const FactorIndex &idx, Lit f,
                     const RemPool &pool, uint32_t rid, bool *found) {
  *found = false;
  const uint32_t n = pool.size_of(rid);
  if (n == 1) {
    *found = true;
    return INVALID_REF; // binary (f ∨ r) — the caller uses kernel_binary_id
  }
  if (!idx.built || f >= idx.occ.size())
    return INVALID_REF; // see the bounds note in remainders_of
  const Lit *r = pool.lits_of(rid);
  for (const Ref ref : idx.occ[f]) {
    const Clause *c = clause_at(k.arena, ref);
    charge_factor_ticks(k, c->size); // the budget must be able to stop this
    if (c->garbage() || c->size != n + 1)
      continue;
    // {f} ∪ r as a set: every literal of c is f or in the sorted remainder
    bool ok = true;
    for (const Lit x : *c) {
      if (x == f)
        continue;
      if (!std::binary_search(r, r + n, x)) {
        ok = false;
        break;
      }
    }
    if (ok) {
      *found = true;
      return ref;
    }
  }
  return INVALID_REF;
}

// Seed/tie-break score — kissat's `watches_score` (factor.c:310), the raw
// watch-list size. Kissat reads it in DENSE mode, where large watches are
// flushed and duplicate binaries physically deleted, so for it this is the
// count of DISTINCT binary neighbours; ours counts duplicate copies and spends
// two words per large watch. Scoring distinct partners instead (the faithful
// port) was measured and is WORSE where it matters: on mp1 it gives 3.35M vs
// 2.50M conflicts on the factored formula, against 4.44M for no scoring at
// all. It wins only marginally on x9 (347k vs 364k, inside SAT noise). Keep
// the raw count until there is evidence the faithful score pays.
uint32_t watch_score(const Kernel &k, Lit l) { return k.watches.heads[l].size; }

bool contains(const std::vector<Lit> &v, Lit l) {
  for (const Lit x : v)
    if (x == l)
      return true;
  return false;
}

// Apply the factoring F × R (all (f∨r) present). Adds a fresh var x, the dividers
// (x∨f), the quotients (¬x∨r), then deletes the grid — in that proof order so the
// quotients' RAT-on-¬x resolves against the dividers to the still-live originals.
// The grid's clause ids/Refs, resolved ONCE per factoring.
//
// Cost fix (R5 increment 2c): the first cut called find_grid_clause per lookup,
// i.e. O(occ(f)) × m × n × 2 — once per LRAT hint and once per delete — which is
// why size 5 ran past its timeout on every target instance. Enumerating each
// factor's remainders once is O(occ(f)) × m, an n-fold reduction, and it reuses
// the enumeration the search already knows how to do.
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

void apply_factoring(Kernel &k, const FactorIndex &idx,
                     const std::vector<Lit> &F,
                     const std::vector<uint32_t> &R, const RemPool &pool,
                     const GridMap &gm) {
  const Var xv = kernel_new_extension_vars(k, 1);
  const Lit px = make_lit(xv, false); // x
  const Lit nx = make_lit(xv, true);  // ¬x
  const bool lrat = k.proof && k.proof->lrat();

  // Dividers (x ∨ fᵢ): RAT on the fresh literal x. At add time no clause contains
  // ¬x (the quotients come after), so the RAT condition is vacuous — pivot-first
  // + empty hints. add_rat's DRAT path is byte-identical to the old add_rup.
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

  // Quotients (¬x ∨ rⱼ): RAT on ¬x. The only clauses containing x (= ¬pivot) are
  // the m dividers, so there is one RAT group per divider: the resolvent
  // (¬x∨rⱼ) ⊗ₓ (x∨fᵢ) = (fᵢ∨rⱼ) is an original grid clause, still live (deletes
  // come last), hence RUP by that original's id. Hints are the flat signed stream
  // [-dividerᵢ, orig(fᵢ,rⱼ)]ᵢ. kernel_binary_id must be read before deletion.
  std::vector<int64_t> hints;
  std::vector<Lit> qlits;
  for (const uint32_t rid : R) {
    const uint32_t rn = pool.size_of(rid);
    const Lit *rl = pool.lits_of(rid);
    // Quotient {¬x} ∪ r, PIVOT FIRST. The RAT-on-¬x argument is indifferent to
    // |r| (design §4): the only clauses containing x are the m dividers, and
    // (¬x ∨ r) ⊗ₓ (x ∨ fᵢ) = {fᵢ} ∪ r, an original grid clause still live
    // because the deletes come last. So the hint stream stays per-divider —
    // only the literal vector and the grid clause's id lookup widen.
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

  // Delete the originals LAST (proof `del`); ALL copies — a duplicated grid
  // binary is fully replaced by the aux-var definition, and the plain
  // kernel_delete_binary refuses copies>1 (see kernel.cpp).
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
      if (k.proof)
        k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
      kernel_mark_garbage(k, c);
    }
}

// DEBUG (S2 diagnosis): dump the current clause DB as DIMACS and exit, to feed
// our factored formula to another solver (separates "are our grids good" from
// "does our engine leverage the aux vars"). Env-gated; no cost when unset.
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
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
    const uint32_t sz = k.watches.heads[l].size;
    for (uint32_t i = 0; i < sz;) {
      const Watch w{ws[i]};
      if (w.binary()) { if (l < w.other()) cls.push_back({dl(l), dl(w.other())}); i += 1; }
      else i += 2;
    }
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
  // Native LRAT: dividers/quotients emit add-RAT with explicit resolution hints
  // (apply_factoring) — Factor is the first RAT-emitting rule, so the LRAT path
  // gets the full transform now (was a NoOp before native add_rat landed).
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;

  // ADMISSION IS THE POLICY'S, NOT THE RULE'S. A binary-density selectivity
  // heuristic must not live here as a NoOp: that is an N1 BREACH — the guard
  // reports applicable, `factor_instances` offers its canonical instance, and
  // apply does nothing on any binary-sparse formula, so no API caller can apply
  // a legal factoring at all. The N1 carve-out covers the built-in POLICY
  // declining, never the RULE refusing everyone. The predicate lives in
  // DefaultPolicy's slot (beside BVE's identical one) and arrives as
  // `Kernel::factor_admit`, which DEFAULTS TRUE, so an API caller that never
  // sets it gets the full capability. `TACTIX_FACTOR_FORCE` overrides, for
  // fuzz on tiny formulas.
  if (!k.factor_admit && !std::getenv("TACTIX_FACTOR_FORCE"))
    return ApplyResult::NoOp;

  // Anti-loop boundary: only ORIGINAL literals may be factors/remainders — never
  // the extension vars Factor itself adds. This must be the true original count
  // (first_extension_var), fixed across applies; using 2*k.nvars would, on a
  // second apply, treat the first apply's extension vars as original and factor
  // over them → unbounded self-factoring.
  const Lit orig_lits = static_cast<Lit>(2 * kernel_num_output_vars(k));

  // Fresh-variable boundary for the post-application demotion (see
  // Kernel::factor_demote_fresh). Every var this application adds lands in
  // [fresh_first, k.nvars) — contiguous, so no list has to be threaded through
  // the grid loop, and the O(#queue) repair is paid ONCE per application
  // rather than once per factoring.
  const Var fresh_first = k.nvars;

  // Composite grids (docs/design/s2b-composite-grids-design.md):
  // k.factor_composite widens the factor/remainder universe to ALL live
  // literals — prior-pass extension vars and this pass's own fresh vars
  // included — growing as the pass applies (kissat resize_factoring).
  // Termination needs no var-class exclusion: every accepted grid has
  // delta >= 1, strictly decreasing the finite live-binary count (a grid over
  // a var's own dividers has m = 1, never accepted). DEFAULT OFF because in a
  // single preprocessing blast composition over-applies — it roughly doubles
  // the grids and regresses the downstream-conflict proxy. It belongs to the
  // small interleaved during-search doses, where kissat's own cross-round
  // composition lives; DefaultPolicy sets it there. `k.factor_composite` is
  // the only switch.
  const bool composite = k.factor_composite;
  Lit live_lits = composite ? static_cast<Lit>(2 * k.nvars) : orig_lits;

  // R-side scratch is indexed by REMAINDER ID, not literal. `inR` grows with
  // the pool rather than being sized up front, since the pool is bounded by
  // material actually seen, not by 2*nvars.
  RemPool pool;
  // One arena pass per apply, only when the widened scope is actually enabled —
  // at factor_size == 2 the index is never built and the binary path is
  // untouched (which is what the bit-identity gate checks).
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

  // Seed schedule. Kissat pops seed candidates from a max-heap keyed by
  // occurrence count and re-scores every literal touched by an applied
  // factoring (`update_factored`, factor.c:800), so the order adapts as the
  // binary graph is consumed; each literal is still seeded at most once per
  // pass (the `f->factor` bit, factor.c:922). A static literal-index scan
  // instead lets an early low-degree seed spend binaries a high-degree seed
  // needed — near-harmless on a sparse graph, decisive on a dense one.
  // Measured on mp1 as conflicts kissat needs on our factored formula:
  // 4.44M unordered -> 3.81M with this heap -> 2.50M once the tie-break below
  // joins it (kissat on its own factoring: 305k).
  // S2a: the scoring/accept SCALARS are the policy's (Kernel::factor_match,
  // a FactorMatchPolicy) when one is attached; the null path is the built-in
  // watch-size scoring, byte-identical to pre-S2a. The MECHANISM (heap,
  // co-occurrence, greedy grow, best-prefix) stays here; watch sizes ≤ 2^24
  // keep the double→uint32_t round-trip order-exact (no tie flips).
  FactorMatchPolicy *const fm = k.factor_match;
  if (fm)
    fm->begin_sweep(k);
  const auto score_of = [&](Lit l) -> uint32_t {
    charge_factor_ticks(k, k.watches.heads[l].size); // heap (re-)score
    return fm ? static_cast<uint32_t>(fm->seed_score(k, l))
              : watch_score(k, l);
  };
  // k.work_budget (kernel.hpp): stop the round once it has spent this many
  // factor_ticks; 0 = unbounded (legacy path). Two quit points, mirroring
  // kissat's (factor.c:442 mid-count, :914 before each popped seed).
  const uint64_t tick_start = k.stats.factor_ticks;
  const uint64_t budget = k.work_budget;
  const auto over_budget = [&]() {
    return budget && k.stats.factor_ticks - tick_start > budget;
  };
  std::priority_queue<std::pair<uint32_t, Lit>> pq;
  std::vector<uint8_t> tried(live_lits, 0);
  // SCHED-F4 · per-literal seed marks over the touched-epoch substrate. kissat
  // keeps exactly this as `flags->factor`, a TWO-BIT per-variable field set per
  // POLARITY by kissat_mark_added_literal (inline.h:60-66) and consulted when a
  // seed is popped (factor.c:922). Without it the seed schedule is a full
  // O(live literals) pass EVERY round, and it is not a cheap one: `score_of`
  // charges factor_ticks proportional to the literal's watch list, so the
  // round pays a fraction of its own budget just to rebuild a heap whose
  // contents mostly cannot have changed. A seed with no new binary and no new
  // small clause re-derives the same grid and the same delta.
  //
  // Coarser than kissat on polarity — our stamps are per VARIABLE, so both
  // polarities are reconsidered when either changes. Conservative.
  // N1: `factor_instances` offers only INSTANCE_CANONICAL, and the canonical
  // apply still runs; this narrows which SEEDS the round tries, exactly as the
  // budget already does. The rule never reports applicable and then refuses —
  // the distinction SCHED-N1FIX turned on. Pinned by
  // tests/test_factor.cpp::test_touch_filter_is_policy_only.
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
        // {h} ∪ r a live clause. For |r| == 1 that is exactly the binary
        // partners of r (the unchanged path). For |r| > 1 scan the occurrences
        // of r's FIRST literal — every clause containing r contains it — and
        // keep those of size |r|+1 whose extra literal is the candidate.
        if (pool.size_of(rid) == 1) {
          binary_partners(k, pool.lit0(rid), live_lits, partners, seen);
        } else {
          partners.clear();
          const uint32_t rn = pool.size_of(rid);
          const Lit *rl = pool.lits_of(rid);
          // Scan the RAREST literal of the remainder, not the first: every
          // clause containing r contains all of them, so any one works, and the
          // cheapest is cadical's `min(|watches|)` direction search
          // (transred.cpp:116-120 idiom; deviation 4 in the header noted we did
          // not do this, and at |r| > 1 it is the difference between scanning a
          // rare literal's list and a hub's).
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
      // Pick the highest co-occurrence count, breaking ties by watch-list size
      // (kissat's `tied_next_factor_score`, factor.c:318). Taking the first max
      // in literal order instead is arbitrary, and on a dense binary graph the
      // max count is tied among many candidates — so the tie-break, not the
      // count, is what actually selects the grid.
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

    // Acceptance: an attached FactorMatchPolicy keeps precedence (the hook
    // subsumes the scalar); otherwise k.factor_bound (kissat's
    // `reduction > bound`, factor.c:939) — default 0 ≡ the legacy
    // delta ≥ FACTOR_MIN_GAIN = 1. The structural delta ≥ 1 floor stays
    // unconditional (termination is delta-monotone; an empty best_F must
    // never reach apply_factoring).
    if (best_delta >= FACTOR_MIN_GAIN &&
        (fm ? fm->accept(static_cast<uint32_t>(best_F.size()),
                         static_cast<uint32_t>(best_R.size()), best_delta)
            : best_delta > static_cast<int>(k.factor_bound))) {
      if (std::getenv("TACTIX_GRID_TRACE")) // S2a Inc 3a: grid dims surface
        std::fprintf(stderr, "[grid] %zux%zu delta=%d\n", best_F.size(),
                     best_R.size(), best_delta);
      build_grid_map(k, fidx, best_F, pool, live_lits, partner_lits,
                     rem_scratch, seen, rem_dedup, gmap);
      apply_factoring(k, fidx, best_F, best_R, pool, gmap);
      applied++;
      k.stats.factorings++;
      // S2b: grow the pass universe over the fresh var (kissat
      // resize_factoring) and seed BOTH its polarities — its divider/quotient
      // binaries are live, so later chains THIS pass can compose over it
      // (`tried` still caps every literal at one seed attempt; termination is
      // delta-monotone regardless, design §4).
      if (composite && static_cast<Lit>(2 * k.nvars) > live_lits) {
        live_lits = static_cast<Lit>(2 * k.nvars);
        seen.resize(live_lits, 0); // inR is remainder-id indexed, grows in mark_R
        cofactor.resize(live_lits, 0);
        tried.resize(live_lits, 0);
        pq.push({score_of(live_lits - 2), static_cast<Lit>(live_lits - 2)});
        pq.push({score_of(live_lits - 1), static_cast<Lit>(live_lits - 1)});
      }
      // Re-score the literals whose binary degree just dropped, so a seed that
      // has since been stripped of its grid stops outranking an intact one
      // (kissat's `update_factored`, factor.c:800). Untried literals only —
      // `tried` still caps each seed at one attempt per pass.
      for (const Lit f : best_F)
        if (!tried[f])
          pq.push({score_of(f), f});
      // best_R holds REMAINDER IDS, not literals — re-seed the literals INSIDE
      // each remainder. Do NOT index `tried` with an id: that is an
      // out-of-bounds a bit-identity check cannot see, because ids stay small
      // at factor_size == 2.
      for (const uint32_t rid : best_R) {
        const Lit *rl = pool.lits_of(rid);
        for (uint32_t i = 0; i < pool.size_of(rid); i++)
          if (rl[i] < live_lits && !tried[rl[i]])
            pq.push({score_of(rl[i]), rl[i]});
      }
    }
  }

  // SCHED-F4 · RESUME (SCHED-F2's mechanism). This round has three early exits
  // that leave seeds unpopped — the tick budget, `FACTOR_MAX_PER_APPLY`, and
  // the mid-grow budget poll — and all three are REACHABLE under DefaultPolicy,
  // so re-arming the tail is what stops the next round from replaying the same
  // head. Collected first and touched afterwards: touching inside the scan
  // would move the very stamps the `touch_wm` test reads, and re-arm literals
  // this round deliberately skipped.
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

  // Fresh extension vars must not out-rank the original variables in the
  // decision order (kissat `adjust_scores_and_phases_of_fresh_variables`,
  // factor.c:951, called unconditionally; cadical factor.cpp:907 under
  // `factorunbump`, default 1). Heuristic-only (`H`) — no proof delta,
  // no σ change — so this is legal from a rule per the H contract, and it is a
  // no-op when nothing was applied or when the solver is unguided.
  // SCHED-C5: `factor_demote_fresh` now DEFAULTS TRUE; `TACTIX_FACTOR_DEMOTE=0`
  // is the off arm. Both references run this on EVERY application, so the
  // `applied` test is the only condition either of them has (kissat's fresh
  // stack is empty when nothing was factored).
  if (k.factor_demote_fresh && applied)
    heuristics_demote_fresh_vars(k, fresh_first);

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

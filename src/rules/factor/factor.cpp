#include "rules/factor/factor.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp" // inprocess_reach_root

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <queue>
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

// DISTINCT binary partners of `l`: each `p` with a live binary (l ∨ p),
// all-unassigned and below the extension boundary `limit`. Deduplicated via the
// caller's `seen` marker (sized `limit`) — duplicate binaries in the watch list
// would otherwise inflate the grid dimensions and the gain, and produce repeated
// (undeletable) grid pairs. Each distinct partner is returned once.
void binary_partners(const Kernel &k, Lit l, Lit limit, std::vector<Lit> &out,
                     std::vector<uint8_t> &seen) {
  out.clear();
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
void apply_factoring(Kernel &k, const std::vector<Lit> &F,
                     const std::vector<Lit> &R) {
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
  for (const Lit r : R) {
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {nx, r}; // pivot (¬x) first
    if (lrat) {
      hints.clear();
      for (size_t i = 0; i < F.size(); i++) {
        hints.push_back(-static_cast<int64_t>(divider_ids[i]));
        hints.push_back(static_cast<int64_t>(kernel_binary_id(k, F[i], r)));
      }
    }
    if (k.proof)
      k.proof->add_rat(id, lits, 2, hints.data(),
                       static_cast<uint32_t>(hints.size()));
    kernel_add_binary_clause(k, nx, r, id);
  }

  // Delete the originals LAST (proof `del`); ALL copies — a duplicated grid
  // binary is fully replaced by the aux-var definition, and the plain
  // kernel_delete_binary refuses copies>1 (see kernel.cpp).
  for (const Lit f : F)
    for (const Lit r : R)
      kernel_delete_binary_all_copies(k, f, r);
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
  // DIAGNOSTIC: dump the post-inprocessing formula (Factor runs after BVE in the
  // name-sorted slot, so this captures the post-BVE state before Factor mutates
  // it) and exit — for the formula-vs-search decomposition of the BVE bimodal.
  if (const char *p = std::getenv("TACTIX_DUMP_CNF"))
    dump_factored_and_exit(k, p);
  // Native LRAT: dividers/quotients emit add-RAT with explicit resolution hints
  // (apply_factoring) — Factor is the first RAT-emitting rule, so the LRAT path
  // gets the full transform now (was a NoOp before native add_rat landed).
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;

  // Density short-circuit. BVA only pays off on a dense binary implication graph;
  // on binary-sparse instances (random 3-SAT) the seed scan is pure overhead and
  // any incidental factoring of a BVE-derived binary only perturbs a chaotic
  // search (Tier A tr-n300-s055 tipped over 30 s). Require ≥ ~one binary per
  // original variable. This is a SELECTIVITY heuristic — its proper home is the
  // policy's scheduling of Factor (S2); it lives here meanwhile as a cheap NoOp,
  // not a guard conjunct (the guard stays the pure `level==0` precondition, so
  // N1 keeps every factoring applicable via the API regardless).
  // TACTIX_FACTOR_FORCE bypasses the selectivity gate so the fuzz harness
  // exercises Factor on small instances (which are almost all below it).
  if (k.binary_ids.size() < kernel_num_output_vars(k) &&
      !std::getenv("TACTIX_FACTOR_FORCE"))
    return ApplyResult::NoOp;

  // Anti-loop boundary: only ORIGINAL literals may be factors/remainders — never
  // the extension vars Factor itself adds. This must be the true original count
  // (first_extension_var), fixed across applies; using 2*k.nvars would, on a
  // second apply, treat the first apply's extension vars as original and factor
  // over them → unbounded self-factoring.
  const Lit orig_lits = static_cast<Lit>(2 * kernel_num_output_vars(k));

  std::vector<uint8_t> inR(orig_lits, 0);       // R membership
  std::vector<uint8_t> seen(orig_lits, 0);      // binary_partners dedup scratch
  std::vector<uint32_t> cofactor(orig_lits, 0); // co-occurrence counts
  std::vector<Lit> touched, partners, R, newR, F, best_F, best_R;
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
  const auto score_of = [&](Lit l) -> uint32_t { return watch_score(k, l); };
  std::priority_queue<std::pair<uint32_t, Lit>> pq;
  std::vector<uint8_t> tried(orig_lits, 0);
  for (Lit l = 0; l < orig_lits; l++)
    if (k.value(l) == 0)
      pq.push({score_of(l), l});

  while (applied < FACTOR_MAX_PER_APPLY && !pq.empty()) {
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
    binary_partners(k, seed, orig_lits, R, seen);
    if (R.size() < 2 || R.size() > FACTOR_DIM_LIMIT)
      continue;
    for (const Lit r : R)
      inR[r] = 1;
    F.assign(1, seed);
    int best_delta = 0;
    best_F.clear();
    best_R.clear();
    const uint32_t init_R = static_cast<uint32_t>(R.size()); // width at m=1
    uint32_t traj_max_n = init_R;                            // widest R along chain

    // greedily grow the factor set, tracking the max-delta prefix.
    while (F.size() < FACTOR_DIM_LIMIT && R.size() >= 2) {
      // count candidate next-factors by co-occurrence across the current R.
      touched.clear();
      for (const Lit r : R) {
        binary_partners(k, r, orig_lits, partners, seen);
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
        const uint32_t score = score_of(h);
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
      binary_partners(k, g, orig_lits, partners, seen);
      newR.clear();
      for (const Lit p : partners)
        if (inR[p])
          newR.push_back(p);
      const uint32_t m = static_cast<uint32_t>(F.size()) + 1;
      const uint32_t n = static_cast<uint32_t>(newR.size());
      if (n > traj_max_n)
        traj_max_n = n;
      const int delta = static_cast<int>(m * n) - static_cast<int>(m + n);
      if (delta > best_delta) {
        best_delta = delta;
        best_F = F;
        best_F.push_back(g);
        best_R = newR;
      }
      // commit g and shrink R to newR to keep growing.
      F.push_back(g);
      for (const Lit r : R)
        inR[r] = 0;
      for (const Lit r : newR)
        inR[r] = 1;
      R.swap(newR);
    }
    for (const Lit r : R) // unmark whatever R ended as
      inR[r] = 0;

    if (best_delta >= FACTOR_MIN_GAIN) {
      if (std::getenv("TACTIX_GRID_TRACE"))
        std::fprintf(stderr, "[grid] m=%zu n=%zu delta=%d init_R=%u max_n=%u\n",
                     best_F.size(), best_R.size(), best_delta, init_R,
                     traj_max_n);
      apply_factoring(k, best_F, best_R);
      applied++;
      k.stats.factorings++;
      // Re-score the literals whose binary degree just dropped, so a seed that
      // has since been stripped of its grid stops outranking an intact one
      // (kissat's `update_factored`, factor.c:800). Untried literals only —
      // `tried` still caps each seed at one attempt per pass.
      for (const Lit f : best_F)
        if (!tried[f])
          pq.push({score_of(f), f});
      for (const Lit r : best_R)
        if (!tried[r])
          pq.push({score_of(r), r});
    }
  }

  if (std::getenv("TACTIX_FACTOR_TRACE"))
    std::fprintf(stderr, "[factor] entered (%u orig lits), %llu factorings\n",
                 orig_lits, (unsigned long long)applied);
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

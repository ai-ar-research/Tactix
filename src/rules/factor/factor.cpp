#include "rules/factor/factor.hpp"

#include "kernel/kernel.hpp"
#include "policies/subpolicy.hpp" // FactorMatchPolicy (S2a grid selection)
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
// Scratch discipline: `unit_id` is indexed by literal and grows on demand rather
// than being fill-initialized per call — the O(nvars)-per-call hazard this
// codebase has hit four times ([[per-call-linear-scratch-hazard]]).
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
  uint32_t count() const { return static_cast<uint32_t>(off.size()); }
  uint32_t size_of(uint32_t id) const { return len[id]; }
  const Lit *lits_of(uint32_t id) const { return pool.data() + off[id]; }
  Lit lit0(uint32_t id) const { return pool[off[id]]; }
};

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
                     const std::vector<uint32_t> &R, const RemPool &pool) {
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
  for (const uint32_t rid : R) {
    // INCREMENT 1: every remainder is one literal, so this is the binary
    // quotient exactly as before. Increment 2 widens `lits` to {¬x} ∪ r; the
    // RAT-on-¬x argument is indifferent to |r| (design §4), and the hint stream
    // below is already per-divider, so only the literal vector changes.
    const Lit r = pool.lit0(rid);
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
    for (const uint32_t rid : R)
      kernel_delete_binary_all_copies(k, f, pool.lit0(rid));
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
  // Density threshold: fire Factor only when binaries ≥ density·num_output_vars.
  // Raised 1→8 with the BVE-LAST fix (2026-07-21): Factor's BVA compresses dense
  // cardinality structure (clique ratio 17.5, mp1 103, x9 8.2) so BVE-last is
  // benign there, but on binary-SPARSE instances (mod4block 4.1) Factor-then-BVE
  // is HARMFUL — the gate fires on the dense ones and skips the sparse ones.
  // Lowered 8→6 (2026-07-24): battleship's ratio is in [6,7) and the 8-gate
  // locked it out of its ONLY robust path — with Factor it solves (78
  // factorings, 175 s); without, every trajectory arm hit 72 M+ conflicts
  // (`benchmarks/reports/2026-07-24-battleship-divergence.md`). mod4block
  // (4.1) stays excluded. NOTE: still an overfit magic number (a SELECTIVITY
  // heuristic — proper home is the policy, N1 keeps the guard pure); the
  // effort-budgeted scheduler (plan rev 53) supersedes it.
  // `TACTIX_FACTOR_DENSITY` overrides for A/B.
  uint32_t density = 6;
  if (const char *s = std::getenv("TACTIX_FACTOR_DENSITY"))
    density = static_cast<uint32_t>(std::atoi(s));
  if (k.binary_ids.size() < density * kernel_num_output_vars(k) &&
      !std::getenv("TACTIX_FACTOR_FORCE"))
    return ApplyResult::NoOp;

  // Anti-loop boundary: only ORIGINAL literals may be factors/remainders — never
  // the extension vars Factor itself adds. This must be the true original count
  // (first_extension_var), fixed across applies; using 2*k.nvars would, on a
  // second apply, treat the first apply's extension vars as original and factor
  // over them → unbounded self-factoring.
  const Lit orig_lits = static_cast<Lit>(2 * kernel_num_output_vars(k));

  // S2b composite grids (docs/design/s2b-composite-grids-design.md):
  // k.factor_composite widens the factor/remainder universe to ALL
  // live literals — prior-pass extension vars and this pass's own fresh vars
  // included — growing as the pass applies (kissat resize_factoring).
  // Termination needs no var-class exclusion: every accepted grid has
  // delta >= 1, strictly decreasing the finite live-binary count (a grid
  // over a var's own dividers has m = 1, never accepted). **Default OFF —
  // measured (design §7): in our single preprocessing blast, composition
  // fires (mp1 940 → 1834 grids) but the downstream-conflict proxy REGRESSES
  // 1.37 M → 2.18 M (the L4 over-application U-shape, Factor edition), and
  // the grid cap does not move. Kept for the S4 arrangement, whose small
  // interleaved doses are where kissat's own cross-round composition lives.**
  // (The TACTIX_FACTOR_COMPOSITE blast-composition env — the S2b negative —
  // was removed in the 2026-07-25 negative-arm cleanup; k.factor_composite
  // is the one switch, set by policies for density-scoped doses.)
  const bool composite = k.factor_composite;
  Lit live_lits = composite ? static_cast<Lit>(2 * k.nvars) : orig_lits;

  // R-side scratch is now indexed by REMAINDER ID, not literal (R5 increment 1).
  // `inR` grows with the pool rather than being sized up front, since the pool
  // is bounded by material actually seen, not by 2*nvars.
  RemPool pool;
  std::vector<uint8_t> inR;                     // R membership, by remainder id
  const auto mark_R = [&](uint32_t rid, uint8_t v) {
    if (inR.size() <= rid)
      inR.resize(rid + 1, 0);
    inR[rid] = v;
  };
  const auto in_R = [&](uint32_t rid) {
    return rid < inR.size() && inR[rid];
  };
  // Intern one literal-remainder, preserving the caller's order — bit-identity
  // at factor_size == 2 depends on R being iterated in binary_partners' order.
  const auto intern_all = [&](const std::vector<Lit> &src,
                              std::vector<uint32_t> &dst) {
    dst.clear();
    for (const Lit p : src)
      dst.push_back(pool.intern_unit(p));
  };
  std::vector<uint8_t> seen(live_lits, 0);      // binary_partners dedup scratch
  std::vector<uint32_t> cofactor(live_lits, 0); // co-occurrence counts
  std::vector<Lit> touched, partners, partner_lits, F, best_F;
  std::vector<uint32_t> R, newR, best_R;
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
  for (Lit l = 0; l < live_lits; l++)
    if (k.value(l) == 0)
      pq.push({score_of(l), l});

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
    binary_partners(k, seed, live_lits, partner_lits, seen);
    intern_all(partner_lits, R);
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
        // INCREMENT 1: |r| == 1, so "the literals h with {h} ∪ r a clause" is
        // exactly binary_partners(r). Increment 2 replaces this with the
        // occurrence scan for clauses {h} ∪ r of size ≤ factor_size.
        binary_partners(k, pool.lit0(rid), live_lits, partners, seen);
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
      binary_partners(k, g, live_lits, partners, seen);
      newR.clear();
      for (const Lit p : partners) {
        const uint32_t rid = pool.intern_unit(p);
        if (in_R(rid))
          newR.push_back(rid);
      }
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
      apply_factoring(k, best_F, best_R, pool);
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
      for (const Lit r : best_R)
        if (!tried[r])
          pq.push({score_of(r), r});
    }
  }

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

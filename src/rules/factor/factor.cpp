#include "rules/factor/factor.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp" // inprocess_reach_root

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
  // add a binary (pivot first) + emit its DRAT proof line (RAT via first literal).
  auto add_bin = [&](Lit pivot, Lit other) {
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {pivot, other};
    if (k.proof)
      k.proof->add_rup(id, lits, 2, nullptr, 0); // DRAT: RUP-or-RAT on first lit
    kernel_add_binary_clause(k, pivot, other, id);
  };
  for (const Lit f : F)
    add_bin(px, f); // divider (x ∨ f) — RAT on x (no ¬x clause yet)
  for (const Lit r : R)
    add_bin(nx, r); // quotient (¬x ∨ r) — RAT on ¬x via dividers + live originals
  for (const Lit f : F)
    for (const Lit r : R)
      // delete originals LAST (proof `del`); ALL copies — a duplicated grid
      // binary is fully replaced by the aux-var definition, and the plain
      // kernel_delete_binary refuses copies>1 (see kernel.cpp).
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
  // Native-LRAT RAT chains (add_rat) are a later stage; keep the LRAT path sound
  // by no-op-ing (mirrors D-CONG-LRAT). DRAT gets the full transform.
  if (k.proof && k.proof->lrat())
    return ApplyResult::NoOp;
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
  if (k.binary_ids.size() < kernel_num_output_vars(k))
    return ApplyResult::NoOp;

  // Anti-loop boundary: only ORIGINAL literals may be factors/remainders — never
  // the extension vars Factor itself adds. This must be the true original count
  // (first_extension_var), fixed across applies; using 2*k.nvars would, on a
  // second apply, treat the first apply's extension vars as original and factor
  // over them → unbounded self-factoring.
  const Lit orig_lits = static_cast<Lit>(2 * kernel_num_output_vars(k));

  std::vector<uint8_t> inR(orig_lits, 0);     // R membership
  std::vector<uint8_t> seen(orig_lits, 0);    // binary_partners dedup scratch
  std::vector<uint32_t> cofactor(orig_lits, 0); // co-occurrence counts
  std::vector<Lit> touched, partners, R, newR, F, best_F, best_R;
  uint64_t applied = 0;

  for (Lit seed = 0; seed < orig_lits && applied < FACTOR_MAX_PER_APPLY; seed++) {
    if (k.value(seed) != 0)
      continue;
    binary_partners(k, seed, orig_lits, R, seen);
    if (R.size() < 2 || R.size() > FACTOR_DIM_LIMIT)
      continue;
    for (const Lit r : R)
      inR[r] = 1;
    F.assign(1, seed);
    int best_delta = 0;
    best_F.clear();
    best_R.clear();

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
      Lit g = INVALID_LIT;
      uint32_t bestc = 1;
      for (const Lit h : touched)
        if (cofactor[h] > bestc) {
          bestc = cofactor[h];
          g = h;
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
      apply_factoring(k, best_F, best_R);
      applied++;
      k.stats.factorings++;
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

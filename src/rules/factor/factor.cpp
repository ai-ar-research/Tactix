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

// binary partners of `l`: each `p` with a live binary (l ∨ p), all-unassigned and
// (optionally) below the extension boundary `limit`.
void binary_partners(const Kernel &k, Lit l, Lit limit, std::vector<Lit> &out) {
  out.clear();
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
  const uint32_t sz = k.watches.heads[l].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      const Lit p = w.other();
      if (p < limit && k.value(p) == 0)
        out.push_back(p);
      i += 1;
    } else {
      i += 2;
    }
  }
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
      kernel_delete_binary(k, f, r); // delete originals LAST (proof `del`)
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

  // Anti-loop boundary: only ORIGINAL literals may be factors/remainders — never
  // the extension vars Factor itself adds. This must be the true original count
  // (first_extension_var), fixed across applies; using 2*k.nvars would, on a
  // second apply, treat the first apply's extension vars as original and factor
  // over them → unbounded self-factoring.
  const Lit orig_lits = static_cast<Lit>(2 * kernel_num_output_vars(k));

  std::vector<uint8_t> inR(orig_lits, 0);     // R membership
  std::vector<uint32_t> cofactor(orig_lits, 0); // co-occurrence counts
  std::vector<Lit> touched, partners, R, newR, F, best_F, best_R;
  uint64_t applied = 0;

  for (Lit seed = 0; seed < orig_lits && applied < FACTOR_MAX_PER_APPLY; seed++) {
    if (k.value(seed) != 0)
      continue;
    binary_partners(k, seed, orig_lits, R);
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
        binary_partners(k, r, orig_lits, partners);
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
      binary_partners(k, g, orig_lits, partners);
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

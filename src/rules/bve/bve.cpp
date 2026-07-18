// E1a: BVE rule skeleton + round-local occurrence index + candidate scheduling.
// No elimination yet (apply is a NoOp — the round lands in E1b). Transliterated
// idioms from kissat/src/eliminate.c (variable scheduling) + our own
// Subsume/Strengthen occurrence pattern; see docs/e1-bve-design.md.
#include "rules/bve/bve.hpp"

#include "kernel/watch.hpp"

#include <algorithm>
#include <utility>

namespace tactix {

// occurrence limit (kissat eliminateocclim): a variable whose two polarities
// together occur more than this is skipped — the O(occ⁺·occ⁻) resolution would
// blow up. E1a only filters the schedule with it; E1b's resolution respects it.
static constexpr uint32_t BVE_OCCLIM = 2000;

bool bve_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void bve_build_occurrences(const Kernel &k, BveOccurrences &occ) {
  const uint32_t nlits = 2 * k.nvars;
  occ.large.assign(nlits, {});
  occ.binary.assign(nlits, {});
  // large IRREDUNDANT clauses: one arena scan. BVE eliminates over F (the
  // irredundant core); the learned set R is redundant and not resolved.
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant())
      for (const Lit l : *c)
        if (k.values[l] == 0) // active-variable literals only
          occ.large[l].push_back(ref);
    ref += clause_words(c->size);
  }
  // binaries: one watch-list scan per active literal. A binary (l ∨ o) is one
  // occurrence of l with partner o; binaries have an irredundant lifecycle in
  // this kernel (never reduced), so all of them count.
  for (Lit l = 0; l < nlits; l++) {
    if (k.values[l] != 0)
      continue;
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
    const uint32_t sz = k.watches.heads[l].size;
    for (uint32_t i = 0; i < sz;) {
      const Watch w{ws[i]};
      if (w.binary()) {
        const Lit o = w.other();
        if (k.values[o] == 0)
          occ.binary[l].push_back(o);
        i += 1;
      } else {
        i += 2; // large watch: blocking word + ref word
      }
    }
  }
}

void bve_schedule(const Kernel &k, std::vector<Var> &out) {
  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  std::vector<std::pair<uint64_t, Var>> ranked; // (score, var), ascending
  for (Var v = 0; v < k.nvars; v++) {
    const Lit pl = make_lit(v, false);
    if (k.values[pl] != 0) // assigned variable: not an elimination candidate
      continue;
    const uint32_t p = occ.count(pl), n = occ.count(make_lit(v, true));
    if (p == 0 && n == 0) // occurs in no elimination clause: nothing to do
      continue;
    if (p > 0 && n > 0 && p + n > BVE_OCCLIM) // occurrence blow-up: skip
      continue;
    ranked.emplace_back(static_cast<uint64_t>(p) * n, v);
  }
  // fewest resolutions first; stable so equal scores keep variable order
  std::stable_sort(ranked.begin(), ranked.end());
  out.clear();
  out.reserve(ranked.size());
  for (const auto &pr : ranked)
    out.push_back(pr.second);
}

// canonical instance = a full round. E1a: no elimination yet — a NoOp (as V3a
// shipped detection-only). E1b drains the root closure and runs the round.
ApplyResult bve_apply(Kernel &k) {
  assert(bve_guard(k));
  return ApplyResult::NoOp;
}

// instance surface: payload = a candidate variable via its positive DIMACS
// literal (lit_instance, rev 34); INSTANCE_CANONICAL = the full round. Total for
// every payload. E1a: no per-variable elimination yet — NoOp.
static uint32_t bve_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Var> cand;
  bve_schedule(k, cand);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(cand.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(make_lit(cand[i], false));
  return n;
}

static ApplyResult bve_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return bve_apply(k);
  Lit l;
  if (!lit_of_instance(inst, &l))
    return ApplyResult::NoOp;
  if (var_of(l) >= k.nvars || k.values[l] != 0)
    return ApplyResult::NoOp;
  return ApplyResult::NoOp; // E1a: no per-variable elimination yet (E1b)
}

TACTIX_REGISTER_RULE(BVE, RuleTier::Inprocess, bve_guard, bve_apply,
                     bve_instances, bve_apply_instance);

} // namespace tactix

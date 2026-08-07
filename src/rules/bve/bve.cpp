// E1 BVE: bounded variable elimination by resolution. E1a landed the skeleton +
// occurrence index + schedule; E1b (this commit) adds the elimination round —
// resolution, the bound, RUP resolvent emission, deletion, and reconstruction
// witnesses. Full cross product (no gates yet — E1c). Transliterated idioms from
// kissat/src/eliminate.c + resolve.c; the add-before-delete / root-false-strip /
// record_unit / witness mechanics mirror our own V3 Substitute (do_substitution),
// the fuzzed template. See docs/e1-bve-design.md.
#include "rules/bve/bve.hpp"

#include "engine/subsolver.hpp"
#include "kernel/watch.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace tactix {

// occurrence limit (kissat eliminateocclim): skip a variable whose two
// polarities together occur more than this — the O(occ⁺·occ⁻) resolution blows
// up. Used both to filter the schedule and to guard the elimination.
static constexpr uint32_t BVE_OCCLIM = 2000;
// resolvent size limit (kissat eliminateclslim): a resolvent larger than this
// aborts the variable.
static constexpr uint32_t BVE_CLSLIM = 100;
// Growth slack: eliminate iff #non-tautological resolvents ≤ occ⁺ + occ⁻ + bound.
// The bound is POLICY-DRIVEN state (`k.bve_bound`, defaulting to BVE_MAX_BOUND);
// DefaultPolicy ramps it cheap-first (0,1,2,4,8) so the eliminations that do not
// grow the formula happen first — kissat eliminate.c set_next_elimination_bound.
// When `k.bve_track_min_bound` is set the resolvent loop runs PAST this slack
// (to the ramp ceiling) to learn the exact slack a rejected variable needs,
// caching it in `k.bve_min_bound` so the policy can skip it at lower ramp levels
// instead of re-resolving it at each. `Kernel::bve_bound` is the ONE channel:
// no in-rule env pin may override a policy's write. TACTIX_BVE_RAMP_MAX sweeps
// the ramp ceiling policy-side.
static uint32_t bve_bound(const Kernel &k) { return k.bve_bound; }

bool bve_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void bve_build_occurrences(const Kernel &k, BveOccurrences &occ) {
  const uint32_t nlits = 2 * k.nvars;
  occ.counts.assign(nlits, 0);
  // large IRREDUNDANT clauses. BVE eliminates over F (the irredundant core); the
  // learned set R is redundant and not resolved.
  if (k.dense) {
    // dense fast-path (increment 2): the shared index already lists each active
    // literal's irredundant large clauses — O(occ), no arena scan. Skip stale
    // garbage entries (lazy-deleted, not yet compacted).
    for (Lit l = 0; l < nlits; l++) {
      if (k.values[l] != 0) // active-variable literals only
        continue;
      for (const Ref ref : k.dense->large[l])
        if (!clause_at(k.arena, ref)->garbage())
          occ.counts[l]++;
    }
  } else {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage() && !c->redundant())
        for (const Lit l : *c)
          if (k.values[l] == 0) // active-variable literals only
            occ.counts[l]++;
      ref += clause_words(c->size);
    }
  }
  // binaries: one watch-list scan per active literal. A binary (l ∨ o) is one
  // occurrence of l with partner o; binaries have an irredundant lifecycle in
  // this kernel (never reduced), so all of them count.
  for (Lit l = 0; l < nlits; l++) {
    if (k.values[l] != 0)
      continue;
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && k.values[w.other()] == 0)
        occ.counts[l]++;
  }
}

void bve_schedule(Kernel &k, std::vector<Var> &out) {
  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  std::vector<std::pair<uint64_t, Var>> ranked; // (score, var), ascending
  for (Var v = 0; v < k.nvars; v++) {
    // Extension variables (E2 Factor / BVA) must never be eliminated: BVE on x
    // resolves its dividers (x∨fᵢ) against its quotients (¬x∨rⱼ) back to the
    // original m·n grid (fᵢ∨rⱼ) — exactly reversing Factor's compression, so
    // BVE and Factor would thrash indefinitely. They exist to compress; skip.
    if (v >= kernel_num_output_vars(k))
      continue;
    // NOTE: the retry bitmap (Kernel::bve_retry) is deliberately NOT consulted
    // here. Whether re-attempting an unchanged variable is worth the work is a
    // SCHEDULING decision, and scheduling belongs to the policy (plan §5 rev
    // 17) — filtering here would also silently narrow the instance surface that
    // RandomPolicy and any future learned policy enumerate as their action
    // space. The rule MAINTAINS the advice (bve_eliminate clears the bit it
    // attempts, the kernel sets it on every mutation that could change the
    // answer) and the policy READS it; see DefaultPolicy's dosing loop.
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
  std::stable_sort(ranked.begin(), ranked.end());
  out.clear();
  out.reserve(ranked.size());
  for (const auto &pr : ranked)
    out.push_back(pr.second);
}

namespace {

// A materialized clause on the pivot: its ACTIVE literals (root-false dropped;
// their unit ids in `absorbed` for LRAT), its proof id, and a delete-handle
// (arena ref for large, or the binary pair). `active` includes the pivot.
struct MClause {
  std::vector<Lit> active;
  std::vector<uint64_t> absorbed;
  uint64_t id = 0;
  Ref ref = INVALID_REF; // INVALID_REF => binary
  Lit ba = INVALID_LIT, bb = INVALID_LIT;
};

// Reusable state for R10's definition extraction, owned by the CALLER of
// `bve_eliminate` (a round creates one and every candidate in that round reuses
// it). Not a per-call local: `SubSolver::reset` recycles its allocations, and
// the pivot→sub-variable map is built by sort+unique over the pivot's own
// literals rather than an O(nvars) scratch array — a per-attempt linear
// fill-init in a per-variable loop is the profile hazard this project has hit
// four times. A null pointer means "definition extraction unavailable here".
struct BveDefScratch {
  SubSolver solver;
  std::vector<Var> vars;    // sorted distinct non-pivot vars = the sub var space
  std::vector<SubLit> lits; // one reduced clause, being encoded
  SubDerivation deriv;
  uint64_t ticks = 0;    // ticks the last attempt spent (0 if none was made)
  bool checked = false;  // an attempt was made at all
  bool exhausted = false; // ...and the tick ceiling cut it short
};

// ---- gate detection (E1c: equivalence + AND/OR; SP-BVE-GATE-1: ITE + XOR;
// R10: general definitions) ----
// Detect a functional gate defining x from its clauses; on success mark the gate
// clauses in pos_gate/neg_gate (1 = gate, 0 = antecedent) and return true.
//
// THE ONE SOUNDNESS OBLIGATION, stated once for all five types. Write the pivot
// clauses as (x∨Cᵢ) for i ∈ P and (¬x∨Dⱼ) for j ∈ N, and let G⁺ ⊆ P, G⁻ ⊆ N be
// the marked gate clauses. Marking is legal exactly when
//
//     ⋀_{i∈G⁺} Cᵢ  ∧  ⋀_{j∈G⁻} Dⱼ   is UNSATISFIABLE,                    (★)
//
// and it then licenses dropping the antecedent×antecedent resolvents. Proof:
// take M ⊨ F′ and suppose neither value of x satisfies every pivot clause, i.e.
// M ⊭ C_{i₀} for some i₀ ∈ P and M ⊭ D_{j₀} for some j₀ ∈ N. By (★) M also
// falsifies some gate clause remainder — say C_{i₁}, i₁ ∈ G⁺ (the G⁻ case is
// symmetric). Then C_{i₁} ∨ D_{j₀} is a resolvent we DID generate, and M
// falsifies both halves: contradiction. So some value of x satisfies all of
// them — which is both the equisatisfiability of the Davis–Putnam step and the
// reason the E1b witness push (all live clauses, pivot blocking) reconstructs.
// A *false* gate would drop needed resolvents (unsound), so every detector here
// establishes (★) exactly, and the whole surface is fuzz-gated.
//
// Note which resolvents that argument needs: every pair with AT LEAST ONE gate
// clause, including gate×gate (i₁ ∈ G⁺ and j₀ ∈ G⁻ is a live case). That is
// what `resolve_gate` selects.
//   - the four SYNTACTIC types leave `resolve_gate` FALSE, and may: their gate
//     clauses are exactly a CNF of x↔φ, so every gate×gate resolvent is a
//     tautology — equivalence and AND/OR clash on the input literal; ITE on the
//     condition or on a branch; two parity clauses with opposite pivot signs
//     carry opposite input-sign parities and so disagree on some input. They
//     are dropped as tautologies, not as an exception to the argument.
//     ITE and XOR are matched EXACTLY — kissat's shortened-clause path
//     (`ifthenelse.c match_ternary_ref`, which accepts a subsuming clause and
//     then sets `resolve_gate`) is deliberately not ported.
//   - the DEFINITION type SETS `resolve_gate`, because (★) is all its core
//     gives: C_{i} ∨ D_{j} for two core clauses need not be tautological.

// Sorted signature of a clause's NON-PIVOT active literals, for the exact
// clause lookups ITE and XOR need. Only clauses with 2..BVE_XOR_ARITY_LIMIT
// non-pivot literals and no repeated VARIABLE are indexed — a repeat means a
// duplicated literal (the clause is shorter than it looks) or a tautology, and
// neither shape may stand in for a gate clause.
constexpr uint32_t BVE_SIG_MAX = BVE_XOR_ARITY_LIMIT;
struct GateSig {
  uint32_t n = 0;
  Lit l[BVE_SIG_MAX] = {0};
  bool operator<(const GateSig &o) const {
    if (n != o.n)
      return n < o.n;
    // Fixed trip count over the WHOLE array, not over `n`: every GateSig is
    // zero-initialized and only ever writes [0, n), so with n == o.n the tail
    // [n, BVE_SIG_MAX) is 0 on both sides and cannot change the order. The
    // fixed bound is what keeps std::sort's by-value pivot copy provably
    // initialized to the optimizer (-Wmaybe-uninitialized at -O3 otherwise).
    for (uint32_t i = 0; i < BVE_SIG_MAX; i++)
      if (l[i] != o.l[i])
        return l[i] < o.l[i];
    return false;
  }
};
using SigIndex = std::vector<std::pair<GateSig, uint32_t>>; // sorted by sig

void bve_build_sig_index(const std::vector<MClause> &side, Lit pivot,
                         SigIndex &out) {
  out.clear();
  for (size_t i = 0; i < side.size(); ++i) {
    const std::vector<Lit> &a = side[i].active;
    if (a.size() < 3 || a.size() > BVE_SIG_MAX + 1)
      continue;
    GateSig s;
    bool ok = true;
    for (const Lit l : a) {
      if (l == pivot)
        continue;
      if (s.n == BVE_SIG_MAX || var_of(l) == var_of(pivot)) {
        ok = false;
        break;
      }
      s.l[s.n++] = l;
    }
    if (!ok || s.n + 1 != a.size())
      continue;
    std::sort(s.l, s.l + s.n);
    for (uint32_t j = 1; j < s.n; j++)
      if (var_of(s.l[j]) == var_of(s.l[j - 1]))
        ok = false;
    if (ok)
      out.emplace_back(s, static_cast<uint32_t>(i));
  }
  std::sort(out.begin(), out.end(),
            [](const std::pair<GateSig, uint32_t> &a,
               const std::pair<GateSig, uint32_t> &b) {
              return a.first < b.first;
            });
}

// index of a clause whose non-pivot literals are exactly `s` (already sorted),
// or -1. Duplicate signatures are two copies of one logical clause; either
// serves as the gate clause.
int bve_find_sig(const SigIndex &idx, const GateSig &s) {
  const auto it = std::lower_bound(
      idx.begin(), idx.end(), s,
      [](const std::pair<GateSig, uint32_t> &e, const GateSig &q) {
        return e.first < q;
      });
  if (it == idx.end() || s < it->first)
    return -1;
  return static_cast<int>(it->second);
}

inline GateSig bve_sig2(Lit a, Lit b) {
  GateSig s;
  s.n = 2;
  s.l[0] = std::min(a, b);
  s.l[1] = std::max(a, b);
  return s;
}

// (5) GENERAL DEFINITION x ↔ φ — the one type that is not a shape match.
// Hand the pivot's own occurrence lists, REDUCED (each clause minus the pivot
// literal), to the sub-solver. A refutation of that clause set is precisely
// condition (★) above for G⁺ = P, G⁻ = N; the sub-solver's CORE names the
// sub-multiset of it that the refutation actually used, so (★) holds for the
// core alone and the core is the gate. Transliterated from kissat
// `definition.c` (`kissat_find_definition`), which asks kitten the same
// question; the engine is ours (src/engine/subsolver.hpp).
//
// NOTHING FROM THE SUB-SOLVER ENTERS THE PROOF. The core only chooses which
// resolvents to generate; every generated resolvent is still a resolution of
// two clauses of F on the pivot, hence RUP from its two parents with the same
// two-id chain E1b already emits. The delta stays RUP-only.
//
// Not ported from kissat: (a) `definitioncores` > 1, which re-solves a
// shrunken-and-SHUFFLED formula to find a smaller core — our sub-solver has a
// fixed decision order and no shuffle, so a second solve would reproduce the
// first core exactly; (b) the one-sided-core FAILED LITERAL (an empty G⁺ or G⁻
// means the other side alone is refuted, so the pivot is forced). That unit is
// entailed but not RUP, so it needs the sub-solver's derivation mapped to
// main-formula ids — V2 Backbone's proof delta, arrived at from another rule's
// round. The gate itself is still extracted and used in both cases; only the
// free unit is left on the table.
uint32_t bve_find_definition(const Kernel &k, Lit px,
                             const std::vector<MClause> &pos,
                             const std::vector<MClause> &neg,
                             std::vector<uint8_t> &pos_gate,
                             std::vector<uint8_t> &neg_gate,
                             BveDefScratch &s) {
  // One-sided pivots have an empty cross product already; there is nothing for
  // a gate to reduce, and (★) would be asking to refute one side alone.
  if (pos.empty() || neg.empty())
    return 0;
  const Var pv = var_of(px);
  s.vars.clear();
  for (const std::vector<MClause> *side : {&pos, &neg})
    for (const MClause &m : *side)
      for (const Lit l : m.active)
        if (var_of(l) != pv)
          s.vars.push_back(var_of(l));
  std::sort(s.vars.begin(), s.vars.end());
  s.vars.erase(std::unique(s.vars.begin(), s.vars.end()), s.vars.end());
  if (s.vars.empty())
    return 0; // every clause reduces to the empty clause: not a real pivot
  const auto sub_of = [&s](Lit l) {
    const auto it = std::lower_bound(s.vars.begin(), s.vars.end(), var_of(l));
    return sub_lit(static_cast<SubVar>(it - s.vars.begin()), negated(l));
  };

  s.checked = true;
  s.solver.reset(static_cast<uint32_t>(s.vars.size()));
  // ids are handed out from 1 in add order, so id ∈ [1, |pos|] names pos[id-1]
  // and the rest name neg[id-|pos|-1]. `add_clause` consumes an id even for a
  // clause it drops (a tautology), which is exactly why that mapping holds.
  for (const std::vector<MClause> *side : {&pos, &neg}) {
    for (const MClause &m : *side) {
      s.lits.clear();
      for (const Lit l : m.active)
        if (var_of(l) != pv)
          s.lits.push_back(sub_of(l));
      s.solver.add_clause(s.lits);
    }
  }
  const SubResult res = s.solver.solve({}, k.bve_definition_budget);
  s.ticks = s.solver.ticks();
  s.exhausted = res == SubResult::Budget;
  if (res != SubResult::Unsat || !s.solver.derivation(s.deriv) ||
      s.deriv.core.empty())
    return 0;
  const uint32_t np = static_cast<uint32_t>(pos.size());
  for (const uint32_t id : s.deriv.core) {
    if (id <= np)
      pos_gate[id - 1] = 1;
    else
      neg_gate[id - np - 1] = 1;
  }
  return BVE_GATE_DEFINITION;
}

// Returns the BVE_GATE_* bit of the type found, or 0 for "no gate" (the caller
// owns the counters — `k` is const here by design: detection is READ-ONLY, and
// the definition attempt reports its cost back through `def` instead).
uint32_t bve_detect_gate(const Kernel &k, Lit px, Lit nx,
                         const std::vector<MClause> &pos,
                         const std::vector<MClause> &neg,
                         std::vector<uint8_t> &pos_gate,
                         std::vector<uint8_t> &neg_gate, bool &resolve_gate,
                         BveDefScratch *def) {
  resolve_gate = false;
  // binary partner lookup: side_bin[lit] = index of a binary {pivot, lit}, or -1.
  std::vector<int> pos_bin(2 * k.nvars, -1), neg_bin(2 * k.nvars, -1);
  for (size_t i = 0; i < pos.size(); ++i)
    if (pos[i].active.size() == 2) {
      const Lit o = pos[i].active[0] == px ? pos[i].active[1] : pos[i].active[0];
      pos_bin[o] = int(i);
    }
  for (size_t j = 0; j < neg.size(); ++j)
    if (neg[j].active.size() == 2) {
      const Lit o = neg[j].active[0] == nx ? neg[j].active[1] : neg[j].active[0];
      neg_bin[o] = int(j);
    }

  // (1) equivalence x↔¬w: pos binary (x∨w) + neg binary (¬x∨¬w) ⟹ ¬w→x, x→¬w.
  if (k.bve_gate_mask & BVE_GATE_EQUIV)
  for (size_t i = 0; i < pos.size(); ++i)
    if (pos[i].active.size() == 2) {
      const Lit w = pos[i].active[0] == px ? pos[i].active[1] : pos[i].active[0];
      const int j = neg_bin[negate(w)];
      if (j >= 0) {
        pos_gate[i] = 1;
        neg_gate[size_t(j)] = 1;
        return BVE_GATE_EQUIV;
      }
    }

  // (2) AND/OR: a base clause (¬x∨a₁∨…∨aₖ) on one side (k≥2) whose every input aᵢ
  // has the matching binary (x∨¬aᵢ) on the other side ⟹ x↔OR(a) (and the dual
  // with pivots swapped ⟹ x↔AND). The base + those k binaries fully define x.
  auto try_and_or = [&](const std::vector<MClause> &base_side, Lit base_pivot,
                        const std::vector<int> &opp_bin,
                        std::vector<uint8_t> &base_gate,
                        std::vector<uint8_t> &opp_gate) -> bool {
    for (size_t b = 0; b < base_side.size(); ++b) {
      const MClause &base = base_side[b];
      if (base.active.size() < 3)
        continue; // a 2-literal base is an equivalence (handled above)
      bool all = true;
      for (const Lit o : base.active)
        if (o != base_pivot && opp_bin[negate(o)] < 0) {
          all = false;
          break;
        }
      if (!all)
        continue;
      base_gate[b] = 1;
      for (const Lit o : base.active)
        if (o != base_pivot)
          opp_gate[size_t(opp_bin[negate(o)])] = 1;
      return true;
    }
    return false;
  };
  if (k.bve_gate_mask & BVE_GATE_AND_OR) {
    if (try_and_or(neg, nx, pos_bin, neg_gate, pos_gate))
      return BVE_GATE_AND_OR;
    if (try_and_or(pos, px, neg_bin, pos_gate, neg_gate))
      return BVE_GATE_AND_OR;
  }

  // the general definition is the LAST resort (kissat gates.c's own order): it
  // costs a sub-solve, where every shape above costs a scan.
  const auto definition = [&]() -> uint32_t {
    if (!def || !(k.bve_gate_mask & BVE_GATE_DEFINITION))
      return 0;
    const uint32_t kind =
        bve_find_definition(k, px, pos, neg, pos_gate, neg_gate, *def);
    // the ONLY place `resolve_gate` is set; see the argument above
    resolve_gate = kind != 0;
    return kind;
  };

  if (!(k.bve_gate_mask & (BVE_GATE_ITE | BVE_GATE_XOR)))
    return definition();
  // exact-match index over both sides, built once and shared by ITE and XOR
  // (kissat/cadical reach the same clauses through the shared occurrence
  // lists; ours are the already-materialized pos/neg MClause vectors, so the
  // index is O(P+N) and costs no O(nvars) scratch).
  SigIndex psig, nsig;
  bve_build_sig_index(pos, px, psig);
  bve_build_sig_index(neg, nx, nsig);

  // (3) ITE x ↔ (c ? ¬e : ¬t): the four ternaries
  //       (x ∨ c ∨ t) (x ∨ ¬c ∨ e) (¬x ∨ c ∨ ¬t) (¬x ∨ ¬c ∨ ¬e),
  // i.e. two pivot-side ternaries whose non-pivot pairs clash on exactly the
  // CONDITION literal, plus the matching pair on the other side (kissat
  // ifthenelse.c kissat_find_if_then_else_gate; cadical gates.cpp
  // find_if_then_else). Both references also try the dual polarity; here the
  // shape is symmetric in the pivot sign — swapping (px,nx) maps the quadruple
  // onto itself with c negated — so one pass over the pos side finds it.
  // WORK BOUND (kissat's own, ifthenelse.c:105-113): skip when the ternary
  // count squared exceeds the occurrence limit, and cap the pair scan at the
  // same limit. Sound at any value: an undetected gate is the full cross
  // product.
  auto try_ite = [&]() -> bool {
    // the index sorts on `n` first, so the ternaries are a prefix of psig
    size_t nter = 0;
    while (nter < psig.size() && psig[nter].first.n == 2)
      nter++;
    if (uint64_t(nter) * nter > BVE_OCCLIM)
      return false;
    uint64_t steps = 0;
    for (size_t i = 0; i + 1 < nter && steps < BVE_OCCLIM; ++i) {
      const Lit u0 = psig[i].first.l[0], u1 = psig[i].first.l[1];
      for (size_t j = i + 1; j < nter && steps < BVE_OCCLIM; ++j) {
        steps++;
        const Lit w0 = psig[j].first.l[0], w1 = psig[j].first.l[1];
        // the two pairs must clash on exactly one literal (the condition) and
        // the two branch literals must be on DIFFERENT variables (equal ones
        // make this an XOR/equivalence, not an ITE — cadical's `abs(ci) ==
        // abs(cj)` reject).
        Lit c = INVALID_LIT, t = INVALID_LIT, e = INVALID_LIT;
        if (u0 == negate(w0)) {
          c = u0, t = u1, e = w1;
        } else if (u0 == negate(w1)) {
          c = u0, t = u1, e = w0;
        } else if (u1 == negate(w0)) {
          c = u1, t = u0, e = w1;
        } else if (u1 == negate(w1)) {
          c = u1, t = u0, e = w0;
        } else {
          continue;
        }
        if (var_of(t) == var_of(e))
          continue;
        steps += 2;
        const int d1 = bve_find_sig(nsig, bve_sig2(c, negate(t)));
        if (d1 < 0)
          continue;
        const int d2 = bve_find_sig(nsig, bve_sig2(negate(c), negate(e)));
        if (d2 < 0)
          continue;
        pos_gate[psig[i].second] = 1;
        pos_gate[psig[j].second] = 1;
        neg_gate[size_t(d1)] = 1;
        neg_gate[size_t(d2)] = 1;
        return true;
      }
    }
    return false;
  };
  if ((k.bve_gate_mask & BVE_GATE_ITE) && try_ite())
    return BVE_GATE_ITE;

  // (4) XOR x ↔ a₁⊕…⊕aₘ: ALL 2^m clauses of the parity relation over the pivot
  //     and m ≤ BVE_XOR_ARITY_LIMIT other variables must be present, 2^(m−1) on
  //     each side (cadical gates.cpp find_xor_gate, option elimxorlim). A base
  //     clause fixes the variable set and the relation's parity R; the 2^m
  //     falsified assignments then name the 2^m clauses exactly, so the search
  //     is a fixed enumeration with an early exit on the first miss.
  auto try_xor = [&]() -> bool {
    uint32_t gate_pos[1u << BVE_XOR_ARITY_LIMIT];
    uint32_t gate_neg[1u << BVE_XOR_ARITY_LIMIT];
    for (const std::pair<GateSig, uint32_t> &base : psig) {
      const uint32_t m = base.first.n; // arity
      if (m < 2)                       // m == 1 is an equivalence (case 1)
        continue;
      // R: the relation is x ⊕ a₁ ⊕ … ⊕ aₘ = R. `base` (a pos-side clause, so
      // x = 0 in its falsifying assignment) is falsified by the assignment
      // "aⱼ true iff base.l[j] is negative", whose parity is p₀; a clause of
      // the relation forbids exactly the parity-(1⊕R) assignments, so R = 1⊕p₀.
      uint32_t p0 = 0;
      for (uint32_t j = 0; j < m; j++)
        p0 ^= static_cast<uint32_t>(negated(base.first.l[j]));
      const uint32_t r = 1u ^ (p0 & 1u);
      uint32_t np = 0, nn = 0;
      bool all = true;
      GateSig probe;
      probe.n = m;
      for (uint32_t a = 0; a < (1u << m) && all; a++) {
        uint32_t par = 0;
        for (uint32_t j = 0; j < m; j++) {
          const uint32_t bit = (a >> j) & 1u;
          par ^= bit;
          // aⱼ true ⇒ the clause carries its NEGATIVE literal
          probe.l[j] = make_lit(var_of(base.first.l[j]), bit != 0);
        }
        std::sort(probe.l, probe.l + m);
        // x's value in this falsified assignment: total parity must be 1⊕R
        const uint32_t xv = (1u ^ r) ^ par;
        const int hit = xv ? bve_find_sig(nsig, probe) : bve_find_sig(psig, probe);
        if (hit < 0)
          all = false;
        else if (xv)
          gate_neg[nn++] = static_cast<uint32_t>(hit);
        else
          gate_pos[np++] = static_cast<uint32_t>(hit);
      }
      if (!all)
        continue;
      for (uint32_t i = 0; i < np; i++)
        pos_gate[gate_pos[i]] = 1;
      for (uint32_t i = 0; i < nn; i++)
        neg_gate[gate_neg[i]] = 1;
      return true;
    }
    return false;
  };
  if ((k.bve_gate_mask & BVE_GATE_XOR) && try_xor())
    return BVE_GATE_XOR;
  return definition();
}

// Eliminate variable x by resolution. When a syntactic gate defines x, only
// gate×antecedent resolvents are generated (bve_detect_gate); otherwise the full
// cross product. At
// a drained root. State is not mutated until the bound is passed, so a NoOp
// return (bound exceeded / duplicate-binary hazard) leaves the kernel untouched
// and the caller may try another candidate. Returns Unsat on an empty resolvent
// or contradictory collapsed units.
ApplyResult bve_eliminate(Kernel &k, Var x, BveDefScratch *def) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit px = make_lit(x, false), nx = make_lit(x, true);
  // ATTEMPTED: retire the retry bit before doing the work, so a variable whose
  // resolvents blow the bound is not re-tried until something about its clauses
  // changes (kissat eliminate.c eliminate_variable, same position). A SUCCESSFUL
  // elimination re-sets it via its own clause deletions, which is harmless — the
  // variable then occurs nowhere and the schedule drops it anyway.
  k.bve_retry[x] = 0;

  std::vector<MClause> pos, neg;            // live irredundant: resolved + witnessed
  std::vector<Ref> del_large;               // redundant/satisfied/taut → delete-only
  std::vector<std::pair<Lit, Lit>> del_bin; // satisfied binaries → delete-only

  // ---- gather x's LARGE clauses ----
  // per-clause classification: tautology/redundant → delete-only; root-satisfied
  // → delete-only; else materialize active literals (root-false stripped, cited)
  // into pos (has px) or neg (has nx).
  auto classify = [&](Ref ref, const Clause *c, bool has_px, bool has_nx) {
    if ((has_px && has_nx) || c->redundant()) {
      del_large.push_back(ref);
      return;
    }
    MClause m;
    m.id = c->id();
    m.ref = ref;
    bool satisfied = false;
    for (const Lit l : *c) {
      const int8_t v = k.value(l);
      if (v > 0) { // root-true → clause satisfied → delete-only, no witness
        satisfied = true;
        break;
      }
      if (v < 0) { // root-false → drop (cite its unit)
        if (lrat)
          m.absorbed.push_back(k.unit_ids[var_of(l)]);
        continue;
      }
      m.active.push_back(l);
    }
    if (satisfied)
      del_large.push_back(ref);
    else
      (has_px ? pos : neg).push_back(std::move(m));
  };
  if (k.dense) {
    // dense fast-path (increment 2): x's large clauses are exactly the entries
    // of dense->large[px] ∪ [nx] (irredundant only — sound because a dense
    // phase runs at a quiescent root where every large clause is irredundant;
    // learned clauses do not exist yet). O(occ(x)), not O(arena). A clause with
    // both px and nx is in both lists — process it once via the px pass and
    // skip it in the nx pass.
    auto has_lit = [](const Clause *c, Lit t) {
      for (const Lit l : *c)
        if (l == t)
          return true;
      return false;
    };
    for (const Ref ref : k.dense->large[px]) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage())
        classify(ref, c, true, has_lit(c, nx));
    }
    for (const Ref ref : k.dense->large[nx]) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage() && !has_lit(c, px)) // px-side already handled tautologies
        classify(ref, c, false, true);
    }
  } else {
    // N1 fallback: one arena scan (any state, no phase index).
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_words(c->size);
      if (!c->garbage()) {
        bool has_px = false, has_nx = false;
        for (const Lit l : *c) {
          has_px |= (l == px);
          has_nx |= (l == nx);
        }
        if (has_px || has_nx)
          classify(ref, c, has_px, has_nx);
      }
      ref += words;
    }
  }

  // ---- gather x's BINARIES (watch scan, DEDUPLICATED) ----
  // Duplicate copies of a pair collapse to ONE logical clause: both copies
  // share one binary_ids id (kernel.hpp invariant), the reconstruction
  // witness wants one entry, and deletion goes through
  // kernel_delete_binary_all_copies (the Factor precedent — one proof `del`
  // per copy, LRAT-fuzz-proven). Do NOT abort the whole candidate on the
  // per-copy hazard ("the singular kernel_delete_binary cannot name a
  // duplicated pair"): a formula whose input already contains many duplicated
  // binary pairs then silently gets NO eliminations at all. Dedup also makes
  // `base` count DISTINCT clauses, matching kissat's DENSE deduped
  // occurrences.
  std::vector<uint8_t> seen_bin(2 * k.nvars, 0);
  std::vector<Lit> touched_bin;
  auto gather_bin = [&](Lit lit, std::vector<MClause> &live) {
    touched_bin.clear();
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (!w.binary())
        continue;
      const Lit o = w.other();
      if (seen_bin[o]) // duplicate copy: same logical clause, same id
        continue;
      seen_bin[o] = 1;
      touched_bin.push_back(o);
      if (k.value(o) > 0) { // satisfied
        del_bin.emplace_back(lit, o);
        continue;
      }
      // o root-false is impossible at a drained fixpoint (it would force `lit`).
      MClause m;
      m.ref = INVALID_REF;
      m.ba = lit;
      m.bb = o;
      m.id = kernel_binary_id(k, lit, o);
      m.active = {lit, o};
      live.push_back(std::move(m));
    }
    for (const Lit o : touched_bin) // clear for the other polarity's gather
      seen_bin[o] = 0;
  };
  gather_bin(px, pos);
  gather_bin(nx, neg);

  if (pos.empty() && neg.empty() && del_large.empty() && del_bin.empty()) {
    return ApplyResult::NoOp; // x occurs nowhere resolvable
  }

  // ---- syntactic gate detection: restrict resolvents to gate×antecedent ----
  std::vector<uint8_t> pos_gate(pos.size(), 0), neg_gate(neg.size(), 0);
  bool resolve_gate = false;
  if (def) {
    def->checked = false;
    def->exhausted = false;
    def->ticks = 0;
  }
  const uint32_t gate_kind = bve_detect_gate(k, px, nx, pos, neg, pos_gate,
                                             neg_gate, resolve_gate, def);
  const bool gate = gate_kind != 0;
  if (def && def->checked) {
    k.stats.bve_definitions_checked++;
    k.stats.bve_definition_ticks += def->ticks;
    k.stats.bve_definition_budget += def->exhausted ? 1 : 0;
  }
  // The (★) argument above is load-bearing, not descriptive, and it needs EVERY
  // pair with a gate clause in it. The four syntactic shapes are allowed to drop
  // the gate x gate pairs only because those are tautologies; a type that
  // cannot say that must ask for them back through `resolve_gate`. Pin which
  // types may, so an inexact matcher (kissat's shortened-clause ITE path) has
  // to be a deliberate edit rather than a silent one.
  assert((!resolve_gate || gate_kind == BVE_GATE_DEFINITION) &&
         "a syntactic gate asked for its gate x gate resolvents back: its "
         "detection is no longer exact, so (star) must be re-argued");
  if (gate) {
    // Reachability + acceptance-lever readout (ledger SP-BVE-GATE-1): the gate
    // is what shrinks the resolvent set the growth bound is applied to, so the
    // pairs it removes are the quantity the bound's MEANING changes by. Counted
    // per ATTEMPT — an over-budget attempt is exactly where the lever matters.
    const size_t gp = size_t(std::count(pos_gate.begin(), pos_gate.end(), 1));
    const size_t gn = size_t(std::count(neg_gate.begin(), neg_gate.end(), 1));
    const size_t charged =
        gp * (neg.size() - gn) + (pos.size() - gp) * gn +
        (resolve_gate ? gp * gn : 0);
    k.stats.bve_gate_pairs_saved += pos.size() * neg.size() - charged;
    switch (gate_kind) {
    case BVE_GATE_EQUIV: k.stats.bve_gates_equiv++; break;
    case BVE_GATE_AND_OR: k.stats.bve_gates_and_or++; break;
    case BVE_GATE_ITE: k.stats.bve_gates_ite++; break;
    case BVE_GATE_XOR: k.stats.bve_gates_xor++; break;
    default: k.stats.bve_gates_definition++; break;
    }
  }

  // ---- generate resolvents (gate×antecedent, else full cross product) + bound ----
  struct Resolvent {
    std::vector<Lit> lits;
    std::vector<uint64_t> chain;
  };
  std::vector<Resolvent> resolvents;
  const size_t base = pos.size() + neg.size();
  const size_t commit_limit = base + bve_bound(k); // eliminate iff ≤ this
  // Needed-bound tracking (DefaultPolicy ramp): keep resolving PAST the commit
  // slack, up to the ramp ceiling, so a rejected variable's EXACT needed slack
  // is learned and cached (Kernel::bve_min_bound) — the policy then skips it at
  // every slack below what it needs instead of re-resolving it at each ramp
  // level. Off (byte-identical to the pre-cache abort) unless the policy asked.
  const size_t gen_limit =
      k.bve_track_min_bound
          ? base + std::max<uint32_t>(bve_bound(k), BVE_MAX_BOUND)
          : commit_limit;
  bool oversized = false; // a resolvent exceeded BVE_CLSLIM → never eliminable
  bool over_gen = false;  // resolvent count passed gen_limit → needs > ceiling
  std::vector<uint8_t> seen(2 * k.nvars, 0); // dedup/tautology scratch
  for (size_t ci = 0; ci < pos.size() && !oversized && !over_gen; ++ci) {
    const MClause &c = pos[ci];
    for (const Lit l : c.active)
      if (l != px)
        seen[l] = 1;
    for (size_t di = 0; di < neg.size(); ++di) {
      if (gate) {
        // resolve gate×antecedent; gate×gate only for an incomplete gate
        // (resolve_gate), antecedent×antecedent never (entailed by the gate).
        const bool cg = pos_gate[ci], dg = neg_gate[di];
        if (!((cg != dg) || (cg && dg && resolve_gate)))
          continue;
      }
      const MClause &d = neg[di];
      k.stats.bve_resolutions++; // one resolvent attempt (kissat resolve.c:157)
      Resolvent r;
      bool taut = false;
      for (const Lit l : c.active)
        if (l != px)
          r.lits.push_back(l);
      for (const Lit l : d.active) {
        if (l == nx)
          continue;
        if (seen[l]) // duplicate with c
          continue;
        if (seen[negate(l)]) { // l and ¬l both present → tautology
          taut = true;
          break;
        }
        r.lits.push_back(l);
      }
      if (taut)
        continue;
      if (r.lits.size() > BVE_CLSLIM) { // oversized resolvent: abort the variable
        oversized = true;
        break;
      }
      if (lrat) {
        r.chain = c.absorbed;
        r.chain.insert(r.chain.end(), d.absorbed.begin(), d.absorbed.end());
        r.chain.push_back(c.id);
        r.chain.push_back(d.id);
      }
      resolvents.push_back(std::move(r));
      if (resolvents.size() > gen_limit) { // past the generation ceiling: stop
        over_gen = true;
        break;
      }
    }
    for (const Lit l : c.active)
      if (l != px)
        seen[l] = 0;
  }
  // Growth decision. Eliminate iff not oversized and within the COMMIT slack;
  // otherwise keep x, and (when tracking) cache the exact slack it needs so the
  // ramp can skip it precisely. An oversized resolvent or growth past the
  // ceiling caps at UINT16_MAX = "never within any ramp level".
  if (oversized || resolvents.size() > commit_limit) {
    if (k.bve_track_min_bound)
      k.bve_min_bound[x] =
          oversized
              ? UINT16_MAX
              : static_cast<uint16_t>(
                    std::min<size_t>(resolvents.size() - base, UINT16_MAX));
    return ApplyResult::NoOp; // no state mutated yet
  }

  // ---- COMMIT: add resolvents (RUP, add-before-delete), witnesses, delete ----
  // Batched root units + opposite-sign refutation: the shared kernel idiom
  // (PendingUnits, kernel.hpp — corpus 007 is the mandating regression).
  PendingUnits pending;
  pending.init(k.nvars);
  auto record_unit = [&](Lit u, uint64_t id) -> bool {
    return pending.record(k, u, id) == PendingUnits::Outcome::Refuted;
  };

  for (Resolvent &r : resolvents) {
    if (r.lits.empty()) { // unreachable at a drained root; defensive
      if (k.proof) {
        if (lrat)
          k.proof->add_empty(++k.next_clause_id, r.chain.data(),
                             static_cast<uint32_t>(r.chain.size()));
        else
          k.proof->add_empty(0, nullptr, 0);
      }
      k.inconsistent = true;
      return ApplyResult::Unsat;
    }
    // a size-2 resolvent already present is subsumed — skip (avoid a duplicate
    // pair that would block a later elimination).
    if (r.lits.size() == 2 && kernel_binary_live(k, r.lits[0], r.lits[1]))
      continue;
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) {
      if (lrat)
        k.proof->add_rup(id, r.lits.data(), static_cast<uint32_t>(r.lits.size()),
                         r.chain.data(), static_cast<uint32_t>(r.chain.size()));
      else
        k.proof->add_rup(0, r.lits.data(), static_cast<uint32_t>(r.lits.size()),
                         nullptr, 0);
    }
    if (r.lits.size() == 1) {
      if (record_unit(r.lits[0], id))
        return ApplyResult::Unsat;
    } else if (r.lits.size() == 2) {
      kernel_add_binary_clause(k, r.lits[0], r.lits[1], id);
    } else {
      kernel_add_large_clause(k, r.lits.data(),
                              static_cast<uint32_t>(r.lits.size()),
                              /*redundant=*/false,
                              static_cast<uint32_t>(r.lits.size()), id);
      // NB: adding a large clause may reallocate the arena, but the Refs held in
      // pos/neg/del_large are OFFSETS (valid across a resize) — only Clause*
      // would dangle, and we re-fetch via clause_at below.
    }
  }

  // reconstruction witnesses: push each removed live clause with the pivot as
  // its blocking literal (both sides; CaDiCaL-style, design §3.4). Satisfied /
  // redundant / tautology clauses need no witness (entailed / already true).
  for (const std::vector<MClause> *side : {&pos, &neg}) {
    const Lit pivot = (side == &pos) ? px : nx;
    for (const MClause &m : *side) {
      std::vector<Lit> rest;
      rest.reserve(m.active.size());
      for (const Lit l : m.active)
        if (l != pivot)
          rest.push_back(l);
      kernel_push_witness(k, pivot, rest.data(),
                          static_cast<uint32_t>(rest.size()));
    }
  }

  // delete every clause on x: live (pos/neg) + delete-only (redundant/satisfied/
  // tautology). Large → proof del + GARBAGE; binary →
  // kernel_delete_binary_all_copies (gathers are deduplicated, so each pair is
  // deleted once — the primitive removes every copy and emits one proof `del`
  // per copy; identical to the singular delete when copies == 1).
  auto del_large_ref = [&](Ref ref) {
    Clause *c = clause_at(k.arena, ref);
    if (c->garbage())
      return;
    kernel_delete_clause(k, c);
  };
  auto del_all = [&](const std::vector<MClause> &side) {
    for (const MClause &m : side) {
      if (m.ref != INVALID_REF) {
        del_large_ref(m.ref);
      } else {
        const bool ok = kernel_delete_binary_all_copies(k, m.ba, m.bb);
        assert(ok);
        (void)ok;
      }
    }
  };
  del_all(pos);
  del_all(neg);
  for (const Ref ref : del_large)
    del_large_ref(ref);
  for (const auto &pr : del_bin) {
    const bool ok = kernel_delete_binary_all_copies(k, pr.first, pr.second);
    assert(ok);
    (void)ok;
  }

  // assert collapsed root units un-propagated; M1 owns the cascade.
  for (const Lit u : pending.units)
    if (k.value(u) == 0)
      kernel_assign_unit(k, u);

  k.stats.bve_eliminated++;
  return ApplyResult::Ok;
}

} // namespace

// canonical instance = a round: eliminate the best schedulable candidate that is
// within the bound (fewest-resolutions-first). Drains the root closure at entry.
ApplyResult bve_apply(Kernel &k) {
  assert(bve_guard(k));
  k.stats.bve_rounds++;
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  std::vector<Var> cand;
  bve_schedule(k, cand);
  BveDefScratch def; // one per ROUND: every candidate reuses its allocations
  for (const Var x : cand) {
    const ApplyResult r = bve_eliminate(k, x, &def);
    if (r == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    if (r == ApplyResult::Ok) {
      // reclaim the clauses this elimination garbaged. Reclamation is NOT part
      // of the rule's effect (deletes are lazy and every reader skips garbage,
      // so never collecting is sound) — it is the caller's cost/memory choice.
      // The canonical round stays self-contained; the apply_instance dosing
      // path leaves it to the scheduler, which reclaims once per phase instead
      // of once per eliminated variable (O(arena) each —).
      kernel_collect_garbage(k);
      return ApplyResult::Ok; // eliminated one variable
    }
    // NoOp (bound / hazard): try the next candidate
  }
  return ApplyResult::NoOp;
}

// instance surface: payload = a candidate variable via its positive DIMACS
// literal (lit_instance); INSTANCE_CANONICAL = the full round.
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
  assert(bve_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (k.values[l] != 0) // the requested var got assigned by the drain
    return ApplyResult::NoOp;
  BveDefScratch def;
  return bve_eliminate(k, var_of(l), &def);
}

TACTIX_REGISTER_RULE(BVE, RuleTier::Inprocess, bve_guard, bve_apply,
                     bve_instances, bve_apply_instance);

} // namespace tactix

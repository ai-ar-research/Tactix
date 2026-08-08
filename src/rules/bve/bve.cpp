// BVE, rule E1: the occurrence index, the candidate schedule, the elimination
// round and the gate reduction. Transliterated idioms from
// kissat/src/eliminate.c + resolve.c + definition.c; the add-before-delete /
// root-false-strip / record_unit / witness mechanics follow V3 Substitute.
// See docs/design/e1-bve-design.md and bve.hpp.
#include "rules/bve/bve.hpp"

#include "engine/subsolver.hpp"
#include "kernel/watch.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace tactix {

// occurrence limit (kissat eliminateocclim): skip a variable whose polarities
// together occur more than this, where O(occ⁺·occ⁻) resolution blows up
static constexpr uint32_t BVE_OCCLIM = 2000;
// cadical `elimocclim` (options.hpp:100), the only effort bound inside the
// backward pass: skip a subsumer whose least-occurring literal is commoner
// than this (backward.cpp:70).
static constexpr size_t BVE_BWD_OCCLIM = 100;
// resolvent size limit (kissat eliminateclslim): a larger resolvent aborts
static constexpr uint32_t BVE_CLSLIM = 100;
// Growth slack: eliminate iff #non-tautological resolvents ≤ occ⁺ + occ⁻ +
// bound. `Kernel::bve_bound` is the one channel for it — no in-rule env pin
// may override a policy's write — and DefaultPolicy ramps it cheap-first, so
// the eliminations that do not grow the formula happen first (kissat
// eliminate.c set_next_elimination_bound).
static uint32_t bve_bound(const Kernel &k) { return k.bve_bound; }

bool bve_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void bve_build_occurrences(const Kernel &k, BveOccurrences &occ) {
  const uint32_t nlits = 2 * k.nvars;
  occ.counts.assign(nlits, 0);
  // Large irredundant clauses only: BVE eliminates over F, not over R.
  if (k.dense) {
    // dense fast-path: the shared index already lists each active literal's
    // irredundant large clauses — O(occ), no arena scan
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
  // binaries: one watch-list scan per active literal. They have an
  // irredundant lifecycle in this kernel, so all of them count.
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
    // Extension variables (E2 Factor) must never be eliminated: resolving x's
    // dividers against its quotients rebuilds the original m·n grid, exactly
    // reversing Factor's compression, so the two rules would thrash.
    if (v >= kernel_num_output_vars(k))
      continue;
    // The retry bitmap (Kernel::bve_retry) is deliberately not consulted here:
    // this rule maintains the advice, the policy reads it. See bve.hpp and
    // docs/implementation-notes.md §6.
    const Lit pl = make_lit(v, false);
    if (k.values[pl] != 0) // assigned variable: not an elimination candidate
      continue;
    const uint32_t p = occ.count(pl), n = occ.count(make_lit(v, true));
    if (p == 0 && n == 0) // occurs in no elimination clause: nothing to do
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

// A materialized clause on the pivot: its active literals (root-false dropped,
// their unit ids kept in `absorbed` for LRAT), its proof id, and a delete
// handle. `active` includes the pivot.
struct MClause {
  std::vector<Lit> active;
  std::vector<uint64_t> absorbed;
  uint64_t id = 0;
  Ref ref = INVALID_REF; // INVALID_REF => binary
  Lit ba = INVALID_LIT, bb = INVALID_LIT;
};

// Reusable state for definition extraction, owned by `bve_eliminate`'s caller:
// a round creates one and every candidate reuses it, so no per-attempt
// O(nvars) fill-init happens inside a per-variable loop. A null pointer means
// definition extraction is unavailable here.
struct BveDefScratch {
  SubSolver solver;
  std::vector<Var> vars;    // sorted distinct non-pivot vars = the sub var space
  std::vector<SubLit> lits; // one reduced clause, being encoded
  SubDerivation deriv;
  uint64_t ticks = 0;    // ticks the last attempt spent (0 if none was made)
  bool checked = false;  // an attempt was made at all
  bool exhausted = false; // ...and the tick ceiling cut it short
};

// ---- gate detection (equivalence, AND/OR, ITE, XOR, general definition) ----
// Mark the clauses of a functional gate defining x in pos_gate/neg_gate
// (1 = gate, 0 = antecedent). Every detector must establish condition (★) of
// docs/calculus.md §6b E1 exactly; a false gate drops needed resolvents. Only
// the definition type sets `resolve_gate` (its gate×gate resolvents are real);
// the four syntactic types are exact CNFs of x↔φ, so ITE and XOR are matched
// exactly and kissat's shortened-clause `match_ternary_ref` path is not ported.

// Sorted signature of a clause's non-pivot active literals, for the exact
// lookups ITE and XOR need. Only clauses with 2..BVE_XOR_ARITY_LIMIT non-pivot
// literals and no repeated variable are indexed: a repeat means a duplicated
// literal or a tautology, and neither may stand in for a gate clause.
constexpr uint32_t BVE_SIG_MAX = BVE_XOR_ARITY_LIMIT;
struct GateSig {
  uint32_t n = 0;
  Lit l[BVE_SIG_MAX] = {0};
  bool operator<(const GateSig &o) const {
    if (n != o.n)
      return n < o.n;
    // Fixed trip count over the whole array, not over `n`: with n == o.n the
    // tail is zero on both sides and cannot change the order, and the fixed
    // bound keeps std::sort's by-value pivot copy provably initialized to the
    // optimizer (-Wmaybe-uninitialized at -O3 otherwise).
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
// or -1; duplicate signatures are copies of one logical clause, either of
// which serves as the gate clause
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

// General definition x ↔ φ, the one type that is not a shape match. Hand the
// pivot's occurrence lists, each clause minus the pivot literal, to the
// sub-solver: refuting that set is exactly (★) for G⁺ = P, G⁻ = N, and the
// core names the sub-multiset the refutation used, so (★) holds for the core
// alone and the core is the gate (kissat `definition.c`). Nothing from the
// sub-solver enters the proof: the core only picks which resolvents are
// generated, each still a resolution of two clauses of F. Not ported:
// `definitioncores` > 1 (a fixed decision order and no shuffle make a second
// solve reproduce the first core), and the one-sided-core failed literal,
// whose forced unit is entailed but not RUP.
uint32_t bve_find_definition(const Kernel &k, Lit px,
                             const std::vector<MClause> &pos,
                             const std::vector<MClause> &neg,
                             std::vector<uint8_t> &pos_gate,
                             std::vector<uint8_t> &neg_gate,
                             BveDefScratch &s) {
  // One-sided pivots already have an empty cross product: nothing for a gate
  // to reduce, and (★) would be asking to refute one side alone.
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
  // Ids are handed out from 1 in add order, so id ∈ [1, |pos|] names
  // pos[id-1] and the rest neg[id-|pos|-1]. That mapping holds because
  // `add_clause` consumes an id even for a clause it drops as a tautology.
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

// Returns the BVE_GATE_* bit of the type found, or 0 for "no gate". `k` is
// const by design: detection is read-only and the caller owns the counters,
// with the definition attempt reporting its cost back through `def`.
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

  // (2) AND/OR: a base clause on one side whose every input has the matching
  // binary on the other; the base plus those binaries fully define x.
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
  // exact-match index over both sides, built once and shared by ITE and XOR;
  // the pos/neg MClause vectors are already materialized, so it is O(P+N) and
  // needs no O(nvars) scratch
  SigIndex psig, nsig;
  bve_build_sig_index(pos, px, psig);
  bve_build_sig_index(neg, nx, nsig);

  // (3) ITE: two pivot-side ternaries whose non-pivot pairs clash on exactly
  // the condition literal, plus the matching pair on the other side (kissat
  // ifthenelse.c kissat_find_if_then_else_gate). One pass over the pos side
  // suffices, the shape being symmetric in the pivot sign. Work bound (kissat
  // ifthenelse.c:105-113), sound at any value.
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
        // the pairs must clash on exactly one literal (the condition) and the
        // branch literals must be on different variables — equal ones make
        // this an XOR/equivalence, not an ITE (cadical's `abs` reject)
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

  // (4) XOR: all 2^m parity clauses over the pivot and m other variables must
  //     be present (cadical gates.cpp find_xor_gate). A base clause fixes the
  //     variable set and the parity R, and the 2^m falsified assignments then
  //     name the 2^m clauses exactly, so this is a fixed enumeration.
  auto try_xor = [&]() -> bool {
    uint32_t gate_pos[1u << BVE_XOR_ARITY_LIMIT];
    uint32_t gate_neg[1u << BVE_XOR_ARITY_LIMIT];
    for (const std::pair<GateSig, uint32_t> &base : psig) {
      const uint32_t m = base.first.n; // arity
      if (m < 2)                       // m == 1 is an equivalence (case 1)
        continue;
      // The relation is x ⊕ a₁ ⊕ … ⊕ aₘ = R. `base` is a pos-side clause, so
      // x = 0 in its falsifying assignment "aⱼ true iff base.l[j] is
      // negative", of parity p₀; a relation clause forbids exactly the
      // parity-(1⊕R) assignments, hence R = 1⊕p₀.
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

// --- eager backward subsumption from fresh resolvents -----------------------
// CaDiCaL `elimbackward` (backward.cpp), drained at the tail of
// try_to_eliminate_variable (elim.cpp:731). Each fresh resolvent is the
// SUBSUMER over the occurrence list of its least-occurring literal: a victim
// `d ⊇ c` is deleted, one resolving with `c` on a single literal is shortened,
// and shortened victims re-enter the queue so a pivot reaches a fixpoint.
// kissat has no analogue; see `docs/implementation-notes.md`'s BVE section.

// Direction is the easy thing to invert: C1/C2's `check_marked` answers "does
// the partner subsume the candidate", the reverse of what is needed here.
// Deleting on that predicate retires the STRONGER clause, and no checker
// catches it — a `del` is unchecked, so the certificate verifies while the
// solver answers SAT on an UNSAT formula.
static void bve_backward(Kernel &k, std::vector<Ref> &queue) {
  if (!k.dense || k.inconsistent)
    return; // N1: the index is an acceleration, and the rule runs without it
  std::vector<uint8_t> marks(2 * static_cast<size_t>(k.nvars) + 2, 0);
  std::vector<Lit> clits, dlits;
  std::vector<Ref> victims;
  const bool lrat = k.proof && k.proof->lrat();
  for (size_t qi = 0; qi < queue.size() && !k.inconsistent; ++qi) {
    const Ref cref = queue[qi];
    {
      const Clause *c = clause_at(k.arena, cref);
      if (c->garbage())
        continue;
      // Snapshot: adding a strengthened victim can reallocate the arena, so a
      // Clause* held across the victim walk would dangle.
      clits.assign(c->begin(), c->end());
    }
    const uint64_t cid = clause_at(k.arena, cref)->id();
    uint32_t size = 0;
    Lit best = INVALID_LIT;
    size_t len = 0;
    bool satisfied = false;
    for (const Lit l : clits) {
      const int8_t v = k.value(l);
      if (v > 0) { satisfied = true; break; }
      if (v < 0)
        continue; // root-false: absorbed, and never marked
      marks[l] = 1;
      size++;
      const size_t n = k.dense->large[l].size();
      if (best == INVALID_LIT || n < len) { best = l; len = n; }
    }
    if (satisfied || best == INVALID_LIT || size == 0 || len > BVE_BWD_OCCLIM) {
      // A root-satisfied subsumer is garbage, not merely unusable — a fresh
      // resolvent goes root-true the moment a sibling unit resolvent is
      // asserted (cadical backward.cpp:66-69 marks it garbage here too).
      if (satisfied)
        kernel_delete_clause(k, clause_at(k.arena, cref));
      for (const Lit l : clits) marks[l] = 0;
      continue;
    }
    k.stats.bve_bwd_queued++;
    bool unit_derived = false;
    victims.assign(k.dense->large[best].begin(), k.dense->large[best].end());
    for (const Ref dref : victims) {
      if (dref == cref || k.inconsistent)
        continue;
      Clause *d = clause_at(k.arena, dref);
      if (d->garbage() || d->redundant() || d->size < size)
        continue;
      // c ⊆ d (subsume), or c ⊆ d with exactly one literal flipped (strengthen).
      uint32_t found = 0;
      Lit negated = INVALID_LIT;
      bool dsat = false, bail = false;
      for (const Lit l : *d) {
        const int8_t v = k.value(l);
        if (v > 0) { dsat = true; break; }
        if (v < 0)
          continue;
        if (marks[l]) { found++; continue; }
        if (marks[negate(l)]) {
          if (negated != INVALID_LIT) { bail = true; break; }
          negated = l;
          found++;
        }
      }
      if (bail || (!dsat && found != size))
        continue;
      if (dsat || negated == INVALID_LIT) {
        // satisfied, or every literal of c occurs in d with the same sign.
        // `d` is never a trail reason, which is what makes the deletion safe
        // without a reason check: `d` comes from `occs(best)` so it contains
        // `best`, and `best` is unassigned by construction — a reason has all
        // its literals assigned. `kernel_delete_clause` does not retire a
        // reason, and the dosed path does not collect until phase exit, so a
        // garbage reason would survive to be cited by a later chain.
        kernel_delete_clause(k, d);
        if (!dsat)
          k.stats.bve_bwd_subsumed++;
        continue;
      }
      // self-subsuming resolution: d resolved with c on `negated` gives
      // d \ {negated}, which strictly subsumes d. `negated != best`, since d
      // holds `best` and a clause holding both polarities is a tautology — so
      // the strengthened clause keeps an unassigned literal and the
      // not-a-reason argument above survives the rewrite (cadical asserts the
      // same thing at backward.cpp:201).
      if (k.subsume_shadow)
        kernel_shadow_retire(k, d->id());
      dlits.clear();
      std::vector<uint64_t> chain;
      for (const Lit l : *d) {
        if (l == negated)
          continue;
        if (k.value(l) < 0) {
          if (lrat)
            chain.push_back(k.unit_ids[var_of(l)]);
          continue;
        }
        dlits.push_back(l);
      }
      if (lrat) {
        // The SUBSUMER's root-false literals need their unit ids too, or `c`
        // is not unit under the RUP assumption and the line is unverifiable —
        // a fresh resolvent can carry a literal falsified by a sibling unit
        // resolvent asserted before this drain (cadical collects units over
        // {c\d} ∪ {d\c}, backward.cpp:149-182). Dedup against d's, which are
        // already in the chain.
        for (const Lit l : clits)
          if (k.value(l) < 0) {
            const uint64_t uid = k.unit_ids[var_of(l)];
            bool dup = false;
            for (const uint64_t have : chain)
              dup |= (have == uid);
            if (!dup)
              chain.push_back(uid);
          }
      }
      const uint64_t id = ++k.next_clause_id;
      if (k.proof) {
        if (lrat) {
          chain.push_back(cid);      // the subsumer propagates `negated` false
          chain.push_back(d->id());  // then the victim conflicts
          k.proof->add_rup(id, dlits.data(),
                           static_cast<uint32_t>(dlits.size()), chain.data(),
                           static_cast<uint32_t>(chain.size()));
        } else {
          k.proof->add_rup(0, dlits.data(),
                           static_cast<uint32_t>(dlits.size()), nullptr, 0);
        }
      }
      k.stats.bve_bwd_strengthened++;
      Ref nref = INVALID_REF;
      switch (kernel_replace_clause(k, d, id, dlits.data(),
                                    static_cast<uint32_t>(dlits.size()),
                                    /*redundant=*/false, 0, &nref)) {
      case ReplaceOutcome::Refuted:
        k.inconsistent = true;
        break;
      case ReplaceOutcome::Unit:
        // kernel_replace_clause owns only the deletion — unit semantics stay
        // with the caller (kernel.hpp), and the victim is already gone, so
        // dropping the literal here silently weakens the formula (wrong-SAT
        // class, invisible to every proof check of an UNSAT run). Root-false
        // literals were stripped into `dlits` above, so the literal is
        // unassigned or already root-true; assert it un-propagated — M1 owns
        // the cascade, the same contract as bve_eliminate's pending drain.
        k.stats.bve_bwd_units++;
        if (k.value(dlits[0]) == 0) {
          k.unit_ids[var_of(dlits[0])] = id;
          kernel_assign_unit(k, dlits[0]);
        }
        // A unit invalidates this subsumer's marks/size snapshot (its own
        // literals may now be assigned): abandon the walk as cadical does
        // (backward.cpp:195) and let the queue re-check at the next entry.
        unit_derived = true;
        break;
      case ReplaceOutcome::Binary:
        break; // leaves the dense index; no longer a victim or a subsumer here
      case ReplaceOutcome::Large:
        if (nref != INVALID_REF)
          queue.push_back(nref); // fixpoint, cadical backward.cpp:202
        break;
      }
      if (unit_derived)
        break;
    }
    for (const Lit l : clits) marks[l] = 0;
  }
}

// Eliminate variable x by resolution at a drained root. No state is mutated
// until the bound is passed, so a NoOp return leaves the kernel untouched and
// the caller may try another candidate. Unsat on an empty resolvent or
// contradictory collapsed units.
ApplyResult bve_eliminate(Kernel &k, Var x, BveDefScratch *def) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit px = make_lit(x, false), nx = make_lit(x, true);
  // Retire the retry bit before doing the work, so a variable whose resolvents
  // blow the bound is not re-tried until its clauses change (kissat
  // eliminate.c eliminate_variable, same position).
  k.bve_retry[x] = 0;

  std::vector<MClause> pos, neg;            // live irredundant: resolved + witnessed
  std::vector<Ref> del_large;               // redundant/satisfied/taut → delete-only
  std::vector<std::pair<Lit, Lit>> del_bin; // satisfied binaries → delete-only

  // ---- gather x's large clauses ----
  // tautology / redundant / root-satisfied → delete-only; otherwise
  // materialize the active literals into pos (has px) or neg (has nx)
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
    // dense fast-path: x's large clauses are exactly dense->large[px] ∪ [nx],
    // irredundant only, sound because a dense phase runs at a quiescent root
    // with no learned clauses yet. A clause holding both px and nx is in both
    // lists and is handled in the px pass.
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

  // ---- gather x's binaries (watch scan, deduplicated) ----
  // Duplicate copies of a pair are one logical clause: they share a single
  // binary_ids id (kernel.hpp invariant), the witness wants one entry, and
  // kernel_delete_binary_all_copies removes them all. Dedup also makes `base`
  // count distinct clauses, matching kissat's deduped dense occurrences.
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

  // Occurrence blow-up (kissat `eliminateocclim` = 2000, resolve.c:282-290):
  // resolving a variable whose two polarities occur together this often costs
  // more than the elimination can repay. The cap sits HERE, on the apply, where
  // kissat's does — deliberately NOT in `bve_schedule`, because that is what
  // `bve_instances` enumerates, and a COST cap there withholds an applicable
  // instance from the public API (N1, `RV-S17`). The rule offers the candidate
  // and declines it; it never fails to offer. `pos`/`neg` are x's occurrence
  // lists, so this needs no second index.
  if (!pos.empty() && !neg.empty() && pos.size() + neg.size() > BVE_OCCLIM)
    return ApplyResult::NoOp;

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
  // (★) needs every pair holding a gate clause. A syntactic shape may drop the
  // gate×gate pairs only because they are tautologies; pinning which types may
  // makes an inexact matcher a deliberate edit rather than a silent one.
  assert((!resolve_gate || gate_kind == BVE_GATE_DEFINITION) &&
         "a syntactic gate asked for its gate x gate resolvents back: its "
         "detection is no longer exact, so (star) must be re-argued");
  if (gate) {
    // The gate shrinks the resolvent set the growth bound is applied to, so
    // the pairs it removes are what the bound's meaning changes by. Counted
    // per attempt: an over-budget attempt is where the lever matters.
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
  // Needed-bound tracking: resolve past the commit slack to the ramp ceiling,
  // caching a rejected variable's exact needed slack in Kernel::bve_min_bound
  // so the policy can skip it at every lower slack instead of re-resolving.
  // Off unless the policy asked, and then identical to the plain abort.
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
  // Growth decision: eliminate iff not oversized and within the commit slack.
  // An oversized resolvent or growth past the ceiling caches UINT16_MAX,
  // meaning "never within any ramp level".
  if (oversized || resolvents.size() > commit_limit) {
    if (k.bve_track_min_bound)
      k.bve_min_bound[x] =
          oversized
              ? UINT16_MAX
              : static_cast<uint16_t>(
                    std::min<size_t>(resolvents.size() - base, UINT16_MAX));
    return ApplyResult::NoOp; // no state mutated yet
  }

  // ---- commit: add resolvents (RUP, add-before-delete), witnesses, delete ---
  // Batched root units + opposite-sign refutation, the shared kernel idiom
  // (PendingUnits, kernel.hpp).
  PendingUnits pending;
  pending.init(k.nvars);
  std::vector<Ref> fresh; // large resolvents, the backward pass's subsumers
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
      fresh.push_back(kernel_add_large_clause(
          k, r.lits.data(), static_cast<uint32_t>(r.lits.size()),
          /*redundant=*/false, static_cast<uint32_t>(r.lits.size()), id));
      // Adding a large clause may reallocate the arena, but the Refs held in
      // pos/neg/del_large are offsets and survive a resize; only a Clause*
      // would dangle, and every use below re-fetches through clause_at.
    }
  }

  // reconstruction witnesses: each removed live clause, pivot as its blocking
  // literal, both sides (design §3.4). Satisfied, redundant and tautological
  // clauses need none — they are entailed or already true.
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
  // Deactivate the pivot with its witness, the V3 Substitute idiom (status and
  // ρ entry created together): both references retire it here — cadical
  // elim.cpp:723 flags.cpp:34-46, kissat eliminate.c:405 flags.c:77-100 — so
  // it leaves the decision structures and the active-variable count instead of
  // burning propagation-free decisions, and the phase fixpoint test sees BVE
  // progress. Reconstruction supplies its value either way.
  if (k.var_status.size() < k.nvars)
    k.var_status.resize(k.nvars, VarStatus::Active);
  if (k.var_status[x] == VarStatus::Active && k.values[px] == 0) {
    assert(k.unassigned_active > 0);
    k.unassigned_active--;
  }
  k.var_status[x] = VarStatus::Eliminated;
  k.retired_events.push_back(x); // the policy deactivates its own H from this

  // delete every clause on x: live (pos/neg) plus delete-only (redundant,
  // satisfied, tautology). Large → proof del + garbage; binary →
  // kernel_delete_binary_all_copies, called once per pair since the gathers
  // are deduplicated.
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

  // cadical drains here: after the resolvents are connected and the pivot's
  // own clauses are garbage, so the antecedents are skipped (elim.cpp:731).
  if (k.bve_backward)
    bve_backward(k, fresh);

  k.stats.bve_eliminated++;
  // The drain can refute (a strengthened victim's unit contradicting a root
  // value). An N1 API consumer trusts ApplyResult alone — returning Ok on an
  // inconsistent kernel invites a wrong SAT downstream (cadical's drain loop
  // guards on unsat for the same reason, backward.cpp:224).
  return k.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok;
}

} // namespace

// canonical instance = a round: eliminate the best schedulable candidate
// within the bound, draining the root closure at entry
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
      // Reclamation is the caller's cost choice, not part of the rule's
      // effect: deletes are lazy and every reader skips garbage. The canonical
      // round stays self-contained; the dosing path leaves it to the
      // scheduler, which reclaims once per phase.
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

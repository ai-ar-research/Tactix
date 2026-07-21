// E1 BVE: bounded variable elimination by resolution. E1a landed the skeleton +
// occurrence index + schedule; E1b (this commit) adds the elimination round —
// resolution, the bound, RUP resolvent emission, deletion, and reconstruction
// witnesses. Full cross product (no gates yet — E1c). Transliterated idioms from
// kissat/src/eliminate.c + resolve.c; the add-before-delete / root-false-strip /
// record_unit / witness mechanics mirror our own V3 Substitute (do_substitution),
// the fuzzed template. See docs/e1-bve-design.md.
#include "rules/bve/bve.hpp"

#include "kernel/watch.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cstdlib>
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
// DefaultPolicy ramps it 0,1,2,4,8,16 so the eliminations that do not grow the
// formula happen first — kissat eliminate.c set_next_elimination_bound (rev 53).
// TACTIX_BVE_BOUND pins it for A/B tuning, overriding the policy's ramp.
static uint32_t bve_bound(const Kernel &k) {
  if (const char *s = std::getenv("TACTIX_BVE_BOUND"))
    return static_cast<uint32_t>(std::atoi(s));
  return k.bve_bound;
}

bool bve_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void bve_build_occurrences(const Kernel &k, BveOccurrences &occ) {
  const uint32_t nlits = 2 * k.nvars;
  occ.large.assign(nlits, {});
  occ.binary.assign(nlits, {});
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
          occ.large[l].push_back(ref);
    }
  } else {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage() && !c->redundant())
        for (const Lit l : *c)
          if (k.values[l] == 0) // active-variable literals only
            occ.large[l].push_back(ref);
      ref += clause_words(c->size);
    }
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

void bve_schedule(Kernel &k, std::vector<Var> &out) {
  // fold anything fixed since the last look into the retry bitmap first, so the
  // filter below never hides a variable whose neighbourhood a root unit changed
  kernel_bve_retry_sync_fixed(k);
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
    // RETRY FILTER (kissat eliminate.c schedule_variables / flags->eliminate):
    // a variable BVE already attempted and whose elimination neighbourhood has
    // not changed since would produce exactly the same NoOp, so re-generating
    // and re-counting its resolvents is pure waste. Skipping it is a WORK
    // decision, not an applicability one — the payload instance surface below
    // narrows with it, but bve_apply_instance still accepts ANY variable
    // payload (N1), and the bit is set by every mutation that could change the
    // answer (see Kernel::bve_retry).
    if (!k.bve_retry[v])
      continue;
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

// live copies of the binary pair (a ∨ b) in a's watch list (V3's helper).
uint32_t bve_binary_copies(const Kernel &k, Lit a, Lit b) {
  uint32_t n = 0;
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  const uint32_t sz = k.watches.heads[a].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == b)
        n++;
      i += 1;
    } else {
      i += 2;
    }
  }
  return n;
}
inline bool bve_binary_live(const Kernel &k, Lit a, Lit b) {
  return bve_binary_copies(k, a, b) > 0;
}

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

// ---- syntactic gate detection (E1c) ----
// Detect a functional gate defining x from its clauses; on success mark the gate
// clauses in pos_gate/neg_gate (1 = gate, 0 = antecedent) and return true. For a
// genuine functional gate x↔φ, BVE may resolve only gate×antecedent pairs: the
// skipped antecedent×antecedent resolvents are entailed by the gate (Eén–Biere
// "Effective preprocessing"), so both the elimination AND the E1b witness push
// (all live clauses, with the pivot blocking) stay sound — the reconstruction
// finds x=φ, which satisfies every antecedent because its gate×antecedent
// resolvent is in F'. A *false* gate would be unsound, so detection is exact and
// fuzz-gated. `resolve_gate` stays false: we detect only COMPLETE gates
// (equivalence, AND/OR) whose gate×gate resolvents are tautological. ITE/XOR are
// deferred (they need the gate×gate pair and a shortened-clause check).
bool bve_detect_gate(const Kernel &k, Lit px, Lit nx,
                     const std::vector<MClause> &pos,
                     const std::vector<MClause> &neg,
                     std::vector<uint8_t> &pos_gate,
                     std::vector<uint8_t> &neg_gate, bool &resolve_gate) {
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
        return true;
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
      return true;
    if (try_and_or(pos, px, neg_bin, pos_gate, neg_gate))
      return true;
  }
  return false;
}

// Eliminate variable x by resolution. When a syntactic gate defines x, only
// gate×antecedent resolvents are generated (bve_detect_gate); otherwise the full
// cross product. At
// a drained root. State is not mutated until the bound is passed, so a NoOp
// return (bound exceeded / duplicate-binary hazard) leaves the kernel untouched
// and the caller may try another candidate. Returns Unsat on an empty resolvent
// or contradictory collapsed units.
ApplyResult bve_eliminate(Kernel &k, Var x) {
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

  // ---- gather x's BINARIES (watch scan) + duplicate-binary hazard pre-check ----
  auto gather_bin = [&](Lit lit, std::vector<MClause> &live) -> bool {
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[lit].offset;
    const uint32_t sz = k.watches.heads[lit].size;
    for (uint32_t i = 0; i < sz;) {
      const Watch w{ws[i]};
      if (!w.binary()) {
        i += 2;
        continue;
      }
      const Lit o = w.other();
      i += 1;
      if (bve_binary_copies(k, lit, o) >= 2)
        return false; // duplicated pair: kernel_delete_binary can't name it
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
    return true;
  };
  if (!gather_bin(px, pos) || !gather_bin(nx, neg))
    return ApplyResult::NoOp; // duplicate-binary hazard

  if (pos.empty() && neg.empty() && del_large.empty() && del_bin.empty()) {
    return ApplyResult::NoOp; // x occurs nowhere resolvable
  }

  // ---- syntactic gate detection: restrict resolvents to gate×antecedent ----
  std::vector<uint8_t> pos_gate(pos.size(), 0), neg_gate(neg.size(), 0);
  bool resolve_gate = false;
  const bool gate =
      bve_detect_gate(k, px, nx, pos, neg, pos_gate, neg_gate, resolve_gate);

  // ---- generate resolvents (gate×antecedent, else full cross product) + bound ----
  struct Resolvent {
    std::vector<Lit> lits;
    std::vector<uint64_t> chain;
  };
  std::vector<Resolvent> resolvents;
  const size_t limit = pos.size() + neg.size() + bve_bound(k);
  std::vector<uint8_t> seen(2 * k.nvars, 0); // dedup/tautology scratch
  for (size_t ci = 0; ci < pos.size(); ++ci) {
    const MClause &c = pos[ci];
    for (const Lit l : c.active)
      if (l != px)
        seen[l] = 1;
    bool aborted = false;
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
        aborted = true;
        break;
      }
      if (lrat) {
        r.chain = c.absorbed;
        r.chain.insert(r.chain.end(), d.absorbed.begin(), d.absorbed.end());
        r.chain.push_back(c.id);
        r.chain.push_back(d.id);
      }
      resolvents.push_back(std::move(r));
      if (resolvents.size() > limit) { // growth bound exceeded: keep x
        aborted = true;
        break;
      }
    }
    for (const Lit l : c.active)
      if (l != px)
        seen[l] = 0;
    if (aborted)
      return ApplyResult::NoOp; // no state mutated yet
  }

  // ---- COMMIT: add resolvents (RUP, add-before-delete), witnesses, delete ----
  std::vector<Lit> units;                              // collapsed root units
  std::vector<Lit> pending_unit(k.nvars, INVALID_LIT); // record_unit dedup by var
  // {u},{¬u} in one round ⊢ ∅ (V3's record_unit, corpus 007).
  auto record_unit = [&](Lit u, uint64_t id) -> bool {
    const Var v = var_of(u);
    if (pending_unit[v] == INVALID_LIT) {
      pending_unit[v] = u;
      k.unit_ids[v] = id;
      units.push_back(u);
      return false;
    }
    if (pending_unit[v] == u)
      return false; // duplicate same-sign
    if (k.proof) {
      if (lrat) {
        const uint64_t ch[2] = {id, k.unit_ids[v]};
        k.proof->add_empty(++k.next_clause_id, ch, 2);
      } else {
        k.proof->add_empty(0, nullptr, 0);
      }
    }
    k.inconsistent = true;
    return true;
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
    if (r.lits.size() == 2 && bve_binary_live(k, r.lits[0], r.lits[1]))
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
  // tautology). Large → proof del + GARBAGE; binary → kernel_delete_binary.
  auto del_large_ref = [&](Ref ref) {
    Clause *c = clause_at(k.arena, ref);
    if (c->garbage())
      return;
    if (k.proof)
      k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
    kernel_mark_garbage(k, c);
  };
  auto del_all = [&](const std::vector<MClause> &side) {
    for (const MClause &m : side) {
      if (m.ref != INVALID_REF) {
        del_large_ref(m.ref);
      } else {
        const bool ok = kernel_delete_binary(k, m.ba, m.bb);
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
    const bool ok = kernel_delete_binary(k, pr.first, pr.second);
    assert(ok);
    (void)ok;
  }

  // assert collapsed root units un-propagated; M1 owns the cascade.
  for (const Lit u : units)
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
  for (const Var x : cand) {
    const ApplyResult r = bve_eliminate(k, x);
    if (r == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    if (r == ApplyResult::Ok) {
      // reclaim the clauses this elimination garbaged. Reclamation is NOT part
      // of the rule's effect (deletes are lazy and every reader skips garbage,
      // so never collecting is sound) — it is the caller's cost/memory choice.
      // The canonical round stays self-contained; the apply_instance dosing
      // path leaves it to the scheduler, which reclaims once per phase instead
      // of once per eliminated variable (O(arena) each — rev 53).
      kernel_collect_garbage(k);
      return ApplyResult::Ok; // eliminated one variable
    }
    // NoOp (bound / hazard): try the next candidate
  }
  return ApplyResult::NoOp;
}

// instance surface: payload = a candidate variable via its positive DIMACS
// literal (lit_instance, rev 34); INSTANCE_CANONICAL = the full round.
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
  return bve_eliminate(k, var_of(l));
}

TACTIX_REGISTER_RULE(BVE, RuleTier::Inprocess, bve_guard, bve_apply,
                     bve_instances, bve_apply_instance);

} // namespace tactix

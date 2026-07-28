#include "rules/bce/bce.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

namespace tactix {

namespace {

// One occurrence of a literal: either a large clause (ref) or a binary partner.
struct Occ {
  Ref ref = INVALID_REF; // INVALID_REF ⇒ binary
  Lit partner = 0;       // binary only
};

bool root_open(const Kernel &k, Lit l) { return k.values[l] == 0; }

// Irredundant, live, and entirely unassigned at the drained root. Requiring
// every literal unassigned does double duty: it skips satisfied clauses (whose
// deletion would be pointless) and it guarantees the clause is NOT a reason for
// any root assignment — a reason is unit, so all but one of its literals are
// assigned. Deleting a live reason would dangle the kernel's bookkeeping.
bool candidate_clause(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant())
    return false;
  for (const Lit l : *c)
    if (!root_open(k, l))
      return false;
  return true;
}

// Root-satisfied clauses are the ONLY ones the opposite side may skip: they
// stay satisfied however the blocking literal is flipped, so they cannot
// constrain blockedness. Everything else live must be tested — REDUNDANT
// clauses included, and clauses carrying root-FALSE literals included.
bool opposite_relevant(const Kernel &k, const Clause *c) {
  if (c->garbage())
    return false;
  for (const Lit l : *c)
    if (k.values[l] > 0)
      return false; // root-satisfied
  return true;
}

// `side == Cand` collects deletion candidates (irredundant, wholly unassigned —
// see candidate_clause); `side == Opposite` collects the clauses the
// blockedness test must quantify over. The distinction is load-bearing and was
// the fuzz-2204 wrong-SAT: filtering the OPPOSITE side by the candidate
// predicate hid `(39 ∨ 9)` — live, unsatisfied, but carrying the root-false
// `39` — which made `(-9 ∨ 33)` look blocked on `-9`. Its resolvent `(33 ∨ 39)`
// is not tautological, so the reconstruction's flip of `9` then falsified
// `(39 ∨ 9)`, a clause nobody had deleted.
enum class Side { Cand, Opposite };

void collect_occs(const Kernel &k, Lit l, Side side, std::vector<Occ> &out) {
  out.clear();
  auto wanted = [&](const Clause *c) {
    return side == Side::Cand ? candidate_clause(k, c) : opposite_relevant(k, c);
  };
  // The dense index lists IRREDUNDANT large clauses only, which is exactly what
  // a candidate needs but NOT what the opposite side needs — so the opposite
  // side always takes the arena scan.
  if (k.dense && side == Side::Cand) {
    for (const Ref ref : k.dense->large[l])
      if (wanted(clause_at(k.arena, ref)))
        out.push_back({ref, 0});
  } else {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_words(c->size);
      if (wanted(c))
        for (const Lit x : *c)
          if (x == l) {
            out.push_back({ref, 0});
            break;
          }
      ref += words;
    }
  }
  // binaries live in the watch lists, not the arena; all of them are
  // irredundant in this kernel (V4 deviation 6)
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
  const uint32_t sz = k.watches.heads[l].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      const Lit o = w.other();
      // candidate: both literals must be unassigned. opposite: only a
      // root-SATISFIED binary may be skipped.
      const bool take = side == Side::Cand ? (root_open(k, l) && root_open(k, o))
                                           : (k.values[o] <= 0);
      if (take)
        out.push_back({INVALID_REF, o});
      i += 1;
    } else {
      i += 2;
    }
  }
}

} // namespace

bool bce_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void bce_literals(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  for (Lit l = 0; l < 2 * k.nvars; l++)
    if (root_open(k, l))
      out.push_back(l);
}

ApplyResult bce_try(Kernel &k, Lit blocking) {
  if (!root_open(k, blocking))
    return ApplyResult::NoOp;
  std::vector<Occ> cands, opposite;
  collect_occs(k, blocking, Side::Cand, cands);
  collect_occs(k, negate(blocking), Side::Opposite, opposite);
  k.stats.bce_ticks += cands.size() + opposite.size();
  if (cands.empty())
    return ApplyResult::NoOp;

  // `marked[lit]` = "¬lit occurs in the candidate" — the tautology test reduces
  // to "does D contain a literal whose negation is in C\{blocking}".
  std::vector<uint8_t> marked(2 * k.nvars, 0);
  std::vector<Lit> lits, rest;
  std::vector<Occ> blocked;

  for (const Occ &co : cands) {
    lits.clear();
    if (co.ref == INVALID_REF) {
      lits.push_back(blocking);
      lits.push_back(co.partner);
    } else {
      const Clause *c = clause_at(k.arena, co.ref);
      if (c->garbage())
        continue;
      for (const Lit l : *c)
        lits.push_back(l);
    }
    for (const Lit l : lits)
      if (l != blocking)
        marked[l] = 1;

    bool is_blocked = true;
    for (const Occ &d : opposite) {
      k.stats.bce_ticks++;
      bool taut = false;
      if (d.ref == INVALID_REF) {
        // D = (¬blocking ∨ partner): the resolvent is tautological iff
        // ¬partner is one of C's other literals
        taut = marked[negate(d.partner)] != 0;
      } else {
        const Clause *dc = clause_at(k.arena, d.ref);
        if (dc->garbage())
          continue;
        for (const Lit l : *dc)
          if (l != negate(blocking) && marked[negate(l)]) {
            taut = true;
            break;
          }
      }
      if (!taut) {
        is_blocked = false;
        break;
      }
    }
    for (const Lit l : lits)
      marked[l] = 0;
    if (is_blocked)
      blocked.push_back(co);
  }
  if (blocked.empty())
    return ApplyResult::NoOp;

  // Delete AFTER the scan: deleting mutates the very occurrence structures the
  // scan reads (the BVE / V4 discipline). Deleting only shrinks the opposite
  // side, so every clause found blocked above is still blocked now.
  for (const Occ &b : blocked) {
    rest.clear();
    if (b.ref == INVALID_REF) {
      rest.push_back(b.partner);
      kernel_push_witness(k, blocking, rest.data(), 1);
      const bool ok = kernel_delete_binary_all_copies(k, blocking, b.partner);
      assert(ok);
      (void)ok;
    } else {
      Clause *c = clause_at(k.arena, b.ref);
      if (c->garbage())
        continue;
      for (const Lit l : *c)
        if (l != blocking)
          rest.push_back(l);
      kernel_push_witness(k, blocking, rest.data(),
                          static_cast<uint32_t>(rest.size()));
      if (k.proof)
        k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
      kernel_mark_garbage(k, c);
    }
    k.stats.bce_deleted++;
  }
  return ApplyResult::Ok;
}

ApplyResult bce_apply(Kernel &k) {
  std::vector<Lit> lits;
  bce_literals(k, lits);
  bool any = false;
  for (const Lit l : lits) {
    const ApplyResult r = bce_try(k, l);
    if (r == ApplyResult::Unsat)
      return r;
    any |= (r == ApplyResult::Ok);
  }
  if (any)
    k.stats.bce_rounds++;
  return any ? ApplyResult::Ok : ApplyResult::NoOp;
}

static uint32_t bce_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> lits;
  bce_literals(k, lits);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(lits.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(lits[i]);
  return n;
}

static ApplyResult bce_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: complete the root closure if applied off a fixpoint (no-op
  // otherwise); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return bce_apply(k);
  Lit blocking;
  if (!lit_of_instance(inst, &blocking))
    return ApplyResult::NoOp;
  return bce_try(k, blocking);
}

TACTIX_REGISTER_RULE(BCE, RuleTier::Inprocess, bce_guard, bce_apply,
                     bce_instances, bce_apply_instance);

} // namespace tactix

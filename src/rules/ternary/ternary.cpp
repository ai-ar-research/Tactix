#include "rules/ternary/ternary.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

namespace tactix {

namespace {

// A ternary candidate: the clause plus its three literals, snapshotted so the
// pair loop never re-reads the arena (which `kernel_add_large_clause` may
// reallocate under us — the same hazard bve.cpp:527 records).
struct Tern {
  uint64_t id;
  Lit l[3];
};

bool root_open(const Kernel &k, Lit l) { return k.values[l] == 0; }

bool has_lit(const Clause *c, Lit t) {
  for (const Lit l : *c)
    if (l == t)
      return true;
  return false;
}

// true iff a live binary (a ∨ b) exists — the watch-list scan congruence.cpp
// uses; kept local rather than shared, since the two callers want different
// staleness policies (see subsume_forward's V4 reconciliation).
bool live_binary(const Kernel &k, Lit a, Lit b) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  const uint32_t sz = k.watches.heads[a].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == b)
        return true;
      i += 1;
    } else {
      i += 2;
    }
  }
  return false;
}

// Ternary, irredundant, all three literals still unassigned at the drained
// root. A clause carrying a root-true literal is satisfied and carries no
// resolvent worth adding; one carrying a root-false literal is really a
// shorter clause, and resolving the stale ternary form would emit a weaker
// consequence than the formula already has.
bool ternary_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant() || c->size != 3)
    return false;
  return root_open(k, c->lits[0]) && root_open(k, c->lits[1]) &&
         root_open(k, c->lits[2]);
}

// Collect the ternary clauses containing `l`. Dense fast-path when the shared
// index is attached (kissat dense.c analogue); otherwise one arena scan, the
// round-local path every inprocessing rule keeps for N1 standalone use.
void collect(const Kernel &k, Lit l, std::vector<Tern> &out) {
  out.clear();
  auto take = [&](const Clause *c) {
    Tern t;
    t.id = c->id();
    t.l[0] = c->lits[0];
    t.l[1] = c->lits[1];
    t.l[2] = c->lits[2];
    out.push_back(t);
  };
  if (k.dense) {
    for (const Ref ref : k.dense->large[l]) {
      const Clause *c = clause_at(k.arena, ref);
      if (ternary_candidate(k, c))
        take(c);
    }
    return;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (ternary_candidate(k, c) && has_lit(c, l))
      take(c);
    ref += words;
  }
}

// Is the exact ternary {a,b,c} already present? Scans one occurrence list —
// cadical's `ternary_find_ternary_clause` shape (deviation 5).
bool ternary_exists(const Kernel &k, Lit a, Lit b, Lit c) {
  auto same = [&](const Clause *x) {
    return has_lit(x, a) && has_lit(x, b) && has_lit(x, c);
  };
  if (k.dense) {
    for (const Ref ref : k.dense->large[a]) {
      const Clause *x = clause_at(k.arena, ref);
      if (!x->garbage() && x->size == 3 && same(x))
        return true;
    }
    return false;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *x = clause_at(k.arena, ref);
    const uint32_t words = clause_words(x->size);
    if (!x->garbage() && x->size == 3 && same(x))
      return true;
    ref += words;
  }
  return false;
}

// Resolve `c` (contains pivot) with `d` (contains ¬pivot) on pivot. Returns
// false when the resolvent is tautological or would exceed three literals —
// cadical's `hyper_ternary_resolve` rejections, minus the "already present"
// test, which the caller does (it differs by resolvent size).
bool resolve(const Tern &c, const Tern &d, Lit pivot, std::vector<Lit> &res) {
  res.clear();
  for (const Lit l : c.l)
    if (l != pivot)
      res.push_back(l);
  assert(res.size() == 2);
  for (const Lit l : d.l) {
    if (l == negate(pivot))
      continue;
    if (l == res[0] || l == res[1])
      continue;
    if (l == negate(res[0]) || l == negate(res[1]))
      return false; // tautological
    if (res.size() == 3)
      return false; // would be quaternary
    res.push_back(l);
  }
  return true;
}

} // namespace

bool ternary_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void ternary_pivots(const Kernel &k, std::vector<Lit> &out,
                   uint64_t *total_clauses) {
  out.clear();
  const uint32_t nlits = 2 * k.nvars;
  std::vector<uint32_t> tern(nlits, 0);
  uint64_t clauses = 0;
  // one pass over the arena (or the dense lists) to count ternary occurrences
  // per literal; a pivot needs at least one on EACH side for a pair to exist.
  // The same pass totals live clauses, which is the base for the max-add
  // governor below (cadical sizes its htrs_limit off the clause count too).
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (!c->garbage())
      clauses++;
    if (ternary_candidate(k, c))
      for (const Lit l : *c)
        tern[l]++;
    ref += words;
  }
  clauses += k.binary_ids.size();
  if (total_clauses)
    *total_clauses = clauses;
  const uint32_t occ_lim = k.ternary_occ_limit;
  for (Lit l = 0; l < nlits; l += 2) // positive literal of each variable
    if (tern[l] && tern[negate(l)] &&
        // cadical ternary.cpp:247 — a pivot in a dense neighbourhood is skipped
        // on BOTH polarities before any resolution work is attempted.
        !(occ_lim && (tern[l] > occ_lim || tern[negate(l)] > occ_lim)))
      out.push_back(l);
}

ApplyResult ternary_try(Kernel &k, Lit pivot) {
  if (k.values[pivot] != 0)
    return ApplyResult::NoOp;
  std::vector<Tern> pos, neg;
  collect(k, pivot, pos);
  collect(k, negate(pivot), neg);
  k.stats.ternary_ticks += pos.size() + neg.size();
  if (pos.empty() || neg.empty())
    return ApplyResult::NoOp;

  const bool lrat = k.proof && k.proof->lrat();
  std::vector<Lit> res;
  bool added = false;
  for (const Tern &c : pos)
    for (const Tern &d : neg) {
      k.stats.ternary_ticks++;
      if (!resolve(c, d, pivot, res))
        continue;
      if (res.size() == 2) {
        if (live_binary(k, res[0], res[1]))
          continue;
      } else if (ternary_exists(k, res[0], res[1], res[2])) {
        continue;
      }
      // RUP from its two parents, in checker-propagation order: negating the
      // resolvent falsifies every literal of `c` but the pivot, so `c` forces
      // the pivot and `d` is then falsified (the E1 resolvent argument).
      const uint64_t id = ++k.next_clause_id;
      const uint64_t chain[2] = {c.id, d.id};
      if (k.proof)
        k.proof->add_rup(lrat ? id : 0, res.data(),
                         static_cast<uint32_t>(res.size()),
                         lrat ? chain : nullptr, lrat ? 2 : 0);
      if (res.size() == 2) {
        kernel_add_binary_clause(k, res[0], res[1], id);
        k.stats.ternary_binaries++;
      } else {
        // redundant (deviation 3): an entailed consequence Reduce may retire
        kernel_add_large_clause(k, res.data(), 3, /*redundant=*/true,
                                /*glue=*/3, id);
        k.stats.ternary_resolvents++;
      }
      added = true;
    }
  return added ? ApplyResult::Ok : ApplyResult::NoOp;
}

ApplyResult ternary_apply(Kernel &k) {
  std::vector<Lit> pivots;
  uint64_t total_clauses = 0;
  ternary_pivots(k, pivots, &total_clauses);
  bool added = false;
  // Budget-aware batch rule (EffortSpec budget_aware): stop mid-round when the
  // policy's ledger in our currency (ternary_ticks) is spent. Unbudgeted, one
  // round can add enough redundant clauses to turn a fast solve into a
  // timeout.
  const uint64_t tick_start = k.stats.ternary_ticks;
  // GROWTH governor (cadical `ternarymaxadd`, ternary.cpp:387): the ticks
  // budget bounds WORK, not OUTPUT, and this rule's failure mode is OUTPUT —
  // resolvent count runs away once BCE stops shrinking its candidate pool.
  // This bounds the clauses one round may add to a fraction of the formula it
  // is growing.
  const uint64_t add_start =
      k.stats.ternary_resolvents + k.stats.ternary_binaries;
  const uint64_t add_limit =
      k.ternary_max_add_percent
          ? total_clauses * k.ternary_max_add_percent / 100
          : 0;
  // SCHED-F3 · repetition filter over the touched-epoch substrate. cadical
  // maintains exactly this as a per-variable `ternary` flag, set when a ternary
  // clause containing the variable is ADDED and explicitly described as
  // "focus on those variables for which we have not tried ternary resolution
  // before and nothing changed for them since then" (ternary.cpp:258-266).
  // A pivot with no new material resolves the same pairs to the same
  // already-present resolvents — `ternary_exists`/`live_binary` reject every
  // one of them, at full P·N cost.
  //
  // Ours is a coarsening of cadical's in one direction (any add touches, not
  // only a ternary one) and NOT in the other: deletion is not consulted,
  // because removing a ternary can only shrink the pair set. Non-wedge is the
  // same theorem as C1/C2's — this rule's own resolvents are adds, so a
  // productive round arms the next one, and a round with nothing new provably
  // has nothing to derive.
  // N1: `ternary_pivots` (what `instances()` enumerates) and `ternary_try`
  // (the `apply_instance` path) are untouched and stay complete.
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.ternary_touch_mark;
  size_t stopped_at = pivots.size();
  for (size_t pi = 0; pi < pivots.size(); pi++) {
    const Lit p = pivots[pi];
    if (k.work_budget && k.stats.ternary_ticks - tick_start > k.work_budget) {
      stopped_at = pi;
      break;
    }
    if (add_limit &&
        (k.stats.ternary_resolvents + k.stats.ternary_binaries) - add_start >=
            add_limit) {
      stopped_at = pi;
      break;
    }
    if (filter && !kernel_touched_since(k, var_of(p), wm)) {
      k.stats.ternary_mark_skipped++;
      continue;
    }
    const ApplyResult r = ternary_try(k, p);
    if (r == ApplyResult::Unsat)
      return r;
    added |= (r == ApplyResult::Ok);
  }
  // RESUME (SCHED-F2's mechanism): re-arm the pivots this round never reached,
  // then advance the watermark. Both cut paths above leave a tail, and the
  // add-limit one is REACHABLE under DefaultPolicy, unlike C1/C2's.
  for (size_t pi = stopped_at; pi < pivots.size(); pi++) {
    if (filter && !kernel_touched_since(k, var_of(pivots[pi]), wm))
      continue;
    kernel_touch_var(k, var_of(pivots[pi]));
    k.stats.ternary_rearmed++;
  }
  if (filter)
    k.ternary_touch_mark = snapshot;
  if (added)
    k.stats.ternary_rounds++;
  return added ? ApplyResult::Ok : ApplyResult::NoOp;
}

static uint32_t ternary_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> pivots;
  ternary_pivots(k, pivots);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(pivots.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(pivots[i]);
  return n;
}

static ApplyResult ternary_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: complete the root closure if applied off a fixpoint (no-op
  // otherwise); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return ternary_apply(k);
  Lit pivot;
  if (!lit_of_instance(inst, &pivot))
    return ApplyResult::NoOp;
  return ternary_try(k, pivot);
}

TACTIX_REGISTER_RULE(HyperTernaryRes, RuleTier::Inprocess, ternary_guard,
                     ternary_apply, ternary_instances, ternary_apply_instance);

} // namespace tactix

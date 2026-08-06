#include "rules/bce/bce.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
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

// Round-local literal→large-clause index over the clauses BCE must see:
// every live, non-root-satisfied clause, REDUNDANT ONES INCLUDED. `Kernel::dense`
// cannot serve here — it carries irredundant clauses only, and blockedness must
// quantify over redundant ones too — so one arena pass per ROUND builds this.
// Do NOT go back to a per-literal arena scan: that makes a canonical round
// O(nlits × arena), which measured as an order-of-magnitude tier regression.
struct BceIndex {
  std::vector<std::vector<Ref>> occ; // by literal; may hold stale garbage Refs
};

void build_index(const Kernel &k, BceIndex &idx) {
  idx.occ.assign(2 * k.nvars, {});
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (opposite_relevant(k, c))
      for (const Lit x : *c)
        idx.occ[x].push_back(ref);
    ref += words;
  }
}

// Binaries live in the watch lists, not the arena; all of them are irredundant
// in this kernel (transitive_reduce.hpp deviation 6).
void collect_binaries(const Kernel &k, Lit l, Side side, std::vector<Occ> &out) {
  for (const WatchEntry w : k.watches.entries(l)) {
    if (!w.binary())
      continue;
    const Lit o = w.other();
    // candidate: both literals must be unassigned. opposite: only a
    // root-SATISFIED binary may be skipped.
    const bool take = side == Side::Cand ? (root_open(k, l) && root_open(k, o))
                                         : (k.values[o] <= 0);
    if (take)
      out.push_back({INVALID_REF, o});
  }
}

void collect_occs(const Kernel &k, const BceIndex &idx, Lit l, Side side,
                  std::vector<Occ> &out) {
  out.clear();
  for (const Ref ref : idx.occ[l]) {
    const Clause *c = clause_at(k.arena, ref);
    // the index is built with the OPPOSITE predicate (the weaker one), so a
    // candidate still has to pass its own, stricter test
    if (side == Side::Cand ? candidate_clause(k, c) : !c->garbage())
      out.push_back({ref, 0});
  }
  collect_binaries(k, l, side, out);
}

} // namespace

bool bce_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void bce_literals(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  for (Lit l = 0; l < 2 * k.nvars; l++)
    if (root_open(k, l))
      out.push_back(l);
}

// One blocking literal, against an already-built index. `marked` is caller-owned
// scratch, cleared on exit — a fresh vector per literal was an O(nvars) fill per
// call, the per-call linear-scratch hazard the codebase has hit four times.
ApplyResult bce_try_indexed(Kernel &k, Lit blocking, const BceIndex &idx,
                            std::vector<uint8_t> &marked) {
  if (!root_open(k, blocking))
    return ApplyResult::NoOp;
  // Occurrence ceiling (Kernel::bce_occ_limit, cadical `blockocclim`): a literal
  // in a dense neighbourhood is declined before any scan. Checked on the RAW
  // index sizes so the decision costs O(1) and cannot itself be the cost, and on
  // BOTH sides because blockedness quantifies over the opposite occurrences too.
  if (k.bce_occ_limit &&
      (idx.occ[blocking].size() > k.bce_occ_limit ||
       idx.occ[negate(blocking)].size() > k.bce_occ_limit))
    return ApplyResult::NoOp;
  std::vector<Occ> cands, opposite;
  collect_occs(k, idx, blocking, Side::Cand, cands);
  collect_occs(k, idx, negate(blocking), Side::Opposite, opposite);
  k.stats.bce_ticks += cands.size() + opposite.size();
  if (cands.empty())
    return ApplyResult::NoOp;

  // `marked[lit]` = "¬lit occurs in the candidate" — the tautology test reduces
  // to "does D contain a literal whose negation is in C\{blocking}".
  std::vector<Lit> lits, rest;
  std::vector<Occ> blocked;

  for (const Occ &co : cands) {
    // §6 · the SCAN is read-only and `marked` is all-zero here (each iteration
    // restores it below), so leaving now costs only un-applied candidates. The
    // deletions already collected stay valid: they were tested against the
    // FULL opposite side, and deleting can only shrink it.
    if (kernel_rule_interrupt(k, k.stats.bce_interrupted))
      break;
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
  // NOT polled: the deletion loop is the rule's OUTPUT, already bounded by the
  // scan that produced it (one witness + one `del` + one garbage mark each),
  // and it is the scan that is unbounded — `bce_occ_limit` ships 0. Aborting
  // here would only discard work already paid for.
  for (const Occ &b : blocked) {
    rest.clear();
    if (b.ref == INVALID_REF) {
      // The primitive REFUSES a duplicated pair (two live copies sharing one
      // binary_ids id — V4 deviation 8). Skip it, and push no witness: a
      // witness for a clause still in the formula is not wrong, but it is a
      // lie about what ρ contains.
      if (!kernel_delete_binary_all_copies(k, blocking, b.partner))
        continue;
      rest.push_back(b.partner);
      kernel_push_witness(k, blocking, rest.data(), 1);
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

// N1 single-instance entry: one literal, so it pays for its own index. One
// arena pass, not one per literal.
ApplyResult bce_try(Kernel &k, Lit blocking) {
  if (!root_open(k, blocking))
    return ApplyResult::NoOp;
  BceIndex idx;
  build_index(k, idx);
  std::vector<uint8_t> marked(2 * k.nvars, 0);
  return bce_try_indexed(k, blocking, idx, marked);
}

ApplyResult bce_apply(Kernel &k) {
  std::vector<Lit> lits;
  bce_literals(k, lits);
  BceIndex idx;
  build_index(k, idx); // ONE arena pass for the whole round
  std::vector<uint8_t> marked(2 * k.nvars, 0);
  bool any = false;
  // Budget-aware batch rule (EffortSpec budget_aware): the policy hands us its
  // remaining ledger in OUR currency (bce_ticks) and we stop mid-round rather
  // than being metered after the fact. Vivify's idiom, vivify.cpp:275.
  const uint64_t tick_start = k.stats.bce_ticks;
  // SCHED-F3 · repetition filter over the touched-epoch substrate. cadical
  // keeps a per-literal `block` flag with precisely this lifecycle: set by
  // `mark_added` on an irredundant add and by `mark_removed(-lit)` on a
  // removal, cleared when the literal is scheduled, with the comment "we do
  // not have to try blocked clause elimination on it again until irredundant
  // clauses with its negation are removed" (block.cpp:730-733,
  // internal.hpp:1125-1137).
  //
  // ▶ THIS IS THE ONE CONSUMER THAT NEEDS THE DELETION SIDE, and it is why
  // `Kernel::removed` exists as a second array. A clause becomes blocked on l
  // either because it appeared or because an opposite occurrence went away;
  // filtering BCE on adds alone would silently drop the second case, which is
  // the case BCE mostly lives on. Folding deletions into `touched` instead
  // would have weakened C1/C2's filter, where the measured saving is.
  //
  // Coarser than cadical on polarity: our stamps are per VARIABLE, so l is
  // reconsidered whenever either polarity changed. Conservative in the safe
  // direction.
  // N1: `bce_literals` (what `instances()` enumerates) and `bce_try` (the
  // `apply_instance` path) are untouched and stay complete.
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.bce_touch_mark;
  size_t stopped_at = lits.size();
  for (size_t li = 0; li < lits.size(); li++) {
    const Lit l = lits[li];
    if (k.work_budget && k.stats.bce_ticks - tick_start > k.work_budget) {
      stopped_at = li;
      break;
    }
    // §6 (SCHED-H1) · TERMINATION is not budget exhaustion: `work_budget` can
    // be 0 (unmetered) and the ledger can be large, so without this poll a
    // round runs to completion after an interrupt arrives. Taken at the same
    // point as the budget cut so the tail is re-armed by the same resume path.
    if (kernel_rule_interrupt(k, k.stats.bce_interrupted)) {
      stopped_at = li;
      break;
    }
    if (filter && !kernel_changed_since(k, var_of(l), wm)) {
      k.stats.bce_mark_skipped++;
      continue;
    }
    const ApplyResult r = bce_try_indexed(k, l, idx, marked);
    if (r == ApplyResult::Unsat)
      return r;
    any |= (r == ApplyResult::Ok);
  }
  // RESUME (SCHED-F2's mechanism): re-arm the literals the budget never
  // reached, then advance the watermark. This rule IS budget-cut in practice
  // (it is `budget_aware` and its ledger share is 20 per mille), so unlike
  // C1/C2's the path is reachable under DefaultPolicy.
  for (size_t li = filter ? stopped_at : lits.size(); li < lits.size(); li++) {
    if (!kernel_changed_since(k, var_of(lits[li]), wm))
      continue;
    kernel_touch_var(k, var_of(lits[li]));
    k.stats.bce_rearmed++;
  }
  if (filter)
    k.bce_touch_mark = snapshot;
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

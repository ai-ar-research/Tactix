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
// every literal unassigned does double duty: it skips satisfied clauses, and it
// guarantees the clause is no reason for a root assignment — a reason is unit,
// so all but one of its literals are assigned, and deleting a live reason would
// dangle the kernel's bookkeeping.
bool candidate_clause(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant())
    return false;
  for (const Lit l : *c)
    if (!root_open(k, l))
      return false;
  return true;
}

// Root-satisfied clauses are the only ones the opposite side may skip: they
// stay satisfied however the blocking literal is flipped. Everything else live
// must be tested — redundant clauses and clauses carrying root-false literals
// included.
bool opposite_relevant(const Kernel &k, const Clause *c) {
  if (c->garbage())
    return false;
  for (const Lit l : *c)
    if (k.values[l] > 0)
      return false; // root-satisfied
  return true;
}

// `side == Cand` collects deletion candidates (irredundant, wholly unassigned);
// `side == Opposite` collects the clauses the blockedness test must quantify
// over. The distinction is load-bearing and was the fuzz-2204 wrong-SAT:
// filtering the opposite side by the candidate predicate hides a live clause
// carrying a root-false literal, which makes another look blocked, and the
// reconstruction's flip then falsifies a clause nobody deleted.
enum class Side { Cand, Opposite };

// Round-local literal→large-clause index over the clauses BCE must see: every
// live, non-root-satisfied clause, redundant ones included. `Kernel::dense`
// cannot serve — it carries irredundant clauses only. One arena pass per round,
// not a per-literal scan: that makes a canonical round O(nlits × arena), an
// order-of-magnitude tier regression.
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
// in this kernel (`docs/implementation-notes.md` §14).
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

// One blocking literal, against an already-built index. `marked` is
// caller-owned scratch, cleared on exit: a fresh vector per literal would be an
// O(nvars) fill per call, the per-call linear-scratch hazard.
ApplyResult bce_try_indexed(Kernel &k, Lit blocking, const BceIndex &idx,
                            std::vector<uint8_t> &marked) {
  if (!root_open(k, blocking))
    return ApplyResult::NoOp;
  // Occurrence ceiling (Kernel::bce_occ_limit, cadical `blockocclim`): a
  // literal in a dense neighbourhood is declined before any scan. Checked on
  // the raw index sizes so the decision is O(1), and on both sides because
  // blockedness quantifies over the opposite occurrences too.
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
    // §6 safe point: the scan is read-only and `marked` is all-zero here, so
    // leaving now costs only un-applied candidates. The deletions already
    // collected stay valid — they were tested against the full opposite side,
    // and deleting can only shrink it.
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

  // Delete after the scan: deleting mutates the occurrence structures the scan
  // reads. It only shrinks the opposite side, so every clause found blocked
  // above is still blocked now. Not polled for termination — this loop is the
  // rule's output, already bounded by the scan that produced it, and it is the
  // scan that is unbounded.
  for (const Occ &b : blocked) {
    rest.clear();
    if (b.ref == INVALID_REF) {
      // The primitive refuses a duplicated pair (two live copies sharing one
      // binary_ids id; see transitive_reduce.hpp). Skip it, and push no
      // witness: a witness for a clause still in the formula is not wrong, but
      // it misstates what ρ contains.
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
      kernel_delete_clause(k, c);
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
  // Budget-aware batch rule (EffortSpec budget_aware): the policy hands the
  // remaining ledger in this rule's currency (bce_ticks) and the round stops
  // mid-way rather than being metered after the fact (Vivify's idiom).
  const uint64_t tick_start = k.stats.bce_ticks;
  // Repetition filter over the touched-epoch substrate; cadical keeps a
  // per-literal `block` flag with the same lifecycle (block.cpp:730-733). This
  // is the one consumer that needs the DELETION side, which is why
  // `Kernel::removed` exists as a second array: a clause becomes blocked on l
  // either because it appeared or because an opposite occurrence went away, and
  // filtering on adds alone drops the second case — the one BCE mostly lives
  // on. Stamps are per variable, coarser than cadical's polarity, which is
  // conservative. N1: `bce_literals` and `bce_try` stay complete.
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
    // §6 termination is not budget exhaustion: `work_budget` can be 0
    // (unmetered), so without this poll a round runs to completion after an
    // interrupt arrives. Taken at the budget cut's point so the tail is
    // re-armed by the same resume path.
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
  // Resume: re-arm the literals the budget never reached, then advance the
  // watermark. This rule is budget-cut in practice (budget_aware, a 20 per
  // mille ledger share), so the path is reachable under DefaultPolicy.
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
    out[i] = lit_instance(k, lits[i]);
  return n;
}

static ApplyResult bce_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return bce_apply(k);
  Lit blocking;
  if (!lit_of_instance(k, inst, &blocking))
    return ApplyResult::NoOp;
  return bce_try(k, blocking);
}

TACTIX_REGISTER_RULE(BCE, RuleTier::Inprocess, bce_guard, bce_apply,
                     bce_instances, bce_apply_instance);

} // namespace tactix

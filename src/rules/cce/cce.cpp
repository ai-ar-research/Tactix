#include "rules/cce/cce.hpp"

#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tactix {

namespace {

// One candidate may not spend more than this many of the rule's own ticks. The
// extension loop restarts the covered scan after EVERY addition, so a single
// dense clause is quadratic in its own neighbourhood; the round budget alone
// would let it swallow a whole dose. Not a reference constant — cadical bounds
// only the round (`covereffort`) and takes the per-clause cost as it comes.
constexpr uint64_t CCE_CANDIDATE_TICKS = 4096;

bool root_open(const Kernel &k, Lit l) { return k.values[l] == 0; }

// Deletion candidates: irredundant, live, entirely unassigned at the drained
// root. Requiring every literal unassigned does double duty (X1 BCE's
// argument): it skips satisfied clauses, and it guarantees the clause is NOT a
// reason for any root assignment — a reason is unit, so all but one of its
// literals are assigned, and deleting a live reason would dangle the kernel's
// bookkeeping.
bool candidate_clause(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant())
    return false;
  for (const Lit l : *c)
    if (!root_open(k, l))
      return false;
  return true;
}

// Root-satisfied clauses are the ONLY ones the CLA / blockedness side may skip:
// they stay satisfied however a witness literal is flipped, so they cannot
// constrain the test. Everything else live must be seen — REDUNDANT clauses
// included, and clauses carrying root-FALSE literals included (header deviation
// 2; X1 BCE's fuzz-2204 wrong-SAT came from filtering exactly this side).
bool relevant(const Kernel &k, const Clause *c) {
  if (c->garbage())
    return false;
  for (const Lit l : *c)
    if (k.values[l] > 0)
      return false;
  return true;
}

// Round-local literal -> large-clause index over everything CCE must see.
// `Kernel::dense` cannot serve: it carries irredundant clauses only, and the
// CLA side must quantify over redundant ones too. ONE arena pass per round —
// a per-literal scan would make a canonical round O(nlits x arena), which
// measured as an order-of-magnitude tier regression for BCE.
struct CceIndex {
  std::vector<std::vector<Ref>> occ; // by literal; may hold stale garbage Refs
  std::vector<Ref> cands;            // deletion candidates, arena order
};

void build_index(const Kernel &k, CceIndex &idx) {
  idx.occ.assign(2 * k.nvars, {});
  idx.cands.clear();
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    if (relevant(k, c)) {
      for (const Lit x : *c)
        idx.occ[x].push_back(ref);
      if (candidate_clause(k, c))
        idx.cands.push_back(ref);
    }
    ref += words;
  }
}

// The extension state for ONE candidate. Every array is round-owned and
// restored through its own touched list, never refilled per candidate — the
// per-call linear-scratch hazard this codebase has hit four times.
struct Coveror {
  std::vector<Lit> added;   // the FULL extension C'' (ALA + CLA + C), the queue
  std::vector<Lit> covered; // the RECORDED clause: C's literals + CLA additions
  std::vector<Lit> pivot;   // witness literal of each recorded segment
  std::vector<uint32_t> prefix; // |covered| when that segment was recorded
  std::vector<Lit> inter;       // running CLA intersection
  std::vector<uint8_t> mark;    // by lit: 1 = literal is IN C'' (i.e. false)
  std::vector<uint8_t> imark;   // by lit: 1 = literal is in `inter`
  std::vector<uint8_t> emark;   // by lit: scratch for one candidate clause
  std::vector<Lit> rest;        // witness-segment scratch

  void resize(uint32_t nvars) {
    mark.assign(2 * nvars, 0);
    imark.assign(2 * nvars, 0);
    emark.assign(2 * nvars, 0);
  }
};

// The assignment C'' induces: a literal of C'' is FALSE, its negation TRUE.
// Root assignments count too — they are permanent, so a root-false literal is
// already "in" the extension for every purpose here and a root-true one
// satisfies its clause outright.
bool is_false(const Kernel &k, const Coveror &cv, Lit l) {
  return cv.mark[l] || k.values[l] < 0;
}
bool is_true(const Kernel &k, const Coveror &cv, Lit l) {
  return cv.mark[negate(l)] || k.values[l] > 0;
}

// Add `l` to the extension. Callers must have established that `l` is neither
// already false nor already true, so C'' can never hold a complementary pair
// (which is what keeps the deleted clause out of its own candidate lists).
void extend(Coveror &cv, Lit l) {
  cv.mark[l] = 1;
  cv.added.push_back(l);
}

// ALA over one newly-false literal `l`: every IRREDUNDANT clause holding `l`
// that is now unit propagates, and one that is now empty means C'' is subsumed.
// Returns true on that conflict (the ATA exit). `self` is the candidate itself,
// which must be ignored or it falsifies immediately and trivially.
bool ala_propagate(Kernel &k, const CceIndex &idx, Coveror &cv, Lit l,
                   Ref self) {
  for (const Ref r : idx.occ[l]) {
    if (r == self)
      continue;
    const Clause *d = clause_at(k.arena, r);
    if (d->garbage() || d->redundant())
      continue; // header deviation 2: ALA is irredundant-only
    k.stats.cce_ticks += d->size;
    Lit unit = INVALID_LIT;
    uint32_t open = 0;
    bool sat = false;
    for (const Lit y : *d) {
      if (is_true(k, cv, y)) {
        sat = true;
        break;
      }
      if (is_false(k, cv, y))
        continue;
      unit = y;
      if (++open > 1)
        break;
    }
    if (sat || open > 1)
      continue;
    if (open == 0)
      return true; // d subseteq C'': asymmetric tautology
    extend(cv, negate(unit));
    k.stats.cce_ala++;
  }
  // Binaries live in the watch lists, not the arena; all of them are
  // irredundant in this kernel (transitive_reduce.hpp deviation 6).
  for (const WatchEntry w : k.watches.entries(l)) {
    if (!w.binary())
      continue;
    const Lit o = w.other();
    k.stats.cce_ticks += 2;
    if (is_true(k, cv, o))
      continue;
    if (is_false(k, cv, o))
      return true; // (l v o) fully false
    extend(cv, negate(o));
    k.stats.cce_ala++;
  }
  return false;
}

// Fold one resolution candidate's eligible literals into `inter`. `nu` is the
// pivot's negation, which never joins. Returns false once the intersection is
// empty (the caller aborts the CLA step).
bool fold(const Kernel &k, Coveror &cv, const Lit *lits, uint32_t n, Lit nu,
          bool &first) {
  if (first) {
    for (uint32_t i = 0; i < n; i++) {
      const Lit t = lits[i];
      // already false = already in C'' (or root-false): nothing to add. The
      // caller has ruled out a true literal, so what is left is unassigned.
      if (t == nu || is_false(k, cv, t) || cv.imark[t])
        continue;
      cv.imark[t] = 1;
      cv.inter.push_back(t);
    }
    first = false;
    return !cv.inter.empty();
  }
  for (uint32_t i = 0; i < n; i++) {
    const Lit t = lits[i];
    if (t != nu && !is_false(k, cv, t))
      cv.emark[t] = 1;
  }
  size_t w = 0;
  for (size_t j = 0; j < cv.inter.size(); j++) {
    const Lit t = cv.inter[j];
    if (cv.emark[t])
      cv.inter[w++] = t;
    else
      cv.imark[t] = 0;
  }
  cv.inter.resize(w);
  for (uint32_t i = 0; i < n; i++)
    cv.emark[lits[i]] = 0;
  return !cv.inter.empty();
}

enum class ClaResult { Nothing, Added, Blocked };

// One CLA step on pivot `u` (a RECORDED literal of C''). Either every clause
// holding `¬u` resolves tautologically against C'' — C'' is blocked on `u` —
// or the literals shared by all the others may be added to C''.
ClaResult cla_step(Kernel &k, const CceIndex &idx, Coveror &cv, Lit u) {
  const Lit nu = negate(u);
  bool first = true, empty = false;
  cv.inter.clear();

  // A candidate is skipped when the resolvent with C'' is already tautological,
  // i.e. it holds a literal TRUE under the extension (cadical's "double
  // satisfied"). That is the same test the blocked exit rests on, so a step
  // that skips every candidate IS the blocked exit.
  for (const Ref r : idx.occ[nu]) {
    const Clause *e = clause_at(k.arena, r);
    if (e->garbage())
      continue;
    k.stats.cce_ticks += e->size;
    bool taut = false;
    for (const Lit t : *e)
      if (t != nu && is_true(k, cv, t)) {
        taut = true;
        break;
      }
    if (taut)
      continue;
    if (!fold(k, cv, e->begin(), e->size, nu, first)) {
      empty = true;
      break;
    }
  }
  if (!empty) {
    for (const WatchEntry w : k.watches.entries(nu)) {
      if (!w.binary())
        continue;
      const Lit o = w.other();
      k.stats.cce_ticks += 2;
      if (is_true(k, cv, o))
        continue;
      const Lit pair[2] = {nu, o};
      if (!fold(k, cv, pair, 2, nu, first)) {
        empty = true;
        break;
      }
    }
  }

  ClaResult res = ClaResult::Nothing;
  if (first) {
    // No unblocked resolution candidate at all — including the vacuous case of
    // no `¬u` occurrence, which is the pure-literal rule. C'' is blocked on u.
    cv.pivot.push_back(u);
    cv.prefix.push_back(static_cast<uint32_t>(cv.covered.size()));
    res = ClaResult::Blocked;
  } else if (!cv.inter.empty()) {
    // Record the segment BEFORE the additions: the witness clause is the
    // extension as it stands, and `u` must be a member of it.
    cv.pivot.push_back(u);
    cv.prefix.push_back(static_cast<uint32_t>(cv.covered.size()));
    for (const Lit o : cv.inter) {
      extend(cv, o);
      cv.covered.push_back(o);
      k.stats.cce_cla++;
    }
    res = ClaResult::Added;
  }
  for (const Lit t : cv.inter)
    cv.imark[t] = 0;
  cv.inter.clear();
  return res;
}

// Try to eliminate one candidate. Returns true if it was deleted.
bool cover_clause(Kernel &k, const CceIndex &idx, Coveror &cv, Ref self) {
  Clause *c = clause_at(k.arena, self);
  cv.added.clear();
  cv.covered.clear();
  cv.pivot.clear();
  cv.prefix.clear();
  k.stats.cce_tried++;

  bool tautological = false;
  for (const Lit l : *c) {
    if (cv.mark[negate(l)]) {
      tautological = true; // a tautological candidate has no ¬-free extension
      break;
    }
    if (cv.mark[l])
      continue; // duplicated literal
    extend(cv, l);
    cv.covered.push_back(l);
  }

  bool ata = false, blocked = false;
  if (!tautological) {
    const uint64_t cap = k.stats.cce_ticks + CCE_CANDIDATE_TICKS;
    // cadical's control flow (cover.cpp:358-367): drain the ALA queue first,
    // then take the next recorded literal for a CLA step, and restart the
    // covered scan after EVERY addition — a new false literal can block a
    // pivot that was not blocked before.
    size_t next_added = 0, next_covered = 0;
    while (k.stats.cce_ticks <= cap) {
      if (next_added < cv.added.size()) {
        const size_t before = cv.added.size();
        if (ala_propagate(k, idx, cv, cv.added[next_added++], self)) {
          ata = true;
          break;
        }
        if (cv.added.size() != before)
          next_covered = 0;
      } else if (next_covered < cv.covered.size()) {
        const ClaResult r = cla_step(k, idx, cv, cv.covered[next_covered++]);
        if (r == ClaResult::Blocked) {
          blocked = true;
          break;
        }
        if (r == ClaResult::Added)
          next_covered = 0;
      } else {
        break;
      }
    }
  }

  const bool eliminated = ata || blocked;
  if (eliminated) {
    // `ρ` in push order; kernel_extend_model replays it in reverse, which is
    // the order the header's reconstruction argument unwinds the CLA steps in.
    // An ATA exit reached with no CLA step in front of it needs NO segment: the
    // extension is then pure unit propagation of ¬C over F∖{C}, so C is RUP
    // there and every model of F∖{C} already satisfies it.
    for (size_t i = 0; i < cv.pivot.size(); i++) {
      const Lit w = cv.pivot[i];
      cv.rest.clear();
      for (uint32_t j = 0; j < cv.prefix[i]; j++)
        if (cv.covered[j] != w)
          cv.rest.push_back(cv.covered[j]);
      kernel_push_witness(k, w, cv.rest.data(),
                          static_cast<uint32_t>(cv.rest.size()));
    }
    kernel_delete_clause(k, c);
    k.stats.cce_deleted++;
    if (blocked)
      k.stats.cce_blocked++;
    else
      k.stats.cce_ata++;
  }

  for (const Lit l : cv.added)
    cv.mark[l] = 0;
  cv.added.clear();
  return eliminated;
}

// Has anything `c`'s applicability can turn on moved since `since`? CCE's
// two-sided read (`kernel_changed_since`, BCE's): a clause becomes blocked when
// an opposite occurrence is REMOVED, and its ALA/CLA closure changes when one is
// ADDED, so both stamps count. `need` is 1, not the subsumption family's 2 — one
// changed variable is enough to flip a blockedness test.
//
// ▶ TWO UNDER-APPROXIMATIONS, both YIELD-ONLY, both recorded in the ledger.
// (a) The extension C'' reaches literals outside vars(C), and a change confined
//     to those is invisible here. (b) `relevant()` quantifies over REDUNDANT
//     clauses (header deviation 2 — it must, and X1's wrong-SAT is why), but the
//     substrate does not stamp redundant creations or Reduce's deletions, so a
//     blockedness that a Reduce created is not seen until something irredundant
//     moves. Neither can make a deletion unjustified: the filter only decides
//     what to TRY, and cover_clause re-reads the live index for every test it
//     passes. They cost deletions, never soundness.
bool changed_clause(const Kernel &k, const Clause *c, uint64_t since) {
  for (const Lit l : *c)
    if (kernel_changed_since(k, var_of(l), since))
      return true;
  return false;
}

// The shared body of the round and the single-literal instance: walk `cands`
// under the policy's remaining ledger. `filter`/`wm` are the round's repetition
// filter; the instance path passes filter=false, since narrowing THAT would
// remove a capability (N1). `stopped_at` receives the index of the first
// candidate the budget never reached, for the caller's re-arm.
bool cover_range(Kernel &k, const CceIndex &idx, Coveror &cv,
                 const std::vector<Ref> &cands, bool filter, uint64_t wm,
                 size_t *stopped_at) {
  const uint64_t tick_start = k.stats.cce_ticks;
  bool any = false;
  if (stopped_at)
    *stopped_at = cands.size();
  for (size_t i = 0; i < cands.size(); i++) {
    if (k.work_budget && k.stats.cce_ticks - tick_start > k.work_budget) {
      if (stopped_at)
        *stopped_at = i;
      break;
    }
    const Ref r = cands[i];
    Clause *c = clause_at(k.arena, r);
    // re-validate: an earlier elimination in this very round may have retired
    // it, and the index is built once
    if (!candidate_clause(k, c))
      continue;
    if (filter && !changed_clause(k, c, wm)) {
      k.stats.cce_mark_skipped++;
      continue;
    }
    any |= cover_clause(k, idx, cv, r);
  }
  return any;
}

} // namespace

bool cce_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void cce_literals(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  for (Lit l = 0; l < 2 * k.nvars; l++)
    if (root_open(k, l))
      out.push_back(l);
}

// N1 single-instance entry: every candidate clause containing `l`. It pays for
// its own index — one arena pass, not one per literal.
ApplyResult cce_try(Kernel &k, Lit l) {
  if (l >= 2 * k.nvars || !root_open(k, l))
    return ApplyResult::NoOp;
  CceIndex idx;
  build_index(k, idx);
  Coveror cv;
  cv.resize(k.nvars);
  std::vector<Ref> cands;
  for (const Ref r : idx.occ[l])
    if (candidate_clause(k, clause_at(k.arena, r)))
      cands.push_back(r);
  if (cands.empty())
    return ApplyResult::NoOp;
  return cover_range(k, idx, cv, cands, /*filter=*/false, 0, nullptr)
             ? ApplyResult::Ok
             : ApplyResult::NoOp;
}

ApplyResult cce_apply(Kernel &k) {
  CceIndex idx;
  build_index(k, idx); // ONE arena pass for the whole round
  Coveror cv;
  cv.resize(k.nvars);
  // Budget-aware batch rule (EffortSpec budget_aware): the policy hands us its
  // remaining ledger in OUR currency and we stop mid-round rather than being
  // metered after the fact (Vivify's idiom, vivify.cpp:275).
  //
  // RESUMPTION + REPETITION, both on the touched-epoch substrate — the same
  // arrangement C1/C2 and BCE use, and the reason this rule no longer keeps a
  // rotation of its own. The first cut of it walked the candidate list from
  // `cce_rounds % ncands`, which was wrong twice over: it put ORDER LOGIC back
  // inside a rule (SCHED-E1 had just moved every such cursor to the policy),
  // and it made a Stats counter BEHAVIOUR-AFFECTING, which the policy layer
  // forbids of itself and which any stats rebase — an incremental re-solve, a
  // per-solve reset — silently resets to index 0.
  //
  // What the rotation was for was starvation: with no per-clause "already tried"
  // flag (cadical's `Clause::covered`) a budget-cut round replays its head. The
  // substrate does that job properly. The tail the budget never reached is
  // RE-ARMED, then the watermark advances, so the next round's candidate set is
  // {the unreached tail} u {whatever changed} — it resumes rather than replaying,
  // and unlike a rotation it also stops re-examining clauses nothing moved in.
  // On the family's off arm (`!k.touch_filter`) there is no filter and therefore
  // no re-arm and no advance, so this rule replays its head — which is what the
  // other five consumers do on that arm too. That arm prices the whole
  // suppressor family; it is not a shipped configuration.
  kernel_touch_sync_fixed(k);
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch; // BEFORE any work, as C1/C2 do
  const uint64_t wm = k.cce_touch_mark;
  size_t stopped_at = idx.cands.size();
  const bool any =
      cover_range(k, idx, cv, idx.cands, filter, wm, &stopped_at);
  for (size_t i = filter ? stopped_at : idx.cands.size(); i < idx.cands.size();
       i++) {
    const Clause *c = clause_at(k.arena, idx.cands[i]);
    if (c->garbage() || !changed_clause(k, c, wm))
      continue; // sharper than a bare re-arm: it had nothing new at `wm` either
    // ONE mark, not one per literal. `changed_clause` needs a single changed
    // variable, so one is exactly enough to re-offer the clause — and a re-arm
    // is a PRODUCER on the shared substrate, so every mark past what the filter
    // needs is a mark that weakens BCE/X2/X4/Factor's filters for nothing. C1/C2
    // touch every literal because their predicate needs TWO (`need = 2`); this
    // one does not. It matters at scale because this rule's dose cuts it early,
    // so the unreached tail is most of the arena and the tail is what is
    // re-armed — the `c cce2` rearm count against `cce_tried` is the reading.
    kernel_touch_var(k, var_of(*c->begin()));
    k.stats.cce_rearmed++;
  }
  if (filter) {
    k.cce_touch_mark = snapshot;
    k.stats.cce_mark_advances++;
  }
  k.stats.cce_rounds++; // ATTEMPTS, not productive rounds. DIAGNOSTIC ONLY.
  return any ? ApplyResult::Ok : ApplyResult::NoOp;
}

static uint32_t cce_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Lit> lits;
  cce_literals(k, lits);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(lits.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(lits[i]);
  return n;
}

static ApplyResult cce_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: complete the root closure if applied off a fixpoint (no-op
  // otherwise); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return cce_apply(k);
  Lit l;
  if (!lit_of_instance(inst, &l))
    return ApplyResult::NoOp;
  return cce_try(k, l);
}

TACTIX_REGISTER_RULE(CCE, RuleTier::Inprocess, cce_guard, cce_apply,
                     cce_instances, cce_apply_instance);

} // namespace tactix

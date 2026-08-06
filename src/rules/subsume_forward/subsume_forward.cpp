// See subsume_forward.hpp for the reference/deviation record.
#include "rules/subsume_forward/subsume_forward.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool subsume_forward_guard(const Kernel &k) {
  // quiescent root (see subsume_forward.hpp)
  // D-SPLIT-3 (guard purification): the ¬conflict_valid conjunct is gone —
  // this is a pure F-rewrite (sound regardless of a recorded conflict), and
  // the conjunct was redundant anyway (conflict_valid ⟹ level>0 at any rule
  // boundary, so it never fired under the retained level==0). calculus §7.
  return !k.inconsistent && k.level == 0;
}

// is `d` (irredundant, live) a subsumer of the marked candidate? true iff
// every literal of d is marked or root-falsified — then units ∪ {d} imply
// the candidate, so deleting the candidate preserves equivalence. A
// root-satisfied d proves nothing about the candidate and never subsumes
// (CaDiCaL subsume_check; kissat forward.c has the same value handling).
static bool subsumes_marked(const Kernel &k, const std::vector<uint8_t> &marks,
                            const Clause *d) {
  for (const Lit l : *d) {
    if (marks[l])
      continue;
    const int8_t v = k.value(l);
    if (v > 0)
      return false; // satisfied subsumer: unusable
    if (v == 0)
      return false; // free literal outside the candidate
    // root-falsified: absorbed by the units
  }
  return true;
}

// a clause qualifies as a candidate (and as a large subsumer): live,
// irredundant, within the size limit, not root-satisfied
static bool is_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant() || c->size > SUBSUME_CLSLIM)
    return false;
  for (const Lit l : *c)
    if (k.value(l) > 0)
      return false; // root-satisfied
  return true;
}

void subsume_forward_candidates(const Kernel &k, std::vector<Ref> &out) {
  out.clear();
  // cache the sort key instead of re-dereferencing clause headers in the
  // comparator (reduce_candidates' Victim pattern)
  struct Cand {
    Ref ref;
    uint32_t size;
  };
  std::vector<Cand> cands;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (is_candidate(k, c))
      cands.push_back({ref, c->size});
    ref += clause_words(c->size);
  }
  std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
    if (a.size != b.size)
      return a.size < b.size;
    return a.ref < b.ref; // total order for determinism
  });
  out.reserve(cands.size());
  for (const Cand &c : cands)
    out.push_back(c.ref);
}

// SCHED-F1 · the ROUND's repetition filter is kernel_clause_new_material
// (kernel.hpp), which carries the argument for the predicate itself.
//
// ▶ POLICY, NOT PRECONDITION (N1). Only the canonical ROUND consults it.
// subsume_forward_candidates — what `instances()` enumerates — and
// subsume_forward_try, the single-candidate `apply_instance` path, stay
// complete, so no API caller, RandomPolicy or phase-6 learner loses the
// ability to subsume any clause at any time; and a skipped candidate is
// re-offered by the very next round the moment anything in it moves. This is
// the same category as the round's `work_budget`, not the category of the
// Factor breach SCHED-N1FIX repaired (a guard that reported applicable while
// apply NoOped). tests/test_subsume.cpp::test_touch_filter_is_policy_only
// pins it through the registry surface and fails on a build that filters
// either of the two complete paths.

// mark the candidate's free literals. NOTE (corpus regression
// 002-underived-root-consequence): a live non-satisfied clause can have
// FEWER than 2 free literals here — engine quiescence (propagate_head ==
// trail.size) does not imply semantic quiescence under N1 histories: a
// root literal appended between Decides has its watch scan consumed at a
// higher level, and a later backtrack pops the derived consequences
// without re-arming the scan. The check semantics below are sound for
// any free count (subsumption is modulo root-falsified literals either
// way); discovery through watch lists may merely miss partners.
static uint32_t mark_candidate(const Kernel &k, std::vector<uint8_t> &marks,
                               const Clause *c, uint8_t on) {
  uint32_t free_lits = 0;
  for (const Lit l : *c) {
    assert(k.value(l) <= 0);
    if (k.value(l) == 0) {
      marks[l] = on;
      free_lits++;
    }
  }
  return free_lits;
}

// binary subsumers of the marked candidate live in the watch lists of its
// free literals: binary (l, o) with both marked subsumes. Skips oversized
// lists (kissat forward_literal's occurrence limit). No redundancy check on
// the binary subsumer: binaries have an irredundant lifecycle, so none can be
// FORGOTTEN — see the binary-subsumer argument in subsume_forward.hpp and the
// one trigger that would make a check mandatory here.
static bool subsumed_by_binary(Kernel &k,
                               const std::vector<uint8_t> &marks,
                               const Clause *c) {
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    const WatchHead &h = k.watches.heads[l];
    if (h.size > SUBSUME_OCCLIM)
      continue;
    const uint32_t *ws = k.watches.arena.data() + h.offset;
    for (uint32_t i = 0; i < h.size;) {
      const Watch w{ws[i]};
      if (w.binary()) {
        k.stats.forward_visits++; // SCHED-B4
        if (marks[w.other()])
          return true;
        i += 1;
      } else {
        i += 2;
      }
    }
  }
  return false;
}

static void delete_subsumed(Kernel &k, Clause *c) {
  if (k.proof)
    k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
  kernel_mark_garbage(k, c);
  k.stats.forward_subsumed++;
}

// canonical instance: one full round (CaDiCaL subsume_round). Candidates
// ascending by size; each is checked against binaries (via watches) and
// against previously connected candidates (via round-local one-literal
// occurrence lists), then connected at its least-occurring free literal.
// Connected clauses are never deleted later in the round (only the
// current candidate dies), so occurrence entries stay valid; nothing
// allocates in the arena, so refs stay stable until the final collect.
ApplyResult subsume_forward_apply(Kernel &k) {
  assert(subsume_forward_guard(k));
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.forward_subsumptions++; // rounds (kissat forward.c stat name)
  // SCHED-F1: snapshot the epoch BEFORE any work, so material this round
  // creates itself (strengthened clauses, new binaries) is still new to the
  // next one — kissat re-arms those same two cases by hand (forward.c:589-645).
  kernel_touch_sync_fixed(k);
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.subsume_touch_mark;
  std::vector<Ref> candidates;
  subsume_forward_candidates(k, candidates);
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<std::vector<Ref>> occs(2 * k.nvars);
  uint64_t deleted = 0;
  // D-STRENGTHEN-BUDGET (ledger): `k.work_budget` in C1's own currency
  // (candidate checks), the Vivify/Factor idiom. This rule had an ENTRY budget
  // (100 per mille = kissat's `forwardeffort`) but could not stop once a round
  // began; see the twin comment in strengthen.cpp for the measured motive.
  // Stopping early is sound: survivors remain candidates for the next round.
  // 0 = unbounded (legacy path).
  // SCHED-B4: budget polled in PARTNER VISITS, the currency the effort ledger
  // now meters this rule in (see Stats::forward_visits). Polling in candidates
  // let one candidate with a saturated occurrence list run unboundedly.
  const uint64_t check_start = k.stats.forward_visits;
  size_t stopped_at = candidates.size(); // index of the first UNREACHED one
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    const Ref ref = candidates[ci];
    if (k.midround_budget && k.work_budget &&
        k.stats.forward_visits - check_start > k.work_budget) {
      k.stats.budget_stops_c1++;
      stopped_at = ci;
      break;
    }
    Clause *c = clause_at(k.arena, ref);
    // SCHED-F1: a candidate with nothing new in it is not CHECKED, but it is
    // still CONNECTED below, so the round's subsumer index stays complete and
    // the filter costs no subsumption that the unfiltered round would find.
    // (kissat drops unscheduled clauses from its index too — connect_subsuming
    // demands flags->subsume on EVERY literal, forward.c:488-492 — and thereby
    // loses old-subsumes-new; the index is round-local and cheap here, so
    // there is no reason to inherit that.)
    const bool check = !filter || kernel_clause_new_material(k, c, wm);
    bool subsumed = false;
    if (check) {
      k.stats.forward_checked++;
      mark_candidate(k, marks, c, 1);
      subsumed = subsumed_by_binary(k, marks, c);
      if (!subsumed) {
        for (const Lit l : *c) {
          if (k.value(l) != 0)
            continue;
          const std::vector<Ref> &os = occs[l];
          if (os.size() > SUBSUME_OCCLIM)
            continue;
          for (const Ref dref : os) {
            k.stats.forward_visits++; // SCHED-B4
            const Clause *d = clause_at(k.arena, dref);
            assert(d->size <= c->size); // connected earlier => not larger
            if (subsumes_marked(k, marks, d)) {
              subsumed = true;
              break;
            }
          }
          if (subsumed)
            break;
        }
      }
      mark_candidate(k, marks, c, 0);
    } else {
      k.stats.forward_mark_skipped++;
    }
    // SCHED-G2 · SHADOW MODE (Kernel::subsume_shadow): record the victim and
    // let it live, so the rest of the solve is a C1-less trajectory in which
    // Strengthen's absorption of this exact clause is observable. Declining a
    // deletion is sound in the same way a budget cut is — the rule has simply
    // done less — and the shadowed clause falls through to `connect` below, so
    // it stays a subsumer for the rest of the round exactly as a survivor does.
    if (subsumed && k.subsume_shadow) {
      kernel_shadow_observe(k, c->id());
      subsumed = false;
    }
    if (subsumed) {
      delete_subsumed(k, c);
      deleted++;
      continue;
    }
    // connect at the least-occurring free literal (skip when even the
    // minimum is saturated — kissat connect_subsuming's occlim)
    Lit min_lit = INVALID_LIT;
    size_t min_occs = 0;
    for (const Lit l : *c) {
      if (k.value(l) != 0)
        continue;
      const size_t n = occs[l].size();
      if (min_lit == INVALID_LIT || n < min_occs) {
        min_lit = l;
        min_occs = n;
      }
    }
    assert(min_lit != INVALID_LIT);
    if (min_occs <= SUBSUME_OCCLIM)
      occs[min_lit].push_back(ref);
  }
  // SCHED-F2 · RESUME. A cut round re-arms the variables of the candidates it
  // never reached, then advances the watermark like any other round — kissat's
  // own arrangement (forward.c:589-645, "reactivating subsume flag of %s in
  // remaining or strengthened"). The next round's candidate set is therefore
  // {the unreached tail} ∪ {new material}, so it RESUMES instead of replaying
  // its head, which is SCHED-B3's starvation in C1's currency. Re-arming in the
  // substrate rather than through a positional cursor is what makes it survive
  // the `kernel_collect_garbage` below, which invalidates every Ref.
  //
  // Sharper than kissat on one point: a tail candidate that FAILS the filter is
  // not re-armed. It had no new material at `wm`, and the new watermark is
  // >= `wm`, so it can have none there either — re-arming it would schedule a
  // check that provably cannot fire.
  for (size_t ci = filter ? stopped_at : candidates.size();
       ci < candidates.size(); ci++) {
    const Clause *c = clause_at(k.arena, candidates[ci]);
    if (c->garbage() || !kernel_clause_new_material(k, c, wm))
      continue;
    for (const Lit l : *c)
      kernel_touch_var(k, var_of(l));
    k.stats.forward_rearmed++;
  }
  // Advancing to the round-entry `snapshot`, not to the current epoch: what
  // this round created — including the re-arms just above — is new material for
  // the next one.
  if (filter) {
    k.subsume_touch_mark = snapshot;
    k.stats.forward_mark_advances++;
  }
  // Completeness, kissat's `completed` (forward.c:664-670): nothing was
  // re-armed. This rule only DELETES, and a deletion creates no new subsumer,
  // so "reached the end of the list" is the whole condition here.
  k.subsume_complete = stopped_at == candidates.size();
  if (!deleted)
    return ApplyResult::NoOp;
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// instance surface: try to subsume exactly one candidate. Discovery goes
// through the WATCH lists of the candidate's free literals: a subsumer d
// watches two of its own literals, and every non-false literal of d is a
// free literal of the candidate, so d appears there (falsified literals
// are never watched in a fully propagated consistent state). Unlike the
// round this sees ALL live clauses, so redundant large clauses must be
// skipped explicitly (deleting an irredundant clause on the strength of
// a deletable one would be unsound once Reduce removes the subsumer).
ApplyResult subsume_forward_try(Kernel &k, Ref candidate) {
  assert(subsume_forward_guard(k));
  Clause *c = clause_at(k.arena, candidate);
  if (!is_candidate(k, c))
    return ApplyResult::NoOp;
  k.stats.forward_checked++;
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  mark_candidate(k, marks, c, 1);
  // one fused scan per free literal's watch list: binary entries subsume
  // when the other literal is marked, large entries via subsumes_marked
  // (a boolean existence check — fusing the formerly separate binary and
  // large passes cannot change the outcome)
  bool subsumed = false;
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    const WatchHead &h = k.watches.heads[l];
    if (h.size > SUBSUME_OCCLIM)
      continue;
    const uint32_t *ws = k.watches.arena.data() + h.offset;
    for (uint32_t i = 0; i < h.size && !subsumed;) {
      const Watch w{ws[i]};
      k.stats.forward_visits++; // SCHED-B4
      if (w.binary()) {
        subsumed = marks[w.other()] != 0;
        i += 1;
        continue;
      }
      const Ref dref = ws[i + 1];
      i += 2;
      if (dref == candidate)
        continue;
      const Clause *d = clause_at(k.arena, dref);
      if (d->garbage() || d->redundant() || d->size > c->size)
        continue;
      subsumed = subsumes_marked(k, marks, d);
    }
    if (subsumed)
      break;
  }
  mark_candidate(k, marks, c, 0);
  if (!subsumed)
    return ApplyResult::NoOp;
  delete_subsumed(k, c);
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// generic instance surface (plan §5.1 v1): payload = candidate arena Ref,
// canonical-first order = size ascending (the round's processing order)
static uint32_t subsume_forward_instances(Kernel &k, uint64_t *out,
                                          uint32_t cap) {
  std::vector<Ref> candidates;
  subsume_forward_candidates(k, candidates);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult subsume_forward_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return subsume_forward_apply(k);
  // validate the payload against clause starts (arbitrary uint64s must be
  // rejected, not dereferenced)
  for (Ref ref = 0; ref < k.arena.size();) {
    if (ref == inst)
      return subsume_forward_try(k, ref);
    if (ref > inst)
      break;
    ref += clause_words(clause_at(k.arena, ref)->size);
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(SubsumeForward, RuleTier::Inprocess, subsume_forward_guard,
                     subsume_forward_apply, subsume_forward_instances,
                     subsume_forward_apply_instance);

} // namespace tactix

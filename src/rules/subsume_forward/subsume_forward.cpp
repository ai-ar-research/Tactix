// See subsume_forward.hpp for the rule's contract.
#include "rules/subsume_forward/subsume_forward.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility subsume_forward_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/calculus.md` §7).
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

// is `d` (irredundant, live) a subsumer of the marked candidate? true iff every
// literal of d is marked or root-falsified — then units ∪ {d} imply the
// candidate, so deleting it preserves equivalence. A root-satisfied d proves
// nothing and never subsumes (cadical subsume_check's value handling).
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

// A clause qualifies as a victim: live, within the size limit, not
// root-satisfied. Redundancy is deliberately not part of it — deleting a
// redundant clause is licensed unconditionally (S2 Forget). The load-bearing
// redundancy check sits on the subsumer instead (subsume_forward.hpp).
static bool is_victim(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->size > SUBSUME_CLSLIM)
    return false;
  for (const Lit l : *c)
    if (k.value(l) > 0)
      return false; // root-satisfied
  return true;
}

// ...and the narrower set the canonical round works on. The redundant tier band
// is open to C2 Strengthen and not to this rule, which C2 absorbs almost
// entirely. A round-scope decision, not a precondition (see the N1 note below).
// Ledger row D-C1C2-VICTIM.
static bool is_round_candidate(const Kernel &k, const Clause *c) {
  return !c->redundant() && is_victim(k, c);
}

void subsume_forward_candidates(const Kernel &k, std::vector<Ref> &out,
                                bool complete) {
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
    if (complete ? is_victim(k, c) : is_round_candidate(k, c))
      cands.push_back({ref, c->size});
    ref += clause_actual_words(c);
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

// Policy, not precondition (N1): only the canonical round consults the
// repetition filter (kernel_clause_new_material) and the round's redundancy
// scope. `instances()` enumerates every victim and `subsume_forward_try`
// accepts any of them, so no caller loses the ability to subsume any clause at
// any time, and a skipped candidate is re-offered the moment anything in it
// moves. Pinned by tests/test_subsume.cpp::test_touch_filter_is_policy_only.
//
// Mark the candidate's free literals. A live non-satisfied clause can have
// fewer than two free literals here: engine quiescence does not imply semantic
// quiescence under N1 histories (corpus 002-underived-root-consequence). The
// check below is sound for any free count, since subsumption is modulo
// root-falsified literals either way; discovery may merely miss partners.
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

static bool watch_population_within(const Kernel &k, Lit lit, bool binary,
                                    uint32_t limit) {
  if (binary)
    return k.watches.binary_size(lit) <= limit;
  uint32_t count = 0;
  // Large entries still require the complete entry-aware walk: unlike the
  // exact binary population, lazy garbage remains part of this physical cap.
  for (const WatchEntry w : k.watches.entries(lit))
    if (!w.binary() && ++count > limit)
      return false;
  return true;
}

// Match Kissat forward_literal's local `steps` accumulator: partner visits are
// a hot-loop scalar and become observable only when the rule returns.  The
// destructor also publishes exact partial work on every early exit.
struct ForwardVisitCounter {
  uint64_t &published;
  uint64_t value;
  explicit ForwardVisitCounter(uint64_t &counter)
      : published(counter), value(counter) {}
  ~ForwardVisitCounter() { published = value; }
};

// Binary subsumers of the marked candidate live in the watch lists of its free
// literals: binary (l, o) with both marked subsumes. Skips oversized lists
// (kissat forward_literal's occurrence limit). No redundancy check on the
// binary subsumer — see subsume_forward.hpp.
static bool subsumed_by_binary(Kernel &k,
                               const std::vector<uint8_t> &marks,
                               const Clause *c, uint64_t &visits,
                               uint64_t visit_start, bool bounded,
                               uint64_t visit_budget,
                               bool &budget_exhausted) {
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    if (!watch_population_within(k, l, true, SUBSUME_BINLIM))
      continue;
    const bool stop_at_large = k.watches.heads[l].binaries_first();
    for (const WatchEntry w : k.watches.entries(l)) {
      if (!w.binary()) {
        if (stop_at_large)
          break;
        continue;
      }
      visits++;
      if (bounded && visits - visit_start >= visit_budget) {
        budget_exhausted = true;
        return false;
      }
      if (marks[w.other()])
        return true;
    }
  }
  return false;
}

static void delete_subsumed(Kernel &k, Clause *c) {
  kernel_delete_clause(k, c);
  k.stats.forward_subsumed++;
}

// Canonical instance: one full round. Candidates ascending by size; each is
// checked against binaries (via watches) and against previously connected
// candidates (via round-local occurrence lists), then connected at its
// least-occurring free literal. Connected clauses are never deleted later in
// the round, so occurrence entries stay valid, and nothing allocates in the
// arena, so refs stay stable until the collect.
ApplyResult subsume_forward_apply(Kernel &k) {
  assert(subsume_forward_guard(k));
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.forward_subsumptions++; // rounds (kissat forward.c stat name)
  // Snapshot the epoch before any work, so material this round creates itself
  // (strengthened clauses, new binaries) is still new to the next one — kissat
  // re-arms those same two cases by hand (forward.c:589-645).
  kernel_touch_sync_fixed(k);
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.subsume_touch_mark;
  std::vector<Ref> candidates;
  subsume_forward_candidates(k, candidates);
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<std::vector<Ref>> occs(2 * k.nvars);
  // `connect_subsuming` in Kissat selects the minimum from the complete dense
  // occurrence population (forward.c:467-507).  Selecting from this initially
  // empty, growing `occs` image can make a physically saturated literal look
  // cold and defeats the population cap.  DefaultPolicy owns the dense image;
  // materialize an exact complete fallback for direct canonical rule calls.
  std::vector<uint32_t> fallback_population;
  if (!k.dense) {
    fallback_population.assign(2 * k.nvars, 0);
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage() && !c->redundant())
        for (const Lit l : *c)
          fallback_population[l]++;
      ref += clause_actual_words(c);
    }
  }
  const auto large_population = [&](Lit l) -> size_t {
    return k.dense ? k.dense->large[l].size() : fallback_population[l];
  };
  uint64_t deleted = 0;
  // `k.work_budget` in this rule's own currency, polled in PARTNER visits;
  // polling in candidates would let one candidate with a saturated occurrence
  // list run unboundedly. Stopping early is sound — survivors remain candidates
  // next round. 0 = unbounded. Dosing: ledger row D-STRENGTHEN-BUDGET.
  ForwardVisitCounter visits(k.stats.forward_visits);
  const uint64_t check_start = visits.value;
  const bool visit_bounded = k.midround_budget && k.work_budget;
  const uint64_t visit_budget = k.work_budget;
  size_t stopped_at = candidates.size(); // index of the first unreached one
  // §6 termination polling, reusing the budget's cut point: an interrupted
  // round is bookkept as an incomplete round, not as a special case, so F/R/τ/ρ
  // and the proof stream cannot drift apart. Nothing is half-mutated at the top
  // of the iteration. One poll per candidate and NO tick cadence: a candidate
  // is up to CLSLIM x OCCLIM partner visits, so the poll is free here, whereas
  // a `!(++tick & 255)` mask makes every round shorter than one mask period
  // unreachable — the shape that lets an abort path fuzz green yet inert.
  bool interrupted = false;
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    const Ref ref = candidates[ci];
    if (visit_bounded && visits.value - check_start >= visit_budget) {
      k.stats.budget_stops_c1++;
      stopped_at = ci;
      break;
    }
    if (kernel_rule_interrupt(k, k.stats.subsume_interrupted)) {
      stopped_at = ci;
      interrupted = true;
      break;
    }
    Clause *c = clause_at(k.arena, ref);
    // A candidate with nothing new in it is not checked, but it is still
    // connected below, so the round's subsumer index stays complete and the
    // filter costs no subsumption the unfiltered round would find. kissat drops
    // unscheduled clauses from its index too (forward.c:488-492) and thereby
    // loses old-subsumes-new; the index is round-local and cheap here.
    const bool check = !filter || kernel_clause_new_material(k, c, wm);
    bool subsumed = false;
    bool budget_exhausted = false;
    if (check) {
      k.stats.forward_checked++;
      mark_candidate(k, marks, c, 1);
      subsumed = subsumed_by_binary(k, marks, c, visits.value, check_start,
                                    visit_bounded, visit_budget,
                                    budget_exhausted);
      if (!subsumed) {
        for (const Lit l : *c) {
          if (budget_exhausted)
            break;
          if (k.value(l) != 0)
            continue;
          const std::vector<Ref> &os = occs[l];
          if (os.size() > SUBSUME_OCCLIM)
            continue;
          for (const Ref dref : os) {
            visits.value++;
            if (visit_bounded &&
                visits.value - check_start >= visit_budget) {
              budget_exhausted = true;
              break;
            }
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
      if (budget_exhausted) {
        k.stats.budget_stops_c1++;
        stopped_at = ci;
        break;
      }
    } else {
      k.stats.forward_mark_skipped++;
    }
    // Shadow mode (`Kernel::subsume_shadow`, diagnostic): record the victim and
    // let it live, so the rest of the solve is a C1-less trajectory in which
    // Strengthen's absorption of this clause is observable. Declining a
    // deletion is sound in the same way a budget cut is, and the shadowed
    // clause still falls through to `connect` below.
    if (subsumed && k.subsume_shadow) {
      kernel_shadow_observe(k, c->id());
      subsumed = false;
    }
    if (subsumed) {
      delete_subsumed(k, c);
      deleted++;
      continue;
    }
    // Connect at the least-occurring free literal in the complete dense
    // population (skip when even the minimum is saturated — Kissat's
    // `connect_subsuming` occlim).
    Lit min_lit = INVALID_LIT;
    size_t min_occs = 0;
    for (const Lit l : *c) {
      if (k.value(l) != 0)
        continue;
      const size_t n = large_population(l);
      if (min_lit == INVALID_LIT || n < min_occs) {
        min_lit = l;
        min_occs = n;
      }
    }
    assert(min_lit != INVALID_LIT);
    if (min_occs <= SUBSUME_OCCLIM)
      occs[min_lit].push_back(ref);
  }
  // Resume. A cut round re-arms the variables of the candidates it never
  // reached, then advances the watermark (kissat forward.c:589-645), so the
  // next round's set is {unreached tail} ∪ {new material}. Re-arming in the
  // substrate rather than through a positional cursor is what makes it survive
  // `kernel_collect_garbage` below, which invalidates every Ref. A tail
  // candidate that fails the filter is not re-armed: it had no new material at
  // `wm` and the new watermark is >= `wm`. An interrupted round does neither —
  // no re-arm, no advance — which re-offers the identical candidate set without
  // perturbing every other rule's filter (same condition in strengthen.cpp).
  for (size_t ci = interrupted ? candidates.size()
                               : (filter ? stopped_at : candidates.size());
       ci < candidates.size(); ci++) {
    const Clause *c = clause_at(k.arena, candidates[ci]);
    if (c->garbage() || !kernel_clause_new_material(k, c, wm))
      continue;
    for (const Lit l : *c)
      kernel_touch_var(k, var_of(l));
    k.stats.forward_rearmed++;
  }
  // Advance to the round-entry `snapshot`, not the current epoch: what this
  // round created — the re-arms above included — is new material next round.
  if (filter && !interrupted) {
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

// Instance surface: try to subsume exactly one candidate. Discovery goes
// through the watch lists of the candidate's free literals — a subsumer d
// watches two of its own literals, and every non-false literal of d is a free
// literal of the candidate, so d appears there. The victim may be redundant
// here, which is the complete surface N1 requires; the subsumer may not, hence
// the `d->redundant()` skip below.
ApplyResult subsume_forward_try(Kernel &k, Ref candidate) {
  assert(subsume_forward_guard(k));
  // The round's `work_budget` is a round device and is not consulted here, so
  // termination gets its own check — one per call, at entry, where nothing is
  // marked. The scan below is bounded, but at CLSLIM x OCCLIM it is the same
  // order of work as one round candidate.
  if (kernel_rule_interrupt(k, k.stats.subsume_interrupted))
    return ApplyResult::NoOp;
  Clause *c = clause_at(k.arena, candidate);
  if (!is_victim(k, c))
    return ApplyResult::NoOp;
  k.stats.forward_checked++;
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  mark_candidate(k, marks, c, 1);
  // one fused scan per free literal's watch list: binary entries subsume when
  // the other literal is marked, large entries via subsumes_marked
  bool subsumed = false;
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    const bool binaries =
        watch_population_within(k, l, true, SUBSUME_BINLIM);
    const bool large =
        watch_population_within(k, l, false, SUBSUME_OCCLIM);
    for (const WatchEntry w : k.watches.entries(l)) {
      if (subsumed)
        break;
      k.stats.forward_visits++;
      if (w.binary()) {
        if (binaries)
          subsumed = marks[w.other()] != 0;
        continue;
      }
      if (!large)
        continue;
      const Ref dref = w.ref();
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

// Generic instance surface: payload = candidate arena Ref, canonical-first
// order = size ascending. Complete over victims — every clause
// `apply_instance` would accept is offered, redundant ones included, so the two
// halves of the surface agree.
static uint32_t subsume_forward_instances(Kernel &k, uint64_t *out,
                                          uint32_t cap) {
  std::vector<Ref> candidates;
  subsume_forward_candidates(k, candidates, /*complete=*/true);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult subsume_forward_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
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
    ref += clause_actual_words(clause_at(k.arena, ref));
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(SubsumeForward, RuleTier::Inprocess, subsume_forward_guard,
                     subsume_forward_apply, subsume_forward_instances,
                     subsume_forward_apply_instance);

} // namespace tactix

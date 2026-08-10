// See strengthen.hpp for the rule's contract.
#include "rules/strengthen/strengthen.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool strengthen_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/calculus.md` §7).
  return !k.inconsistent && k.level == 0;
}

// Victim criteria, shared with C1 (duplicated deliberately — modules stay
// self-contained per §5.3). Redundancy is not one of them: deleting a redundant
// clause is S2 Forget, and strengthening one emits an add-RUP that stands
// against the whole formula at emit time.
static bool is_victim(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->size > STRENGTHEN_CLSLIM)
    return false;
  for (const Lit l : *c)
    if (k.value(l) > 0)
      return false; // root-satisfied
  return true;
}

// Does the canonical round work on `c`? Irredundant always; a redundant clause
// only inside the tier band, and only with the band on
// (`Kernel::touch_redundant`). cadical's scope test (subsume.cpp:388 filters on
// `likely_to_be_kept_clause`), evaluated LIVE, so band membership is read now
// rather than at creation. Ledger row D-C1C2-VICTIM.
static bool in_round_scope(const Kernel &k, const Clause *c) {
  if (!is_victim(k, c))
    return false;
  return !c->redundant() ||
         (k.touch_redundant &&
          kernel_likely_kept(k, true, c->glue(), c->size));
}

void strengthen_candidates(const Kernel &k, std::vector<Ref> &out,
                           bool complete) {
  out.clear();
  struct Cand {
    Ref ref;
    uint32_t size;
    bool redundant;
  };
  std::vector<Cand> cands;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (complete ? is_victim(k, c) : in_round_scope(k, c))
      cands.push_back({ref, c->size, c->redundant()});
    ref += clause_words(c->size);
  }
  // Irredundant first, then size ascending. The band is the larger population
  // by orders of magnitude and its clauses are small, so a plain size-ascending
  // order would let them crowd the round's budget and starve the irredundant
  // work; ordering the band behind makes it a strict addition at the tail.
  std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
    if (a.redundant != b.redundant)
      return !a.redundant;
    if (a.size != b.size)
      return a.size < b.size;
    return a.ref < b.ref; // total order for determinism
  });
  out.reserve(cands.size());
  for (const Cand &c : cands)
    out.push_back(c.ref);
}

// outcome of checking one partner clause against the marked candidate
struct CheckResult {
  enum { NONE, SUBSUMED, STRENGTHEN } kind = NONE;
  Lit flipped = INVALID_LIT; // partner literal in opposite phase; the
                             // candidate loses negate(flipped)
};

// cadical subsume.cpp subsume_check, transliterated: every partner literal must
// be marked in the same phase, root-falsified (absorbed by units), or — at most
// once — marked in the opposite phase, which is the strengthen case.
static CheckResult check_marked(const Kernel &k,
                                const std::vector<uint8_t> &marks,
                                const Clause *d) {
  CheckResult r;
  for (const Lit l : *d) {
    if (marks[l])
      continue;
    const int8_t v = k.value(l);
    if (v > 0)
      return {}; // satisfied partner proves nothing
    if (v < 0)
      continue; // absorbed
    if (marks[negate(l)]) {
      if (r.flipped != INVALID_LIT)
        return {}; // two flips: plain resolution, not self-subsuming
      r.flipped = l;
      continue;
    }
    return {}; // free literal outside the candidate
  }
  r.kind = r.flipped == INVALID_LIT ? CheckResult::SUBSUMED
                                    : CheckResult::STRENGTHEN;
  return r;
}

static void mark_candidate(const Kernel &k, std::vector<uint8_t> &marks,
                           const Clause *c, uint8_t on) {
  for (const Lit l : *c)
    if (k.value(l) == 0)
      marks[l] = on;
}

static bool watch_population_within(const Kernel &k, Lit lit, bool binary,
                                    uint32_t limit) {
  uint32_t count = 0;
  for (const WatchEntry w : k.watches.entries(lit))
    if (w.binary() == binary && ++count > limit)
      return false;
  return true;
}

// partner found by a scan: NONE / SUBSUMED / STRENGTHEN with the flipped
// literal plus what the LRAT chain needs (partner id + its absorbed units)
struct Partner {
  CheckResult res;
  uint64_t id = 0;
  std::vector<uint64_t> absorbed; // unit ids of root-falsified partner lits
};

// scan the candidate's watch lists (both phases) for binary partners and the
// round's occurrence lists (both phases) for large ones. occs == nullptr
// selects the watch-list-only discovery the single-candidate instance surface
// uses. Free literals only; both scans are read-only.
static Partner find_partner(Kernel &k, const std::vector<uint8_t> &marks,
                            const Clause *c,
                            const std::vector<std::vector<Ref>> *occs,
                            Ref self) {
  Partner p;
  const bool lrat = k.proof && k.proof->lrat();
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; phase++) {
      const Lit probe = phase ? negate(l) : l;
      // binaries through the watch lists: (probe, o). probe == l is a
      // same-phase anchor; probe == ¬l anchors the flip on l itself.
      if (watch_population_within(k, probe, true, STRENGTHEN_BINLIM)) {
        for (const WatchEntry w : k.watches.entries(probe)) {
          if (!w.binary())
            continue;
          k.stats.strengthen_visits++; // a binary partner compared
          const Lit o = w.other();
          if (phase == 0) {
            // no redundancy check on the binary partner: the binary-subsumer
            // argument in subsume_forward.hpp covers this
            if (marks[o]) { // (l ∨ o), both in candidate: subsumes
              p.res.kind = CheckResult::SUBSUMED;
              if (lrat)
                p.id = kernel_binary_id(k, probe, o);
              return p;
            }
            if (marks[negate(o)]) { // (l ∨ o), candidate has ¬o: drop ¬o
              p.res.kind = CheckResult::STRENGTHEN;
              p.res.flipped = o;
              if (lrat)
                p.id = kernel_binary_id(k, probe, o);
              return p;
            }
          } else if (marks[o]) { // (¬l ∨ o), candidate has l, o: drop l
            p.res.kind = CheckResult::STRENGTHEN;
            p.res.flipped = probe;
            if (lrat)
              p.id = kernel_binary_id(k, probe, o);
            return p;
          }
        }
      }
      if (!occs) // instance surface: large partners via watch lists below
        continue;
      const std::vector<Ref> &os = (*occs)[probe];
      if (os.size() > STRENGTHEN_OCCLIM)
        continue;
      for (const Ref dref : os) {
        k.stats.strengthen_visits++;
        const Clause *d = clause_at(k.arena, dref);
        const CheckResult r = check_marked(k, marks, d);
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        if (lrat) {
          p.id = d->id();
          for (const Lit m : *d)
            if (k.value(m) < 0)
              p.absorbed.push_back(k.unit_ids[var_of(m)]);
        }
        return p;
      }
    }
  }
  if (occs)
    return p;
  // instance surface: large partners live in the watch lists of the candidate's
  // own free literals — a partner watches two of its own literals, each a
  // candidate literal or its negation — so rerun the phase loop for them
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; phase++) {
      const Lit probe = phase ? negate(l) : l;
      if (!watch_population_within(k, probe, false, STRENGTHEN_OCCLIM))
        continue;
      for (const WatchEntry w : k.watches.entries(probe)) {
        if (w.binary())
          continue;
        const Ref dref = w.ref();
        if (dref == self)
          continue;
        k.stats.strengthen_visits++;
        const Clause *d = clause_at(k.arena, dref);
        if (d->garbage() || d->redundant() || d->size > c->size)
          continue;
        const CheckResult r = check_marked(k, marks, d);
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        if (lrat) {
          p.id = d->id();
          for (const Lit m : *d)
            if (k.value(m) < 0)
              p.absorbed.push_back(k.unit_ids[var_of(m)]);
        }
        return p;
      }
    }
  }
  return p;
}

// act() on a found partner. Returns: 0 = candidate unchanged (NONE),
// 1 = candidate died (subsumed), 2 = strengthened into *ref (rescan),
// 3 = strengthened to binary (done with candidate), 4 = unit asserted
// (round must end: values changed), 5 = empty clause derived (UNSAT)
static int act(Kernel &k, Ref *ref, const Partner &p, uint64_t *changed) {
  if (p.res.kind == CheckResult::NONE)
    return 0;
  Clause *c = clause_at(k.arena, *ref);
  if (p.res.kind == CheckResult::SUBSUMED) {
    // C1 shadowed this clause and C2 has now deleted it on its own.
    if (kernel_shadow_retire(k, c->id()))
      k.stats.c1_shadow_deleted++;
    kernel_delete_clause(k, c);
    k.stats.strengthen_subsumed++;
    (*changed)++;
    return 1;
  }
  // Strengthen: rebuild from free literals minus the resolved one. A shadowed
  // clause rewritten away is also handled, but that is a different disposal
  // from a deletion, so it gets its own counter.
  if (kernel_shadow_retire(k, c->id()))
    k.stats.c1_shadow_rewritten++;
  const Lit gone = negate(p.res.flipped);
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<Lit> lits;
  std::vector<uint64_t> chain;
  lits.reserve(c->size);
  for (const Lit l : *c) {
    if (l == gone)
      continue;
    if (k.value(l) < 0) { // stripped; its unit absorbs it for the checker
      if (lrat)
        chain.push_back(k.unit_ids[var_of(l)]);
      continue;
    }
    lits.push_back(l);
  }
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat) {
      chain.insert(chain.end(), p.absorbed.begin(), p.absorbed.end());
      chain.push_back(p.id);    // partner propagates the flipped literal
      chain.push_back(c->id()); // then the original conflicts
      k.proof->add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()),
                       chain.data(), static_cast<uint32_t>(chain.size()));
    } else {
      k.proof->add_rup(0, lits.data(), static_cast<uint32_t>(lits.size()),
                       nullptr, 0);
    }
  }
  k.stats.strengthened++;
  (*changed)++;
  // rewrites preserve redundancy (the flush pre-pass rewrites redundant clauses
  // too) and glue can only shrink; read before the replacement garbages `c`.
  // Reached from `act` and from `flush_root`, so root-unit stripping promotes
  // on the same rule.
  const bool redundant = c->redundant();
  const uint32_t glue = kernel_shrunken_glue(
      k, c->glue(), static_cast<uint32_t>(lits.size()));
  switch (kernel_replace_clause(k, c, id, lits.data(),
                                static_cast<uint32_t>(lits.size()), redundant,
                                redundant ? glue : 0, ref)) {
  case ReplaceOutcome::Refuted:
    return 5; // corpus 002: the empty clause just emitted IS the refutation
  case ReplaceOutcome::Unit: // unit: assert at root, end the round
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
    return 4;
  case ReplaceOutcome::Binary:
    return 3;
  default:
    return 2;
  }
}

// C2 consumes root units through the same proof-safe clause primitive as the
// startup sparse collector. Map its typed result onto act()'s local control
// codes: 0 none, 1 deleted, 2 rewritten, 3 binary, 4 unit, 5 refuted.
static int flush_root(Kernel &k, Ref *ref, uint64_t *changed) {
  switch (kernel_flush_root_clause(k, ref, changed)) {
  case RootFlushOutcome::None: return 0;
  case RootFlushOutcome::Deleted: return 1;
  case RootFlushOutcome::Rewritten: return 2;
  case RootFlushOutcome::Binary: return 3;
  case RootFlushOutcome::Unit: return 4;
  case RootFlushOutcome::Refuted: return 5;
  }
  assert(false);
  return 5;
}

ApplyResult strengthen_apply(Kernel &k) {
  assert(strengthen_guard(k));
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.strengthen_rounds++;
  // Repetition filter, C1's arrangement exactly (see the N1 note in
  // subsume_forward.cpp). The snapshot is taken before the flush pre-pass on
  // purpose: the flush rewrites every clause a root fix shortened, each rewrite
  // is an add, so those land above the snapshot and this round already sees
  // them as new.
  kernel_touch_sync_fixed(k);
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.strengthen_touch_mark;
  uint64_t changed = 0;
  bool ended = false;
  // §6 termination polling, at all THREE unbounded loops — the flush pre-pass,
  // the candidate loop, and the cascade over one shrinking candidate — since
  // one round can own most of a solve and a poll in only one still ignores the
  // interrupt for minutes. Every site aborts between candidates or clauses,
  // never mid-rewrite, so an interrupted round is exactly an incomplete round.
  // No tick cadence at any of them (see subsume_forward.cpp).
  bool interrupted = false;
  const auto poll = [&]() {
    if (!kernel_rule_interrupt(k, k.stats.strengthen_interrupted))
      return false;
    interrupted = true;
    return true;
  };
  // Flush pre-pass (see flush_root): the whole arena, before the candidate
  // round, so the round never sees clauses with root-assigned literals. A
  // flush-derived unit ends the pass (values changed) and the policy
  // re-propagates. One collect at the end covers both phases.
  for (Ref ref = 0; ref < k.arena.size();) {
    if (poll())
      break;
    const uint32_t words = clause_words(clause_at(k.arena, ref)->size);
    Ref cur = ref;
    if (!clause_at(k.arena, ref)->garbage()) {
      const int r = flush_root(k, &cur, &changed);
      if (r == 5)
        return ApplyResult::Unsat;
      if (r == 4) {
        ended = true;
        break;
      }
    }
    ref += words;
  }
  std::vector<Ref> candidates;
  if (!ended && !interrupted)
    strengthen_candidates(k, candidates);
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<std::vector<Ref>> occs(2 * k.nvars);
  // `k.work_budget` in this rule's own currency (partner visits). An entry
  // budget alone bounds how often a round runs, never how much one round does,
  // which lets a single uninterruptible round own most of a solve; cadical
  // bounds the same family per round (`subsumeeffort`, options.hpp:218-221).
  // Stopping early loses no work — survivors stay candidates. 0 = unbounded.
  // Ledger row D-STRENGTHEN-BUDGET.
  const uint64_t check_start = k.stats.strengthen_visits;
  // Connect a survivor at its least-occurring free literal. Shared by the
  // checked path and by the filter's skip path, so the round's partner index
  // stays complete and the filter costs no strengthening.
  //
  // Redundant survivors are NOT indexed: `occs` is the partner index, a partner
  // justifies deleting or rewriting another clause, so it must not be
  // forgettable (the header's Forget argument). Widening the victim set
  // therefore leaves the partner set where it was, which is also what keeps the
  // band's cost bounded — band clauses are checked, never chased.
  const auto connect = [&](Ref cr) {
    const Clause *c = clause_at(k.arena, cr);
    if (c->redundant())
      return;
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
    // under-derived roots (corpus 002) can leave a candidate with no free
    // literals; nothing to connect then
    if (min_lit != INVALID_LIT && min_occs <= STRENGTHEN_OCCLIM)
      occs[min_lit].push_back(cr);
  };
  size_t stopped_at = candidates.size(); // index of the first unreached one
  const uint64_t produced_before = k.stats.strengthened;
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    Ref cref = candidates[ci];
    if (k.midround_budget && k.work_budget &&
        k.stats.strengthen_visits - check_start > k.work_budget) {
      k.stats.budget_stops_c2++;
      stopped_at = ci;
      break;
    }
    if (poll()) {
      stopped_at = ci;
      break;
    }
    // A band candidate reads the band stamps too, an irredundant one does not
    // (kernel_clause_new_material's `kept`): a redundant clause can never be a
    // partner here, so no band event can change an irredundant candidate's
    // outcome.
    const bool red = clause_at(k.arena, cref)->redundant();
    if (filter &&
        !kernel_clause_new_material(k, clause_at(k.arena, cref), wm, 2, red)) {
      k.stats.strengthen_mark_skipped++;
      k.stats.strengthen_red_skipped += red;
      connect(cref);
      continue;
    }
    k.stats.strengthen_red_checked += red;
    const uint64_t red_sub_before = k.stats.strengthen_subsumed;
    const uint64_t red_str_before = k.stats.strengthened;
    for (;;) { // strengthening cascades on the same candidate
      Clause *c = clause_at(k.arena, cref);
      k.stats.strengthen_checked++;
      mark_candidate(k, marks, c, 1);
      const Partner p = find_partner(k, marks, c, &occs, cref);
      mark_candidate(k, marks, clause_at(k.arena, cref), 0);
      const int r = act(k, &cref, p, &changed);
      if (r == 5)
        return ApplyResult::Unsat;
      if (r == 0) { // survivor
        connect(cref);
        break;
      }
      if (r == 2) {
        // The cascade is bounded by the clause's size, but at CLSLIM that is a
        // thousand full partner scans on one candidate, so it polls too. The
        // rewrite that produced `cref` is complete and logged; stopping here
        // only skips further shrinking of a clause that is already sound.
        if (poll())
          break;
        continue; // rescan the shrunken clause
      }
      if (r == 4)
        ended = true;
      break; // 1 (died) or 3 (became binary) or 4
    }
    if (red) { // the band's YIELD, read against `strengthen_red_checked`
      k.stats.strengthen_red_subsumed +=
          k.stats.strengthen_subsumed - red_sub_before;
      k.stats.strengthen_red_strengthened +=
          k.stats.strengthened - red_str_before;
    }
    if (ended || interrupted) {
      stopped_at = ci + 1; // this candidate was checked; the rest were not
      break;
    }
  }
  // Resume (see the twin comment in subsume_forward.cpp); `ended` leaves a tail
  // too, re-armed for the same reason a budget cut's is. A re-arm writes the
  // array the candidate's own creation would have written — `touched` for an
  // irredundant clause, `touched_kept` for a band one — because re-arming a
  // band candidate through `touched` would leak the band into
  // BCE/X2/X4/Factor's filters and break `Kernel::touched_kept`'s isolation.
  for (size_t ci = interrupted ? candidates.size()
                               : (filter ? stopped_at : candidates.size());
       ci < candidates.size(); ci++) {
    const Clause *c = clause_at(k.arena, candidates[ci]);
    const bool red = c->redundant();
    if (c->garbage() || !kernel_clause_new_material(k, c, wm, 2, red))
      continue;
    for (const Lit l : *c)
      red ? kernel_touch_kept_var(k, var_of(l)) : kernel_touch_var(k, var_of(l));
    k.stats.strengthen_rearmed++;
  }
  // An interrupted round does not advance the watermark at all: the re-arm loop
  // above is what makes advancing safe, and an abort inside the flush pre-pass
  // leaves `candidates` empty, so there is no tail to re-arm and advancing
  // would retire material this round never looked at.
  if (filter && !interrupted) {
    k.strengthen_touch_mark = snapshot;
    k.stats.strengthen_mark_advances++;
  }
  // Completeness, kissat's `completed` (forward.c:664-670): nothing was
  // re-armed. Unlike C1 this rule rewrites clauses, and a rewrite is an add the
  // substrate arms, so a productive round is incomplete whatever it reached.
  // `!ended`/`!interrupted` cover a flush pre-pass that stopped early, leaving
  // `candidates` empty — which would otherwise read as a vacuous fixpoint.
  k.strengthen_complete = !ended && !interrupted &&
                          stopped_at == candidates.size() &&
                          k.stats.strengthened == produced_before;
  if (!changed)
    return ApplyResult::NoOp;
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// Single-candidate instance surface (payload = arena Ref, like C1); applies at
// most one subsume-or-strengthen step, no cascade.
ApplyResult strengthen_try(Kernel &k, Ref candidate) {
  assert(strengthen_guard(k));
  // `work_budget` is a round device and does not reach this path, so
  // termination is checked here at entry, before the per-clause flush, where
  // nothing is part-rewritten.
  if (kernel_rule_interrupt(k, k.stats.strengthen_interrupted))
    return ApplyResult::NoOp;
  Clause *c = clause_at(k.arena, candidate);
  if (c->garbage())
    return ApplyResult::NoOp;
  { // per-clause root flush first (mirrors the round's pre-pass)
    uint64_t flushed = 0;
    Ref ref = candidate;
    const int r = flush_root(k, &ref, &flushed);
    if (r == 5)
      return ApplyResult::Unsat;
    if (r != 0) {
      kernel_collect_garbage(k);
      return ApplyResult::Ok;
    }
  }
  // The victim may be redundant on this path (N1: the instance surface is
  // complete over everything the round could accept, and more); the partner may
  // not — find_partner's `d->redundant()` skip is the load-bearing check.
  if (!is_victim(k, c))
    return ApplyResult::NoOp;
  k.stats.strengthen_checked++;
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  mark_candidate(k, marks, c, 1);
  const Partner p = find_partner(k, marks, c, nullptr, candidate);
  mark_candidate(k, marks, clause_at(k.arena, candidate), 0);
  uint64_t changed = 0;
  Ref ref = candidate;
  const int r = act(k, &ref, p, &changed);
  if (r == 5)
    return ApplyResult::Unsat;
  if (!changed)
    return ApplyResult::NoOp;
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// Complete over victims, band or no band, so `instances()` never advertises
// less than `apply_instance` accepts.
static uint32_t strengthen_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Ref> candidates;
  strengthen_candidates(k, candidates, /*complete=*/true);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult strengthen_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return strengthen_apply(k);
  for (Ref ref = 0; ref < k.arena.size();) {
    if (ref == inst)
      return strengthen_try(k, ref);
    if (ref > inst)
      break;
    ref += clause_words(clause_at(k.arena, ref)->size);
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Strengthen, RuleTier::Inprocess, strengthen_guard,
                     strengthen_apply, strengthen_instances,
                     strengthen_apply_instance);

} // namespace tactix

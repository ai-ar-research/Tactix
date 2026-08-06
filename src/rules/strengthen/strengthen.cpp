// See strengthen.hpp for the reference/deviation record.
#include "rules/strengthen/strengthen.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool strengthen_guard(const Kernel &k) {
  // quiescent root (C1's argument, strengthen.hpp)
  // D-SPLIT-3 (guard purification): the ¬conflict_valid conjunct is gone —
  // this is a pure F-rewrite (sound regardless of a recorded conflict), and
  // the conjunct was redundant anyway (conflict_valid ⟹ level>0 at any rule
  // boundary, so it never fired under the retained level==0). calculus §7.
  return !k.inconsistent && k.level == 0;
}

// candidate criteria shared with C1 (duplicated deliberately — modules
// stay self-contained per §5.3; the ~20 lines are the coupling price)
static bool is_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->redundant() || c->size > STRENGTHEN_CLSLIM)
    return false;
  for (const Lit l : *c)
    if (k.value(l) > 0)
      return false; // root-satisfied
  return true;
}

void strengthen_candidates(const Kernel &k, std::vector<Ref> &out) {
  out.clear();
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

// outcome of checking one partner clause against the marked candidate
struct CheckResult {
  enum { NONE, SUBSUMED, STRENGTHEN } kind = NONE;
  Lit flipped = INVALID_LIT; // partner literal in opposite phase; the
                             // candidate loses negate(flipped)
};

// CaDiCaL subsume.cpp subsume_check (MIT, transliterated): every partner
// literal must be marked in the same phase, root-falsified (absorbed by
// units), or — at most once — marked in the OPPOSITE phase (strengthen)
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

// partner found by a scan: NONE / SUBSUMED / STRENGTHEN with the flipped
// literal plus what the LRAT chain needs (partner id + its absorbed units)
struct Partner {
  CheckResult res;
  uint64_t id = 0;
  std::vector<uint64_t> absorbed; // unit ids of root-falsified partner lits
};

// scan the candidate's watch lists (both phases) for binary partners and
// the round's occurrence lists (both phases) for large partners.
// occs == nullptr selects the watch-list-only discovery used by the
// single-candidate instance surface (which then also checks large watched
// clauses). Free literals only; both scans are read-only.
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
      const WatchHead &h = k.watches.heads[probe];
      if (h.size <= STRENGTHEN_OCCLIM) {
        const uint32_t *ws = k.watches.arena.data() + h.offset;
        for (uint32_t i = 0; i < h.size;) {
          const Watch w{ws[i]};
          if (!w.binary()) {
            i += 2;
            continue;
          }
          i += 1;
          k.stats.strengthen_visits++; // SCHED-B4: a binary partner compared
          const Lit o = w.other();
          if (phase == 0) {
            // no redundancy check on the binary partner: the binary-subsumer
            // argument in subsume_forward.hpp (deviation 3 above) covers this
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
        k.stats.strengthen_visits++; // SCHED-B4
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
  // instance surface: large partners live in the watch lists of the
  // candidate's own free literals (any partner watches two of its
  // literals; each is a candidate literal or its negation — for the
  // flipped one the partner appears in watches[negate(cand lit)],
  // covered by the phase loop above rerun for large entries)
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; phase++) {
      const Lit probe = phase ? negate(l) : l;
      const WatchHead &h = k.watches.heads[probe];
      if (h.size > STRENGTHEN_OCCLIM)
        continue;
      const uint32_t *ws = k.watches.arena.data() + h.offset;
      for (uint32_t i = 0; i < h.size;) {
        const Watch w{ws[i]};
        if (w.binary()) {
          i += 1;
          continue;
        }
        const Ref dref = ws[i + 1];
        i += 2;
        if (dref == self)
          continue;
        k.stats.strengthen_visits++; // SCHED-B4
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
    if (k.proof)
      k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
    kernel_mark_garbage(k, c);
    k.stats.strengthen_subsumed++;
    (*changed)++;
    return 1;
  }
  // STRENGTHEN: rebuild from free literals minus the resolved one
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
  // rewrites preserve redundancy (the round's candidates are irredundant,
  // but the flush pre-pass rewrites redundant clauses too); glue can only
  // shrink with the clause — read BEFORE the replacement garbages `c`
  const bool redundant = c->redundant();
  const uint32_t glue =
      std::min<uint32_t>(c->glue(), static_cast<uint32_t>(lits.size()));
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

// canonical instance: one full round (fused subsume-or-strengthen;
// CaDiCaL subsume_round shape — see strengthen.hpp deviations)
// root flushing (kissat collect.c's sparse-collection effect, recomposed
// rule-side so proof deltas stay rule territory; the recorded V1/V2
// prerequisite — probe/backbone root units get consumed here): a
// root-SATISFIED clause is deleted; root-FALSIFIED literals are stripped
// by a degenerate self-strengthen (act()'s STRENGTHEN path with the
// absorbing unit as the partner: add-RUP of the free literals, chain
// [units..., the clause], delete the original). Runs as a pre-pass over
// ALL live large clauses — any redundancy, any size (act preserves
// redundancy and shrinks glue). Returns like act(), plus 1 for a
// satisfied deletion.
static int flush_root(Kernel &k, Ref *ref, uint64_t *changed) {
  Clause *c = clause_at(k.arena, *ref);
  uint32_t falsified = 0;
  for (const Lit l : *c) {
    if (k.value(l) > 0 && k.var_level(var_of(l)) == 0) {
      if (k.proof) // root-satisfied: delete
        k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
      kernel_mark_garbage(k, c);
      k.stats.flushed++;
      (*changed)++;
      return 1;
    }
    if (k.value(l) < 0)
      falsified++;
  }
  if (!falsified)
    return 0;
  Partner p;
  p.res.kind = CheckResult::STRENGTHEN;
  for (const Lit l : *c)
    if (k.value(l) < 0) {
      p.res.flipped = negate(l); // act removes negate(flipped) == l
      break;
    }
  if (k.proof && k.proof->lrat())
    p.id = k.unit_ids[var_of(p.res.flipped)]; // the absorbing unit
  k.stats.flushed++;
  const int r = act(k, ref, p, changed);
  k.stats.strengthened--; // attributed to `flushed`, not `strengthened`
  return r;
}

ApplyResult strengthen_apply(Kernel &k) {
  assert(strengthen_guard(k));
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.strengthen_rounds++;
  // SCHED-F1 · repetition filter, C1's arrangement exactly (see the N1 note in
  // subsume_forward.cpp; the predicate is kernel_clause_new_material). The
  // snapshot is taken before the FLUSH pre-pass on purpose: the flush rewrites
  // every clause a root fix shortened, and each rewrite is an add, so those
  // clauses land above the snapshot and this round's own candidate filter
  // already sees them as new.
  kernel_touch_sync_fixed(k);
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.strengthen_touch_mark;
  uint64_t changed = 0;
  bool ended = false;
  // flush pre-pass (see flush_root): the whole arena, before the
  // candidate round, so the round never sees clauses with root-assigned
  // literals. A flush-derived unit ends the pass (values changed); the
  // policy re-propagates. One collect at the end covers both phases.
  for (Ref ref = 0; ref < k.arena.size();) {
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
  if (!ended)
    strengthen_candidates(k, candidates);
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<std::vector<Ref>> occs(2 * k.nvars);
  // `k.work_budget` in this rule's own currency, the Vivify/Factor idiom (see
  // the D-STRENGTHEN-BUDGET ledger row). An ENTRY budget alone bounds how OFTEN
  // a round runs and never how much one round does, which lets a single
  // uninterruptible round own most of a solve; cadical bounds the same family
  // PER round (`subsumeeffort` with `subsumemaxeff`/`subsumemineff`,
  // options.hpp:218-221). Stopping early is sound and loses no work: the
  // survivors stay candidates and the next round re-derives them. 0 =
  // unbounded. Polled in PARTNER VISITS (Stats::strengthen_visits).
  const uint64_t check_start = k.stats.strengthen_visits;
  // connect a survivor at its least-occurring free literal. Shared by the
  // checked path and by the SCHED-F1 skip path: a skipped candidate is still
  // indexed, so the round's partner index stays complete and the filter costs
  // no strengthening the unfiltered round would have found.
  const auto connect = [&](Ref cr) {
    const Clause *c = clause_at(k.arena, cr);
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
    // under-derived roots (corpus 002) can leave a candidate without
    // free literals; nothing to connect then
    if (min_lit != INVALID_LIT && min_occs <= STRENGTHEN_OCCLIM)
      occs[min_lit].push_back(cr);
  };
  size_t stopped_at = candidates.size(); // index of the first UNREACHED one
  const uint64_t produced_before = k.stats.strengthened;
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    Ref cref = candidates[ci];
    if (k.midround_budget && k.work_budget &&
        k.stats.strengthen_visits - check_start > k.work_budget) {
      k.stats.budget_stops_c2++;
      stopped_at = ci;
      break;
    }
    if (filter && !kernel_clause_new_material(k, clause_at(k.arena, cref), wm)) {
      k.stats.strengthen_mark_skipped++;
      connect(cref);
      continue;
    }
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
      if (r == 2)
        continue; // rescan the shrunken clause
      if (r == 4)
        ended = true;
      break; // 1 (died) or 3 (became binary) or 4
    }
    if (ended) {
      stopped_at = ci + 1; // this candidate was checked; the rest were not
      break;
    }
  }
  // SCHED-F2 · RESUME (see the twin comment in subsume_forward.cpp for the
  // argument). `ended` — a derived unit stopped the round — leaves a tail too,
  // and it is re-armed for exactly the same reason a budget cut's is.
  for (size_t ci = stopped_at; ci < candidates.size(); ci++) {
    const Clause *c = clause_at(k.arena, candidates[ci]);
    if (c->garbage() || (filter && !kernel_clause_new_material(k, c, wm)))
      continue;
    for (const Lit l : *c)
      kernel_touch_var(k, var_of(l));
    k.stats.strengthen_rearmed++;
  }
  if (filter) {
    k.strengthen_touch_mark = snapshot;
    k.stats.strengthen_mark_advances++;
  }
  // Completeness, kissat's `completed` (forward.c:664-670): nothing was
  // re-armed. Unlike C1 this rule REWRITES clauses, and a strengthened clause
  // is exactly what kissat re-arms by hand — here the rewrite is an add, so the
  // substrate arms it — so a productive round is INCOMPLETE, whatever it
  // reached.
  // `!ended` also covers the FLUSH pre-pass deriving a unit, which leaves
  // `candidates` empty and would otherwise read as a vacuous fixpoint.
  k.strengthen_complete = !ended && stopped_at == candidates.size() &&
                          k.stats.strengthened == produced_before;
  if (!changed)
    return ApplyResult::NoOp;
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// single-candidate instance surface (payload = arena Ref, like C1);
// applies at most one subsume-or-strengthen step (no cascade)
ApplyResult strengthen_try(Kernel &k, Ref candidate) {
  assert(strengthen_guard(k));
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
  if (!is_candidate(k, c))
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

static uint32_t strengthen_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Ref> candidates;
  strengthen_candidates(k, candidates);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult strengthen_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
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

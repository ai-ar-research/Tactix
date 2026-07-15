// See strengthen.hpp for the reference/deviation record.
#include "rules/strengthen/strengthen.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool strengthen_guard(const Kernel &k) {
  // quiescent root (C1's argument, strengthen.hpp)
  return !k.inconsistent && !k.conflict_valid && !k.pending.valid &&
         k.level == 0 && k.propagate_head == k.trail.size();
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
          const Lit o = w.other();
          if (phase == 0) {
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

// act on a found partner. Returns: 0 = candidate unchanged (NONE),
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
    c->flags |= Clause::GARBAGE;
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
    k.proof->del(id, c->id(), c->lits, c->size);
  }
  c->flags |= Clause::GARBAGE; // before any allocation moves the arena
  k.stats.strengthened++;
  (*changed)++;
  if (lits.empty()) {
    // possible only in an under-derived root state (see corpus regression
    // 002: engine quiescence != semantic quiescence under N1 histories) —
    // the empty clause just emitted IS the refutation
    k.inconsistent = true;
    return 5;
  }
  if (lits.size() == 1) { // unit: assert at root, end the round
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
    return 4;
  }
  if (lits.size() == 2) {
    kernel_add_binary_clause(k, lits[0], lits[1], id);
    return 3;
  }
  *ref = kernel_add_large_clause(k, lits.data(),
                                 static_cast<uint32_t>(lits.size()),
                                 /*redundant=*/false, /*glue=*/0, id);
  return 2;
}

// canonical instance: one full round (fused subsume-or-strengthen;
// CaDiCaL subsume_round shape — see strengthen.hpp deviations)
ApplyResult strengthen_apply(Kernel &k) {
  assert(strengthen_guard(k));
  k.stats.strengthen_rounds++;
  std::vector<Ref> candidates;
  strengthen_candidates(k, candidates);
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<std::vector<Ref>> occs(2 * k.nvars);
  uint64_t changed = 0;
  bool ended = false;
  for (Ref cref : candidates) {
    for (;;) { // strengthening cascades on the same candidate
      Clause *c = clause_at(k.arena, cref);
      k.stats.strengthen_checked++;
      mark_candidate(k, marks, c, 1);
      const Partner p = find_partner(k, marks, c, &occs, cref);
      mark_candidate(k, marks, clause_at(k.arena, cref), 0);
      const int r = act(k, &cref, p, &changed);
      if (r == 5)
        return ApplyResult::Unsat;
      if (r == 0) { // survivor: connect at least-occurring free literal
        c = clause_at(k.arena, cref);
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
          occs[min_lit].push_back(cref);
        break;
      }
      if (r == 2)
        continue; // rescan the shrunken clause
      if (r == 4)
        ended = true;
      break; // 1 (died) or 3 (became binary) or 4
    }
    if (ended)
      break;
  }
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

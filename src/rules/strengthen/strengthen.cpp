// See strengthen.hpp for the rule's contract.
#include "rules/strengthen/strengthen.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility strengthen_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/calculus.md` §7).
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
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

static bool touched_on_two_variables(const Kernel &k, const Clause *c,
                                     uint64_t watermark) {
  uint32_t touched = 0;
  for (const Lit lit : *c)
    if ((kernel_touched_since(k, var_of(lit), watermark) ||
         (c->redundant() &&
          kernel_kept_since(k, var_of(lit), watermark))) &&
        ++touched == 2)
      return true;
  return false;
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
    ref += clause_actual_words(c);
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

// partner found by a scan: NONE / SUBSUMED / STRENGTHEN with the flipped
// literal plus what the LRAT chain needs (partner id + its absorbed units)
struct Partner {
  CheckResult res;
  uint64_t id = 0;
  std::vector<uint64_t> absorbed; // unit ids of root-falsified partner lits
};

// Kissat's forward-subsumption index is one mixed watch stream per literal.
// Dense-mode entry first flushes every large reference, leaving the binary
// prefix.  Survivors are then connected in candidate order and binaries made
// by an earlier strengthening are appended at that exact point.  Keeping this
// local image avoids hoisting such late binaries ahead of already-connected
// large partners, which changes the last winning strengthener.
struct ForwardWatch {
  uint32_t payload = INVALID_REF;
  bool binary = false;

  static ForwardWatch binary_entry(Lit other) {
    ForwardWatch result;
    result.payload = other;
    result.binary = true;
    return result;
  }

  static ForwardWatch large_entry(Ref ref) {
    ForwardWatch result;
    result.payload = ref;
    return result;
  }

  Lit other() const {
    assert(binary);
    return payload;
  }

  Ref ref() const {
    assert(!binary);
    return payload;
  }
};
static_assert(sizeof(ForwardWatch) == 8);

// Kissat's forward_literal accumulates `steps` in a local scalar and publishes
// it once after the watch scan (forward.c:197-303).  Keep the same hot-loop
// representation while preserving Tactix's stricter per-partner budget cut:
// every return, including UNSAT exits after a found partner, commits the exact
// scalar through this scope guard.
struct StrengthenVisitCounter {
  uint64_t &published;
  uint64_t value;
  explicit StrengthenVisitCounter(uint64_t &counter)
      : published(counter), value(counter) {}
  ~StrengthenVisitCounter() { published = value; }
};

static Partner find_forward_partner(
    Kernel &k, const std::vector<uint8_t> &marks, const Clause *c,
    const std::vector<std::vector<ForwardWatch>> &watches, uint64_t &visits,
    uint64_t visit_start, bool bounded, uint64_t visit_budget,
    bool *budget_exhausted) {
  Partner p;
  const bool lrat = k.proof && k.proof->lrat();
  const auto budget_cut = [&]() {
    if (!budget_exhausted || !bounded ||
        visits - visit_start < visit_budget)
      return false;
    *budget_exhausted = true;
    return true;
  };
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; ++phase) {
      const Lit probe = phase ? negate(l) : l;
      const std::vector<ForwardWatch> &stream = watches[probe];
      if (stream.size() > STRENGTHEN_OCCLIM)
        continue;
      for (const ForwardWatch &watch : stream) {
        if (watch.binary) {
          if (phase)
            continue;
          if (budget_cut())
            return p;
          visits++;
          const Lit other = watch.other();
          if (marks[other]) {
            p.res.kind = CheckResult::SUBSUMED;
            if (lrat)
              p.id = kernel_binary_id(k, probe, other);
            return p;
          }
          if (marks[negate(other)]) {
            p.res.kind = CheckResult::STRENGTHEN;
            p.res.flipped = other;
            p.absorbed.clear();
            if (lrat)
              p.id = kernel_binary_id(k, probe, other);
            // A binary strengthener terminates this physical watch walk.
            // The opposite-phase walk for the same candidate literal still
            // follows, just as the second forward_literal call in Kissat.
            break;
          }
          continue;
        }
        if (budget_cut())
          return p;
        visits++;
        const Clause *d = clause_at(k.arena, watch.ref());
        if (d->garbage())
          continue;
        const CheckResult r = check_marked(k, marks, d);
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        p.absorbed.clear();
        if (lrat) {
          p.id = d->id();
          for (const Lit m : *d)
            if (k.value(m) < 0)
              p.absorbed.push_back(k.unit_ids[var_of(m)]);
        }
        if (r.kind == CheckResult::SUBSUMED)
          return p;
        // Large strengtheners do not stop the walk.  A later large or binary
        // entry is allowed to overwrite the literal to remove.
      }
    }
  }
  return p;
}

// Pinned Kissat forward.c begins every pass by collapsing duplicate binaries
// on variables whose persistent `subsume` flag is armed. Tactix's equivalent
// flag image is the Strengthen touch watermark. The same scan detects
// (l∨a),(l∨¬a), whose hyper-unary resolvent is l, before compacting later
// copies. Returns the number of retired copies, or UINT64_MAX on refutation.
static uint64_t sweep_forward_binaries(Kernel &k, uint64_t since,
                                       bool filtered) {
  struct UnitWitness {
    Lit unit;
    uint64_t first, second;
  };
  std::vector<UnitWitness> units;
  std::vector<std::pair<Lit, Lit>> duplicates;
  std::vector<uint8_t> marks(2 * static_cast<size_t>(k.nvars), 0);
  std::vector<uint64_t> ids(2 * static_cast<size_t>(k.nvars), 0);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    const Var v = var_of(lit);
    if (kernel_var_retired(k, v) || k.value(lit) != 0 ||
        (filtered && !kernel_touched_since(k, v, since)))
      continue;
    std::vector<Lit> touched;
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (!w.binary())
        continue;
      const Lit other = w.other();
      const Var ov = var_of(other);
      if (kernel_var_retired(k, ov) || k.value(other) != 0 ||
          (filtered && !kernel_touched_since(k, ov, since)))
        continue;
      if (marks[other]) {
        // The current list itself is the exact multiplicity authority.  Record
        // the pair while it is already in cache instead of rescanning this
        // (potentially very high-degree) list once for every binary below.
        // Only one mirror owns the candidate; sort/unique still protects the
        // result from triples and later additions in the same physical list.
        if (lit < other)
          duplicates.emplace_back(lit, other);
        continue; // a later physical duplicate; compacted below
      }
      if (marks[negate(other)])
        units.push_back(
            {lit, ids[negate(other)], kernel_binary_id(k, lit, other)});
      marks[other] = 1;
      ids[other] = kernel_binary_id(k, lit, other);
      touched.push_back(other);
    }
    for (const Lit other : touched) {
      marks[other] = 0;
      ids[other] = 0;
    }
  }
  uint64_t removed = 0;
  std::sort(duplicates.begin(), duplicates.end());
  duplicates.erase(std::unique(duplicates.begin(), duplicates.end()),
                   duplicates.end());
  for (const auto &[a, b] : duplicates)
    removed += kernel_remove_duplicate_binary_copies(k, a, b);

  for (const UnitWitness &w : units) {
    const int8_t value = k.value(w.unit);
    if (value > 0)
      continue;
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) {
      const uint64_t chain[2] = {w.first, w.second};
      k.proof->add_rup(k.proof->lrat() ? id : 0, &w.unit, 1,
                       k.proof->lrat() ? chain : nullptr,
                       k.proof->lrat() ? 2 : 0);
    }
    if (value < 0) {
      kernel_emit_empty_from_pair(k, id, k.unit_ids[var_of(w.unit)]);
      return UINT64_MAX;
    }
    k.unit_ids[var_of(w.unit)] = id;
    kernel_assign_unit(k, w.unit);
    removed++;
  }
  if (!units.empty() && inprocess_reach_root(k))
    return UINT64_MAX;
  return removed;
}

// scan the candidate's watch lists (both phases) for binary partners and the
// round's occurrence lists (both phases) for large ones. occs == nullptr
// selects the watch-list-only discovery the single-candidate instance surface
// uses. Free literals only; both scans are read-only.
static Partner find_partner(Kernel &k, const std::vector<uint8_t> &marks,
                            const Clause *c,
                            const std::vector<std::vector<Ref>> *occs,
                            Ref self, uint64_t &visits, uint64_t visit_start,
                            bool bounded, uint64_t visit_budget,
                            bool *budget_exhausted) {
  Partner p;
  const bool lrat = k.proof && k.proof->lrat();
  const auto budget_cut = [&]() {
    if (!budget_exhausted || !bounded ||
        visits - visit_start < visit_budget)
      return false;
    *budget_exhausted = true;
    return true;
  };
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; phase++) {
      const Lit probe = phase ? negate(l) : l;
      // Kissat scans binaries only in `forward_literal(lit, true)`.  Its
      // opposite-phase `forward_literal(NOT(lit), false)` walk considers large
      // occurrences only.  Opposite-phase binary discovery is logically
      // redundant (the same binary is reachable from its same-phase literal)
      // but changes which of several valid strengtheners wins first.
      if (phase == 0 &&
          watch_population_within(k, probe, true, STRENGTHEN_BINLIM)) {
        const bool stop_at_large = k.watches.heads[probe].binaries_first();
        bool strengthened_by_binary = false;
        for (const WatchEntry w : k.watches.entries(probe)) {
          if (!w.binary()) {
            if (stop_at_large)
              break;
            continue;
          }
          if (budget_cut())
            return p;
          visits++; // a binary partner compared
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
              p.absorbed.clear();
              if (lrat)
                p.id = kernel_binary_id(k, probe, o);
              // Kissat breaks this one `forward_literal` watch walk, but
              // carries `remove` into the remaining candidate literals where
              // a later strengthener may overwrite it.
              strengthened_by_binary = true;
              break;
            }
          }
        }
        if (strengthened_by_binary)
          continue;
      }
      if (!occs) // instance surface: large partners via watch lists below
        continue;
      const std::vector<Ref> &os = (*occs)[probe];
      if (os.size() > STRENGTHEN_OCCLIM)
        continue;
      for (const Ref dref : os) {
        if (budget_cut())
          return p;
        visits++;
        const Clause *d = clause_at(k.arena, dref);
        const CheckResult r = check_marked(k, marks, d);
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        p.absorbed.clear();
        if (lrat) {
          p.id = d->id();
          for (const Lit m : *d)
            if (k.value(m) < 0)
              p.absorbed.push_back(k.unit_ids[var_of(m)]);
        }
        if (r.kind == CheckResult::SUBSUMED)
          return p;
        // Large strengtheners do not break `forward_literal`; the last valid
        // one in the remaining physical occurrence scans wins.
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
        if (budget_cut())
          return p;
        visits++;
        const Clause *d = clause_at(k.arena, dref);
        if (d->garbage() || d->redundant() || d->size > c->size)
          continue;
        const CheckResult r = check_marked(k, marks, d);
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        p.absorbed.clear();
        if (lrat) {
          p.id = d->id();
          for (const Lit m : *d)
            if (k.value(m) < 0)
              p.absorbed.push_back(k.unit_ids[var_of(m)]);
        }
        if (r.kind == CheckResult::SUBSUMED)
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
static int act(Kernel &k, Ref *ref, const Partner &p, uint64_t *changed,
               std::pair<Lit, Lit> *new_binary = nullptr) {
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
  if (lits.size() >= 3) {
    // Kissat shrinks this record in place.  Preserve its arena/dense-occurrence
    // position instead of letting Tactix's packed append representation turn a
    // rewrite into new BVE material and a different later occurrence order.
    kernel_replace_large_clause_in_place(
        k, *ref, id, lits.data(), static_cast<uint32_t>(lits.size()),
        redundant ? glue : 0);
    return 2;
  }
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
    if (new_binary)
      *new_binary = {lits[0], lits[1]};
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
  const uint64_t binary_removed = sweep_forward_binaries(k, wm, filter);
  if (binary_removed == UINT64_MAX)
    return ApplyResult::Unsat;
  uint64_t changed = binary_removed;
  bool ended = false;
  // §6 termination polling at both unbounded loops — the flush pre-pass and
  // the candidate loop.  Like Kissat's `forward_subsume_all_clauses`, one
  // candidate gets at most one subsume-or-strengthen action in a round, so
  // there is no same-candidate cascade to poll.  Every site aborts between
  // candidates or clauses, never mid-rewrite, so an interrupted round is
  // exactly an incomplete round.
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
    const uint32_t words = clause_actual_words(clause_at(k.arena, ref));
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
  if (!ended && !interrupted) {
    strengthen_candidates(k, candidates);
    if (filter) {
      size_t kept = 0;
      for (const Ref ref : candidates) {
        const Clause *c = clause_at(k.arena, ref);
        if (touched_on_two_variables(k, c, wm)) {
          candidates[kept++] = ref;
          continue;
        }
        k.stats.strengthen_mark_skipped++;
        k.stats.strengthen_red_skipped += c->redundant();
      }
      candidates.resize(kept);
    }
  }
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<std::vector<ForwardWatch>> forward_watches(2 * k.nvars);
  // `kissat_enter_dense_mode` flushes every large search watch before the
  // first forward round, and each preceding elimination round flushes its
  // dense large occurrences before the next one.  Initialize the exact
  // surviving binary prefix; large partners are appended by `connect` below.
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary())
        forward_watches[lit].push_back(
            ForwardWatch::binary_entry(watch.other()));
  // `k.work_budget` in this rule's own currency (partner visits). An entry
  // budget alone bounds how often a round runs, never how much one round does,
  // which lets a single uninterruptible round own most of a solve; cadical
  // bounds the same family per round (`subsumeeffort`, options.hpp:218-221).
  // Stopping early loses no work — survivors stay candidates. 0 = unbounded.
  // Ledger row D-STRENGTHEN-BUDGET.
  StrengthenVisitCounter visits(k.stats.strengthen_visits);
  const uint64_t check_start = visits.value;
  const bool visit_bounded = k.midround_budget && k.work_budget;
  const uint64_t visit_budget = k.work_budget;
  // Connect a scheduled survivor at its least-occurring free literal in the
  // current mixed forward stream.  Under the canonical filter every active
  // variable must be armed, exactly as Kissat's `connect_subsuming` requires;
  // clauses that failed the two-variable scheduling predicate never enter the
  // stream.
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
    if (filter)
      for (const Lit l : *c) {
        if (k.value(l) != 0)
          continue;
        if (!kernel_touched_since(k, var_of(l), wm))
          return;
      }
    Lit min_lit = INVALID_LIT;
    size_t min_occs = 0;
    for (const Lit l : *c) {
      if (k.value(l) != 0)
        continue;
      const size_t n = forward_watches[l].size();
      // `connect_subsuming` updates the winner on equality, hence the last
      // minimum literal in physical clause order wins.
      if (min_lit == INVALID_LIT || n <= min_occs) {
        min_lit = l;
        min_occs = n;
      }
    }
    // under-derived roots (corpus 002) can leave a candidate with no free
    // literals; nothing to connect then
    if (min_lit != INVALID_LIT && min_occs <= STRENGTHEN_OCCLIM)
      forward_watches[min_lit].push_back(ForwardWatch::large_entry(cr));
  };
  size_t stopped_at = candidates.size(); // index of the first unreached one
  const uint64_t produced_before = k.stats.strengthened;
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    Ref cref = candidates[ci];
    if (visit_bounded &&
        visits.value - check_start >= visit_budget) {
      k.stats.budget_stops_c2++;
      stopped_at = ci;
      break;
    }
    if (poll()) {
      stopped_at = ci;
      break;
    }
    const bool red = clause_at(k.arena, cref)->redundant();
    k.stats.strengthen_red_checked += red;
    const uint64_t red_sub_before = k.stats.strengthen_subsumed;
    const uint64_t red_str_before = k.stats.strengthened;
    bool budget_exhausted = false;
    Clause *c = clause_at(k.arena, cref);
    k.stats.strengthen_checked++;
    mark_candidate(k, marks, c, 1);
    const Partner p = find_forward_partner(
        k, marks, c, forward_watches, visits.value, check_start,
        visit_bounded, visit_budget, &budget_exhausted);
    mark_candidate(k, marks, clause_at(k.arena, cref), 0);
    if (!budget_exhausted) {
      std::pair<Lit, Lit> new_binary{INVALID_LIT, INVALID_LIT};
      const int r = act(k, &cref, p, &changed, &new_binary);
      if (r == 5)
        return ApplyResult::Unsat;
      // Kissat connects both an unchanged survivor and the once-strengthened
      // candidate for use as a partner by later candidates.  It does not
      // rescan the strengthened candidate until a later forward round.
      if (r == 0 || r == 2)
        connect(cref);
      if (r == 3) {
        const Lit a = new_binary.first, b = new_binary.second;
        assert(a != INVALID_LIT && b != INVALID_LIT);
        forward_watches[a].push_back(ForwardWatch::binary_entry(b));
        forward_watches[b].push_back(ForwardWatch::binary_entry(a));
      }
      if (r == 4)
        ended = true;
    }
    if (red) { // the band's YIELD, read against `strengthen_red_checked`
      k.stats.strengthen_red_subsumed +=
          k.stats.strengthen_subsumed - red_sub_before;
      k.stats.strengthen_red_strengthened +=
          k.stats.strengthened - red_str_before;
    }
    if (budget_exhausted) {
      k.stats.budget_stops_c2++;
      stopped_at = ci;
      break;
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
    if (c->garbage())
      continue;
    const bool red = c->redundant();
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
  StrengthenVisitCounter visits(k.stats.strengthen_visits);
  const Partner p = find_partner(k, marks, c, nullptr, candidate, visits.value,
                                 visits.value, false, 0, nullptr);
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
    ref += clause_actual_words(clause_at(k.arena, ref));
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Strengthen, RuleTier::Inprocess, strengthen_guard,
                     strengthen_apply, strengthen_instances,
                     strengthen_apply_instance);

} // namespace tactix

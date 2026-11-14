// See strengthen.hpp for the rule's contract.
#include "rules/strengthen/strengthen.hpp"

#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/bve/bve.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

RuleEligibility strengthen_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/dev/calculus.md` §7).
  return root_level_guard(k);
}

// Victim criteria shared with C1 (rules/instance_helpers.hpp): live and not
// root-satisfied. Redundancy is not a criterion: deleting a redundant clause is
// Forget, and a strengthening add-RUP stands against the whole formula.
static bool is_victim(const Kernel &k, const Clause *c) {
  return root_clause_relevant(k, c);
}

// Canonical round scope: irredundant always; a redundant clause only inside the
// tier band with `Kernel::touch_redundant` on, read live (cadical
// subsume.cpp:388 `likely_to_be_kept_clause`). Ledger row D-C1C2-VICTIM.
static bool in_round_scope(const Kernel &k, const Clause *c) {
  if (c->size > k.strengthen_parameters.clause_size_limit ||
      !is_victim(k, c))
    return false;
  return !c->redundant() ||
         (k.touch_redundant &&
          kernel_likely_kept(k, true, c->glue(), c->size));
}

// The explicit CaDiCaL victim-band arm retains its shared-touch protocol.
// Only the canonical irredundant round owns native material flags.
static bool round_marked(const Kernel &k, Var v, uint64_t watermark) {
  return k.touch_redundant ? kernel_touched_since(k, v, watermark)
                           : kernel_has_subsume_material(k, v);
}

static bool material_on_two_variables(const Kernel &k, const Clause *c,
                                     uint64_t watermark) {
  uint32_t touched = 0;
  for (const Lit lit : *c)
    if ((round_marked(k, var_of(lit), watermark) ||
         (c->redundant() && kernel_kept_since(k, var_of(lit), watermark))) &&
        ++touched == 2)
      return true;
  return false;
}

void strengthen_candidates(const Kernel &k, std::vector<Ref> &out,
                           bool complete) {
  out.clear();
  struct Cand {
    Ref ref;
    uint64_t position;
    uint32_t size;
    bool redundant;
  };
  std::vector<Cand> cands;
  for (const auto [ref, c] : arena_records(k.arena)) {
    if (complete ? is_victim(k, c) : in_round_scope(k, c))
      cands.push_back(
          {ref, kernel_clause_position(k, ref), c->size, c->redundant()});
  }
  // Irredundant first, then size ascending, so the large small-clause band
  // cannot starve irredundant work.
  std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
    if (a.redundant != b.redundant)
      return !a.redundant;
    if (a.size != b.size)
      return a.size < b.size;
    // Kissat's stable size radix keeps arena order; break equal sizes by
    // logical position.
    if (a.position != b.position)
      return a.position < b.position;
    return a.ref < b.ref; // defensive total order for aliased positions
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

// cadical subsume.cpp subsume_check: each partner literal is marked in the same
// phase, root-falsified, or at most once opposite (strengthen).
static CheckResult check_marked(const Kernel &k,
                                const std::vector<uint8_t> &marks,
                                const Clause *d, bool *satisfied = nullptr) {
  CheckResult r;
  for (const Lit l : *d) {
    if (marks[l])
      continue;
    const int8_t v = k.value(l);
    if (v > 0) {
      if (satisfied)
        *satisfied = true;
      return {}; // satisfied partner proves nothing
    }
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
// literal
struct Partner {
  CheckResult res;
};

// Kissat's forward index is one mixed watch stream per literal: the binary
// prefix after dense entry, then survivors in candidate order, with binaries
// from earlier strengthening appended at that point.
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

// Kissat's forward cost: one per candidate, plus per non-empty non-saturated
// stream one plus its 4-byte cache lines (ceil(size / 32)), plus one per large
// watch visited; published on every exit by PublishedCounter.
static uint64_t forward_cache_lines(size_t watches) {
  return (watches + 31u) / 32u;
}

// Write the root unit immediately with UNIT_REASON so it survives deletion of a
// connected reason.
static void forward_assign_root(Kernel &k, Lit unit, uint32_t reason,
                                bool binary) {
  kernel_assign(k, unit, 0, reason, binary);
  if (k.proof) {
    k.proof->add_rup(0, &unit, 1);
    kernel_note_dsr_root_unit(k, unit);
  }
  Assigned &assigned = k.assigned[var_of(unit)];
  assigned.reason = UNIT_REASON;
  assigned.binary = false;
  assigned.jumped = false;
}

static void forward_refute(Kernel &k) {
  kernel_emit_empty(k);
  k.conflict_valid = false;
}

// Shared by the private partial forward image and the caller's complete image.
template <class Streams>
static bool forward_propagate_units(Kernel &k, const Streams &watches,
                                    uint64_t &changed) {
  while (k.propagate_head < k.trail.size()) {
    const Lit false_lit = negate(k.trail[k.propagate_head++]);
    k.stats.propagations++;
    const auto &stream = watches[false_lit];
    uint64_t ticks = 1 + forward_cache_lines(stream.size());
    for (const auto watch : stream) {
      if (watch.binary) {
        const Lit other = watch.payload;
        if (k.value(other) > 0)
          continue;
        if (k.value(other) < 0) {
          forward_refute(k);
          return false; // native does not charge the conflicting scan
        }
        if (!kernel_var_retired(k, var_of(other)))
          forward_assign_root(k, other, false_lit, true);
        continue;
      }
      ++ticks;
      Clause *clause = clause_at(k.arena, watch.payload);
      if (clause->garbage())
        continue;
      Lit unit = INVALID_LIT;
      unsigned non_false = 0;
      bool satisfied = false;
      for (const Lit lit : *clause) {
        if (lit == false_lit || k.value(lit) < 0)
          continue;
        if (k.value(lit) > 0) {
          satisfied = true;
          kernel_delete_clause(k, clause);
          ++changed;
          break;
        }
        unit = lit;
        if (++non_false == 2)
          break;
      }
      if (satisfied)
        continue;
      if (!non_false) {
        forward_refute(k);
        return false;
      }
      if (non_false == 1)
        forward_assign_root(k, unit, watch.payload, false);
    }
    const bool saved_inprocessing = k.inprocessing;
    k.inprocessing = true; // dense ticks never increase search_ticks
    kernel_charge_ticks(k, ticks);
    k.inprocessing = saved_inprocessing;
  }
  return true;
}

// Compact the private image only after closing its units; Eliminate's caller
// reconnects before replaying the forward suffix.
static bool forward_flush_units(
    Kernel &k, std::vector<std::vector<ForwardWatch>> &watches,
    uint64_t &changed) {
  const uint32_t from = k.propagate_head;
  if (from == k.trail.size())
    return true;
  if (k.large_watches_connected)
    k.binary_only_head = std::min(k.binary_only_head, from);
  if (!forward_propagate_units(k, watches, changed))
    return false;
  for (uint32_t i = from; i < k.propagate_head; ++i) {
    auto &stream = watches[k.trail[i]];
    size_t kept = 0;
    for (const ForwardWatch watch : stream) {
      if (watch.binary) {
        stream[kept++] = watch;
        // Normally no BVE heap exists during the forward pass. Preserve the
        // shared removal notification if a public caller has installed one.
        if (k.value(watch.other()) == 0)
          kernel_notify_bve_score(k, var_of(watch.other()), true);
        continue;
      }
      Clause *clause = clause_at(k.arena, watch.ref());
      if (!clause->garbage()) {
        kernel_delete_clause(k, clause);
        ++changed;
      }
    }
    stream.resize(kept);
  }
  return true;
}

ApplyResult strengthen_reconnect_and_flush(Kernel &k, uint32_t trail_from) {
  assert(k.level == 0 && k.dense && k.dense->track_order);
  assert(!k.large_watches_connected && trail_from <= k.trail.size());
  if (k.inconsistent)
    return ApplyResult::Unsat;
  // eliminate.c rebuilds full connections after every forward call.
  kernel_rebuild_dense_occurrences(k);
  k.propagate_head = trail_from;
  const auto propagate = [&] {
    uint64_t changed = 0;
    if (!forward_propagate_units(k, k.dense->ordered, changed))
      return ApplyResult::Unsat;
    bve_flush_units_while_connected(k, trail_from);
    return ApplyResult::Ok;
  };
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  // Rearm before journal eligibility is evaluated: the partial forward scan
  // may have consumed the old cursor without covering this complete image.
  return journal_direct_rule(k, "Propagate", "post-forward-connected",
                             propagate, ApplyResult::Ok);
#else
  return propagate();
#endif
}

static Partner find_forward_partner(
    Kernel &k, const std::vector<uint8_t> &marks, const Clause *c,
    std::vector<std::vector<ForwardWatch>> &watches, uint64_t &steps,
    uint64_t &changed) {
  Partner p;
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; ++phase) {
      const Lit probe = phase ? negate(l) : l;
      std::vector<ForwardWatch> &stream = watches[probe];
      if (stream.size() > k.strengthen_parameters.occurrence_limit)
        continue;
      if (stream.empty())
        continue;
      steps += 1 + forward_cache_lines(stream.size());
      size_t read = 0, write = 0;
      bool stop_stream = false;
      while (read < stream.size()) {
        const ForwardWatch watch = stream[read++];
        stream[write++] = watch;
        if (watch.binary) {
          if (phase)
            continue;
          const Lit other = watch.other();
          if (marks[other]) {
            p.res.kind = CheckResult::SUBSUMED;
            stop_stream = true;
            break;
          }
          if (marks[negate(other)]) {
            p.res.kind = CheckResult::STRENGTHEN;
            p.res.flipped = other;
            // A binary strengthener ends this walk; the opposite-phase walk
            // still follows (Kissat's second forward_literal).
            stop_stream = true;
            break;
          }
          continue;
        }
        steps++;
        const Clause *d = clause_at(k.arena, watch.ref());
        if (d->garbage()) {
          write--;
          continue;
        }
        bool satisfied = false;
        const CheckResult r = check_marked(k, marks, d, &satisfied);
        if (satisfied) {
          kernel_delete_clause(k, clause_at(k.arena, watch.ref()));
          ++changed;
          --write;
          break; // native cleans this entry and retains the unscanned tail
        }
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        if (r.kind == CheckResult::SUBSUMED) {
          stop_stream = true;
          break;
        }
        // Large strengtheners do not stop the walk.  A later large or binary
        // entry is allowed to overwrite the literal to remove.
      }
      // `forward_literal` removes garbage met before its result and keeps the
      // tail.
      while (read < stream.size())
        stream[write++] = stream[read++];
      stream.resize(write);
      if (stop_stream && p.res.kind == CheckResult::SUBSUMED)
        return p;
    }
  }
  return p;
}

// Kissat forward.c first collapses duplicate binaries on armed `subsume`
// variables, also deriving l from (l∨a),(l∨¬a). Returns the number of retired
// copies, or UINT64_MAX on refutation.
static uint64_t sweep_forward_binaries(Kernel &k, uint64_t watermark,
                                       bool filtered) {
  std::vector<Lit> units;
  std::vector<std::pair<Lit, Lit>> duplicates;
  std::vector<uint8_t> marks(2 * static_cast<size_t>(k.nvars), 0);
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit) {
    const Var v = var_of(lit);
    if (kernel_var_retired(k, v) || k.value(lit) != 0 ||
        (filtered && !round_marked(k, v, watermark)))
      continue;
    std::vector<Lit> touched;
    for (const WatchEntry w : k.watches.entries(lit)) {
      if (!w.binary())
        continue;
      const Lit other = w.other();
      const Var ov = var_of(other);
      if (kernel_var_retired(k, ov) || k.value(other) != 0 ||
          (filtered && !round_marked(k, ov, watermark)))
        continue;
      if (marks[other]) {
        // Record multiplicity from the current list; one mirror owns the
        // candidate.
        if (lit < other)
          duplicates.emplace_back(lit, other);
        continue; // a later physical duplicate; compacted below
      }
      if (marks[negate(other)])
        units.push_back(lit);
      marks[other] = 1;
      touched.push_back(other);
    }
    for (const Lit other : touched)
      marks[other] = 0;
  }
  uint64_t removed = 0;
  std::sort(duplicates.begin(), duplicates.end());
  duplicates.erase(std::unique(duplicates.begin(), duplicates.end()),
                   duplicates.end());
  for (const auto &[a, b] : duplicates)
    removed += kernel_remove_duplicate_binary_copies(k, a, b);

  for (const Lit unit : units) {
    const int8_t value = k.value(unit);
    if (value > 0)
      continue;
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, &unit, 1);
    if (value < 0) {
      kernel_emit_empty(k);
      return UINT64_MAX;
    }
    k.unit_ids[var_of(unit)] = id;
    kernel_assign_unit(k, unit);
    removed++;
  }
  if (!units.empty() && inprocess_reach_root(k))
    return UINT64_MAX;
  return removed;
}

// Single-candidate partner discovery: same-phase binary partners, then large
// partners in the free literals' watch lists. Read-only.
static Partner find_partner(Kernel &k, const std::vector<uint8_t> &marks,
                            const Clause *c, Ref self, uint64_t &visits) {
  Partner p;
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    // Kissat scans binaries only in `forward_literal(lit, true)`; the opposite
    // phase visits large occurrences only, which fixes the winning
    // strengthener.
    const bool stop_at_large = k.watches.heads[l].binaries_first();
    for (const WatchEntry w : k.watches.entries(l)) {
      if (!w.binary()) {
        if (stop_at_large)
          break;
        continue;
      }
      visits++; // a binary partner compared
      const Lit o = w.other();
      // no redundancy check on the binary partner: the binary-subsumer
      // argument in subsume_forward.hpp covers this
      if (marks[o]) { // (l ∨ o), both in candidate: subsumes
        p.res.kind = CheckResult::SUBSUMED;
        return p;
      }
      if (marks[negate(o)]) { // (l ∨ o), candidate has ¬o: drop ¬o
        p.res.kind = CheckResult::STRENGTHEN;
        p.res.flipped = o;
        // Kissat breaks this walk but carries `remove` into later candidate
        // literals.
        break;
      }
    }
  }
  // Large partners watch two of their own literals, so rerun the phase loop
  // over the candidate's free literals.
  for (const Lit l : *c) {
    if (k.value(l) != 0)
      continue;
    for (int phase = 0; phase < 2; phase++) {
      const Lit probe = phase ? negate(l) : l;
      for (const WatchEntry w : k.watches.entries(probe)) {
        if (w.binary())
          continue;
        const Ref dref = w.ref();
        if (dref == self)
          continue;
        visits++;
        const Clause *d = clause_at(k.arena, dref);
        if (d->garbage() || d->redundant() || d->size > c->size)
          continue;
        const CheckResult r = check_marked(k, marks, d);
        if (r.kind == CheckResult::NONE)
          continue;
        p.res = r;
        if (r.kind == CheckResult::SUBSUMED)
          return p;
        // Large strengtheners do not break `forward_literal`; the last valid
        // one in the remaining physical occurrence scans wins.
      }
    }
  }
  return p;
}

struct SemanticStrengthening {
  Ref candidate;
  Lit gone;
};

// Enumerate distinct C2 outcomes, not partners. A redundant large partner is
// legal for STRENGTHEN (the added RUP clause stays) but not for SUBSUMED/C1.
static void semantic_strengthenings(Kernel &k,
                                    std::vector<SemanticStrengthening> &out) {
  out.clear();
  std::vector<Ref> candidates;
  strengthen_candidates(k, candidates, /*complete=*/true);
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  std::vector<uint8_t> possible(2 * k.nvars, 0);
  for (const Ref candidate : candidates) {
    const Clause *c = clause_at(k.arena, candidate);
    mark_candidate(k, marks, c, 1);
    bool subsumed = false;
    for (const Lit l : *c) {
      if (k.value(l) != 0)
        continue;
      for (const WatchEntry watch : k.watches.entries(l)) {
        if (!watch.binary())
          continue;
        const Lit other = watch.other();
        if (marks[other])
          subsumed = true;
        else if (marks[negate(other)])
          possible[negate(other)] = 1;
      }
    }
    for (const auto [partner, d] : arena_records(k.arena)) {
      if (partner != candidate && !d->garbage()) {
        const CheckResult result = check_marked(k, marks, d);
        if (result.kind == CheckResult::SUBSUMED && !d->redundant())
          subsumed = true;
        else if (result.kind == CheckResult::STRENGTHEN)
          possible[negate(result.flipped)] = 1;
      }
    }
    if (subsumed)
      out.push_back({candidate, INVALID_LIT});
    for (const Lit literal : *c)
      if (k.value(literal) == 0 && possible[literal]) {
        out.push_back({candidate, literal});
        possible[literal] = 0;
      }
    mark_candidate(k, marks, c, 0);
  }
}

static bool find_semantic_strengthener(Kernel &k,
                                       const std::vector<uint8_t> &marks,
                                       const Clause *candidate, Ref self,
                                       Lit gone, Partner &out) {
  for (const Lit l : *candidate) {
    if (k.value(l) != 0)
      continue;
    for (const WatchEntry watch : k.watches.entries(l)) {
      if (!watch.binary())
        continue;
      const Lit other = watch.other();
      if (gone == INVALID_LIT) {
        if (!marks[other])
          continue;
        out.res.kind = CheckResult::SUBSUMED;
        out.res.flipped = INVALID_LIT;
      } else {
        if (negate(other) != gone || !marks[gone])
          continue;
        out.res.kind = CheckResult::STRENGTHEN;
        out.res.flipped = other;
      }
      return true;
    }
  }
  for (const auto [partner, d] : arena_records(k.arena)) {
    if (partner != self && !d->garbage()) {
      const CheckResult result = check_marked(k, marks, d);
      const bool selected =
          gone == INVALID_LIT
              ? result.kind == CheckResult::SUBSUMED && !d->redundant()
              : result.kind == CheckResult::STRENGTHEN &&
                    negate(result.flipped) == gone;
      if (selected) {
        out.res = result;
        return true;
      }
    }
  }
  return false;
}

// act() on a found partner: 0 = unchanged, 1 = subsumed, 2 = strengthened into
// *ref (rescan), 3 = strengthened to binary, 4 = unit asserted (caller
// propagates), 5 = empty clause (UNSAT).
static int act(Kernel &k, Ref *ref, const Partner &p, uint64_t *changed,
               std::pair<Lit, Lit> *new_binary = nullptr) {
  if (p.res.kind == CheckResult::NONE)
    return 0;
  Clause *c = clause_at(k.arena, *ref);
  if (p.res.kind == CheckResult::SUBSUMED) {
    kernel_delete_clause(k, c);
    k.stats.strengthen_subsumed++;
    (*changed)++;
    return 1;
  }
  // Strengthen: rebuild from free literals minus the resolved one.
  const Lit gone = negate(p.res.flipped);
  std::vector<Lit> lits;
  lits.reserve(c->size);
  for (const Lit l : *c) {
    if (l == gone)
      continue;
    if (k.value(l) < 0) // stripped; its unit absorbs it for the checker
      continue;
    lits.push_back(l);
  }
  const uint64_t id = ++k.next_clause_id;
  emit_rup_lemma(k, id, lits);
  k.stats.strengthened++;
  (*changed)++;
  // Rewrites preserve redundancy and glue (kissat forward.c shrinks in place).
  const bool redundant = c->redundant();
  const uint32_t glue = c->glue();
  if (lits.size() >= 3) {
    // Shrink in place as Kissat, keeping arena/dense position.
    kernel_replace_large_clause_in_place(
        k, *ref, id, lits.data(), static_cast<uint32_t>(lits.size()),
        redundant ? glue : 0, "strengthen");
    return 2;
  }
  switch (kernel_replace_clause(k, c, id, lits.data(),
                                static_cast<uint32_t>(lits.size()), redundant,
                                redundant ? glue : 0, ref)) {
  case ReplaceOutcome::Refuted:
    return 5; // corpus 002: the empty clause just emitted IS the refutation
  case ReplaceOutcome::Unit: // caller owns the appropriate propagation image
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

ApplyResult strengthen_apply(Kernel &k) {
  assert(strengthen_guard(k));
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.strengthen_rounds++;
  // Native forward admission consumes addition marks, not root-fixed
  // notifications (Kissat's mark_fixed_literal does not set flags.subsume).
  const bool filter = k.touch_filter;
  const uint64_t snapshot = k.touch_epoch;
  const uint64_t wm = k.strengthen_touch_mark;
  const uint64_t binary_removed = sweep_forward_binaries(k, wm, filter);
  if (binary_removed == UINT64_MAX)
    return ApplyResult::Unsat;
  uint64_t changed = binary_removed;
  // §6 termination polling between candidates only; as in Kissat's
  // `forward_subsume_all_clauses`, a candidate gets at most one action per
  // round, so an interrupted round is an incomplete round. No tick cadence.
  bool interrupted = false;
  const auto poll = [&]() {
    if (!kernel_rule_interrupt(k, k.stats.strengthen_interrupted))
      return false;
    interrupted = true;
    return true;
  };
  // forward.c runs no sparse root collection here: root-satisfied clauses
  // retire during discovery, root-false literals remain unless strengthened.
  interrupted = poll();
  if (!interrupted)
    for (const auto [ref, c] : arena_records(k.arena)) {
      if (!c->garbage() && !c->redundant() &&
          c->size <= k.strengthen_parameters.clause_size_limit) {
        bool satisfied = false;
        for (const Lit lit : *c)
          satisfied |= k.value(lit) > 0;
        if (satisfied) {
          kernel_delete_clause(k, c);
          k.stats.flushed++;
          changed++;
        }
      }
    }
  std::vector<Ref> candidates;
  if (!interrupted) {
    strengthen_candidates(k, candidates);
    if (filter) {
      size_t kept = 0;
      for (const Ref ref : candidates) {
        const Clause *c = clause_at(k.arena, ref);
        if (material_on_two_variables(k, c, wm)) {
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
  // `kissat_enter_dense_mode` flushes large watches; start from the surviving
  // binary prefix and append large partners in `connect`.
  for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary())
        forward_watches[lit].push_back(
            ForwardWatch::binary_entry(watch.other()));
  // Kissat's `SET_EFFORT_LIMIT(forward_steps)` is tested between candidates and
  // stops only when spent > budget; survivors stay candidates. Direct calls are
  // unbounded unless given a budget.
  PublishedCounter steps(k.stats.strengthen_visits);
  const uint64_t check_start = steps.value;
  const bool step_bounded = k.midround_budget;
  const uint64_t step_budget = k.work_budget;
  // Connect a survivor at its least-occurring free literal (Kissat
  // `connect_subsuming`); every active variable must be armed under the
  // canonical filter. Redundant survivors are not indexed: a partner must not
  // be forgettable, and band clauses are checked, never chased.
  const auto connect = [&](Ref cr) {
    const Clause *c = clause_at(k.arena, cr);
    if (c->redundant())
      return;
    if (filter)
      for (const Lit l : *c) {
        if (k.value(l) != 0)
          continue;
        if (!round_marked(k, var_of(l), wm))
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
    if (min_lit != INVALID_LIT &&
        min_occs <= k.strengthen_parameters.occurrence_limit)
      forward_watches[min_lit].push_back(ForwardWatch::large_entry(cr));
  };
  size_t stopped_at = candidates.size(); // index of the first unreached one
  std::vector<Ref> rewritten_survivors;
  std::vector<std::pair<Lit, Lit>> new_binaries;
  for (size_t ci = 0; ci < candidates.size(); ci++) {
    Ref cref = candidates[ci];
    if (step_bounded && steps.value - check_start > step_budget) {
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
    Clause *c = clause_at(k.arena, cref);
    if (c->garbage())
      continue;
    // Re-evaluate roots before marking; the candidate step is charged only when
    // two free literals remain.
    unsigned non_false = 0;
    Lit remaining = INVALID_LIT;
    bool satisfied = false;
    for (const Lit lit : *c) {
      if (k.value(lit) > 0) {
        satisfied = true;
        break;
      }
      if (k.value(lit) == 0) {
        ++non_false;
        remaining = lit;
        marks[lit] = 1;
      }
    }
    k.stats.strengthen_checked++;
    if (satisfied) {
      mark_candidate(k, marks, c, 0);
      kernel_delete_clause(k, c);
      ++k.stats.flushed;
      ++changed;
      continue;
    }
    if (!non_false) {
      forward_refute(k);
      return ApplyResult::Unsat;
    }
    if (non_false == 1) {
      marks[remaining] = 0;
      forward_assign_root(k, remaining, cref, false);
      kernel_delete_clause(k, c);
      ++changed;
      if (!forward_flush_units(k, forward_watches, changed))
        return ApplyResult::Unsat;
      continue;
    }
    steps.value++; // Kissat `INC(forward_steps)` in forward_marked_clause.
    const Partner p =
        find_forward_partner(k, marks, c, forward_watches, steps.value, changed);
    mark_candidate(k, marks, clause_at(k.arena, cref), 0);
    std::pair<Lit, Lit> new_binary{INVALID_LIT, INVALID_LIT};
    const int r = act(k, &cref, p, &changed, &new_binary);
    if (r == 5)
      return ApplyResult::Unsat;
    // Kissat connects unchanged and once-strengthened candidates as partners
    // but does not rescan the latter this round.
    if (r == 0 || r == 2)
      connect(cref);
    if (r == 2)
      rewritten_survivors.push_back(cref);
    if (r == 3) {
      new_binaries.push_back(new_binary);
      const Lit a = new_binary.first, b = new_binary.second;
      assert(a != INVALID_LIT && b != INVALID_LIT);
      forward_watches[a].push_back(ForwardWatch::binary_entry(b));
      forward_watches[b].push_back(ForwardWatch::binary_entry(a));
    }
    if (r == 4 && !forward_flush_units(k, forward_watches, changed))
      return ApplyResult::Unsat;
    if (red) { // the band's YIELD, read against `strengthen_red_checked`
      k.stats.strengthen_red_subsumed +=
          k.stats.strengthen_subsumed - red_sub_before;
      k.stats.strengthen_red_strengthened +=
          k.stats.strengthened - red_str_before;
    }
    if (interrupted) {
      stopped_at = ci + 1; // this candidate was checked; the rest were not
      break;
    }
  }
  // Resume (see subsume_forward.cpp): re-arm `touched` for irredundant and
  // `touched_kept` for band clauses, keeping the band out of other filters.
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
  // An interrupted round does not advance the watermark.
  if (filter && !interrupted) {
    k.strengthen_touch_mark = snapshot;
    k.stats.strengthen_mark_advances++;
  }
  // Retire the native forward flag image only at a normal boundary.
  if (!interrupted && !k.touch_redundant) {
    kernel_clear_subsume_material(k);
    const auto rearm = [&](Ref ref) {
      const Clause *c = clause_at(k.arena, ref);
      if (c->garbage() || c->redundant())
        return;
      for (const Lit lit : *c)
        kernel_rearm_subsume_material(k, var_of(lit));
    };
    for (size_t ci = stopped_at; ci < candidates.size(); ++ci)
      rearm(candidates[ci]);
    for (const Ref ref : rewritten_survivors)
      rearm(ref);
    for (const auto &[a, b] : new_binaries) {
      kernel_rearm_subsume_material(k, var_of(a));
      kernel_rearm_subsume_material(k, var_of(b));
    }
  }
  // A unit-only rewrite does not re-arm forward subsumption; live large
  // rewrites, new binaries, unreached candidates or interruption do.
  const bool live_rewrite = std::any_of(
      rewritten_survivors.begin(), rewritten_survivors.end(),
      [&](Ref ref) { return !clause_at(k.arena, ref)->garbage(); });
  k.strengthen_complete = !interrupted && stopped_at == candidates.size() &&
                          new_binaries.empty() && !live_rewrite;
  if (!changed)
    return ApplyResult::NoOp;
  // Collection prevents stale source allocations from changing later sparse
  // watch order.
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

// Single-candidate instance surface (payload = arena Ref, like C1); applies at
// most one subsume-or-strengthen step, no cascade.
ApplyResult strengthen_try(Kernel &k, Ref candidate) {
  assert(strengthen_guard(k));
  // Check termination at entry, before the per-clause flush.
  if (kernel_rule_interrupt(k, k.stats.strengthen_interrupted))
    return ApplyResult::NoOp;
  Clause *c = clause_at(k.arena, candidate);
  if (c->garbage())
    return ApplyResult::NoOp;
  // The victim may be redundant here (N1); the partner may not (find_partner's
  // `d->redundant()` skip).
  if (!is_victim(k, c))
    return ApplyResult::NoOp;
  k.stats.strengthen_checked++;
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  mark_candidate(k, marks, c, 1);
  uint64_t visits = k.stats.strengthen_visits;
  const Partner p = find_partner(k, marks, c, candidate, visits);
  k.stats.strengthen_visits = visits;
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
  return copy_instance_prefix(candidates, out, cap);
}

// Semantic batches keep arena offsets stable; collection is a separate
// boundary. Legacy calls collect immediately.
static ApplyResult strengthen_apply_semantic_instance(Kernel &k, uint64_t inst) {
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return strengthen_apply(k);
  Ref selected_ref;
  Lit selected_gone;
  clause_literal_of_instance(inst, &selected_ref, &selected_gone);
  if (selected_gone != INVALID_LIT && selected_gone >= 2 * k.nvars)
    return ApplyResult::NoOp;
  const Ref candidate_ref = arena_clause_instance(k, selected_ref);
  if (candidate_ref == INVALID_REF)
    return ApplyResult::NoOp;
  Clause *candidate = clause_at(k.arena, candidate_ref);
  if (!is_victim(k, candidate) ||
      (selected_gone != INVALID_LIT &&
       (k.value(selected_gone) != 0 ||
        !clause_contains(candidate, selected_gone))))
    return ApplyResult::NoOp;
  std::vector<uint8_t> marks(2 * k.nvars, 0);
  mark_candidate(k, marks, candidate, 1);
  Partner partner;
  const bool found = find_semantic_strengthener(
      k, marks, candidate, candidate_ref, selected_gone, partner);
  mark_candidate(k, marks, candidate, 0);
  if (!found)
    return ApplyResult::NoOp;
  uint64_t changed = 0;
  Ref ref = candidate_ref;
  const int result = act(k, &ref, partner, &changed);
  if (result == 5)
    return ApplyResult::Unsat;
  return changed ? ApplyResult::Ok : ApplyResult::NoOp;
}

static uint32_t strengthen_semantic_instances(Kernel &k, uint64_t *out,
                                              uint32_t cap) {
  std::vector<SemanticStrengthening> choices;
  semantic_strengthenings(k, choices);
  return copy_instance_prefix(choices, out, cap,
                              [](const SemanticStrengthening &choice) {
                                return clause_literal_instance(
                                    choice.candidate, choice.gone);
                              });
}

TACTIX_REGISTER_RULE(Strengthen, RuleTier::Inprocess, strengthen_guard,
                     strengthen_apply, strengthen_instances,
                     (apply_root_arena_instance<strengthen_apply,
                                                strengthen_try>),
                     strengthen_semantic_instances,
                     strengthen_apply_semantic_instance);

} // namespace tactix

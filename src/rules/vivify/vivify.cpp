// See vivify.hpp for the reference/deviation record.
#include "rules/vivify/vivify.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool vivify_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct (D-SPLIT-3): this is a pure
  // F-rewrite, sound regardless of a recorded conflict.
  return !k.inconsistent && k.level == 0;
}

// A retained probing trail makes k.value() speculative, so every candidacy and
// probe-list decision reads the root value (kissat `kissat_fixed`). The
// root-satisfied deletion path uses the same test, and a clause satisfied only
// under an assumption must never be deleted (design §3.1).
static inline int8_t root_value(const Kernel &k, Lit l) {
  const int8_t v = k.value(l);
  return (v != 0 && k.var_level(var_of(l)) == 0) ? v : 0;
}

// Does `c` fall in the band `k.vivify_scope` selects? The redundant side is
// split into kissat's three tiers (vivify.c:236-256) so a policy can spend
// three separate shares. Design §2.5.
static bool in_scope(const Kernel &k, const Clause *c) {
  const bool red = c->redundant();
  const uint32_t glue = c->glue();
  switch (k.vivify_scope) {
  case 1:
    return red && glue <= k.tier2; // the whole redundant band (legacy)
  case 2:
    return !red;                        // irredundant
  case 3:
    return red && glue <= k.tier1;      // tier1
  case 4:
    return red && glue > k.tier1 && glue <= k.tier2; // tier2
  case 5:
    return red && glue > k.tier2;       // tier3 (C10 hand-off H3)
  default:
    // Scope 0 is the unscoped N1 path, so it means every band, tier3
    // included: bounding it by tier2 would remove a capability rather than
    // decline by default (tests/test_vivify_n1.cpp).
    return true;
  }
}

// candidate: live, 3 <= size <= lim, not root-satisfied; in the scope's band
static bool is_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->size > VIVIFY_CLSLIM || c->size < 3)
    return false;
  // The live tier, not the static default (kissat `vivify_tier2_limit`,
  // vivify.c:132, whose `vivifyfocusedtiers` ships on). The cut binds only for
  // the scopes that bound the redundant side by tier2 (1/3/4); on 0 and 5 it
  // would contradict `in_scope` and re-close the tier3 band.
  if (c->redundant() && c->glue() > k.tier2 && k.vivify_scope != 0 &&
      k.vivify_scope != 5)
    return false;
  for (const Lit l : *c)
    if (root_value(k, l) > 0)
      return false;
  return true;
}

// --- the round's global literal order ---------------------------------------
// What makes cross-candidate reuse possible is the order, not the reuse loop
// (cadical vivify.cpp:35-42; kissat vivify_sort_clause_by_counts). Deliberate
// deviation: kissat permutes the literals in the arena and must bracket the
// round with flush_large_watches/watch_large_clauses, whereas the order here
// lives in a round-scoped side vector, so no arena is touched.
struct VivifyOrder {
  std::vector<uint32_t> counts; // per-literal occurrences; persistent scratch
  std::vector<Lit> touched;     // ...restored in O(touched), never O(nvars)
  bool operator()(Lit a, Lit b) const { // a ≺ b: more occurrences first
    if (counts[a] != counts[b])
      return counts[a] > counts[b];
    return a < b; // total order for determinism
  }
};

static VivifyOrder &vivify_order() {
  static thread_local VivifyOrder ord;
  return ord;
}

static void order_clear(VivifyOrder &ord) {
  for (const Lit l : ord.touched)
    ord.counts[l] = 0;
  ord.touched.clear();
}

static void order_sort(const VivifyOrder &ord, std::vector<Lit> &out) {
  std::sort(out.begin(), out.end(), [&ord](Lit a, Lit b) { return ord(a, b); });
}

// The non-root-fixed literals of `c`, in probe order — clause order unless
// `ordered`, which is off for the no-reuse arm and for the standalone path,
// where `counts` was never filled and no next candidate shares a prefix.
static void probe_list(const Kernel &k, const Clause *c,
                       const VivifyOrder &ord, std::vector<Lit> &out,
                       bool ordered) {
  out.clear();
  for (const Lit l : *c)
    if (root_value(k, l) == 0)
      out.push_back(l);
  if (ordered)
    order_sort(ord, out);
}

// The scheduling side-effects a ROUND needs and a const query must not have.
struct VivifySchedule {
  uint64_t prioritized = 0; // candidates carrying the leftover bit
  uint64_t flushed = 0;     // prefix-subsumed before any propagation
  bool rearmed = false;     // ...the set was empty, so the scope was re-armed
};

// `commit` writes the re-arm back into the clause headers; the const query
// applies the same rule virtually, reporting the order a round would use.
static void vivify_schedule(const Kernel &k, std::vector<Ref> &out,
                            VivifySchedule *commit) {
  out.clear();
  VivifyOrder &ord = vivify_order();
  if (ord.counts.size() < 2 * k.nvars)
    ord.counts.resize(2 * k.nvars, 0);
  order_clear(ord);
  // kissat counts over the whole live database, not just the scope: the
  // count_clause call at vivify.c:276 precedes the tier filter.
  if (k.vivify_reuse)
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage())
        for (const Lit l : *c) {
          if (ord.counts[l]++ == 0)
            ord.touched.push_back(l);
        }
      ref += clause_words(c->size);
    }

  struct Cand {
    Ref ref;
    uint32_t prioritized, redundant, band, size;
    uint32_t key_off, key_len; // probe list, in the global order
  };
  std::vector<Cand> cands;
  std::vector<Lit> keys, probes;
  uint64_t prioritized = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (is_candidate(k, c) && in_scope(k, c)) {
      probe_list(k, c, ord, probes, k.vivify_reuse);
      uint32_t band = 0;
      if (c->redundant()) {
        if (c->glue() <= k.tier1)
          band = 1;
        else if (c->glue() <= k.tier2)
          band = 2;
        else
          band = 3;
      }
      cands.push_back({ref, c->vivify() ? 1u : 0u,
                       c->redundant() ? 1u : 0u, band, c->size,
                       static_cast<uint32_t>(keys.size()),
                       static_cast<uint32_t>(probes.size())});
      prioritized += c->vivify();
      keys.insert(keys.end(), probes.begin(), probes.end());
    }
    ref += clause_words(c->size);
  }
  // kissat vivify.c:296-301: an empty leftover set re-arms the whole scope.
  // The bit is a priority mechanism and never a suppressor.
  const bool rearm = prioritized == 0 && !cands.empty();
  if (rearm)
    for (Cand &c : cands)
      c.prioritized = 1;
  if (commit) {
    commit->rearmed = rearm;
    commit->prioritized = rearm ? cands.size() : prioritized;
    if (rearm) {
      Arena &arena = const_cast<Arena &>(k.arena);
      for (const Cand &c : cands)
        clause_at(arena, c.ref)->set_vivify(true);
    }
  }

  const Lit *kb = keys.data();
  const auto key_less = [&](const Cand &a, const Cand &b) {
    if (a.band != b.band)
      return a.band < b.band;
    const uint32_t n = std::min(a.key_len, b.key_len);
    for (uint32_t i = 0; i < n; i++) {
      const Lit x = kb[a.key_off + i], y = kb[b.key_off + i];
      if (x != y)
        return ord(x, y);
    }
    return a.key_len < b.key_len;
  };
  const auto key_prefix = [&](const Cand &a, const Cand &b) {
    if (a.band != b.band || a.key_len > b.key_len)
      return false;
    for (uint32_t i = 0; i < a.key_len; i++)
      if (kb[a.key_off + i] != kb[b.key_off + i])
        return false;
    return true;
  };

  if (commit && k.vivify_flush && cands.size() > 1) {
    std::stable_sort(cands.begin(), cands.end(), key_less);
    Arena &arena = const_cast<Arena &>(k.arena);
    Kernel &mut = const_cast<Kernel &>(k);
    uint32_t write = 0;
    const Cand *prev[4] = {nullptr, nullptr, nullptr, nullptr};
    for (const Cand &cand : cands) {
      const uint32_t part = cand.band;
      if (prev[part] && key_prefix(*prev[part], cand)) {
        kernel_delete_clause(mut, clause_at(arena, cand.ref));
        commit->flushed++;
      } else {
        cands[write++] = cand;
        prev[part] = &cands[write - 1];
      }
    }
    cands.resize(write);
  }

  std::sort(cands.begin(), cands.end(),
            [&](const Cand &a, const Cand &b) {
              if (a.prioritized != b.prioritized)
                return a.prioritized > b.prioritized; // leftovers first
              if (!k.vivify_reuse) {
                if (a.redundant != b.redundant)
                  return a.redundant < b.redundant; // irredundant first
                if (a.size != b.size)
                  return a.size < b.size;
                return a.ref < b.ref;
              }
              // lexicographic under the global literal order, so candidates
              // sharing a leading literal become adjacent
              const uint32_t n = std::min(a.key_len, b.key_len);
              for (uint32_t i = 0; i < n; i++) {
                const Lit x = kb[a.key_off + i], y = kb[b.key_off + i];
                if (x != y)
                  return ord(x, y);
              }
              if (a.key_len != b.key_len)
                return a.key_len < b.key_len; // shorter prefix first
              return a.ref < b.ref;           // total order for determinism
            });
  out.reserve(cands.size());
  for (const Cand &c : cands)
    out.push_back(c.ref);
}

void vivify_candidates(const Kernel &k, std::vector<Ref> &out) {
  vivify_schedule(k, out, nullptr);
}

// decision-free probe assumption / abandonment (Lucky's primitives)
static void assume(Kernel &k, Lit lit) {
  k.level++;
  k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
  kernel_assign(k, lit, k.level, DECISION_REASON, false);
}

// Discard the whole probing trail and close the scope. Every operation that
// can invalidate a retained trail is preceded by this, so a trail is only ever
// carried across candidates that changed nothing — invariant VP1 (design §3).
static void reset_to_root(Kernel &k) {
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  // Hand search back the watch order and `searched` hints these propagations
  // churned. Must run before rewrite() touches the arena, the rollback log
  // holding Refs; every rewrite site below is reached through here.
  kernel_probing_end(k);
}

// last == 0 means the sorted entries already end in the conflicting hint
// (the implied literal's own reason); otherwise `last` closes the chain
static void emit_chain(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                       std::vector<std::pair<uint32_t, uint64_t>> &entries,
                       uint64_t last) {
  std::sort(entries.begin(), entries.end());
  std::vector<uint64_t> chain;
  chain.reserve(entries.size() + 1);
  for (const auto &e : entries)
    chain.push_back(e.second);
  if (last)
    chain.push_back(last);
  k.proof->add_rup(id, lits.data(), static_cast<uint32_t>(lits.size()),
                   chain.data(), static_cast<uint32_t>(chain.size()));
}

// Replace `c` (at cref) by `lits`, proof add before delete; `entries`+`last`
// form the LRAT chain. The replacement keeps the candidate's redundancy and
// takes the shared post-shrink glue (kernel_shrunken_glue). No
// partner-redundancy question arises: the strengthener is the propagation
// itself, so the rewrite is implied by F ∪ R and subsumes the original.
static ApplyResult rewrite(Kernel &k, Ref cref, const std::vector<Lit> &lits,
                           std::vector<std::pair<uint32_t, uint64_t>> &entries,
                           uint64_t last, bool *ended_round) {
  Clause *c = clause_at(k.arena, cref);
  const bool lrat = k.proof && k.proof->lrat();
  const bool redundant = c->redundant();
  const uint32_t glue = kernel_shrunken_glue(
      k, c->glue(), static_cast<uint32_t>(lits.size()));
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat)
      emit_chain(k, id, lits, entries, last);
    else
      k.proof->add_rup(0, lits.data(), static_cast<uint32_t>(lits.size()),
                       nullptr, 0);
  }
  k.stats.vivified++;
  // shared replacement tail; glue is a redundant-clause concept, 0 otherwise
  switch (kernel_replace_clause(k, c, id, lits.data(),
                                static_cast<uint32_t>(lits.size()), redundant,
                                redundant ? glue : 0, nullptr)) {
  case ReplaceOutcome::Refuted:
    return ApplyResult::Unsat;
  case ReplaceOutcome::Unit:
    // a unit rewrite asserts at the root and ends the round: values changed,
    // so every candidacy decision taken since the round began is stale
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
    *ended_round = true;
    return ApplyResult::Ok;
  default:
    return ApplyResult::Ok;
  }
}

// Probe one candidate; on an actionable outcome, rewrite. `reuse` means the
// round hands over a live probing trail and owns the scope; otherwise this
// path opens and closes its own scope around a root-to-root probe. In both
// modes reset_to_root() runs before any mutation (design §3).
static ApplyResult vivify_one(Kernel &k, Ref cref, bool *ended_round,
                              bool reuse) {
  Clause *c = clause_at(k.arena, cref);
  if (!is_candidate(k, c)) {
    // re-check root satisfaction for deletion (kissat/CaDiCaL delete a
    // satisfied candidate here). Root, not current: with a retained trail a
    // speculatively satisfied clause is not deletable.
    if (!c->garbage() && c->size >= 3) {
      bool satisfied = false;
      for (const Lit l : *c)
        satisfied |= root_value(k, l) > 0;
      if (satisfied && in_scope(k, c)) {
        reset_to_root(k); // a deletion is a database change: §2.4 step 5
        c = clause_at(k.arena, cref);
        kernel_delete_clause(k, c);
        k.stats.vivify_deleted++;
        return ApplyResult::Ok;
      }
    }
    return ApplyResult::NoOp;
  }
  k.stats.vivify_checked++;
  if (c->redundant() && c->glue() > k.tier2)
    k.stats.vivify_tier3++; // 0 here means the tier3 band is inert
  const bool lrat = k.proof && k.proof->lrat();
  // probe literals in the round's global literal order (design §2.2) — the
  // order is what makes a prefix shared with the next candidate, so it is not
  // separable from the reuse loop below
  std::vector<Lit> probes;
  std::vector<uint64_t> absorbed; // unit ids of root-falsified lits (LRAT)
  probes.reserve(c->size);
  probe_list(k, c, vivify_order(), probes, reuse);
  if (lrat)
    for (const Lit l : *c)
      if (root_value(k, l) < 0)
        absorbed.push_back(k.unit_ids[var_of(l)]);
  if (probes.size() <= 2)
    return ApplyResult::NoOp; // effectively binary: C2 territory
  const uint64_t cid = c->id();
  if (reuse && k.level > 0) {
    // (a) The candidate may be the reason of a retained trail literal, the
    // prefix having been propagated ignoring the previous candidate (kissat
    // vivify.c:995-1005). Not only a quality question: in the implied-negation
    // case the candidate's id is the closing hint, so a reason walk reaching
    // it too would emit that id twice in one chain (design §3.2).
    for (const Lit l : *c) {
      const Var v = var_of(l);
      if (k.value(l) > 0 && k.var_level(v) > 0) {
        const Assigned &a = k.assigned[v];
        if (!a.binary && a.reason == cref) {
          kernel_backtrack(k, a.level - 1, /*update_phases=*/false);
          break;
        }
      }
    }
    // (b) Trim assumptions the previous (longer) candidate made that are not
    // literals of this one: VP3 makes them inert, and trimming keeps a value
    // from being read under an assumption outside the candidate (§3.4).
    if (k.level > probes.size())
      kernel_backtrack(k, static_cast<uint32_t>(probes.size()),
                       /*update_phases=*/false);
  }
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  // Persistent walk scratch: a per-candidate assign() would be an O(nvars)
  // zero-fill per vivified candidate. Restored all-zero in O(marked) at the
  // single exit below, and LRAT-gated, so a proofs-off round pays nothing.
  static thread_local std::vector<uint8_t> seen;
  static thread_local std::vector<Var> seen_touched;
  if (lrat) {
    if (seen.size() < k.nvars)
      seen.resize(k.nvars, 0);
    seen_touched.clear();
  }
  // Marks on the probe literals (kissat vivify_mark_sorted_literals), so a
  // propagation can be scanned for an implied clause literal at once.
  // Mandatory under a count-based probe order, which can put the implied
  // literal anywhere the lazy `v > 0` test below would miss it.
  static thread_local std::vector<uint8_t> marked;
  if (marked.size() < 2 * k.nvars)
    marked.resize(2 * k.nvars, 0);
  for (const Lit l : probes)
    marked[l] = 1;

  std::vector<Lit> next;
  ApplyResult res = ApplyResult::NoOp;
  // Watch-list and clause-header churn from the assumption propagations below
  // is speculative and is handed back to search when the scope closes. In
  // reuse mode the round owns the scope, so it is not reopened per candidate.
  if (!reuse)
    kernel_probing_begin(k);
  uint32_t reused = 0, made = 0;
  for (uint32_t i = 0; i < probes.size(); i++) {
    const Lit li = probes[i];
    const uint32_t lvl = i + 1; // the decision level this probe occupies
    if (lvl <= k.level) {
      // Reuse test on the frame decision, not on the value: a literal false
      // for some other reason is not a reused assumption, and a value may
      // only be read once exactly this candidate's assumptions hold (VP3).
      if (k.trail[k.frames[lvl]] == negate(li)) {
        reused++;
        continue;
      }
      kernel_backtrack(k, lvl - 1, /*update_phases=*/false);
    }
    assert(k.level == lvl - 1); // VP3: only this candidate's assumptions hold
    const int8_t v = k.value(li);
    if (v > 0) {
      // implied: (probes[0..i-1], li) subsumes the candidate, and strictly,
      // since i + 1 == probes.size() would need li to have been assigned by
      // assumption, which v > 0 with li unassumed excludes
      if (i + 1 >= probes.size())
        break; // no strict gain; defensive
      next.assign(probes.begin(), probes.begin() + i);
      next.push_back(li);
      // chain: li's derivation alone — its own reason (the max-trail entry) is
      // the conflicting hint under the ¬li assumption, and the candidate is
      // not cited, a hint after the conflict being illegal
      if (lrat)
        kernel_reason_walk(k, var_of(li), seen, entries, &seen_touched);
      reset_to_root(k);
      res = rewrite(k, cref, next, entries, 0, ended_round);
      break;
    }
    if (v < 0) {
      // implied negation: li is redundant in the candidate
      next.assign(probes.begin(), probes.end());
      next.erase(next.begin() + i);
      if (lrat) {
        kernel_reason_walk(k, var_of(li), seen, entries, &seen_touched);
        for (const uint64_t u : absorbed)
          entries.emplace_back(0, u);
      }
      reset_to_root(k);
      res = rewrite(k, cref, next, entries, cid, ended_round);
      break;
    }
    const uint32_t before = static_cast<uint32_t>(k.trail.size());
    assume(k, negate(li));
    made++;
    if (kernel_propagate_probing(k, cref)) {
      // probe conflict: (probes[0..i]) is RUP — strict gain iff it drops
      // literals (later probes or root-falsified originals)
      const bool gain = i + 1 < probes.size() || probes.size() < c->size;
      if (!gain) {
        // A conflict invalidates only the level that produced it, so drop that
        // level and keep the prefix below it for the next candidate (kissat
        // vivify.c:1223-1226). The conflict handoff fields are cleared with
        // it: a recorded conflict may never outlive its candidate.
        k.conflict_valid = false;
        k.conflict_ref = INVALID_REF;
        kernel_backtrack(k, lvl - 1, /*update_phases=*/false);
        break;
      }
      next.assign(probes.begin(), probes.begin() + i + 1);
      uint64_t last = 0;
      if (lrat)
        last = kernel_conflict_reason_walk(k, seen, entries, &seen_touched);
      reset_to_root(k);
      res = rewrite(k, cref, next, entries, last, ended_round);
      break;
    }
    // Eager implied literal (kissat vivify.c:1078-1090): scan what this
    // assumption propagated for another literal of the candidate, giving
    // (probes[0..i], implied) — stronger than waiting for position j > i.
    // kissat's `better_implied` refinement (vivify.c:1101-1117) is not
    // ported; the first in trail order is taken.
    Lit implied = INVALID_LIT;
    // From before + 1: trail[before] is the assumption itself, a marked
    // literal's negation and never an implied one.
    for (uint32_t t = before + 1; t < k.trail.size(); t++)
      if (marked[k.trail[t]]) {
        implied = k.trail[t];
        break;
      }
    if (implied != INVALID_LIT && i + 2 < c->size) {
      // i + 2 >= c->size is "no strict gain" and must continue, not break: the
      // assumption is already made and a later position can still find a
      // conflict, so breaking would lose coverage the lazy scan had.
      next.assign(probes.begin(), probes.begin() + i + 1);
      next.push_back(implied);
      // same chain shape as the lazy case above: `implied`'s own derivation,
      // whose max-trail entry is the conflicting hint under the negation of
      // the emitted clause; the candidate is not cited
      if (lrat)
        kernel_reason_walk(k, var_of(implied), seen, entries, &seen_touched);
      reset_to_root(k);
      res = rewrite(k, cref, next, entries, 0, ended_round);
      break;
    }
  }
  for (const Lit l : probes) // restore the persistent mark scratch all-zero
    marked[l] = 0;
  k.stats.vivify_reused += reused;
  k.stats.vivify_probes += made;
  if (reused && reused == probes.size())
    k.stats.vivify_reuse_full++; // wholly covered: nothing new was assumed
  if (!reuse || k.conflict_valid)
    reset_to_root(k); // standalone mode returns to the root every candidate;
                      // a recorded conflict must never outlive the candidate
  for (const Var tv : seen_touched) // restore the persistent scratch all-zero
    seen[tv] = 0;
  return res;
}

// canonical instance: one full round over the tier-aware schedule
ApplyResult vivify_apply(Kernel &k) {
  assert(vivify_guard(k));
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.vivify_rounds++;
  // The round owns the probing scope from here and must not inherit one: a
  // scope left recording by an earlier rule would make the `!recording` test
  // below skip opening this round's. Idempotent outside a scope.
  kernel_probing_end(k);
  std::vector<Ref> candidates;
  VivifySchedule sched;
  vivify_schedule(k, candidates, &sched);
  k.stats.vivify_prioritized += sched.prioritized;
  k.stats.vivify_reprioritized += sched.rearmed;
  k.stats.vivify_flushed += sched.flushed;
  uint64_t changed = sched.flushed;
  bool ended = false;
  const bool reuse = k.vivify_reuse;
  // k.work_budget in Vivify's currency (propagation ticks); 0 = unbounded
  const uint64_t tick_start = k.stats.ticks;
  for (const Ref cref : candidates) {
    if (k.work_budget && k.stats.ticks - tick_start > k.work_budget)
      break;
    // Termination poll on every candidate (kissat vivify.c:1291), with no
    // `!(++tick & 255)` window: a window an instance never reaches makes the
    // abort path vacuous. The two counters are separate on purpose.
    k.stats.vivify_polls++;
    if (kernel_terminating(k)) {
      k.stats.vivify_terminated++;
      break;
    }
    // Reopen the scope only when the previous candidate closed it by mutating
    // the database. Test `recording`, not `level == 0`: a candidate can
    // decline at the root without closing the scope, and reopening a
    // recording scope rolls the round's watch state back.
    if (reuse && !k.probing.recording)
      kernel_probing_begin(k);
    const ApplyResult r = vivify_one(k, cref, &ended, reuse);
    // Cleared once a candidate has been tried, so a budget cut or termination
    // leaves exactly the untried tail armed for the next round (kissat
    // vivify.c:1311). A garbage clause still has its header until the collect.
    clause_at(k.arena, cref)->set_vivify(false);
    if (r == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    if (r == ApplyResult::Ok)
      changed++;
    if (ended)
      break;
  }
  reset_to_root(k); // the round's single exit: budget cut, termination, unit
                    // rewrite and completion all leave through here
  if (!changed)
    return ApplyResult::NoOp;
  kernel_collect_garbage(k);
  return ApplyResult::Ok;
}

ApplyResult vivify_try(Kernel &k, Ref candidate) {
  assert(vivify_guard(k));
  bool ended = false;
  // Standalone: its own root-to-root probing scope, no retained trail — the N1
  // surface must not depend on a round having run first.
  const ApplyResult r = vivify_one(k, candidate, &ended, /*reuse=*/false);
  if (r == ApplyResult::Ok)
    kernel_collect_garbage(k);
  return r;
}

static uint32_t vivify_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Ref> candidates;
  vivify_candidates(k, candidates);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(candidates.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = candidates[i];
  return n;
}

static ApplyResult vivify_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U (no-op at a
  // fixpoint); a root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return vivify_apply(k);
  for (Ref ref = 0; ref < k.arena.size();) {
    if (ref == inst)
      return vivify_try(k, ref);
    if (ref > inst)
      break;
    ref += clause_words(clause_at(k.arena, ref)->size);
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Vivify, RuleTier::Inprocess, vivify_guard,
                     vivify_apply, vivify_instances, vivify_apply_instance);

} // namespace tactix

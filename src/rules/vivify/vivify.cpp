// See vivify.hpp for the reference/deviation record.
#include "rules/vivify/vivify.hpp"

#include "rules/propagate/propagate.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool vivify_guard(const Kernel &k) {
  // quiescent root (C1/C2's argument)
  // D-SPLIT-3 (guard purification): the ¬conflict_valid conjunct is gone —
  // this is a pure F-rewrite (sound regardless of a recorded conflict), and
  // the conjunct was redundant anyway (conflict_valid ⟹ level>0 at any rule
  // boundary, so it never fired under the retained level==0). calculus §7.
  return !k.inconsistent && k.level == 0;
}

// A retained probing trail makes k.value() speculative, so every candidacy and
// probe-list decision reads the ROOT value instead — kissat's `kissat_fixed`
// (vivify.c:60, 940). Getting this wrong is the one way trail reuse produces a
// WRONG answer rather than a slow one: the root-satisfied DELETION path is
// reached through the same test, and a clause satisfied only under an
// assumption must never be deleted. Design §3.1.
static inline int8_t root_value(const Kernel &k, Lit l) {
  const int8_t v = k.value(l);
  return (v != 0 && k.var_level(var_of(l)) == 0) ? v : 0;
}

// Does `c` fall in the band `k.vivify_scope` selects? SCHED-G3 splits the
// redundant side into kissat's three tiers (vivify.c:236-256) so the policy can
// spend its three separate shares; scope 1 keeps the whole tier2-bounded
// redundant band for external policies. Design §2.5.
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
    // scope 0 (the unscoped N1 path) keeps the pre-tier3 candidate set: a
    // redundant clause is in scope only within the live tier2.
    return !red || glue <= k.tier2;
  }
}

// candidate: live, 3 <= size <= lim, not root-satisfied; in the scope's band
static bool is_candidate(const Kernel &k, const Clause *c) {
  if (c->garbage() || c->size > VIVIFY_CLSLIM || c->size < 3)
    return false;
  // SCHED-C1: the LIVE tier, not the static default. kissat's
  // `vivify_tier2_limit` (vivify.c:132) reads `solver->tier2[0]` whenever
  // `vivifyfocusedtiers` is set, and that option DEFAULTS TO 1 -- so the dynamic
  // limit is the reference's shipped behaviour, not an opt-in. `k.tier2` is
  // recomputed from the live glue distribution on every reduce and, until now,
  // was read by nothing: we owned the mechanism and never connected it.
  if (c->redundant() && c->glue() > k.tier2 && k.vivify_scope != 5)
    return false;
  for (const Lit l : *c)
    if (root_value(k, l) > 0)
      return false;
  return true;
}

// --- the round's global literal order (SCHED-G3) ----------------------------
// Both references make cross-candidate reuse possible not with the reuse loop
// but with the ORDER: pick one global literal order by occurrence count, sort
// each clause's literals by it, then sort clauses lexicographically under it.
// cadical vivify.cpp:35-42 states it outright ("instead of building a trie we
// simply sort clauses and literals in such a way that we get the same effect");
// kissat does the same in vivify_sort_clause_by_counts + worse_candidate.
//
// DEVIATION from kissat, deliberate: kissat permutes the literals IN THE ARENA
// and therefore brackets the round with flush_large_watches/watch_large_clauses.
// We keep the order in a round-scoped side vector and sort only our local probe
// list, so the arena and the watch arena are never touched. Same effect, no
// watch rebuild.
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

// Sort `out` (a candidate's probe literals) into the global order.
static void order_sort(const VivifyOrder &ord, std::vector<Lit> &out) {
  std::sort(out.begin(), out.end(), [&ord](Lit a, Lit b) { return ord(a, b); });
}

// The non-root-fixed literals of `c`, in probe order. Clause literal order
// unless `ordered` — which is off both for the pre-G3 arm (`!vivify_reuse`) and
// for the standalone single-candidate path, where `counts` was never filled for
// this call and there is no next candidate to share a prefix with anyway.
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
  bool rearmed = false;     // ...the set was empty, so the scope was re-armed
};

// `commit` writes the re-arm back into the clause headers. The const query
// (vivify_candidates / the N1 instance enumeration) applies the same rule
// virtually, so it reports the order the round would use without touching the
// database.
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
    uint32_t prioritized, redundant, size;
    uint32_t key_off, key_len; // probe list, in the global order
  };
  std::vector<Cand> cands;
  std::vector<Lit> keys, probes;
  uint64_t prioritized = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (is_candidate(k, c) && in_scope(k, c)) {
      probe_list(k, c, ord, probes, k.vivify_reuse);
      cands.push_back({ref, c->vivify() ? 1u : 0u, c->redundant() ? 1u : 0u,
                       c->size, static_cast<uint32_t>(keys.size()),
                       static_cast<uint32_t>(probes.size())});
      prioritized += c->vivify();
      keys.insert(keys.end(), probes.begin(), probes.end());
    }
    ref += clause_words(c->size);
  }
  // SCHED-F5, kissat vivify.c:296-301: when the leftover set is empty the
  // WHOLE scope is re-armed. The bit is a priority mechanism, never a
  // suppressor — that property is exactly what the per-variable stage-F
  // substrate could not express, and why F5 routed here.
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

// Discard the whole probing trail and close the scope. THE reset in the trail-
// reuse design (§2.4/§3): every operation that can invalidate a retained trail
// — a rewrite, a deletion, a root unit, a garbage collection — is preceded by
// this, so a trail is never carried ACROSS a database change, only across
// candidates that changed nothing. That single rule is what makes invariant
// VP1 hold, and it is also why kernel_probing_end's "no clause added, deleted
// or relocated inside a scope" contract cannot be violated here.
static void reset_to_root(Kernel &k) {
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  // trail is back at the clean root: give search back the watch order and
  // `searched` hints these propagations churned (kernel.hpp
  // kernel_probing_begin). MUST run before rewrite() touches the arena — the
  // log holds Refs — and every rewrite site below is reached through here.
  kernel_probing_end(k);
}

// The reason walk (kernel_reason_walk) and conflict seed (kernel_conflict_
// reason_walk) are shared kernel helpers (kernel.cpp) — the proof-critical
// DFS lives in exactly one place, used by Vivify, Probe and Backbone.

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

// replace `c` (at cref) by `lits` with proof add before delete. Returns
// ApplyResult::Unsat for an empty rewrite, Ok otherwise. `entries`+`last`
// form the LRAT chain; ignored when not in LRAT mode.
static ApplyResult rewrite(Kernel &k, Ref cref, const std::vector<Lit> &lits,
                           std::vector<std::pair<uint32_t, uint64_t>> &entries,
                           uint64_t last, bool *ended_round) {
  Clause *c = clause_at(k.arena, cref);
  const bool lrat = k.proof && k.proof->lrat();
  const bool redundant = c->redundant();
  const uint32_t glue =
      std::min<uint32_t>(c->glue(), static_cast<uint32_t>(lits.size()));
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    if (lrat)
      emit_chain(k, id, lits, entries, last);
    else
      k.proof->add_rup(0, lits.data(), static_cast<uint32_t>(lits.size()),
                       nullptr, 0);
  }
  k.stats.vivified++;
  // shared replacement tail (kernel_replace_clause); glue is a
  // redundant-clause concept — 0 for irredundant (review-aligned)
  switch (kernel_replace_clause(k, c, id, lits.data(),
                                static_cast<uint32_t>(lits.size()), redundant,
                                redundant ? glue : 0, nullptr)) {
  case ReplaceOutcome::Refuted:
    return ApplyResult::Unsat;
  case ReplaceOutcome::Unit:
    k.unit_ids[var_of(lits[0])] = id;
    kernel_assign_unit(k, lits[0]);
    *ended_round = true; // values changed: the round's reasoning is stale
    return ApplyResult::Ok;
  default:
    return ApplyResult::Ok;
  }
}

// probe one candidate; on an actionable outcome, rewrite. Returns NoOp
// (nothing found), Ok (rewritten/deleted), or Unsat.
//
// `reuse` = the round hands over a live probing trail from the previous
// candidate and owns the scope; nullptr = the standalone path (vivify_try),
// which opens and closes its own scope around a root-to-root probe. On any
// actionable outcome this function calls reset_to_root() BEFORE mutating, in
// both modes, which is the reuse design's whole safety argument (§3).
static ApplyResult vivify_one(Kernel &k, Ref cref, bool *ended_round,
                              bool reuse) {
  Clause *c = clause_at(k.arena, cref);
  if (!is_candidate(k, c)) {
    // re-check ROOT satisfaction for deletion (a satisfied candidate is
    // dead weight; kissat/CaDiCaL delete it here). Root, not current: with a
    // retained trail a speculatively satisfied clause is NOT deletable.
    if (!c->garbage() && c->size >= 3) {
      bool satisfied = false;
      for (const Lit l : *c)
        satisfied |= root_value(k, l) > 0;
      if (satisfied && in_scope(k, c)) {
        reset_to_root(k); // a deletion is a database change: §2.4 step 5
        c = clause_at(k.arena, cref);
        if (k.proof)
          k.proof->del(k.next_clause_id, c->id(), c->lits, c->size);
        kernel_mark_garbage(k, c);
        k.stats.vivify_deleted++;
        return ApplyResult::Ok;
      }
    }
    return ApplyResult::NoOp;
  }
  k.stats.vivify_checked++;
  if (c->redundant() && c->glue() > k.tier2)
    k.stats.vivify_tier3++; // C10 hand-off H3: 0 here means tier3 is inert
  const bool lrat = k.proof && k.proof->lrat();
  // probe literals: the non-root-fixed ones, in the round's global literal
  // order (§2.2) — the order is what makes a prefix shared with the next
  // candidate, so it is not separable from the reuse loop below.
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
    // (a) The candidate may be the REASON of a retained trail literal: the
    // prefix was propagated ignoring the PREVIOUS candidate, not this one.
    // Both references check exactly this and backtrack below it (kissat
    // vivify.c:995-1005, cadical vivify.cpp:940-965). It is not only a quality
    // question — in the implied-negation case the candidate's id is the
    // CLOSING hint, so a reason walk that also reached it would emit the same
    // id twice in one LRAT chain, and a hint that is already satisfied on
    // replay is a rejection (§3.2).
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
    // literals of this one. VP3 already makes them inert, but leaving them is
    // one careless edit away from reading a value justified by an assumption
    // outside the candidate (§3.4).
    if (k.level > probes.size())
      kernel_backtrack(k, static_cast<uint32_t>(probes.size()),
                       /*update_phases=*/false);
  }
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  // Persistent walk scratch (C4-perf twin of congruence_rup_chain's): the
  // old per-candidate seen.assign(k.nvars, 0) was an O(nvars) zero-fill per
  // vivified candidate under LRAT. Grown on demand; restored all-zero in
  // O(marked) at the single exit below. lrat-gated (review #9): only the
  // reason walk reads it, so a proofs-off round pays nothing.
  static thread_local std::vector<uint8_t> seen;
  static thread_local std::vector<Var> seen_touched;
  if (lrat) {
    if (seen.size() < k.nvars)
      seen.resize(k.nvars, 0);
    seen_touched.clear();
  }
  // kissat's marks on the probe literals (vivify_mark_sorted_literals,
  // vivify.c:867-874), so a propagation can be scanned for an implied clause
  // literal the moment it produces one. Not optional once the probe order is
  // count-based: under clause order the implied literal was usually the next
  // position and the lazy `v > 0` test below caught it, but the count order
  // can place it anywhere later, and without the eager scan the new order
  // would strictly LOSE strengthenings the old one found. Persistent scratch,
  // restored in O(probes).
  static thread_local std::vector<uint8_t> marked;
  if (marked.size() < 2 * k.nvars)
    marked.resize(2 * k.nvars, 0);
  for (const Lit l : probes)
    marked[l] = 1;

  std::vector<Lit> next;
  ApplyResult res = ApplyResult::NoOp;
  // Everything the assumption propagations below churn in the watch lists /
  // clause headers is speculative and is handed back to search when the scope
  // closes (kernel.hpp kernel_probing_begin). In reuse mode the ROUND owns the
  // scope — one per run of non-mutating candidates — so it is not reopened per
  // candidate; reset_to_root() closes it at the first mutation.
  if (!reuse)
    kernel_probing_begin(k);
  uint32_t reused = 0, made = 0;
  for (uint32_t i = 0; i < probes.size(); i++) {
    const Lit li = probes[i];
    const uint32_t lvl = i + 1; // the decision level this probe occupies
    if (lvl <= k.level) {
      // Reuse test on the FRAME DECISION, not on the value: a literal that is
      // false for some other reason is not a reused assumption. Nothing is
      // read here — see VP3, and kissat's `assert (!value || LEVEL (lit) <=
      // level)` on the fall-through path, which pins that a value may only be
      // interpreted once exactly this candidate's assumptions are in force.
      if (k.trail[k.frames[lvl]] == negate(li)) {
        reused++;
        continue;
      }
      kernel_backtrack(k, lvl - 1, /*update_phases=*/false);
    }
    assert(k.level == lvl - 1); // VP3: only this candidate's assumptions hold
    const int8_t v = k.value(li);
    if (v > 0) {
      // implied: (probes[0..i-1], li) subsumes the candidate — a strict
      // strengthening since i + 1 < probes.size() (v > 0 needs at least
      // one assumption, and the LAST probe reaching here means every
      // earlier one was assumed, i.e. i+1 == size only if li was
      // assigned by assumption — excluded by v > 0 with li unassumed)
      if (i + 1 >= probes.size())
        break; // no strict gain; defensive
      next.assign(probes.begin(), probes.begin() + i);
      next.push_back(li);
      // chain: li's derivation alone — its own reason (the max-trail
      // entry) is the conflicting hint under the ¬li assumption; the
      // candidate is NOT cited (a hint after the conflict is illegal)
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
        // one and keep the prefix below it for the next candidate — kissat's
        // "forcing backtracking at least one level after conflict"
        // (vivify.c:1223-1226). The conflict handoff fields must be cleared
        // with it: a recorded conflict may never outlive its candidate.
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
    // EAGER implied literal (kissat vivify.c:1078-1090): scan what this
    // assumption just propagated for another literal OF THE CANDIDATE. Then
    // (probes[0..i], lm) is implied and subsumes the candidate. Strictly
    // stronger than waiting for position j > i, which would need j - i more
    // assumptions to reach the same conclusion.
    Lit implied = INVALID_LIT;
    // from before + 1: trail[before] is the assumption itself, which is a
    // marked literal's NEGATION and never an implied one (kissat gets the same
    // exclusion from the sign of its two-sided marks)
    for (uint32_t t = before + 1; t < k.trail.size(); t++)
      if (marked[k.trail[t]]) {
        implied = k.trail[t];
        break;
      }
    if (implied != INVALID_LIT) {
      if (i + 2 >= c->size)
        break; // no strict gain
      next.assign(probes.begin(), probes.begin() + i + 1);
      next.push_back(implied);
      // the same chain shape as the lazy case below: `implied`'s own
      // derivation, whose max-trail entry is the conflicting hint under the
      // negation of the emitted clause. The candidate is NOT cited.
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
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.vivify_rounds++;
  // The round owns the probing scope from here, so it must not inherit one:
  // if some earlier rule left a scope recording at the root, the `!recording`
  // test below would skip opening ours and every probe would land in a log
  // that closes somewhere else. Idempotent outside a scope.
  kernel_probing_end(k);
  std::vector<Ref> candidates;
  VivifySchedule sched;
  vivify_schedule(k, candidates, &sched);
  k.stats.vivify_prioritized += sched.prioritized;
  k.stats.vivify_reprioritized += sched.rearmed;
  uint64_t changed = 0;
  bool ended = false;
  const bool reuse = k.vivify_reuse;
  // k.work_budget (kernel.hpp), in Vivify's currency (propagation ticks):
  // stop visiting candidates once the round has spent the configured ticks;
  // 0 = unbounded (legacy path).
  const uint64_t tick_start = k.stats.ticks;
  for (const Ref cref : candidates) {
    if (k.work_budget && k.stats.ticks - tick_start > k.work_budget)
      break;
    // SCHED-H1 (Vivify half). Polled on EVERY candidate, exactly as kissat
    // does (vivify.c:1291) — deliberately with no `!(++tick & 255)` window,
    // because a poll window an instance never reaches is what let task #10's
    // abort paths pass 4 x 10,000 fuzz cases while vacuous, and a poll with no
    // window cannot have one. The two counters are separate on purpose: the
    // site being REACHED and the site FIRING are different claims.
    k.stats.vivify_polls++;
    if (kernel_terminating(k)) {
      k.stats.vivify_terminated++;
      break;
    }
    // In reuse mode the trail from the previous candidate is still standing
    // and the probing scope is still open; the scope is (re)opened here only
    // when the previous candidate closed it by mutating the database. Testing
    // `recording` rather than `level == 0` matters: a candidate can decline at
    // the root (too few probes, a conflict on the first assumption) without
    // ever having closed the scope, and re-opening one that is already
    // recording would roll the round's watch state back mid-round.
    if (reuse && !k.probing.recording)
      kernel_probing_begin(k);
    const ApplyResult r = vivify_one(k, cref, &ended, reuse);
    // The bit is cleared once a candidate has been TRIED, so a budget cut or a
    // termination leaves exactly the untried tail armed for the next round
    // (SCHED-F5, kissat vivify.c:1311). The clause may now be garbage — the
    // header is still there until the collect below.
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
  // Standalone: its own root-to-root probing scope, no retained trail. The N1
  // surface must not depend on a round having run before it.
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
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
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

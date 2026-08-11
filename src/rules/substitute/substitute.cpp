// See substitute.hpp for the rule's contract. The SCC pass is transliterated
// from kissat decompose.c/substitute.c and cadical decompose.cpp (iterative
// Tarjan), deliberately not CryptoMiniSat's recursive sccfinder: its
// max_scc_depth early-return silently drops deep SCCs, a completeness loss,
// whereas an explicit work stack survives arbitrarily deep chains.
#include "rules/substitute/substitute.hpp"

#include "rules/propagate/propagate.hpp"

#include "kernel/watch.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <numeric>
#include <set>
#include <unordered_set>
#include <utility>

namespace tactix {

bool substitute_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/calculus.md` §7).
  return !k.inconsistent && k.level == 0;
}

SccResult substitute_find_sccs(const Kernel &k) {
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  SccResult res;
  res.repr.assign(nlits, INVALID_LIT);
  if (nlits == 0)
    return res;

  std::vector<uint32_t> dfs(nlits, 0); // 1-based DFS index; 0 = unvisited
  std::vector<uint32_t> low(nlits, 0); // low-link
  std::vector<uint8_t> onstack(nlits, 0);
  std::vector<Lit> tstack; // Tarjan's component stack
  uint32_t next = 1;

  // explicit DFS work stack: (node, resume point in the node's watch list)
  struct Frame {
    Lit v;
    WatchIter it;
  };
  std::vector<Frame> work;

  // Kissat substitute.c builds the implication SCC graph from ACTIVE
  // variables only.  Retired identities can remain addressable and stale
  // binaries can mention them, but they must not be substituted a second time:
  // that would enqueue a duplicate deactivation for an already-disconnected
  // decision variable.
  auto active = [&](Lit l) {
    return k.values[l] == 0 && !kernel_var_retired(k, var_of(l));
  };
  // Out-edges of p are the binary watches of negate(p): (negate(p) ∨ q) is
  // exactly p → q. The watch arena is never mutated here, so a suspended
  // iterator stays valid across the whole DFS.
  auto out_edges = [&](Lit p) { return k.watches.entries(negate(p)); };

  for (Lit root = 0; root < nlits; root++) {
    if (dfs[root] != 0 || !active(root))
      continue;
    dfs[root] = low[root] = next++;
    onstack[root] = 1;
    tstack.push_back(root);
    k.stats.substitute_ticks +=
        1 + kernel_cache_lines(k.watches.size(negate(root)));
    work.push_back({root, out_edges(root).begin()});

    while (!work.empty()) {
      const Lit v = work.back().v;
      const WatchIter e = out_edges(v).end();
      WatchIter it = work.back().it;
      bool descended = false;
      for (; it != e; ++it) {
        const WatchEntry w = *it;
        if (!w.binary())
          continue;
        const Lit q = w.other();
        if (!active(q))
          continue; // edge into a root-fixed literal: not a node
        if (dfs[q] == 0) {
          // tree edge: save v's resume point (past this edge), descend into q
          work.back().it = ++it;
          dfs[q] = low[q] = next++;
          onstack[q] = 1;
          tstack.push_back(q);
          k.stats.substitute_ticks +=
              1 + kernel_cache_lines(k.watches.size(negate(q)));
          // may reallocate `work`; touch it no more
          work.push_back({q, out_edges(q).begin()});
          descended = true;
          break;
        }
        if (onstack[q] && dfs[q] < low[v])
          low[v] = dfs[q]; // frond to an on-stack node
      }
      if (descended)
        continue;
      work.back().it = it;

      if (low[v] == dfs[v]) {
        // v roots an SCC: pop the component off tstack
        size_t start = tstack.size();
        do {
          --start;
        } while (tstack[start] != v);
        Lit rep = tstack[start];
        for (size_t i = start; i < tstack.size(); i++)
          rep = std::min(rep, tstack[i]);
        const size_t members = tstack.size() - start;
        for (size_t i = start; i < tstack.size(); i++) {
          const Lit m = tstack[i];
          res.repr[m] = rep;
          onstack[m] = 0;
        }
        if (members > 1) {
          res.nontrivial_reps.push_back(rep);
          for (size_t i = start; i < tstack.size(); i++)
            if (res.repr[negate(tstack[i])] == rep) { // l and ¬l share the SCC
              res.contradiction = true;
              if (res.contradiction_lit == INVALID_LIT)
                res.contradiction_lit = tstack[i]; // x with ¬x in this SCC
              break;
            }
        }
        tstack.resize(start);
      }

      // return from v to its parent: fold v's low-link up, then pop v
      const Lit done = v;
      work.pop_back();
      if (!work.empty()) {
        const Lit parent = work.back().v;
        if (low[done] < low[parent])
          low[parent] = low[done];
      }
    }
  }
  std::sort(res.nontrivial_reps.begin(), res.nontrivial_reps.end());
  return res;
}

// One directed graph path `from -> ... -> to` as its edge clause-ids in forward
// order (LRAT only; the caller guards on k.proof->lrat()). An edge p -> q is
// the clause (¬p ∨ q). Active literals only, matching the SCC nodes; returns
// false if no path exists, which never happens for two members of one SCC but
// keeps the function total for direct callers.
//
// Chain order is load-bearing: for the RUP unit {to} the checker assumes the
// path's start literal true and unit-propagates the edges in order, so edge
// (¬p ∨ q) with p true forces q. A shuffled chain leaves a hint non-unit and
// the checker rejects.
bool big_path_chain(const Kernel &k, Lit from, Lit to,
                    std::vector<uint64_t> &chain) {
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  // Persistent scratch: three fill-initialized O(nlits) vectors here meant one
  // large memset per bridge, the per-call linear-scratch hazard.
  // parent/parent_edge need no clearing — they are read only behind the `seen`
  // guard and every marked literal writes both first — and `seen` restores
  // all-zero in O(marked), since every marked literal is queued except `to`.
  static thread_local std::vector<Lit> parent;
  static thread_local std::vector<uint64_t> parent_edge;
  static thread_local std::vector<uint8_t> seen;
  static thread_local std::vector<Lit> queue;
  if (seen.size() < static_cast<size_t>(nlits)) {
    parent.resize(nlits);
    parent_edge.resize(nlits);
    seen.resize(nlits, 0);
  }
  queue.clear();
  queue.push_back(from);
  seen[from] = 1;
  bool found = false;
  for (size_t qi = 0; qi < queue.size() && !found; qi++) {
    const Lit cur = queue[qi];
    const Lit nl = negate(cur); // edges cur -> q are the binary watches of ¬cur
    for (const WatchEntry w : k.watches.entries(nl)) {
      if (!w.binary())
        continue;
      const Lit q = w.other();
      if (k.values[q] != 0 || kernel_var_retired(k, var_of(q)) || seen[q])
        continue;
      seen[q] = 1;
      parent[q] = cur;
      parent_edge[q] = kernel_binary_id(k, nl, q); // clause (¬cur ∨ q) = cur->q
      if (q == to) {
        found = true;
        break;
      }
      queue.push_back(q);
    }
  }
  const auto restore_seen = [&] {
    for (const Lit l : queue)
      seen[l] = 0;
    seen[to] = 0; // marked without being queued when found
  };
  if (!found) {
    restore_seen();
    return false;
  }
  chain.clear();
  for (Lit cur = to; cur != from; cur = parent[cur])
    chain.push_back(parent_edge[cur]);
  std::reverse(chain.begin(), chain.end());
  restore_seen();
  return true;
}

namespace {

// x ≡ ¬x: emit the refutation (`docs/calculus.md` V3).
// {¬x} is RUP (assume x; the path x -> ¬x propagates ¬x) and {x} likewise via
// ¬x -> x, and the two resolve to the empty clause. Uses add_rup rather than
// add_derived_unit, which emits nothing in DRAT, because these are
// failed-literal units the DRAT checker must see. Self-contained: not M1's.
void emit_unsat(Kernel &k, Lit x) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit nx = negate(x);
  std::vector<uint64_t> chain_nx, chain_x;
  if (lrat) {
    // {¬x}: chain = path x -> ¬x; {x}: chain = path ¬x -> x
    const bool ok1 = big_path_chain(k, x, nx, chain_nx);
    const bool ok2 = big_path_chain(k, nx, x, chain_x);
    assert(ok1 && ok2 && "x≡¬x SCC must have both directed paths");
    (void)ok1;
    (void)ok2;
  }
  const uint64_t id_nx = ++k.next_clause_id;
  const uint64_t id_x = ++k.next_clause_id;
  const uint64_t id_e = ++k.next_clause_id;
  if (k.proof) {
    k.proof->add_rup(id_nx, &nx, 1, chain_nx.data(),
                     static_cast<uint32_t>(chain_nx.size()));
    k.proof->add_rup(id_x, &x, 1, chain_x.data(),
                     static_cast<uint32_t>(chain_x.size()));
    const uint64_t empty_chain[2] = {id_nx, id_x};
    k.proof->add_empty(id_e, empty_chain, 2);
  }
  k.inconsistent = true;
}

// The equivalent-literal rewrite. `sub[l]` marks the non-representative
// SCC-member literals to rewrite (l → r.repr[l]); the caller selects them, as
// the whole round or as one requested SCC plus its mirror. Emits the transient
// equivalence lemmas {¬l, r.repr[l]}, rewrites every large and binary
// occurrence add-before-delete, deletes the lemmas, pushes the reconstruction
// witnesses once per substituted variable, and asserts any collapsed root units
// un-propagated (M1 owns the cascade, the Probe/Vivify batch discipline).
// Returns Unsat on an empty rewrite, Ok if anything changed, NoOp otherwise.
enum class SubstitutionArenaOrder {
  Policy,
  PreserveClausePosition,
};

// Sweep rewrites each connected clause in place (`sweep.c`'s
// `substitute_connected_clauses`).  Tactix clauses cannot shrink in place: the
// arena record has no separate allocation-size field, so a replacement is
// appended and the old record is garbaged.  Preserve the same *logical* pose at
// collection time by assigning every large replacement the ordinal of its old
// record.  This is a general representation bridge, not a candidate-order
// heuristic: all untouched clauses keep their ordinal and binary/unit
// collapses simply leave a hole.
struct PositionPreservingOrder final : ArenaOrdering {
  std::vector<uint64_t> rank;
  uint64_t ranks = 0;

  void order(const Kernel &, std::vector<Ref> &refs) override {
    // Every original clause position owns one rank, and replacement clauses
    // inherit that rank after the old record is garbaged.  Thus live ranked
    // records form a partial permutation: restore it in linear time instead of
    // comparison-sorting the complete formula after every productive Sweep
    // environment.  The tail is defensive for unranked/duplicate records and
    // retains their incoming arena order, matching the old stable-sort tie.
    std::vector<Ref> by_rank(ranks, INVALID_REF), tail;
    tail.reserve(refs.size());
    for (const Ref ref : refs) {
      const uint64_t r = ref < rank.size() ? rank[ref] : UINT64_MAX;
      if (r < ranks && by_rank[r] == INVALID_REF)
        by_rank[r] = ref;
      else
        tail.push_back(ref);
    }
    refs.clear();
    refs.reserve(by_rank.size() + tail.size());
    for (const Ref ref : by_rank)
      if (ref != INVALID_REF)
        refs.push_back(ref);
    refs.insert(refs.end(), tail.begin(), tail.end());
  }
};

ApplyResult do_substitution(Kernel &k, const SccResult &r,
                            const std::vector<uint8_t> &sub,
                            SubstitutionArenaOrder arena_order =
                                SubstitutionArenaOrder::Policy) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  PositionPreservingOrder position_order;
  if (arena_order == SubstitutionArenaOrder::PreserveClausePosition) {
    position_order.rank.assign(k.arena.size(), UINT64_MAX);
    uint64_t ordinal = 0;
    for (Ref ref = 0; ref < k.arena.size(); ordinal++) {
      const Clause *c = clause_at(k.arena, ref);
      position_order.rank[ref] = ordinal;
      ref += clause_words(c->size);
    }
    position_order.ranks = ordinal;
  }
  // The active substitution set: the caller's `sub`, which step (A) may narrow
  // to `sub_filtered`. The lambda reads through the pointer, so repointing it
  // below re-aims every later use in one place.
  std::vector<uint8_t> sub_filtered;
  const std::vector<uint8_t> *subp = &sub;
  auto substitutable = [&](Lit l) { return l < nlits && (*subp)[l]; };

  // (A) hazard pre-check, per SCC. A pair with >= 2 live copies cannot be
  // deleted by name (kernel_delete_binary refuses — one id cannot name two
  // clauses). The skip must stay per class, never widened back to a whole
  // round: a per-round skip's survival probability collapses as the marked set
  // grows, and the instances with the most equivalences to exploit mark the
  // most literals. Poisoning only the offending class keeps the all-or-nothing
  // granularity the proof needs while denying one bad pair a veto over the
  // round. Both endpoints' classes are poisoned, since rewriting a surviving
  // class could otherwise still rename a literal of that pair.
  std::vector<uint8_t> poisoned(nlits, 0);
  bool any_hazard = false;
  auto poison_class = [&](Lit rep) {
    if (rep >= nlits)
      return;
    poisoned[rep] = 1;
    poisoned[negate(rep)] = 1;
    any_hazard = true;
  };
  for (Lit l = 0; l < nlits; l++) {
    if (!substitutable(l))
      continue;
    for (const WatchEntry w : k.watches.entries(l)) {
      if (!w.binary())
        continue;
      if (kernel_binary_copies(k, l, w.other(), 2) >= 2) {
        if (k.subst_hazard_round) { // A/B: the old per-ROUND skip
          k.stats.substitute_hazard_skips++;
          return ApplyResult::NoOp;
        }
        poison_class(r.repr[l]);
        const Lit o = w.other();
        if (o < nlits && r.repr[o] != INVALID_LIT)
          poison_class(r.repr[o]);
      }
    }
  }
  if (any_hazard) {
    sub_filtered = sub;
    uint64_t survivors = 0;
    for (Lit l = 0; l < nlits; l++) {
      if (!sub_filtered[l])
        continue;
      const Lit rep = r.repr[l];
      if (rep < nlits && poisoned[rep]) {
        sub_filtered[l] = 0;
        k.stats.substitute_hazard_lits++;
      } else {
        survivors++;
      }
    }
    subp = &sub_filtered;
    if (!survivors) {
      k.stats.substitute_hazard_skips++; // nothing left — a true round skip
      return ApplyResult::NoOp;
    }
    k.stats.substitute_hazard_partial++;
  }

  // (B) transient equivalence lemmas {¬l, repr[l]}, one per member literal,
  // derived before any binary mutation so big_path_chain reads the intact
  // graph. RUP: assume l, ¬repr[l]; the path l→…→repr[l] forces repr[l].
  std::vector<uint64_t> lemma(nlits, 0);
  for (Lit l = 0; l < nlits; l++) {
    if (!substitutable(l))
      continue;
    const Lit s = r.repr[l];
    std::vector<uint64_t> chain;
    if (lrat) {
      const bool ok = big_path_chain(k, l, s, chain);
      assert(ok && "SCC members are strongly connected");
      (void)ok;
    }
    const uint64_t id = ++k.next_clause_id;
    lemma[l] = id;
    if (k.proof) {
      const Lit lits2[2] = {negate(l), s};
      if (lrat)
        k.proof->add_rup(id, lits2, 2, chain.data(),
                         static_cast<uint32_t>(chain.size()));
      else
        k.proof->add_rup(0, lits2, 2, nullptr, 0);
    }
  }

  std::vector<uint8_t> seen(nlits, 0); // dedup scratch for a rewritten clause
  // Batched root units + opposite-sign refutation: the shared kernel idiom
  // (PendingUnits, kernel.hpp).
  PendingUnits pending;
  pending.init(k.nvars);
  bool changed = false;

  // Record a size-collapsed root unit `u`, deduping per variable within the
  // round. Returns true iff `u` contradicts a unit already derived this round,
  // in which case the two resolve to ∅ and the caller returns Unsat. Without
  // this, a batch deriving both `x` and `¬x` would assert the first and
  // silently drop the second, losing the conflict.
  auto record_unit = [&](Lit u, uint64_t id) -> bool {
    return pending.record(k, u, id) == PendingUnits::Outcome::Refuted;
  };

  // Rewrite one literal sequence: apply repr, strip root-false literals (their
  // unit ids join `chain`), detect root-true/tautology. Fills `out` with the
  // kept active literals and `chain` with the antecedent ids, without the
  // original clause id — the caller appends that. Returns true iff the rewrite
  // is satisfied or tautological, in which case the caller only deletes.
  auto rewrite_lits = [&](const Lit *lits, uint32_t size, std::vector<Lit> &out,
                          std::vector<uint64_t> &chain) -> bool {
    out.clear();
    chain.clear();
    bool satisfied = false;
    for (uint32_t i = 0; i < size; i++) {
      const Lit m = lits[i];
      const Lit mp = substitutable(m) ? r.repr[m] : m;
      if (substitutable(m) && lrat)
        chain.push_back(lemma[m]);
      if (k.value(mp) > 0) { // root-true → clause satisfied
        satisfied = true;
        break;
      }
      if (k.value(mp) < 0) { // root-false → strip (cite its unit)
        if (lrat)
          chain.push_back(k.unit_ids[var_of(mp)]);
        continue;
      }
      if (seen[mp]) // duplicate literal
        continue;
      if (seen[negate(mp)]) { // mp and ¬mp both present → tautology
        satisfied = true;
        break;
      }
      seen[mp] = 1;
      out.push_back(mp);
    }
    for (const Lit l : out)
      seen[l] = 0; // unmark
    if (satisfied) {
      out.clear();
      return true;
    }
    if (lrat) { // a duplicated member literal would push its lemma twice
      std::sort(chain.begin(), chain.end());
      chain.erase(std::unique(chain.begin(), chain.end()), chain.end());
    }
    return false;
  };

  // (C) rewrite large clauses. Snapshot the original refs first (adds append to
  // the arena). Vivify's ordering: add_rup, del and the garbage mark all read
  // the clause before the arena-mutating add, so the Clause* never dangles.
  std::vector<Ref> refs;
  // During preprocessing the dense index covers every live large clause
  // (there are no learned clauses yet).  Sweep substitutes tiny, freshly
  // proved classes one seed at a time; collecting just their connected clauses
  // avoids rescanning the complete input arena after every seed.  Later rounds
  // can contain redundant clauses, which are intentionally absent from the
  // dense index, so they retain the complete arena snapshot.
  if (k.stats.conflicts == 0 && k.dense) {
    std::unordered_set<Ref> connected;
    for (Lit l = 0; l < nlits; l++) {
      if (!substitutable(l))
        continue;
      for (const Ref ref : k.dense->large[l]) {
        const Clause *c = clause_at(k.arena, ref);
        if (!c->garbage())
          connected.insert(ref);
      }
    }
    refs.assign(connected.begin(), connected.end());
    std::sort(refs.begin(), refs.end()); // preserve current arena traversal
  } else {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage())
        refs.push_back(ref);
      ref += clause_words(c->size);
    }
  }
  std::vector<Lit> out;
  std::vector<uint64_t> chain;
  for (const Ref ref : refs) {
    Clause *c = clause_at(k.arena, ref);
    bool touched = false;
    for (const Lit m : *c)
      if (substitutable(m)) {
        touched = true;
        break;
      }
    if (!touched)
      continue;
    const bool credundant = c->redundant();
    const uint32_t cglue = c->glue();
    const uint64_t cid = c->id();
    const bool sat = rewrite_lits(c->lits, c->size, out, chain);
    if (sat) { // satisfied/tautology → delete only (deletions are proof-free)
      kernel_delete_clause(k, c); // cid == c->id()
      changed = true;
      continue;
    }
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) {
      if (lrat) {
        chain.push_back(cid); // original clause closes the RUP chain
        k.proof->add_rup(id, out.data(), static_cast<uint32_t>(out.size()),
                         chain.data(), static_cast<uint32_t>(chain.size()));
      } else {
        k.proof->add_rup(0, out.data(), static_cast<uint32_t>(out.size()),
                         nullptr, 0);
      }
    }
    changed = true;
    // shared replacement tail; glue 0 for irredundant (review-aligned);
    // c dangles after (arena may reallocate) and is not touched again
    // Clamped on cadical's rule (decompose.cpp:629 → shrink_clause); kissat's
    // substitute.c does not clamp here at all. Deviation:
    // docs/implementation-notes.md §13.3.
    Ref replacement_ref = INVALID_REF;
    switch (kernel_replace_clause(
        k, c, id, out.data(), static_cast<uint32_t>(out.size()), credundant,
        credundant ? kernel_shrunken_glue(
                         k, cglue, static_cast<uint32_t>(out.size()))
                   : 0,
        arena_order == SubstitutionArenaOrder::PreserveClausePosition
            ? &replacement_ref
            : nullptr)) {
    case ReplaceOutcome::Refuted: // all-false rewrite (unreachable at a
      return ApplyResult::Unsat;  // quiescent root)
    case ReplaceOutcome::Unit:
      if (record_unit(out[0], id))
        return ApplyResult::Unsat;
      break;
    default:
      break;
    }
    if (replacement_ref != INVALID_REF) {
      assert(arena_order == SubstitutionArenaOrder::PreserveClausePosition);
      if (position_order.rank.size() <= replacement_ref)
        position_order.rank.resize(replacement_ref + 1, UINT64_MAX);
      assert(ref < position_order.rank.size());
      position_order.rank[replacement_ref] = position_order.rank[ref];
    }
  }

  // (D) rewrite BINARY clauses. Snapshot the member pairs first (rewriting
  // mutates watch lists); each pair collected once via its canonical key.
  std::vector<std::pair<Lit, Lit>> bins;
  std::set<uint64_t> seen_pair;
  for (Lit l = 0; l < nlits; l++) {
    if (!substitutable(l))
      continue;
    for (const WatchEntry w : k.watches.entries(l)) {
      if (!w.binary())
        continue;
      const Lit z = w.other();
      if (seen_pair.insert(kernel_binary_key(l, z)).second)
        bins.emplace_back(l, z);
    }
  }
  for (const auto &pr : bins) {
    const Lit l = pr.first, z = pr.second;
    if (!kernel_binary_live(k, l, z))
      continue; // defensive (never happens: added pairs are rep-only)
    const Lit lp = substitutable(l) ? r.repr[l] : l;
    const Lit zp = substitutable(z) ? r.repr[z] : z;
    const bool taut = (lp == negate(zp));
    if (taut || k.value(lp) > 0 || k.value(zp) > 0) { // satisfied → delete-only
      const bool ok = kernel_delete_binary(k, l, z);
      assert(ok);
      (void)ok;
      changed = true;
      continue;
    }
    std::vector<uint64_t> ch;
    if (lrat) {
      if (substitutable(l))
        ch.push_back(lemma[l]);
      if (substitutable(z))
        ch.push_back(lemma[z]);
    }
    out.clear();
    for (const Lit x : {lp, zp}) {
      if (k.value(x) < 0) { // root-false → strip (cite its unit)
        if (lrat)
          ch.push_back(k.unit_ids[var_of(x)]);
      } else if (std::find(out.begin(), out.end(), x) == out.end()) {
        out.push_back(x); // active, deduped
      }
    }
    if (out.size() == 2 && kernel_binary_live(k, out[0], out[1])) {
      // rewritten target already present → delete original only (no add needed)
      const bool ok = kernel_delete_binary(k, l, z);
      assert(ok);
      (void)ok;
      changed = true;
      continue;
    }
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) {
      if (lrat) {
        ch.push_back(kernel_binary_id(k, l, z)); // original closes the chain
        k.proof->add_rup(id, out.data(), static_cast<uint32_t>(out.size()),
                         ch.data(), static_cast<uint32_t>(ch.size()));
      } else {
        k.proof->add_rup(0, out.data(), static_cast<uint32_t>(out.size()),
                         nullptr, 0);
      }
    }
    if (out.empty()) { // both root-false (unreachable at a quiescent root)
      k.inconsistent = true;
      return ApplyResult::Unsat;
    }
    if (out.size() == 1) {
      if (record_unit(out[0], id))
        return ApplyResult::Unsat;
    } else {
      kernel_add_binary_clause(k, out[0], out[1], id); // add before delete
    }
    const bool ok = kernel_delete_binary(k, l, z);
    assert(ok);
    (void)ok;
    changed = true;
  }

  // (E) delete the transient equivalence lemmas (keep the core clean).
  if (k.proof)
    for (Lit l = 0; l < nlits; l++)
      if (substitutable(l)) {
        const Lit lits2[2] = {negate(l), r.repr[l]};
        k.proof->del(k.next_clause_id, lemma[l], lits2, 2);
      }

  // (F) reconstruction witnesses, once per substituted variable: the
  // order-independent symmetric pair for l ≡ s = repr[l], {¬l, s} witnessed by
  // ¬l and {l, ¬s} witnessed by l, so kernel_extend_model recovers l's value
  // from s regardless of any search assignment to l.
  for (Var v = 0; v < k.nvars; v++) {
    const Lit l = make_lit(v, false);
    if (!substitutable(l))
      continue;
    const Lit s = r.repr[l];
    const Lit rest_a[1] = {s};
    k.stats.substitute_vars++; // one per substituted variable (see Stats)
    // Retire v here, at the witness push: the status and the reconstruction
    // witness must be created together, because a retired variable with no
    // witness is never decided and never reconstructed — a wrong SAT answer
    // rather than a crash (`docs/design/variable-retirement-design-v2.md` §3).
    if (k.var_status.size() < k.nvars)
      k.var_status.resize(k.nvars, VarStatus::Active);
    if (k.var_status[v] == VarStatus::Active &&
        k.values[make_lit(v, false)] == 0) {
      assert(k.unassigned_active > 0); // it leaves the active pool
      k.unassigned_active--;
    }
    k.var_status[v] = VarStatus::Substituted;
    k.retired_events.push_back(v);
    k.substituted_events.push_back(v);
    kernel_push_witness(k, negate(l), rest_a, 1); // clause {s, ¬l}
    const Lit rest_b[1] = {negate(s)};
    kernel_push_witness(k, l, rest_b, 1); // clause {¬s, l}
  }

  // (G) assert the collapsed root units un-propagated; M1 owns the cascade and
  // emits nothing new (each unit already carries its add-RUP id).
  for (const Lit u : pending.units)
    if (k.value(u) == 0)
      kernel_assign_unit(k, u);

  if (changed) {
    if (arena_order == SubstitutionArenaOrder::PreserveClausePosition) {
      ArenaOrdering *const saved = k.arena_ordering;
      k.arena_ordering = &position_order;
      kernel_collect_garbage(k); // reclaim without moving replacements to tail
      k.arena_ordering = saved;
    } else {
      kernel_collect_garbage(k); // reclaim the garbaged large clauses
    }
  }
  return changed ? ApplyResult::Ok : ApplyResult::NoOp;
}

} // namespace

// Canonical instance: one full round. Detect SCCs; on x ≡ ¬x refute, otherwise
// substitute every non-trivial SCC onto its representative.
ApplyResult substitute_apply(Kernel &k) {
  assert(substitute_guard(k));
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k);
  if (r.contradiction) {
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  k.stats.substitute_sccs += r.nontrivial_reps.size(); // counted before the
  // early return, so "no SCC found" is distinguishable from "found, dropped"
  if (r.nontrivial_reps.empty())
    return ApplyResult::NoOp;
  // every non-representative member of a non-trivial SCC is substitutable
  std::vector<uint8_t> sub(2 * k.nvars, 0);
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    if (r.repr[l] != INVALID_LIT && r.repr[l] != l) {
      sub[l] = 1;
      k.stats.substitute_scc_lits++;
    }
  return do_substitution(k, r, sub);
}

static ApplyResult substitute_apply_equivalences_impl(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &equivalences,
    bool drain_root) {
  if (equivalences.empty())
    return ApplyResult::NoOp;
  assert(k.level == 0 && !k.conflict_valid);
  if (drain_root && inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k);
  if (r.contradiction) {
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  k.stats.substitute_sccs += r.nontrivial_reps.size();
  std::vector<uint8_t> sub(2 * k.nvars, 0);
  for (const auto &eq : equivalences) {
    const Lit a = eq.first, b = eq.second;
    if (a >= r.repr.size() || b >= r.repr.size())
      continue;
    const Lit rep = r.repr[a];
    if (rep == INVALID_LIT || r.repr[b] != rep)
      continue; // the supplied binaries no longer state a live SCC class
    const Lit mirror = r.repr[negate(a)];
    for (Lit l = 0; l < static_cast<Lit>(r.repr.size()); l++)
      if (r.repr[l] != INVALID_LIT && r.repr[l] != l &&
          (r.repr[l] == rep || r.repr[l] == mirror)) {
        if (!sub[l])
          k.stats.substitute_scc_lits++;
        sub[l] = 1;
      }
  }
  if (std::none_of(sub.begin(), sub.end(), [](uint8_t marked) {
        return marked != 0;
      }))
    return ApplyResult::NoOp;
  // This path implements Kissat Sweep's connected-clause rewrite. Do not let
  // DefaultPolicy's CaDiCaL-style VMTF arena pose reorder the whole formula at
  // the helper's garbage collection boundary; ordinary Substitute retains
  // that policy, while Sweep preserves the live arena traversal it inherited.
  ArenaOrdering *const saved_order = k.arena_ordering;
  k.arena_ordering = nullptr;
  const ApplyResult result = do_substitution(
      k, r, sub, SubstitutionArenaOrder::PreserveClausePosition);
  k.arena_ordering = saved_order;
  return result;
}

ApplyResult substitute_apply_equivalences(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &equivalences) {
  return substitute_apply_equivalences_impl(k, equivalences, true);
}

ApplyResult substitute_apply_sweep_equivalences(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &equivalences) {
  if (equivalences.empty())
    return ApplyResult::NoOp;
  assert(k.level == 0 && !k.conflict_valid);

  // Sweep has just inserted both implication binaries for every pair.  Its
  // resumable loop needs those identities substituted before choosing the next
  // seed, but a whole-formula Tarjan pass here makes that bridge O(seeds * F).
  // Build exactly the equivalence classes stated by this emission batch; any
  // older/larger SCC remains visible to the normal Substitute slot immediately
  // after Sweep.  This is complete for the requested rewrite and preserves the
  // same minimum-literal representative as substitute_find_sccs.
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  std::vector<Lit> members;
  members.reserve(4 * equivalences.size());
  for (const auto &eq : equivalences) {
    const Lit a = eq.first, b = eq.second;
    if (a >= nlits || b >= nlits || a == b || a == negate(b) ||
        k.value(a) != 0 || k.value(b) != 0)
      continue;
    members.insert(members.end(), {a, b, negate(a), negate(b)});
  }
  if (members.empty())
    return ApplyResult::NoOp;
  std::sort(members.begin(), members.end());
  members.erase(std::unique(members.begin(), members.end()), members.end());

  std::vector<uint32_t> parent(members.size());
  std::iota(parent.begin(), parent.end(), 0);
  const auto index_of = [&](Lit l) {
    const auto it = std::lower_bound(members.begin(), members.end(), l);
    assert(it != members.end() && *it == l);
    return static_cast<uint32_t>(it - members.begin());
  };
  const auto find = [&](uint32_t start) {
    uint32_t root = start;
    while (parent[root] != root)
      root = parent[root];
    while (parent[start] != start) {
      const uint32_t next = parent[start];
      parent[start] = root;
      start = next;
    }
    return root;
  };
  const auto unite = [&](Lit a, Lit b) {
    uint32_t ra = find(index_of(a)), rb = find(index_of(b));
    if (ra == rb)
      return;
    if (members[rb] < members[ra])
      std::swap(ra, rb);
    parent[rb] = ra;
  };
  for (const auto &eq : equivalences) {
    const Lit a = eq.first, b = eq.second;
    if (a >= nlits || b >= nlits || a == b || a == negate(b) ||
        k.value(a) != 0 || k.value(b) != 0)
      continue;
    unite(a, b);
    unite(negate(a), negate(b));
  }

  SccResult r;
  r.repr.assign(nlits, INVALID_LIT);
  std::vector<uint32_t> class_size(members.size(), 0);
  for (uint32_t i = 0; i < members.size(); i++)
    class_size[find(i)]++;
  for (uint32_t i = 0; i < members.size(); i++) {
    const uint32_t root = find(i);
    r.repr[members[i]] = members[root];
    if (i == root && class_size[root] > 1)
      r.nontrivial_reps.push_back(members[root]);
  }

  k.stats.substitute_rounds++;
  k.stats.substitute_sccs += r.nontrivial_reps.size();
  std::vector<uint8_t> sub(nlits, 0);
  for (const Lit l : members)
    if (r.repr[l] != l) {
      sub[l] = 1;
      k.stats.substitute_scc_lits++;
    }

  ArenaOrdering *const saved_order = k.arena_ordering;
  k.arena_ordering = nullptr;
  const ApplyResult result = do_substitution(
      k, r, sub, SubstitutionArenaOrder::PreserveClausePosition);
  k.arena_ordering = saved_order;
  return result;
}

// Instance surface: payload = an SCC representative literal. A single instance
// substitutes the requested class — the SCC with representative `rep` and its
// mirror ¬rep — so both polarities of every member variable go together.
static uint32_t substitute_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  SccResult r = substitute_find_sccs(k);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(r.nontrivial_reps.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(k, r.nontrivial_reps[i]);
  return n;
}

static ApplyResult substitute_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return substitute_apply(k);
  Lit rep;
  if (!lit_of_instance(k, inst, &rep))
    return ApplyResult::NoOp;
  if (var_of(rep) >= k.nvars || k.values[rep] != 0)
    return ApplyResult::NoOp;
  // re-derive from the live graph, which may have changed since instances()
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k);
  if (r.contradiction) { // a global x ≡ ¬x refutes regardless of the request
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  // substitute only members whose representative is rep or its mirror ¬rep; a
  // dissolved SCC marks nothing and yields NoOp
  k.stats.substitute_sccs += r.nontrivial_reps.size();
  const Lit nrep = negate(rep);
  std::vector<uint8_t> sub(2 * k.nvars, 0);
  bool any = false;
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    if (r.repr[l] != INVALID_LIT && r.repr[l] != l &&
        (r.repr[l] == rep || r.repr[l] == nrep)) {
      sub[l] = 1;
      any = true;
    }
  if (!any)
    return ApplyResult::NoOp;
  return do_substitution(k, r, sub);
}

TACTIX_REGISTER_RULE(Substitute, RuleTier::Inprocess, substitute_guard,
                     substitute_apply, substitute_instances,
                     substitute_apply_instance);

} // namespace tactix

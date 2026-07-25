// V3a: SCC detection over the binary implication graph (guard + iterative
// Tarjan). Transliterated from kissat/src/decompose.c + substitute.c and
// cadical/src/decompose.cpp (iterative Tarjan; MIT) — deliberately NOT
// CryptoMiniSat's recursive sccfinder with its max_scc_depth early-return,
// which silently drops deep SCCs (a completeness loss). An explicit work
// stack survives arbitrarily deep implication chains.
#include "rules/substitute/substitute.hpp"

#include "rules/propagate/propagate.hpp"

#include "kernel/watch.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace tactix {

bool substitute_guard(const Kernel &k) {
  // quiescent root (C1/C2/C3/V1/V2's argument)
  // D-SPLIT-3 (guard purification): the ¬conflict_valid conjunct is gone —
  // this is a pure F-rewrite (sound regardless of a recorded conflict), and
  // the conjunct was redundant anyway (conflict_valid ⟹ level>0 at any rule
  // boundary, so it never fired under the retained level==0). calculus §7.
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

  // explicit DFS work stack: (node, next watch-word offset still to scan)
  struct Frame {
    Lit v;
    uint32_t wi;
  };
  std::vector<Frame> work;

  auto active = [&](Lit l) { return k.values[l] == 0; };
  // Out-edges of p (implications p → q) are the binary watches of negate(p):
  // a binary clause (negate(p) ∨ q) is exactly the implication p → q. The
  // watch arena is never mutated here (read-only pass), so its pointers stay
  // valid for the whole walk.
  auto wbase = [&](Lit l) {
    return k.watches.arena.data() + k.watches.heads[l].offset;
  };
  auto wsize = [&](Lit l) { return k.watches.heads[l].size; };

  for (Lit root = 0; root < nlits; root++) {
    if (dfs[root] != 0 || !active(root))
      continue;
    dfs[root] = low[root] = next++;
    onstack[root] = 1;
    tstack.push_back(root);
    work.push_back({root, 0});

    while (!work.empty()) {
      const Lit v = work.back().v;
      const Lit nv = negate(v);
      const uint32_t *ws = wbase(nv);
      const uint32_t sz = wsize(nv);
      uint32_t wi = work.back().wi;
      bool descended = false;
      while (wi < sz) {
        const Watch w{ws[wi]};
        if (!w.binary()) {
          wi += 2; // large watch: blocking word + ref word
          continue;
        }
        const Lit q = w.other();
        wi += 1;
        if (!active(q))
          continue; // edge into a root-fixed literal: not a node
        if (dfs[q] == 0) {
          // tree edge: save v's resume point, then descend into q
          work.back().wi = wi;
          dfs[q] = low[q] = next++;
          onstack[q] = 1;
          tstack.push_back(q);
          work.push_back({q, 0}); // may reallocate `work`; touch it no more
          descended = true;
          break;
        }
        if (onstack[q] && dfs[q] < low[v])
          low[v] = dfs[q]; // frond to an on-stack node
      }
      if (descended)
        continue;
      work.back().wi = wi;

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

// One directed BIG path `from -> ... -> to` as its edge clause-ids in FORWARD
// order (LRAT only; the caller guards on k.proof->lrat()). Out-edges of p are
// the binary watches of negate(p): an edge p -> q is the clause (¬p ∨ q), id
// kernel_binary_id(k, negate(p), q). Active literals only, matching the SCC
// nodes. Returns false if no path (never happens for two members of one SCC —
// it is strongly connected — but stays total for direct/off-SCC callers).
//
// Chain order is load-bearing: for the RUP unit {to} the checker assumes the
// path's start literal true (the unit's negation for the from==negate(to)
// case) and unit-propagates the edges in order — edge (¬p ∨ q) with p true
// forces q, so the path forces `to` and closes the conflict. A shuffled chain
// leaves a hint non-unit and the checker rejects.
bool big_path_chain(const Kernel &k, Lit from, Lit to,
                    std::vector<uint64_t> &chain) {
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  std::vector<Lit> parent(nlits, INVALID_LIT);
  std::vector<uint64_t> parent_edge(nlits, 0);
  std::vector<uint8_t> seen(nlits, 0);
  std::vector<Lit> queue;
  queue.push_back(from);
  seen[from] = 1;
  bool found = false;
  for (size_t qi = 0; qi < queue.size() && !found; qi++) {
    const Lit cur = queue[qi];
    const Lit nl = negate(cur); // edges cur -> q are the binary watches of ¬cur
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[nl].offset;
    const uint32_t sz = k.watches.heads[nl].size;
    for (uint32_t i = 0; i < sz;) {
      const Watch w{ws[i]};
      if (!w.binary()) {
        i += 2; // large watch: blocking word + ref word
        continue;
      }
      const Lit q = w.other();
      i += 1;
      if (k.values[q] != 0 || seen[q])
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
  if (!found)
    return false;
  chain.clear();
  for (Lit cur = to; cur != from; cur = parent[cur])
    chain.push_back(parent_edge[cur]);
  std::reverse(chain.begin(), chain.end());
  return true;
}

namespace {

// x ≡ ¬x: emit the refutation (design §3.3). {¬x} is RUP (assume x; the path
// x -> ¬x propagates ¬x; conflict) and {x} likewise via ¬x -> x; the two
// resolve to the empty clause. Uses add_rup — NOT add_derived_unit, which
// emits nothing in DRAT — because these are failed-literal (RUP) units the
// DRAT checker must see, exactly as Probe/Backbone emit theirs. Sets
// k.inconsistent; the empty clause IS the refutation (Vivify/Strengthen's
// Unsat contract). Self-contained: does not lean on M1.
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

// live copies of the binary pair (a ∨ b) in a's watch list.
uint32_t binary_copies(const Kernel &k, Lit a, Lit b) {
  uint32_t n = 0;
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  const uint32_t sz = k.watches.heads[a].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == b)
        n++;
      i += 1;
    } else {
      i += 2;
    }
  }
  return n;
}
inline bool binary_live(const Kernel &k, Lit a, Lit b) {
  return binary_copies(k, a, b) > 0;
}

// The equivalent-literal rewrite (V3b-3). `sub[l]` marks the non-representative
// SCC-member literals to rewrite (l → r.repr[l]); the caller selects them (the
// whole round, or one requested SCC + its mirror). Emits the transient
// equivalence lemmas {¬l, r.repr[l]}, rewrites every large and binary
// occurrence add-before-delete, deletes the lemmas, pushes the reconstruction
// witnesses once per substituted variable, and asserts any collapsed root units
// un-propagated (M1 owns the cascade — the Probe/Vivify batch discipline).
// Returns Unsat on an empty rewrite, Ok if anything changed, NoOp otherwise.
ApplyResult do_substitution(Kernel &k, const SccResult &r,
                            const std::vector<uint8_t> &sub) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  auto substitutable = [&](Lit l) { return l < nlits && sub[l]; };

  // (A) hazard pre-check: a pair with ≥2 live copies cannot be deleted by name
  // (kernel_delete_binary refuses — one id can't name two clauses), so skip the
  // whole round rather than emit a half-applied proof. Rare; the erratum's
  // finer per-SCC skip is a future refinement (deferred slot, correctness-
  // first). Scan only the member binaries we would touch.
  for (Lit l = 0; l < nlits; l++) {
    if (!substitutable(l))
      continue;
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
    const uint32_t sz = k.watches.heads[l].size;
    for (uint32_t i = 0; i < sz;) {
      const Watch w{ws[i]};
      if (!w.binary()) {
        i += 2;
        continue;
      }
      if (binary_copies(k, l, w.other()) >= 2)
        return ApplyResult::NoOp; // duplicated original — unsafe this round
      i += 1;
    }
  }

  // (B) transient equivalence lemmas {¬l, repr[l]}, one per member literal.
  // Derived BEFORE any binary mutation so big_path_chain reads the intact BIG.
  // RUP: assume l, ¬repr[l]; the path l→…→repr[l] propagates repr[l]; conflict.
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

  // Record a size-collapsed root unit `u` (clause {u}, proof id `id`), deduping
  // per variable within the round. Returns true iff `u` CONTRADICTS a unit
  // already derived this round (`¬u` batched) — the formula is then refuted:
  // {u} and {¬u} resolve to ∅, so emit the empty clause and set `inconsistent`
  // (the caller returns Unsat). Without this, a batch that derives both `x` and
  // `¬x` (e.g. an SCC member forced true by one clause and false by another)
  // would assert the first and silently drop the second, losing the conflict.
  auto record_unit = [&](Lit u, uint64_t id) -> bool {
    return pending.record(k, u, id) == PendingUnits::Outcome::Refuted;
  };

  // Rewrite one literal sequence: apply repr, strip root-false literals (their
  // unit ids join `chain`), detect root-true/tautology. Fills `out` with the
  // kept ACTIVE literals and `chain` with the antecedent ids (lemmas + stripped
  // units, WITHOUT the original clause id — the caller appends that). Returns
  // true iff the rewrite is satisfied/tautological (caller then delete-only).
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

  // (C) rewrite LARGE clauses. Snapshot the original refs first (adds append to
  // the arena). Follow Vivify's ordering — add_rup, del, GARBAGE all read the
  // clause BEFORE the arena-mutating add — so the Clause* never dangles.
  std::vector<Ref> refs;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage())
      refs.push_back(ref);
    ref += clause_words(c->size);
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
      if (k.proof)
        k.proof->del(k.next_clause_id, cid, c->lits, c->size);
      kernel_mark_garbage(k, c);
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
    switch (kernel_replace_clause(
        k, c, id, out.data(), static_cast<uint32_t>(out.size()), credundant,
        credundant ? std::min<uint32_t>(cglue, out.size()) : 0, nullptr)) {
    case ReplaceOutcome::Refuted: // all-false rewrite (unreachable at a
      return ApplyResult::Unsat;  // quiescent root)
    case ReplaceOutcome::Unit:
      if (record_unit(out[0], id))
        return ApplyResult::Unsat;
      break;
    default:
      break;
    }
  }

  // (D) rewrite BINARY clauses. Snapshot the member pairs first (rewriting
  // mutates watch lists); each pair collected once via its canonical key.
  std::vector<std::pair<Lit, Lit>> bins;
  std::set<uint64_t> seen_pair;
  for (Lit l = 0; l < nlits; l++) {
    if (!substitutable(l))
      continue;
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
    const uint32_t sz = k.watches.heads[l].size;
    for (uint32_t i = 0; i < sz;) {
      const Watch w{ws[i]};
      if (!w.binary()) {
        i += 2;
        continue;
      }
      const Lit z = w.other();
      if (seen_pair.insert(kernel_binary_key(l, z)).second)
        bins.emplace_back(l, z);
      i += 1;
    }
  }
  for (const auto &pr : bins) {
    const Lit l = pr.first, z = pr.second;
    if (!binary_live(k, l, z))
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
    if (out.size() == 2 && binary_live(k, out[0], out[1])) {
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

  // (F) reconstruction witnesses, once per substituted variable (both
  // polarities are non-representative, so the positive literal detects it):
  // the order-independent symmetric pair for l ≡ s = repr[l] —
  // {¬l, s} (witness ¬l) and {l, ¬s} (witness l) — so kernel_extend_model
  // recovers l's value from s regardless of any search assignment to l.
  for (Var v = 0; v < k.nvars; v++) {
    const Lit l = make_lit(v, false);
    if (!substitutable(l))
      continue;
    const Lit s = r.repr[l];
    const Lit rest_a[1] = {s};
    kernel_push_witness(k, negate(l), rest_a, 1); // clause {s, ¬l}
    const Lit rest_b[1] = {negate(s)};
    kernel_push_witness(k, l, rest_b, 1); // clause {¬s, l}
  }

  // (G) assert the collapsed root units un-propagated; M1 owns the cascade and
  // emits nothing new (each unit already carries its add-RUP id).
  for (const Lit u : pending.units)
    if (k.value(u) == 0)
      kernel_assign_unit(k, u);

  if (changed)
    kernel_collect_garbage(k); // reclaim the garbaged large clauses
  return changed ? ApplyResult::Ok : ApplyResult::NoOp;
}

} // namespace

// canonical instance: one full round. Detect SCCs; on x ≡ ¬x refute; else
// substitute every non-trivial SCC onto its representative (V3b-3). Runs only
// at a quiescent root (guarded).
ApplyResult substitute_apply(Kernel &k) {
  assert(substitute_guard(k));
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k);
  if (r.contradiction) {
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  if (r.nontrivial_reps.empty())
    return ApplyResult::NoOp;
  // every non-representative member of a non-trivial SCC is substitutable
  std::vector<uint8_t> sub(2 * k.nvars, 0);
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    if (r.repr[l] != INVALID_LIT && r.repr[l] != l)
      sub[l] = 1;
  return do_substitution(k, r, sub);
}

// instance surface: payload = an SCC representative literal (lit_instance /
// lit_of_instance, rev 34); INSTANCE_CANONICAL = the full round. A single
// instance substitutes exactly the requested equivalence class — the SCC with
// representative `rep` AND its mirror SCC (representative ¬rep), so both
// polarities of every member variable are eliminated together. Total for every
// payload.
static uint32_t substitute_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  SccResult r = substitute_find_sccs(k);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(r.nontrivial_reps.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(r.nontrivial_reps[i]);
  return n;
}

static ApplyResult substitute_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return substitute_apply(k);
  Lit rep;
  if (!lit_of_instance(inst, &rep))
    return ApplyResult::NoOp;
  if (var_of(rep) >= k.nvars || k.values[rep] != 0)
    return ApplyResult::NoOp;
  // re-derive from the CURRENT BIG (it may have changed since instances()).
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k);
  if (r.contradiction) { // a global x ≡ ¬x refutes regardless of the request
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  // substitute only the requested class: members whose representative is rep or
  // ¬rep (the mirror). If the requested SCC dissolved, nothing is marked → NoOp.
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

// V3a: SCC detection over the binary implication graph (guard + iterative
// Tarjan). Transliterated from kissat/src/decompose.c + substitute.c and
// cadical/src/decompose.cpp (iterative Tarjan; MIT) — deliberately NOT
// CryptoMiniSat's recursive sccfinder with its max_scc_depth early-return,
// which silently drops deep SCCs (a completeness loss). An explicit work
// stack survives arbitrarily deep implication chains.
#include "rules/substitute/substitute.hpp"

#include "kernel/watch.hpp"
#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

bool substitute_guard(const Kernel &k) {
  // quiescent root (C1/C2/C3/V1/V2's argument)
  return !k.inconsistent && !k.conflict_valid &&
         k.level == 0 && k.propagate_head == k.trail.size();
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

namespace {

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

} // namespace

// canonical instance: one full round. V3b-1 detects and, on x ≡ ¬x, refutes;
// a consistent formula is a NoOp (the equivalent-literal rewrite lands in
// V3b-3). Runs only at a quiescent root (guarded).
ApplyResult substitute_apply(Kernel &k) {
  assert(substitute_guard(k));
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k);
  if (r.contradiction) {
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  return ApplyResult::NoOp; // no rewrite until V3b-3
}

// instance surface: payload = an SCC representative literal (lit_instance /
// lit_of_instance, rev 34); INSTANCE_CANONICAL = the full round. Until the
// rewrite lands, a single instance can still surface a global UNSAT (the whole
// formula is refuted regardless of which SCC was asked for); a consistent
// formula is a NoOp. Total for every payload.
static uint32_t substitute_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  SccResult r = substitute_find_sccs(k);
  const uint32_t n =
      std::min<uint32_t>(cap, static_cast<uint32_t>(r.nontrivial_reps.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(r.nontrivial_reps[i]);
  return n;
}

static ApplyResult substitute_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return substitute_apply(k);
  Lit rep;
  if (!lit_of_instance(inst, &rep))
    return ApplyResult::NoOp;
  if (var_of(rep) >= k.nvars || k.values[rep] != 0)
    return ApplyResult::NoOp;
  // re-derive from the CURRENT BIG (it may have changed since instances()).
  // A global x ≡ ¬x refutes the whole formula whichever SCC was requested;
  // otherwise V3b-1 has no per-SCC rewrite to do yet -> NoOp.
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k);
  if (r.contradiction) {
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  return ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Substitute, RuleTier::Inprocess, substitute_guard,
                     substitute_apply, substitute_instances,
                     substitute_apply_instance);

} // namespace tactix

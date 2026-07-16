// V3a: SCC detection over the binary implication graph (guard + iterative
// Tarjan). Transliterated from kissat/src/decompose.c + substitute.c and
// cadical/src/decompose.cpp (iterative Tarjan; MIT) — deliberately NOT
// CryptoMiniSat's recursive sccfinder with its max_scc_depth early-return,
// which silently drops deep SCCs (a completeness loss). An explicit work
// stack survives arbitrarily deep implication chains.
#include "rules/substitute/substitute.hpp"

#include "kernel/watch.hpp"

#include <algorithm>

namespace tactix {

bool substitute_guard(const Kernel &k) {
  // quiescent root (C1/C2/C3/V1/V2's argument)
  return !k.inconsistent && !k.conflict_valid && !k.pending.valid &&
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

} // namespace tactix

// See transitive_reduce.hpp for the reference/deviation record. V4 deletes
// transitively-redundant binaries; the walk is read-only (the V3a idiom) and
// every mutation goes through kernel_delete_binary, which owns the proof
// delete and the root-reason retirement.
#include "rules/transitive_reduce/transitive_reduce.hpp"

#include "rules/propagate/propagate.hpp"

#include "kernel/watch.hpp"

#include <algorithm>

namespace tactix {

bool transitive_reduce_guard(const Kernel &k) {
  // quiescent root (C1/C2/C3/V1/V2/V3's argument)
  // D-SPLIT-3 (guard purification): the ¬conflict_valid conjunct is gone —
  // this is a pure F-rewrite (sound regardless of a recorded conflict), and
  // the conjunct was redundant anyway (conflict_valid ⟹ level>0 at any rule
  // boundary, so it never fired under the retained level==0). calculus §7.
  return !k.inconsistent && k.level == 0;
}

namespace {

// Out-edges of p (implications p → q) are the binary watches of negate(p): a
// binary clause (negate(p) ∨ q) is exactly the implication p → q (the V3a
// read-only walk idiom, substitute.cpp:42-49). Nothing pushes to the watch
// arena during a walk, so these pointers stay valid for its whole duration —
// kernel_delete_binary only ever runs between walks, and compacts in place.
inline const uint32_t *wbase(const Kernel &k, Lit l) {
  return k.watches.arena.data() + k.watches.heads[l].offset;
}
inline uint32_t wsize(const Kernel &k, Lit l) {
  return k.watches.heads[l].size;
}

// per-round scratch (mirrors vivify's local buffers — nothing rule-scoped
// leaks into the kernel). `dead` is indexed by the OTHER endpoint x and means
// "the pair (src ∨ x) is excluded from every walk": that single flag serves
// both as the candidate's self-exclusion and as the round-local tombstone.
struct Walk {
  std::vector<uint8_t> reached; // by lit
  std::vector<Lit> touched;     // the marked lits, for O(marked) clearing
  std::vector<Lit> queue;       // BFS frontier
  std::vector<uint8_t> dead;    // by lit x: pair (src ∨ x) excluded
  std::vector<Lit> dead_list;   // the tombstoned x's of the current source
  std::vector<uint8_t> copies;  // by lit x: live copies of (src ∨ x), capped
  std::vector<Lit> cands;       // the current source's canonical candidates
  uint64_t ticks = 0;           // cache lines scanned; reduce_source flushes
                                // into stats.transitive_ticks (const-walk
                                // helpers charge here instead of the kernel)

  void init(const Kernel &k) {
    const size_t nlits = 2 * static_cast<size_t>(k.nvars);
    reached.assign(nlits, 0);
    dead.assign(nlits, 0);
    copies.assign(nlits, 0);
  }
};

// Reachability walk from ¬src over the binary implication graph, skipping
// every edge whose CLAUSE is one of this source's excluded (src ∨ x) pairs —
// in either direction, which is what keeps a candidate out of its own
// justifying path (hpp deviation 2). Read-only: never touches trail/values/
// levels. Returns true iff dst is reached; sets *failed when some literal is
// reached together with its negation, i.e. ¬src ⊨ q ∧ ¬q, so ¬src is a failed
// literal and src a forced unit (deferred to V1 Probe — hpp deviation 5).
bool walk_reaches(const Kernel &k, Walk &w, Lit src, Lit dst, bool *failed) {
  *failed = false;
  for (const Lit l : w.touched)
    w.reached[l] = 0;
  w.touched.clear();
  w.queue.clear();

  const Lit root = negate(src);
  w.reached[root] = 1;
  w.touched.push_back(root);
  w.queue.push_back(root);
  bool found = false;
  for (size_t qi = 0; qi < w.queue.size(); qi++) {
    const Lit l = w.queue[qi];
    const Lit nl = negate(l); // edges l → q are the binary watches of ¬l
    const uint32_t *ws = wbase(k, nl);
    const uint32_t sz = wsize(k, nl);
    // the walk's real memory traffic, in propagate's cache-line unit
    // (kernel.cpp:536): this scan is the whole cost of visiting `l`
    w.ticks += 1 + (static_cast<uint64_t>(sz) >> 4);
    for (uint32_t i = 0; i < sz;) {
      const Watch wt{ws[i]};
      if (!wt.binary()) {
        i += 2; // large watch: blocking word + ref word
        continue;
      }
      const Lit q = wt.other();
      i += 1;
      // this edge's clause is (nl ∨ q): excluded iff it is a dead (src ∨ x)
      // pair. Catches the candidate edge ¬src → dst at the root AND its
      // contrapositive ¬dst → src wherever it turns up
      if ((nl == src && w.dead[q]) || (q == src && w.dead[nl]))
        continue;
      // root-fixed literals are not nodes. At an engine fixpoint an active
      // node has no edge to a root-FALSE literal (that edge would have forced
      // the node false), and every successor of a root-TRUE literal is itself
      // root-true — so no path to an active dst is lost. Skipping is in any
      // case conservative: fewer paths found, never more
      if (k.values[q] != 0)
        continue;
      if (w.reached[q])
        continue;
      w.reached[q] = 1;
      w.touched.push_back(q);
      if (w.reached[negate(q)]) {
        *failed = true; // ¬src implies both polarities of q
        return false;
      }
      if (q == dst) {
        found = true; // deviation 3: normally NO early exit — the walk runs to
                      // exhaustion so failed-literal detection sees everything
        if (k.transitive_early_exit)
          return true; // kissat transitive.c:130; forfeits this walk's units
      }
      w.queue.push_back(q);
    }
  }
  return found;
}

// Collect one source's canonical candidates into w.cands (ascending) and
// return ¬src's binary out-degree. Leaves w.copies clear.
uint32_t collect_candidates(const Kernel &k, Walk &w, Lit src) {
  w.cands.clear();
  uint32_t out_degree = 0;
  // watches[src] holds (src ∨ q) for every out-edge ¬src → q, so its binary
  // count IS ¬src's out-degree
  const uint32_t *ws = wbase(k, src);
  const uint32_t sz = wsize(k, src);
  w.ticks += 1 + (static_cast<uint64_t>(sz) >> 4); // src-list scan traffic
  for (uint32_t i = 0; i < sz;) {
    const Watch wt{ws[i]};
    if (!wt.binary()) {
      i += 2;
      continue;
    }
    const Lit dst = wt.other();
    i += 1;
    out_degree++;
    // dst > src tests each undirected clause from exactly one endpoint (the
    // BIG holds an implication and its contrapositive, so both endpoints'
    // tests are equivalent — kissat transitive.c:101 dedups the same way). A
    // root-fixed dst means (src ∨ dst) is root-satisfied: C2's flush owns it
    if (dst <= src || k.values[dst] != 0)
      continue;
    if (w.copies[dst] == 0)
      w.cands.push_back(dst);
    if (w.copies[dst] < 255)
      w.copies[dst]++;
  }
  // drop duplicated pairs (hpp deviation 8: binary_ids names a PAIR, so one
  // copy of a duplicate cannot be deleted without stranding the other on a
  // retired id) and clear w.copies in the same pass
  size_t n = 0;
  for (size_t i = 0; i < w.cands.size(); i++) {
    const Lit dst = w.cands[i];
    const bool unique = w.copies[dst] == 1;
    w.copies[dst] = 0;
    if (unique)
      w.cands[n++] = dst; // in-place: n <= i always
  }
  w.cands.resize(n);
  // canonical order = ascending literal code, so the round is a function of
  // the GRAPH, not of watch-list layout (the tombstone makes which edge of a
  // mutually-redundant pair survives order-dependent)
  std::sort(w.cands.begin(), w.cands.end());
  return out_degree;
}

// one source: walk each candidate, tombstone the redundant ones, then delete.
// Preconditions: guard holds, src is in range and active. Leaves w clean.
ApplyResult reduce_source(Kernel &k, Lit src, Walk &w) {
  const uint32_t out_degree = collect_candidates(k, w, src);
  if (out_degree < 2 || w.cands.empty()) {
    k.stats.transitive_ticks += w.ticks;
    w.ticks = 0;
    return ApplyResult::NoOp; // a lone out-edge has no alternate path
  }

  for (const Lit dst : w.cands) {
    k.stats.transitive_checked++;
    w.dead[dst] = 1; // the candidate is excluded from its own path
    bool failed = false;
    const bool redundant = walk_reaches(k, w, src, dst, &failed);
    if (failed) {
      // ¬src is a failed literal: V1 Probe owns the unit and its chain, and
      // once src is fixed C2's flush removes these binaries anyway. Abort the
      // source — nothing has been deleted yet, only tombstoned
      k.stats.transitive_failed++;
      w.dead[dst] = 0;
      for (const Lit d : w.dead_list)
        w.dead[d] = 0;
      w.dead_list.clear();
      k.stats.transitive_ticks += w.ticks;
      w.ticks = 0;
      return ApplyResult::NoOp;
    }
    if (redundant)
      w.dead_list.push_back(dst); // tombstone stays: later walks skip it
    else
      w.dead[dst] = 0; // survives: restore the edge for later walks
  }

  // mutate only now: kernel_delete_binary compacts watches[src], the very
  // list the walks above read
  ApplyResult res = ApplyResult::NoOp;
  for (const Lit dst : w.dead_list) {
    if (kernel_delete_binary(k, src, dst)) {
      k.stats.transitive_deleted++;
      res = ApplyResult::Ok;
    }
    w.dead[dst] = 0;
  }
  w.dead_list.clear();
  k.stats.transitive_ticks += w.ticks;
  w.ticks = 0;
  return res;
}

} // namespace

void transitive_reduce_sources(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  for (Lit src = 0; src < nlits; src++) {
    if (k.values[src] != 0)
      continue; // root-fixed: (src ∨ ·) is satisfied or already propagated
    const uint32_t *ws = wbase(k, src);
    const uint32_t sz = wsize(k, src);
    uint32_t out_degree = 0;
    bool canonical = false;
    for (uint32_t i = 0; i < sz;) {
      const Watch wt{ws[i]};
      if (!wt.binary()) {
        i += 2;
        continue;
      }
      const Lit dst = wt.other();
      i += 1;
      out_degree++;
      if (dst > src && k.values[dst] == 0)
        canonical = true;
    }
    if (out_degree >= 2 && canonical)
      out.push_back(src);
  }
}

ApplyResult transitive_reduce_try(Kernel &k, Lit src) {
  assert(transitive_reduce_guard(k));
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (var_of(src) >= k.nvars || k.values[src] != 0)
    return ApplyResult::NoOp; // out of range / root-fixed
  // Persistent per-instance scratch. A fresh Walk here costs three
  // O(2·nvars) zero-fills + three allocations PER DOSED CANDIDATE, which is
  // several percent of a whole solve. Reuse is behaviour-identical because
  // every Walk array is invariant-clean between calls
  // (`dead`/`copies` restored on exit, `reached` cleared at walk entry via
  // `touched`); re-init only when nvars grew (Factor/BVE extension vars).
  // thread_local keeps the scratch rule-scoped (nothing leaks into the
  // kernel) and is safe under the plan's sequential-solving contract.
  static thread_local Walk w;
  if (w.reached.size() != 2 * static_cast<size_t>(k.nvars))
    w.init(k);
  return reduce_source(k, src, w);
}

// canonical instance: one full round over every source literal
ApplyResult transitive_reduce_apply(Kernel &k) {
  assert(transitive_reduce_guard(k));
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (a no-op otherwise); a root conflict during the drain refutes.
  // `transitive_reduce_try` and `_apply_instance` have always done this; the
  // canonical `apply` was the ONE inprocessing entry point of eleven that did
  // not, and it is reachable undrained through the public API (`tactix_apply`)
  // and the slot. V4 is sound either way — the walk skips assigned literals, so
  // an undrained root finds FEWER paths and deletes less, never more — but a
  // policy author reasons from the blanket "every inprocessing rule drains at
  // entry" claim, so the exception is closed rather than documented.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.transitive_rounds++;
  std::vector<Lit> sources;
  transitive_reduce_sources(k, sources);
  if (sources.empty())
    return ApplyResult::NoOp;
  Walk w;
  w.init(k);
  ApplyResult res = ApplyResult::NoOp;
  for (const Lit src : sources) {
    // a source can go stale as earlier sources delete edges (never assigned:
    // V4 changes no values); reduce_source re-derives everything it needs
    if (reduce_source(k, src, w) == ApplyResult::Ok)
      res = ApplyResult::Ok;
  }
  return res;
}

// instance surface: payload = source literal, the shared lit_instance encoding
// (like Decide / V1 Probe / V2 Backbone); INSTANCE_CANONICAL = the full round
static uint32_t transitive_reduce_instances(Kernel &k, uint64_t *out,
                                            uint32_t cap) {
  std::vector<Lit> srcs;
  transitive_reduce_sources(k, srcs);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(srcs.size()));
  // The set, in the rule's own order (ascending by source literal). WHERE a
  // caller starts in it is that caller's business: SCHED-B3's resumption
  // cursor lived here and moved to the POLICY in SCHED-E1
  // (DefaultPolicy::SlotEntry::cursor / order_candidates), which is where a
  // scheduling decision belongs and where every dosed entry can share it.
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(srcs[i]);
  return n;
}

static ApplyResult transitive_reduce_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL guard purification: complete the root closure U if applied off a
  // fixpoint (no-op otherwise). A root conflict during the drain refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return transitive_reduce_apply(k);
  Lit src;
  if (!lit_of_instance(inst, &src))
    return ApplyResult::NoOp;
  return transitive_reduce_try(k, src);
}

TACTIX_REGISTER_RULE(TransitiveReduce, RuleTier::Inprocess,
                     transitive_reduce_guard, transitive_reduce_apply,
                     transitive_reduce_instances,
                     transitive_reduce_apply_instance);

} // namespace tactix

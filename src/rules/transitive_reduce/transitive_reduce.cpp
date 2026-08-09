// See transitive_reduce.hpp for the rule's contract. The walk is read-only and
// every mutation goes through kernel_delete_binary, which owns the proof delete
// and the root-reason retirement.
#include "rules/transitive_reduce/transitive_reduce.hpp"

#include "rules/propagate/propagate.hpp"

#include "kernel/watch.hpp"

#include <algorithm>

namespace tactix {

bool transitive_reduce_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/calculus.md` §7).
  return !k.inconsistent && k.level == 0;
}

namespace {

// Out-edges of p are the binary watches of negate(p): (negate(p) ∨ q) is
// exactly p → q. Nothing pushes to the watch arena during a walk, so an entries
// range stays valid throughout — kernel_delete_binary only ever runs between
// walks, and compacts in place.

// Per-round scratch; nothing rule-scoped leaks into the kernel. `dead` is
// indexed by the other endpoint x and means "the pair (src ∨ x) is excluded
// from every walk": one flag serving both as the candidate's self-exclusion and
// as the round-local tombstone.
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

// Reachability walk from ¬src over the binary implication graph, skipping every
// edge whose clause is one of this source's excluded (src ∨ x) pairs, in either
// direction — that is what keeps a candidate out of its own justifying path.
// Read-only. Returns true iff dst is reached; sets *failed when some literal is
// reached with its negation, i.e. ¬src is a failed literal, deferred to V1.
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
    // the walk's real memory traffic, in propagate's cache-line unit: this
    // scan is the whole cost of visiting `l`
    w.ticks += 1 + (static_cast<uint64_t>(k.watches.size(nl)) >> 4);
    for (const WatchEntry wt : k.watches.entries(nl)) {
      if (!wt.binary())
        continue;
      const Lit q = wt.other();
      // this edge's clause is (nl ∨ q): excluded iff it is a dead (src ∨ x)
      // pair. Catches the candidate edge ¬src → dst at the root AND its
      // contrapositive ¬dst → src wherever it turns up
      if ((nl == src && w.dead[q]) || (q == src && w.dead[nl]))
        continue;
      // root-fixed literals are not nodes. At an engine fixpoint an active node
      // has no edge to a root-false literal, and every successor of a root-true
      // literal is itself root-true, so no path to an active dst is lost;
      // skipping is conservative anyway — fewer paths found, never more
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
        found = true; // normally no early exit: the walk runs to exhaustion so
                      // failed-literal detection sees the whole reachable set
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
  w.ticks += // src-list scan traffic
      1 + (static_cast<uint64_t>(k.watches.size(src)) >> 4);
  for (const WatchEntry wt : k.watches.entries(src)) {
    if (!wt.binary())
      continue;
    const Lit dst = wt.other();
    out_degree++;
    // dst > src tests each undirected clause from one endpoint only; both
    // endpoints' tests are equivalent (kissat transitive.c:101 dedups the same
    // way). A root-fixed dst means (src ∨ dst) is satisfied: C2's flush owns it
    if (dst <= src || k.values[dst] != 0)
      continue;
    if (w.copies[dst] == 0)
      w.cands.push_back(dst);
    if (w.copies[dst] < 255)
      w.copies[dst]++;
  }
  // drop duplicated pairs (binary_ids names a literal pair, so one copy of a
  // duplicate cannot be deleted without stranding the other on a retired id)
  // and clear w.copies in the same pass
  size_t n = 0;
  for (size_t i = 0; i < w.cands.size(); i++) {
    const Lit dst = w.cands[i];
    const bool unique = w.copies[dst] == 1;
    w.copies[dst] = 0;
    if (unique)
      w.cands[n++] = dst; // in-place: n <= i always
  }
  w.cands.resize(n);
  // canonical order = ascending literal code, so the round is a function of the
  // graph, not of watch-list layout — the tombstone makes which edge of a
  // mutually-redundant pair survives order-dependent
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
    // Poll per dst walk, as kissat does (transitive.c:181-183): a source's
    // candidate set is unbounded, so a poll only between sources leaves both
    // the dose and an interrupt unobservable for the length of one source.
    // Breaking here is sound — nothing is deleted until the loop below, and
    // each tombstone was established against the graph minus the earlier ones,
    // so a prefix of `dead_list` is exactly the result of a shorter round.
    k.stats.transitive_polls++;
    if (kernel_terminating(k)) {
      k.stats.transitive_terminated++;
      break;
    }
    k.stats.transitive_checked++;
    w.dead[dst] = 1; // the candidate is excluded from its own path
    bool failed = false;
    const bool redundant = walk_reaches(k, w, src, dst, &failed);
    // Credit in flight so the policy's between-instance meter check sees this
    // source's spend as it happens rather than after it is over.
    if (k.transitive_live_meter) {
      k.stats.transitive_ticks += w.ticks;
      w.ticks = 0;
    }
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
    uint32_t out_degree = 0;
    bool canonical = false;
    for (const WatchEntry wt : k.watches.entries(src)) {
      if (!wt.binary())
        continue;
      const Lit dst = wt.other();
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
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (var_of(src) >= k.nvars || k.values[src] != 0)
    return ApplyResult::NoOp; // out of range / root-fixed
  // Persistent per-instance scratch: a fresh Walk costs three O(2·nvars)
  // zero-fills per dosed candidate, the per-call linear-scratch hazard. Reuse
  // is behaviour-identical because every Walk array is invariant-clean between
  // calls (`dead`/`copies` restored on exit, `reached` cleared at walk entry);
  // re-init only when nvars grew. thread_local keeps the scratch rule-scoped
  // and is safe under the plan's sequential-solving contract.
  static thread_local Walk w;
  if (w.reached.size() != 2 * static_cast<size_t>(k.nvars))
    w.init(k);
  return reduce_source(k, src, w);
}

// canonical instance: one full round over every source literal
ApplyResult transitive_reduce_apply(Kernel &k) {
  assert(transitive_reduce_guard(k));
  // D-LEVEL: drain the root closure U (no-op at a fixpoint); a conflict
  // refutes. Every inprocessing entry point drains — the rule is sound either
  // way, since an undrained root finds fewer paths, but a policy author reasons
  // from the blanket claim.
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

// instance surface: payload = source literal, the shared lit_instance encoding;
// INSTANCE_CANONICAL = the full round
static uint32_t transitive_reduce_instances(Kernel &k, uint64_t *out,
                                            uint32_t cap) {
  std::vector<Lit> srcs;
  transitive_reduce_sources(k, srcs);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(srcs.size()));
  // The set, in the rule's own order. Where a caller starts in it is that
  // caller's business: the resumption cursor lives in the policy
  // (DefaultPolicy::SlotEntry::cursor), where a scheduling decision belongs.
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(k, srcs[i]);
  return n;
}

static ApplyResult transitive_reduce_apply_instance(Kernel &k, uint64_t inst) {
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (inst == INSTANCE_CANONICAL)
    return transitive_reduce_apply(k);
  Lit src;
  if (!lit_of_instance(k, inst, &src))
    return ApplyResult::NoOp;
  return transitive_reduce_try(k, src);
}

TACTIX_REGISTER_RULE(TransitiveReduce, RuleTier::Inprocess,
                     transitive_reduce_guard, transitive_reduce_apply,
                     transitive_reduce_instances,
                     transitive_reduce_apply_instance);

} // namespace tactix

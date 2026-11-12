// See transitive_reduce.hpp for the contract. Walks are read-only; mutations go
// through kernel_delete_binary (proof delete and root-reason retirement).
#include "rules/transitive_reduce/transitive_reduce.hpp"

#include "proof/proof.hpp"
#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"

#include "kernel/watch.hpp"

#include <algorithm>
#include <utility>

namespace tactix {

RuleEligibility transitive_reduce_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/dev/calculus.md` §7).
  return root_level_guard(k);
}

namespace {

// Out-edges of p are the binary watches of ¬p. Deletions run only between
// walks, so entries ranges stay valid.

// Per-round scratch. `dead[x]` excludes the pair (src ∨ x) from every walk: the
// candidate's self-exclusion and the round's tombstone.
struct Walk {
  std::vector<uint8_t> reached; // by lit
  std::vector<Lit> touched;     // the marked lits, for O(marked) clearing
  std::vector<Lit> queue;       // BFS frontier
  std::vector<uint32_t> dead;   // by lit x: excluded copies of (src ∨ x)
  std::vector<Lit> dead_list;   // one entry per tombstoned physical copy
  std::vector<uint32_t> seen;   // per-walk source-copy ordinal by destination
  std::vector<Lit> seen_list;   // touched entries of `seen`
  std::vector<Lit> cands;       // physical source-watch candidate order
  uint64_t ticks = 0;           // cache lines scanned; reduce_source flushes
                                // into stats.transitive_ticks (const-walk
                                // helpers charge here instead of the kernel)

  // Arrays and their marked-entry lists form one state, re-initialised whenever
  // `nvars` changes (VariableCompaction).
  void init(const Kernel &k) {
    const size_t nlits = 2 * static_cast<size_t>(k.nvars);
    reached.assign(nlits, 0);
    dead.assign(nlits, 0);
    seen.assign(nlits, 0);
    touched.clear();
    queue.clear();
    dead_list.clear();
    seen_list.clear();
    cands.clear();
  }
};

// Kissat charges `1 + ceil(raw watch words / 32)` per opened list
// (`kissat_cache_lines(n,sizeof(watch))`); binaries are one word, large watches
// two.
uint64_t transitive_watch_ticks(const Kernel &k, Lit lit) {
  const uint32_t words = k.watches.size(lit);
  return 1 + ((static_cast<uint64_t>(words) + 31) >> 5);
}

// Reachability from ¬src over the binary graph, skipping the excluded
// (src ∨ x) pairs in either direction. Returns true iff dst is reached; sets
// *failed when a literal and its negation are reached (Kissat commits {src}).
bool walk_reaches(const Kernel &k, Walk &w, Lit src, Lit dst, bool *failed) {
  *failed = false;
  for (const Lit l : w.touched)
    w.reached[l] = 0;
  w.touched.clear();
  w.queue.clear();
  for (const Lit lit : w.seen_list)
    w.seen[lit] = 0;
  w.seen_list.clear();

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
    w.ticks += transitive_watch_ticks(k, nl);
    uint32_t seen_mirror_copies = 0;
    for (const WatchEntry wt : k.watches.entries(nl)) {
      if (!wt.binary())
        continue;
      const Lit q = wt.other();
      // `dead` counts prior tombstones plus the current occurrence (Kissat
      // excludes the current slot): all are absent on the source direction,
      // only the prior ones on the mirror.
      if (nl == src) {
        uint32_t &seen = w.seen[q];
        if (!seen)
          w.seen_list.push_back(q);
        if (seen++ < w.dead[q])
          continue;
      }
      if (q == src) {
        const uint32_t current = nl == dst ? 1 : 0;
        assert(w.dead[nl] >= current);
        if (seen_mirror_copies++ < w.dead[nl] - current)
          continue;
      }
      // Root-fixed literals are not nodes; at a fixpoint no path to an active
      // dst is lost, and skipping only finds fewer paths.
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

// Collect one source's candidates in physical watch order; duplicate copies
// are independent (Kissat).
void collect_candidates(const Kernel &k, Walk &w, Lit src) {
  w.cands.clear();
  w.ticks += transitive_watch_ticks(k, src); // src-list scan traffic
  for (const WatchEntry wt : k.watches.entries(src)) {
    if (!wt.binary())
      continue;
    const Lit dst = wt.other();
    // Kissat tests each clause from one endpoint by compact literal order; use
    // the compact-position bridge. A root-fixed dst is C2's flush.
    const bool owned = kernel_compact_import_lit_order(k, dst) >
                       kernel_compact_import_lit_order(k, src);
    if (!owned || k.values[dst] != 0)
      continue;
    w.cands.push_back(dst);
  }
}

// one source: walk each candidate, tombstone the redundant ones, then delete.
// Preconditions: guard holds, src is in range and active. Leaves w clean.
ApplyResult reduce_source(Kernel &k, Lit src, Walk &w) {
  const uint64_t source_ticks_before = k.stats.transitive_ticks + w.ticks;
  const uint64_t source_checked_before = k.stats.transitive_checked;
  collect_candidates(k, w, src);
  if (w.cands.empty()) {
    k.stats.transitive_ticks += w.ticks;
    w.ticks = 0;
    return ApplyResult::NoOp;
  }

  bool failed_source = false;
  for (const Lit dst : w.cands) {
    // Poll per dst walk (kissat transitive.c:181-183). Breaking is sound:
    // nothing is deleted before the loop below, and a prefix of `dead_list` is
    // a shorter round.
    k.stats.transitive_polls++;
    if (kernel_terminating(k)) {
      k.stats.transitive_terminated++;
      break;
    }
    k.stats.transitive_checked++;
    w.dead[dst]++; // exclude this physical candidate occurrence
    bool failed = false;
    const bool redundant = walk_reaches(k, w, src, dst, &failed);
    // Credit in flight so the policy's between-instance meter check sees this
    // source's spend as it happens rather than after it is over.
    if (k.transitive_live_meter) {
      k.stats.transitive_ticks += w.ticks;
      w.ticks = 0;
    }
    if (failed) {
      // Kissat keeps earlier deletions from this source, then learns {src};
      // release only the current candidate's exclusion.
      k.stats.transitive_units++;
      w.dead[dst]--;
      failed_source = true;
      break;
    }
    if (redundant)
      w.dead_list.push_back(dst); // this copy stays tombstoned for later walks
    else
      w.dead[dst]--; // this copy survives: restore it for later walks
    // Kissat tests its absolute effort limit after each charged candidate
    // (`transitive.c`); DefaultPolicy passes the residue via `work_budget`.
    const uint64_t budget_spent =
        k.transitive_budget_checked
            ? k.stats.transitive_checked - source_checked_before
            : k.stats.transitive_ticks + w.ticks - source_ticks_before;
    if (kernel_work_budget_exceeded(k, budget_spent))
      break;
  }

  // mutate only now: kernel_delete_binary compacts watches[src], the very
  // list the walks above read
  ApplyResult res = ApplyResult::NoOp;
  for (const Lit dst : w.dead_list) {
    if (kernel_delete_binary_one_copy(k, src, dst)) {
      k.stats.transitive_deleted++;
      res = ApplyResult::Ok;
    }
    w.dead[dst]--;
  }
  w.dead_list.clear();
  k.stats.transitive_ticks += w.ticks;
  w.ticks = 0;
  if (failed_source) {
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, &src, 1);
    k.unit_ids[var_of(src)] = id;
    kernel_assign_unit(k, src);
    res = ApplyResult::Ok;
  }
  return res;
}

} // namespace

void transitive_reduce_sources(const Kernel &k, std::vector<Lit> &out) {
  out.clear();
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  for (Lit src = 0; src < nlits; src++) {
    const Var v = var_of(src);
    if (k.values[src] == 0 && !kernel_var_retired(k, v))
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
  // Persistent scratch, invariant-clean between calls; re-init when nvars grew.
  static thread_local Walk w;
  if (w.reached.size() != 2 * static_cast<size_t>(k.nvars))
    w.init(k);
  return reduce_source(k, src, w);
}

// canonical instance: one full round over every source literal
ApplyResult transitive_reduce_apply(Kernel &k) {
  assert(transitive_reduce_guard(k));
  // D-LEVEL: drain the root closure; a conflict refutes.
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
    // Earlier sources can fix variables, as in transitive.c after a failed
    // walk.
    if (k.values[src] != 0 || kernel_var_retired(k, var_of(src)))
      continue;
    if (reduce_source(k, src, w) == ApplyResult::Ok) {
      res = ApplyResult::Ok;
      if (inprocess_reach_root(k))
        return ApplyResult::Unsat;
    }
  }
  return res;
}

// Instance surface: payload = source literal (lit_instance);
// INSTANCE_CANONICAL = the full round. The resume cursor is the policy's
// (DefaultPolicy::SlotEntry::cursor).
TACTIX_REGISTER_RULE(TransitiveReduce, RuleTier::Inprocess,
                     transitive_reduce_guard, transitive_reduce_apply,
                     literal_list_instances<transitive_reduce_sources>,
                     (apply_root_literal_instance<transitive_reduce_apply,
                                                  transitive_reduce_try>));

} // namespace tactix

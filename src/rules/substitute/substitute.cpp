// See substitute.hpp for the rule's contract. The SCC pass is transliterated
// from kissat decompose.c/substitute.c and cadical decompose.cpp (iterative
// Tarjan), deliberately not CryptoMiniSat's recursive sccfinder: its
// max_scc_depth early-return silently drops deep SCCs, a completeness loss,
// whereas an explicit work stack survives arbitrarily deep chains.
#include "rules/substitute/substitute.hpp"

#include "diagnostics/event_trace.hpp"
#include "diagnostics/rule_journal.hpp"
#include "rules/propagate/propagate.hpp"

#include "kernel/watch.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#ifdef TACTIX_ENABLE_EVENT_TRACE
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace tactix {

static const bool substitute_time =
    std::getenv("TACTIX_SUBSTITUTE_TIME") != nullptr;

namespace {

// Allocation-boundary tracing is compiled only into semantic-trace builds.
// Substitute owns several literal-indexed tables; on giant formulas the last
// successful line identifies which dense allocation crossed the memory limit.
#ifdef TACTIX_ENABLE_EVENT_TRACE
void substitute_stage_trace(const Kernel &k, const char *stage,
                            size_t first = 0, size_t second = 0,
                            size_t third = 0) {
  static FILE *out = nullptr;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    const char *path = std::getenv("TACTIX_SUBSTITUTE_MEMORY_TRACE");
    if (path && *path) {
      out = std::fopen(path, "w");
      if (out)
        std::setvbuf(out, nullptr, _IOLBF, 0);
    }
  }
  if (!out)
    return;
  struct rusage usage {};
  getrusage(RUSAGE_SELF, &usage);
  unsigned long virtual_pages = 0;
  if (FILE *statm = std::fopen("/proc/self/statm", "r")) {
    (void)std::fscanf(statm, "%lu", &virtual_pages);
    std::fclose(statm);
  }
  const unsigned long virtual_kib =
      virtual_pages * static_cast<unsigned long>(sysconf(_SC_PAGESIZE) / 1024);
  std::fprintf(out,
               "%s\trss_kib=%ld\tvm_kib=%lu\tvars=%u\tarena_words=%zu\t"
               "watch_words=%zu\twatch_capacity=%zu\tfirst=%zu\tsecond=%zu\t"
               "third=%zu\n",
               stage, usage.ru_maxrss, virtual_kib, k.nvars, k.arena.size(),
               k.watches.arena.size(), k.watches.arena.capacity(), first,
               second, third);
}
#else
inline void substitute_stage_trace(const Kernel &, const char *, size_t = 0,
                                   size_t = 0, size_t = 0) {}
#endif

} // namespace

RuleEligibility substitute_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/calculus.md` §7).
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

SccResult substitute_find_sccs(const Kernel &k, bool collect_instances) {
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  SccResult res;
  substitute_stage_trace(k, "scc-entry", nlits);
  res.repr.assign(nlits, INVALID_LIT);
  substitute_stage_trace(k, "scc-repr", res.repr.size(),
                         res.repr.capacity());
  if (nlits == 0)
    return res;

  std::vector<uint32_t> dfs(nlits, 0); // 1-based DFS index; 0 = unvisited
  substitute_stage_trace(k, "scc-dfs", dfs.size(), dfs.capacity());
  std::vector<uint32_t> low(nlits, 0); // low-link
  substitute_stage_trace(k, "scc-low", low.size(), low.capacity());
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

  std::vector<Var> roots(k.nvars);
  substitute_stage_trace(k, "scc-roots", roots.size(), roots.capacity());
  kernel_vars_in_import_order(k, roots);
  for (const Var root_var : roots) {
    for (uint32_t root_sign = 0; root_sign < 2; root_sign++) {
      const Lit root = make_lit(root_var, root_sign != 0);
      if (dfs[root] != 0 || !active(root))
        continue;
      dfs[root] = low[root] = next++;
      const uint32_t mark_root = dfs[root];
      bool failed_root = false;
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
            tstack.push_back(q);
            k.stats.substitute_ticks +=
                1 + kernel_cache_lines(k.watches.size(negate(q)));
            // may reallocate `work`; touch it no more
            work.push_back({q, out_edges(q).begin()});
            descended = true;
            break;
          }
          // `repr` is written exactly when a component leaves Tarjan's stack,
          // so a visited literal with no representative is still on-stack.
          // Reusing that output state avoids a dense byte per literal without
          // changing DFS order or the low-link recurrence.
          if (res.repr[q] == INVALID_LIT && dfs[q] < low[v])
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
          for (size_t i = start + 1; i < tstack.size(); i++)
            if (kernel_import_lit_order(k, tstack[i]) <
                kernel_import_lit_order(k, rep))
              rep = tstack[i];
          const size_t members = tstack.size() - start;
          for (size_t i = start; i < tstack.size(); i++) {
            const Lit m = tstack[i];
            res.repr[m] = rep;
          }
          if (members > 1) {
            res.nontrivial_count++;
            if (collect_instances)
              res.nontrivial_reps.push_back(rep);
          }
          for (size_t i = start; i < tstack.size(); i++) {
            const Lit m = tstack[i];
            const Lit mirror_rep = res.repr[negate(m)];
            if (mirror_rep == rep) { // l and ¬l share the SCC
              res.contradiction = true;
              if (res.contradiction_lit == INVALID_LIT)
                res.contradiction_lit = m; // x with ¬x in this SCC
              break;
            }
            // If the mirror SCC was completed during this root's DFS, root
            // reaches both m and ¬m.  Thus root is failed and ¬root is a
            // unit.  Kissat records at most one such witness per DFS root.
            if (!failed_root && mirror_rep != INVALID_LIT &&
                dfs[negate(m)] >= mark_root) {
              res.failed_units.push_back(negate(root));
              res.failed_witness.push_back(m);
              failed_root = true;
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
  }
  if (collect_instances)
    std::sort(res.nontrivial_reps.begin(), res.nontrivial_reps.end(),
              [&](Lit a, Lit b) {
                return kernel_import_lit_order(k, a) <
                       kernel_import_lit_order(k, b);
              });
  substitute_stage_trace(k, "scc-complete", res.nontrivial_count,
                         res.failed_units.size(), tstack.capacity());
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

// Emit and close the failed-root units discovered as a by-product of the SCC
// DFS. For unit ¬root, assuming root follows two binary implication paths to
// witness and ¬witness. Concatenating those paths is therefore an LRAT RUP
// chain; DRAT needs no explicit antecedents. All proof lines are emitted before
// assignments so every path is read from the same active graph.
ApplyResult apply_failed_root_units(Kernel &k, const SccResult &r) {
  if (r.failed_units.empty())
    return ApplyResult::NoOp;
  const bool lrat = k.proof && k.proof->lrat();
  PendingUnits pending;
  pending.init(k);
  std::vector<uint64_t> first, second, chain;
  std::unordered_set<uint64_t> seen_ids;
  for (size_t pos = r.failed_units.size(); pos-- > 0;) {
    const Lit unit = r.failed_units[pos]; // reference pops its unit stack
    if (k.value(unit) > 0)
      continue;
    assert(k.value(unit) == 0 && "failed DFS roots are active at discovery");
    chain.clear();
    if (lrat) {
      const Lit root = negate(unit), witness = r.failed_witness[pos];
      first.clear();
      second.clear();
      const bool first_ok =
          root == witness || big_path_chain(k, root, witness, first);
      const bool second_ok = root == negate(witness) ||
                             big_path_chain(k, root, negate(witness), second);
      assert(first_ok && second_ok &&
             "failed-root witness must have both implication paths");
      if (!first_ok || !second_ok)
        continue;
      seen_ids.clear();
      for (const uint64_t id : first)
        if (seen_ids.insert(id).second)
          chain.push_back(id);
      for (const uint64_t id : second)
        if (seen_ids.insert(id).second)
          chain.push_back(id);
    }
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, &unit, 1,
                       lrat ? chain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(chain.size()) : 0);
    if (pending.record(k, unit, id) == PendingUnits::Outcome::Refuted)
      return ApplyResult::Unsat;
  }
  for (const Lit unit : pending.units) {
    if (k.value(unit) > 0)
      continue;
    if (k.value(unit) < 0) {
      pending.refute(k, unit); // the preceding propagation already refuted
      return ApplyResult::Unsat;
    }
    pending.commit(k, unit);
    kernel_assign_unit(k, unit);
    if (inprocess_reach_root(k))
      return ApplyResult::Unsat;
  }
  return pending.units.empty() ? ApplyResult::NoOp : ApplyResult::Ok;
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
  PreserveAllClausePositions,
};

// A canonical Substitute pass visits every pre-pass arena record once and
// appends at most one large replacement for it. Encode that forwarding ref in
// the now-garbage source's otherwise-dead `searched` word. This is the same
// logical-position ledger as SweepSubstitutionSession, without two hash-table
// entries per replacement on giant complete passes.
struct CanonicalSubstitutionOrder final : ArenaOrdering {
  Ref original_words = 0;

  void begin(const Kernel &k) {
    original_words = static_cast<Ref>(k.arena.size());
  }
  void inherit(Kernel &k, Ref replacement, Ref source) {
    assert(source < original_words && replacement >= original_words);
    Clause *old = clause_at(k.arena, source);
    assert(old->garbage());
    old->searched = replacement;
  }
  void order(const Kernel &k, std::vector<Ref> &refs) override {
    size_t written = 0;
    for (Ref ref = 0; ref < original_words;) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage()) {
        refs[written++] = ref;
      } else if (c->searched >= original_words &&
                 c->searched < k.arena.size()) {
        const Clause *replacement = clause_at(k.arena, c->searched);
        if (!replacement->garbage())
          refs[written++] = c->searched;
      }
      ref += clause_actual_words(c);
    }
    assert(written == refs.size());
    refs.resize(written);
  }
};

// Sweep rewrites connected clauses in place. Tactix arena records cannot
// shrink, so replacements append and garbage their source. Assign every large
// replacement its source ordinal at collection: untouched clauses keep their
// logical pose, while binary and unit collapses leave holes.

// Mirror the rewrite's member predicate: root-fixed or retired members have
// left the active implication graph and are declined, matching Kissat's
// `add_representative_equivalences`. Otherwise the logical dense stream can
// drift from the physical watch lists.
inline bool substitution_member_live(const Kernel &k,
                                     const std::vector<uint8_t> &members,
                                     Lit l) {
  return l < members.size() && members[l] && k.values[l] == 0 &&
         !kernel_var_retired(k, var_of(l));
}

// The dense Sweep indexes are maintained by this session alone, so every lookup
// below is an invariant of the session, never a property of the input. Assert
// it, but never mutate through a failed lookup: `erase(end())` and
// `erase(begin() - 1)` both memmove outside the allocation, which turns a
// bookkeeping slip into heap corruption far from its cause.
bool dense_erase_first(std::vector<Ref> &occ, Ref ref) {
  const auto at = std::find(occ.begin(), occ.end(), ref);
  assert(at != occ.end() && "dense occurrence list lost its clause");
  if (at == occ.end())
    return false;
  occ.erase(at);
  return true;
}

bool dense_erase_last(std::vector<Ref> &occ, Ref ref) {
  const auto at = std::find(occ.rbegin(), occ.rend(), ref);
  assert(at != occ.rend() && "dense occurrence list lost its clause");
  if (at == occ.rend())
    return false;
  occ.erase(std::next(at).base());
  return true;
}

bool dense_replace_first(std::vector<Ref> &occ, Ref from, Ref to) {
  const auto at = std::find(occ.begin(), occ.end(), from);
  assert(at != occ.end() && "dense occurrence list lost its clause");
  if (at == occ.end())
    return false;
  *at = to;
  return true;
}
} // namespace

// The ledger slot naming `ref` in `lit`'s stream, or null when the session never
// indexed it. Callers must test the result before writing through it, for the
// reason given on `dense_erase_first`. The stream must already be
// materialized (`prime_large`): building it here would read the watches and
// `Kernel::dense` after the kernel's rewrite, where the source is garbage,
// unindexed, or already shadowed by its binary image.
SweepSubstitutionSession::DenseOccurrence *
SweepSubstitutionSession::find_large(const Kernel &k, Lit lit, Ref ref) {
  if (!dense_enabled || lit >= dense_occurrences.size())
    return nullptr;
  assert(dense_initialized[lit] &&
         "sweep ledger stream materialized after the kernel rewrite");
  std::vector<DenseOccurrence> &ordered = occurrences(k, lit);
  for (DenseOccurrence &occurrence : ordered)
    if (!occurrence.binary && occurrence.payload == ref)
      return &occurrence;
  assert(false && "sweep ledger lost a large occurrence");
  return nullptr;
}

void SweepSubstitutionSession::erase_large(const Kernel &k, Lit lit, Ref ref) {
  DenseOccurrence *const at = find_large(k, lit, ref);
  if (!at)
    return;
  std::vector<DenseOccurrence> &ordered = dense_occurrences[lit];
  ordered.erase(ordered.begin() + (at - ordered.data()));
}

void SweepSubstitutionSession::begin(const Kernel &k) {
  original_words = static_cast<Ref>(k.arena.size());
  changed = false;
  origin.clear();
  latest.clear();
  reset_transaction();
  for (const Lit lit : dense_initialized_lits) {
    dense_occurrences[lit].clear();
    dense_initialized[lit] = 0;
  }
  dense_initialized_lits.clear();
  dirty_binary_owners.clear();
  dense_enabled = k.dense;
  const size_t nlits = 2 * static_cast<size_t>(k.nvars);
  if (dense_occurrences.size() < nlits)
    dense_occurrences.resize(nlits);
  if (dense_initialized.size() < nlits)
    dense_initialized.resize(nlits);
}

std::vector<SweepSubstitutionSession::DenseOccurrence> &
SweepSubstitutionSession::occurrences(const Kernel &k, Lit lit) {
  assert(dense_enabled && k.dense);
  assert(lit < dense_occurrences.size());
  std::vector<DenseOccurrence> &ordered = dense_occurrences[lit];
  if (dense_initialized[lit])
    return ordered;
  assert(ordered.empty());
  dense_initialized[lit] = 1;
  dense_initialized_lits.push_back(lit);
  ordered.reserve(k.watches.size(lit) + k.dense->large[lit].size());
  for (const WatchEntry watch : k.watches.entries(lit))
    if (watch.binary()) {
      ordered.push_back({true, watch.other()});
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_binary_lifecycle(k, "sweep-ledger-begin", lit,
                                   watch.other(), 0, 0);
#endif
    }
  for (const Ref ref : k.dense->large[lit]) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage() && !clause->redundant())
      ordered.push_back({false, ref});
  }
  return ordered;
}

void SweepSubstitutionSession::prime_equivalence(const Kernel &k, Lit a,
                                                  Lit b) {
  if (!dense_enabled)
    return;
  for (const Lit lit : {a, negate(a), b, negate(b)})
    (void)occurrences(k, lit);
}

void SweepSubstitutionSession::sync_new_binaries(
    const Kernel &k,
    const std::vector<std::pair<Lit, Lit>> &equivalences) {
  if (!dense_enabled) {
    dirty_binary_owners.clear(); // no ledger to re-pose; end the transaction
    return;
  }
  // A dirty owner's binary substream is the transaction's prediction of the
  // physical rewrite, and the physical outcome may supersede it.  Only for
  // such an owner is a ledger-only binary an expected residue; everywhere
  // else it is a producer that bypassed the session.
  std::sort(dirty_binary_owners.begin(), dirty_binary_owners.end());
  dirty_binary_owners.erase(
      std::unique(dirty_binary_owners.begin(), dirty_binary_owners.end()),
      dirty_binary_owners.end());
  const auto dirty = [&](Lit lit) {
    return std::binary_search(dirty_binary_owners.begin(),
                              dirty_binary_owners.end(), lit);
  };
  (void)dirty; // assertion-only in release builds
  std::vector<Lit> touched;
  touched.reserve(4 * equivalences.size() + dirty_binary_owners.size());
  for (const auto &eq : equivalences)
    touched.insert(touched.end(),
                   {eq.first, negate(eq.first), eq.second, negate(eq.second)});
  touched.insert(touched.end(), dirty_binary_owners.begin(),
                 dirty_binary_owners.end());
  std::sort(touched.begin(), touched.end());
  touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
  // Mutation provenance already marks both the source owner and the target
  // owner of every move, each owner rewritten in place, and both endpoints of
  // every large-to-binary collapse. Expanding once more through all of their
  // neighbors is therefore redundant and can turn a local equivalence into a
  // near-whole-formula scan on a high-degree graph.
  for (const Lit lit : touched) {
    if (lit >= dense_occurrences.size())
      continue;
    // The watch lists are the authority on which binaries exist; the ledger
    // only adds Kissat's dense stream order on top of them. Reconcile both
    // directions so the two agree exactly afterwards. The physical sub-list can
    // be permuted relative to the ledger by a prior inline substitution, so
    // match the multiset rather than a prefix.
    std::unordered_map<Lit, size_t> unmatched;
    std::vector<DenseOccurrence> &stream = occurrences(k, lit);
    for (const DenseOccurrence occurrence : stream)
      if (occurrence.binary)
        unmatched[static_cast<Lit>(occurrence.payload)]++;
    size_t stale = unmatched.size();
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary())
        continue;
      const auto found = unmatched.find(watch.other());
      if (found != unmatched.end() && found->second) {
        if (!--found->second)
          stale--;
        continue;
      }
      // Only unmatched watches are new, in the creation order observed here.
      stream.push_back({true, watch.other()});
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_binary_lifecycle(
          k, "sweep-ledger-adopt", lit, watch.other(),
          kernel_binary_id(k, lit, watch.other()), 1);
#endif
    }
    // Every producer of a binary updates this ledger, so on a stream the
    // transaction never mutated nothing may be left: a residue there means a
    // binary was published behind the session's back.  On a dirty stream the
    // residue is the superseded prediction.  Either way purge it rather than
    // let a phantom clause reach a later environment, since the ledger must
    // not claim a clause the formula no longer contains.
    assert((!stale || dirty(lit)) &&
           "sweep ledger holds a binary absent from the watches");
    if (stale) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
      for (const auto &[other, count] : unmatched)
        if (count)
          event_trace_binary_lifecycle(
              k, "sweep-ledger-purge", lit, other,
              kernel_binary_id(k, lit, other),
              static_cast<uint32_t>(std::min(count, size_t{UINT32_MAX})));
#endif
      purge_stale_binaries(lit, unmatched);
    }
  }
  dirty_binary_owners.clear();
}

void SweepSubstitutionSession::purge_stale_binaries(
    Lit lit, std::unordered_map<Lit, size_t> &unmatched) {
  std::vector<DenseOccurrence> &ordered = dense_occurrences[lit];
  size_t kept = 0;
  for (const DenseOccurrence occurrence : ordered) {
    if (occurrence.binary) {
      const auto found = unmatched.find(static_cast<Lit>(occurrence.payload));
      if (found != unmatched.end() && found->second) {
        found->second--;
        continue;
      }
    }
    ordered[kept++] = occurrence;
  }
  ordered.resize(kept);
}

void SweepSubstitutionSession::reset_transaction() {
  delayed_moves.clear();
  retiring.clear();
  retiring_repr.clear();
  retiring_rank.clear();
  retiring_position.clear();
}

// A retiring literal's large occurrences move with its binaries, but only the
// clause rewrite knows what each one became, so the stream index is stored
// under the pair that identifies it while the stream is still intact.
static uint64_t sweep_stream_key(Lit lit, Ref source) {
  return (static_cast<uint64_t>(lit) << 32) | source;
}

// The retiring literal whose stream releases this clause to `representative`.
// Kissat introduces a representative into a clause during the first
// `substitute_connected_clauses` call that rewrites one of the clause's own
// literals to it; every later call for the same clause finds it already
// watched and appends nothing. Tactix rewrites the whole class at once, so
// pick that same first call: the lowest-ranked retiring member of the source
// clause whose representative is this one.
bool SweepSubstitutionSession::move_origin(const Clause &old,
                                           Lit representative, Ref source,
                                           bool final_binary,
                                           uint32_t &rank,
                                           uint32_t &position) const {
  rank = UINT32_MAX;
  for (const Lit lit : old) {
    const auto found = retiring_rank.find(lit);
    if (found == retiring_rank.end() ||
        retiring_repr[found->second] != representative)
      continue;
    // A surviving large occurrence is appended by the first retiring member
    // that introduces the representative.  If later members of the same
    // clause retire, Kissat sees the representative already watched and keeps
    // that first position.  A final binary has the opposite owner: it is born
    // only when the last duplicate is removed and the large clause reaches
    // size two, so it belongs to the latest retiring member's mixed stream.
    // The distinction is representation semantics, independent of Sweep's
    // caller-owned effort or cadence.
    if (rank == UINT32_MAX ||
        (final_binary ? found->second > rank : found->second < rank))
      rank = found->second;
  }
  if (rank == UINT32_MAX)
    return false;
  const auto at = retiring_position.find(sweep_stream_key(retiring[rank],
                                                          source));
  // A redundant source clause never entered the dense stream, so its collapse
  // has no recorded position; it follows that literal's other occurrences.
  position = at == retiring_position.end() ? UINT32_MAX : at->second;
  return true;
}

void SweepSubstitutionSession::push_move(const Kernel &k, const Clause &old,
                                         Lit representative, Ref source,
                                         DenseOccurrence image) {
  if (image.binary)
    dirty_binary_owners.push_back(representative);
  uint32_t rank = 0, position = 0;
  if (move_origin(old, representative, source, image.binary, rank, position))
    delayed_moves.push_back({representative, rank, position, image});
  else
    occurrences(k, representative).push_back(image);
}

void SweepSubstitutionSession::rewrite_binary_occurrences(
    const Kernel &k, const std::vector<std::pair<Lit, Lit>> &map,
    const std::vector<Lit> &sources) {
  reset_transaction();
  dirty_binary_owners.clear();
  // Ask the rewrite's own member question rather than the requested class map:
  // a member already root-fixed or retired is declined by `do_substitution`,
  // and mirroring it here would move ledger entries the watch lists keep.
  const auto requested_image = [&](Lit l) {
    const auto it = std::lower_bound(
        map.begin(), map.end(), l,
        [](const std::pair<Lit, Lit> &entry, Lit needle) {
          return entry.first < needle;
        });
    return it != map.end() && it->first == l ? it->second : l;
  };
  const auto member_live = [&](Lit l) {
    return requested_image(l) != l && k.values[l] == 0 &&
           !kernel_var_retired(k, var_of(l));
  };
  // The retiring list is the transaction's membership, not a view of the dense
  // ledger: `do_substitution` reads it to decide which watch lists to snapshot
  // before it drops every binary incident to a retiring literal. Record it even
  // when this session holds no ledger (Sweep with the dense occurrence index
  // disconnected), or that rewrite loses each mirror while the opposite
  // direction survives in place, leaving a half-watched clause that still
  // propagates but has no proof identity.
  for (const Lit source : sources) {
    if (!member_live(source))
      continue; // declined here exactly as the rewrite declines it
    if (retiring_rank.emplace(source,
                              static_cast<uint32_t>(retiring.size())).second) {
      retiring.push_back(source);
      retiring_repr.push_back(requested_image(source));
    }
  }
  if (!dense_enabled)
    return; // no dense stream to re-pose; the membership above still stands
  // A binary changes iff at least one endpoint retires.  The retiring owner's
  // directed stream names every opposite endpoint, hence the exact set of
  // directed owners whose entries can change is {retiring literals} union
  // {their binary neighbors}. Kissat walks precisely these connected streams;
  // scanning every imported literal here made each productive Sweep seed
  // O(F) even though the semantic transaction was local.
  std::vector<Lit> affected_owners;
  for (const Lit source : retiring) {
    affected_owners.push_back(source);
    for (const DenseOccurrence occurrence : occurrences(k, source))
      if (occurrence.binary)
        affected_owners.push_back(static_cast<Lit>(occurrence.payload));
  }
  std::sort(affected_owners.begin(), affected_owners.end(),
            [&](Lit a, Lit b) {
              const uint64_t oa = kernel_import_lit_order(k, a);
              const uint64_t ob = kernel_import_lit_order(k, b);
              return oa != ob ? oa < ob : a < b;
            });
  affected_owners.erase(
      std::unique(affected_owners.begin(), affected_owners.end()),
      affected_owners.end());
  for (const Lit owner : affected_owners) {
    const Lit target_owner =
        member_live(owner) ? requested_image(owner) : owner;
    const auto rank = retiring_rank.find(owner);
    const bool retires = rank != retiring_rank.end();
    std::vector<DenseOccurrence> &ordered = occurrences(k, owner);
    size_t kept = 0;
    for (size_t at = 0; at < ordered.size(); at++) {
      DenseOccurrence occurrence = ordered[at];
      if (!occurrence.binary) {
        if (retires)
          retiring_position.emplace(
              sweep_stream_key(owner, static_cast<Ref>(occurrence.payload)),
              static_cast<uint32_t>(at));
        ordered[kept++] = occurrence;
        continue;
      }
      const Lit other = static_cast<Lit>(occurrence.payload);
      const Lit target_other =
          member_live(other) ? requested_image(other) : other;
      if (target_owner == target_other ||
          target_owner == negate(target_other)) {
        // This directed occurrence is actually removed. Identity entries
        // are intentionally not dirty: marking every binary merely because
        // the transaction visited it turns a local reconciliation into a
        // whole-formula scan after every productive Sweep seed.
        dirty_binary_owners.push_back(owner);
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_binary_lifecycle(
            k, "sweep-ledger-drop", owner, other,
            kernel_binary_id(k, owner, other),
            kernel_binary_copies(k, owner, other));
#endif
        continue;
      }
      if (target_owner == owner) {
        if (target_other != other) {
          dirty_binary_owners.push_back(owner);
#ifdef TACTIX_ENABLE_EVENT_TRACE
          event_trace_binary_lifecycle(
              k, "sweep-ledger-rewrite", owner, other,
              kernel_binary_id(k, owner, other),
              kernel_binary_copies(k, owner, other));
          event_trace_binary_lifecycle(k, "sweep-ledger-target", owner,
                                       target_other, 0, 0);
#endif
        }
        occurrence.payload = target_other;
        ordered[kept++] = occurrence;
      } else {
        // Every literal the rewrite retires lost exactly one merge, so the
        // caller's source list names it. Keep an unranked move reachable
        // rather than dropping it: membership is the ledger's contract and
        // order is only the pose.
        assert(retires && "sweep ledger moved an unranked source stream");
        dirty_binary_owners.push_back(owner);
        dirty_binary_owners.push_back(target_owner);
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_binary_lifecycle(
            k, "sweep-ledger-move", owner, other,
            kernel_binary_id(k, owner, other),
            kernel_binary_copies(k, owner, other));
        event_trace_binary_lifecycle(k, "sweep-ledger-target", target_owner,
                                     target_other, 0, 0);
#endif
        delayed_moves.push_back({target_owner,
                                 retires ? rank->second : UINT32_MAX,
                                 static_cast<uint32_t>(at),
                                 {true, target_other}});
      }
    }
    ordered.resize(kept);
  }
}

bool SweepSubstitutionSession::substitutes(Lit lit) const {
  return retiring_rank.find(lit) != retiring_rank.end();
}

uint32_t SweepSubstitutionSession::retiring_index(Lit lit) const {
  const auto it = retiring_rank.find(lit);
  assert(it != retiring_rank.end());
  return it->second;
}

Lit SweepSubstitutionSession::representative(Lit lit) const {
  const auto it = retiring_rank.find(lit);
  return it == retiring_rank.end() ? lit : retiring_repr[it->second];
}

void SweepSubstitutionSession::flush_moves(const Kernel &k) {
#ifndef TACTIX_ENABLE_EVENT_TRACE
  (void)k;
#endif
  // One retiring literal's `delayed` stack is flushed before the next literal
  // is walked, so a representative sees each source's occurrences contiguously
  // and in that source's own encounter order, binaries and large clauses
  // interleaved exactly as the single watch stream met them.
  std::stable_sort(delayed_moves.begin(), delayed_moves.end(),
                   [](const DelayedMove &a, const DelayedMove &b) {
                     if (a.source != b.source)
                       return a.source < b.source;
                     return a.position < b.position;
                   });
  for (const DelayedMove &move : delayed_moves) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (move.source < retiring.size())
      event_trace_sweep_rewrite_occurrence(
          k, retiring[move.source], move.representative, move.position,
          move.image.binary, move.image.payload);
    if (move.image.binary && move.source < retiring.size())
      event_trace_sweep_rewrite_binary(
          k, retiring[move.source], move.representative, move.position,
          static_cast<Lit>(move.image.payload));
#endif
    occurrences(k, move.representative).push_back(move.image);
  }
  reset_transaction();
}

void SweepSubstitutionSession::prime_large(const Kernel &k, Ref source) {
  if (!dense_enabled)
    return;
  const Clause *const old = clause_at(k.arena, source);
  if (old->redundant() || old->garbage())
    return; // dense Sweep indexes live irredundant clauses only
  // Output-sensitive: only a clause the rewrite actually touches reaches
  // here, and each of its literals is materialized at most once per call.
  for (const Lit lit : *old)
    (void)occurrences(k, lit);
}

void SweepSubstitutionSession::remove_large(Kernel &k, Ref source) {
  const Clause *const old = clause_at(k.arena, source);
  if (old->redundant())
    return; // dense Sweep indexes irredundant clauses only
  for (const Lit lit : *old) {
    if (k.dense)
      dense_erase_first(k.dense->large[lit], source);
    if (dense_enabled)
      erase_large(k, lit, source);
  }
}

void SweepSubstitutionSession::collapse_large_to_binary(Kernel &k, Ref source,
                                                         Lit a, Lit b,
                                                         bool append_a,
                                                         bool append_b) {
  const Clause *const old = clause_at(k.arena, source);
  // Kissat's dense stream holds every binary but only the irredundant large
  // clauses, so a redundant source has no large occurrence to retire — while
  // the binary the kernel just published is a real member of that stream and
  // must still be recorded, or the next environment cannot see the clause.
  const bool indexed = !old->redundant();
  dirty_binary_owners.push_back(a);
  dirty_binary_owners.push_back(b);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_binary_lifecycle(k, "sweep-ledger-collapse-target", a, b,
                               kernel_binary_id(k, a, b),
                               kernel_binary_copies(k, a, b));
  event_trace_binary_collapse(k, source, a, b);
#endif
  if (indexed)
    for (const Lit lit : *old) {
      if (k.dense)
        dense_erase_first(k.dense->large[lit], source);
      if (!dense_enabled)
        continue;
      DenseOccurrence *const at = find_large(k, lit, source);
      if (!at)
        continue;
      // The endpoint the substitution introduced takes Kissat's appended
      // `delayed` watch; the endpoint that survived from the source clause
      // takes its in-place replacement (`kissat_substitute_large_watch`).
      if (lit == a && !append_a)
        *at = {true, b};
      else if (lit == b && !append_b)
        *at = {true, a};
      else {
        std::vector<DenseOccurrence> &ordered = occurrences(k, lit);
        ordered.erase(ordered.begin() + (at - ordered.data()));
      }
    }
  if (!dense_enabled)
    return;
  const auto rewritten_in_place = [&](Lit lit, bool append) {
    if (append || !indexed)
      return false;
    return std::find(old->begin(), old->end(), lit) != old->end();
  };
  if (!rewritten_in_place(a, append_a))
    push_move(k, *old, a, source, {true, b});
  if (!rewritten_in_place(b, append_b))
    push_move(k, *old, b, source, {true, a});
}

void SweepSubstitutionSession::rewrite_large_in_place(const Kernel &k,
                                                       Ref source,
                                                       const Lit *image,
                                                       uint32_t size) {
  const Clause *const old = clause_at(k.arena, source);
  if (old->redundant() || !dense_enabled)
    return;
  const auto image_holds = [&](Lit lit) {
    return std::find(image, image + size, lit) != image + size;
  };
  const auto source_held = [&](Lit lit) {
    return std::find(old->begin(), old->end(), lit) != old->end();
  };
  for (const Lit lit : *old) {
    DenseOccurrence *const at = find_large(k, lit, source);
    if (!at || image_holds(lit))
      continue;
    std::vector<DenseOccurrence> &ordered = occurrences(k, lit);
    ordered.erase(ordered.begin() + (at - ordered.data()));
  }
  for (uint32_t i = 0; i < size; i++)
    if (!source_held(image[i]))
      push_move(k, *old, image[i], source, {false, source});
}

void SweepSubstitutionSession::inherit(Kernel &k, Ref replacement, Ref source) {
  const Clause *const old = clause_at(k.arena, source);
  const Clause *const image = clause_at(k.arena, replacement);
  // `kernel_add_large_clause` appends the replacement to every dense
  // occurrence list.  Kissat rewrites the source allocation in place:
  // literals retained by the image therefore keep the source occurrence,
  // removed literals disappear, and only a newly introduced representative
  // is appended.  Restore that pose immediately, not merely at the final
  // arena collection, because later environments in this same Sweep pass
  // traverse the live dense lists and their structural caps make the order
  // semantic.  A replacement inherits the source's redundancy and
  // `kernel_add_large_clause` indexes only irredundant records, so one
  // predicate decides whether either index holds this clause at all.
  const bool indexed = !old->redundant();
  const auto image_holds = [&](Lit lit) {
    return std::find(image->begin(), image->end(), lit) != image->end();
  };
  const auto source_held = [&](Lit lit) {
    return std::find(old->begin(), old->end(), lit) != old->end();
  };
  if (k.dense && indexed) {
    // Remove the append performed by `kernel_add_large_clause`; the loop below
    // puts each image occurrence at its Kissat position.
    for (const Lit lit : *image)
      dense_erase_last(k.dense->large[lit], replacement);
    for (const Lit lit : *old) {
      if (image_holds(lit))
        dense_replace_first(k.dense->large[lit], source, replacement);
      else
        dense_erase_first(k.dense->large[lit], source);
    }
    for (const Lit lit : *image)
      if (!source_held(lit))
        k.dense->large[lit].push_back(replacement);
  }
  if (indexed && dense_enabled) {
    for (const Lit lit : *old) {
      DenseOccurrence *const at = find_large(k, lit, source);
      if (!at)
        continue;
      if (image_holds(lit))
        at->payload = replacement;
      else {
        std::vector<DenseOccurrence> &ordered = occurrences(k, lit);
        ordered.erase(ordered.begin() + (at - ordered.data()));
      }
    }
    for (const Lit lit : *image)
      if (!source_held(lit))
        push_move(k, *old, lit, source, {false, replacement});
  }
  const auto inherited = origin.find(source);
  const Ref original = inherited == origin.end() ? source : inherited->second;
  origin.insert_or_assign(replacement, original);
  latest.insert_or_assign(original, replacement);
}

void SweepSubstitutionSession::order(const Kernel &k,
                                     std::vector<Ref> &refs) {
  // Every original clause position owns one rank, and replacement clauses
  // inherit that rank after the old record is garbaged.  Thus live ranked
  // records form a partial permutation: restore it in linear time instead of
  // comparison-sorting the complete formula after every productive Sweep
  // environment.  The tail is defensive for unranked/duplicate records and
  // retains their incoming arena order, matching the old stable-sort tie.
  std::vector<Ref> ordered;
  ordered.reserve(refs.size());
  for (Ref ref = 0; ref < original_words;) {
    const Clause *clause = clause_at(k.arena, ref);
    if (!clause->garbage()) {
      ordered.push_back(ref);
    } else {
      const auto replacement = latest.find(ref);
      if (replacement != latest.end()) {
        const Clause *live = clause_at(k.arena, replacement->second);
        if (!live->garbage())
          ordered.push_back(replacement->second);
      }
    }
    ref += clause_actual_words(clause);
  }
  // New clauses which are not replacements retain their append order.
  for (const Ref ref : refs)
    if (ref >= original_words && !origin.count(ref))
      ordered.push_back(ref);
  refs.swap(ordered);
}

uint64_t SweepSubstitutionSession::collect(Kernel &k) {
  if (!changed)
    return 0;
  // Surviving large clauses now keep their Ref and allocation span.  Clauses
  // that collapse or disappear are ordinary lazy garbage, which every arena
  // and watch consumer already skips and a later kernel collection reclaims.
  // Thus Sweep has no representation debt to settle at its call boundary.
  // Keeping this boundary method makes the rule/strategy interface explicit
  // while avoiding a whole-formula compaction that Kissat does not perform.
  (void)k;
  changed = false;
  return 0;
}

namespace {

ApplyResult do_substitution(Kernel &k, SccResult &r,
                            std::vector<uint8_t> &&sub,
                            SubstitutionArenaOrder arena_order =
                                SubstitutionArenaOrder::Policy,
                            SweepSubstitutionSession *session = nullptr) {
  const bool lrat = k.proof && k.proof->lrat();
  const uint64_t subst_t0 = substitute_time ? now_nanos() : 0;
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  substitute_stage_trace(k, "rewrite-entry", nlits, r.nontrivial_count);
  CanonicalSubstitutionOrder local_order;
  ArenaOrdering *const position_order = session
                                             ? static_cast<ArenaOrdering *>(session)
                                             : static_cast<ArenaOrdering *>(&local_order);
  if (arena_order != SubstitutionArenaOrder::Policy && !session)
    local_order.begin(k);
  // A member can already be root-fixed here: the caller's map predates the
  // round's failed root units. Kissat skips exactly those at the same point
  // (substitute.c add_representative_equivalences, `if (!ACTIVE (idx))`), and
  // a fixed member has left the active implication graph, so its equivalence
  // lemma has no path to cite and would go out with an empty chain.
  // Snapshot, never a live test: the phases below assert units, so a live
  // predicate could hold while (D) rewrites a variable away and fail by the
  // time (F) owes it a witness — retired-without-witness answers a wrong SAT.
  std::vector<uint8_t> live_sub;
  if (!session) {
    live_sub = std::move(sub);
    for (Lit l = 0; l < nlits; l++)
      live_sub[l] =
          static_cast<uint8_t>(substitution_member_live(k, live_sub, l));
  }
  substitute_stage_trace(k, "rewrite-live-sub", live_sub.size(),
                         live_sub.capacity());
  const auto substitutable = [&](Lit l) {
    return session ? session->substitutes(l)
                   : l < nlits && static_cast<bool>(live_sub[l]);
  };
  const auto representative = [&](Lit l) {
    return session ? session->representative(l) : r.repr[l];
  };
  const auto for_each_substitutable = [&](const auto &fn) {
    if (session)
      for (const Lit l : session->retiring_sources())
        fn(l);
    else
      for (Lit l = 0; l < nlits; l++)
        if (substitutable(l))
          fn(l);
  };

  // (B) transient equivalence lemmas {¬l, repr[l]}, one per member literal,
  // derived before any binary mutation so big_path_chain reads the intact
  // graph. RUP: assume l, ¬repr[l]; the path l→…→repr[l] forces repr[l].
  // Clause ids are antecedents only in LRAT. DRAT ignores them, so avoid a
  // 16-byte-per-variable zero table on the production proof path.
  std::vector<uint64_t> lemma;
  if (lrat)
    lemma.assign(session ? session->retiring_sources().size() : nlits, 0);
  const auto lemma_index = [&](Lit l) {
    return session ? static_cast<size_t>(session->retiring_index(l))
                   : static_cast<size_t>(l);
  };
  const auto lemma_id = [&](Lit l) { return lemma[lemma_index(l)]; };
  for_each_substitutable([&](Lit l) {
    const Lit s = representative(l);
    std::vector<uint64_t> chain;
    if (lrat) {
      const bool ok = big_path_chain(k, l, s, chain);
      assert(ok && "SCC members are strongly connected");
      (void)ok;
    }
    const uint64_t id = ++k.next_clause_id;
    if (lrat)
      lemma[lemma_index(l)] = id;
    if (k.proof) {
      const Lit lits2[2] = {negate(l), s};
      if (lrat)
        k.proof->add_rup(id, lits2, 2, chain.data(),
                         static_cast<uint32_t>(chain.size()));
      else
        k.proof->add_rup(0, lits2, 2, nullptr, 0);
    }
  });
  const uint64_t subst_lemmas = substitute_time ? now_nanos() : 0;
  substitute_stage_trace(k, "rewrite-lemmas", lemma.size(), lemma.capacity());

  // Canonical Substitute benefits from a literal-indexed byte table.  Sweep
  // rewrites a small connected frontier many times on giant formulas, so a
  // sparse set avoids allocating/zeroing 2*nvars for every equivalence.
  std::vector<uint8_t> seen;
  std::unordered_set<Lit> sparse_seen;
  if (session)
    sparse_seen.reserve(2 * session->retiring_sources().size());
  else
    seen.assign(nlits, 0);
  const auto seen_contains = [&](Lit l) {
    return session ? sparse_seen.find(l) != sparse_seen.end()
                   : static_cast<bool>(seen[l]);
  };
  const auto seen_mark = [&](Lit l) {
    if (session)
      sparse_seen.insert(l);
    else
      seen[l] = 1;
  };
  const auto seen_clear = [&](Lit l) {
    if (session)
      sparse_seen.erase(l);
    else
      seen[l] = 0;
  };
  substitute_stage_trace(k, "rewrite-seen", seen.size(), seen.capacity());
  // Batched root units + opposite-sign refutation: the shared kernel idiom
  // (PendingUnits, kernel.hpp).
  PendingUnits pending;
  substitute_stage_trace(k, "rewrite-pending-begin", k.nvars);
  pending.init(k);
  substitute_stage_trace(k, "rewrite-pending-end", pending.units.size(),
                         pending.ids.size(), pending.units.capacity());
  bool changed = false;

  // Record a size-collapsed root unit `u`, deduping per variable within the
  // round. Returns true iff `u` contradicts a unit already derived this round,
  // in which case the two resolve to ∅ and the caller returns Unsat. Without
  // this, a batch deriving both `x` and `¬x` would assert the first and
  // silently drop the second, losing the conflict.
  auto record_unit = [&](Lit u, uint64_t id) -> bool {
    if (k.value(u) > 0)
      return false;
    if (k.value(u) < 0) {
      kernel_emit_empty_from_pair(k, id, k.unit_ids[var_of(u)]);
      return true;
    }
    return pending.record(k, u, id) == PendingUnits::Outcome::Refuted;
  };

  // Kissat substitutes binaries first, then assigns and propagates every unit
  // they produced before it starts the large-clause pass.  Process the batch
  // in stack-pop order, propagating after each assignment, and reset the table
  // so large-clause units form the second independent batch.
  auto flush_units = [&]() -> bool {
    for (auto it = pending.units.rbegin(); it != pending.units.rend(); ++it) {
      const Lit u = *it;
      if (k.value(u) > 0)
        continue;
      if (k.value(u) < 0) {
        pending.refute(k, u); // ¬u became root-true since the record
        return true;
      }
      pending.commit(k, u); // the id is this variable's identity from here
      kernel_assign_unit(k, u);
      if (inprocess_reach_root(k))
        return true;
    }
    pending.init(k);
    return false;
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
      const Lit mp = substitutable(m) ? representative(m) : m;
      if (k.value(mp) > 0) { // root-true → clause satisfied
        satisfied = true;
        break;
      }
      if (k.value(mp) < 0) { // root-false → strip (cite its unit)
        // Propagation order, not literal order: the representative's unit is
        // what makes the bridging lemma unit, so it precedes it. A checker
        // rejects a hint that is not unit at its turn, and the reverse order
        // is exactly that (the D-CONG-LRAT lesson).
        if (lrat) {
          chain.push_back(k.unit_ids[var_of(mp)]);
          if (substitutable(m))
            chain.push_back(lemma_id(m));
        }
        continue;
      }
      if (substitutable(m) && lrat)
        chain.push_back(lemma_id(m));
      if (seen_contains(mp)) // duplicate literal
        continue;
      if (seen_contains(negate(mp))) { // mp and ¬mp both present → tautology
        satisfied = true;
        break;
      }
      seen_mark(mp);
      out.push_back(mp);
    }
    for (const Lit l : out)
      seen_clear(l); // unmark
    if (satisfied) {
      out.clear();
      return true;
    }
    if (lrat) { // a duplicated member literal would push its lemma twice
      // In place, never by sorting: the order is the proof. Every other chain
      // here sorts pairs keyed by trail position (probe.cpp emit_unit_chain),
      // which is propagation order; sorting bare ids is not.
      size_t kept = 0;
      for (size_t rd = 0; rd < chain.size(); rd++) {
        bool duplicate = false;
        for (size_t j = 0; j < kept && !duplicate; j++)
          duplicate = chain[j] == chain[rd];
        if (!duplicate)
          chain[kept++] = chain[rd];
      }
      chain.resize(kept);
    }
    return false;
  };

  // (D) rewrite large clauses. Snapshot the original refs first (adds append to
  // the arena only for Sweep's connected subset. The canonical pass streams to
  // a captured original arena end, just as Kissat's `all_clauses` traversal
  // does: refs survive vector relocation, and every Clause* is reacquired.
  // Vivify's ordering: add_rup, del and the garbage mark all read the clause
  // before the arena-mutating add, so the Clause* never dangles.
  std::vector<Lit> out;
  std::vector<uint64_t> chain;
  size_t large_visited = 0, large_touched = 0;
  bool traced_large_replacement = false;
  const auto rewrite_large_ref = [&](Ref ref) -> ApplyResult {
    Clause *c = clause_at(k.arena, ref);
    if (!large_visited++)
      substitute_stage_trace(k, "rewrite-large-first-ref", ref, c->size,
                             k.arena.capacity());
    bool touched = false;
    for (const Lit m : *c) {
      const Lit mp = substitutable(m) ? representative(m) : m;
      if (substitutable(m) || k.value(m) != 0 || k.value(mp) != 0) {
        touched = true;
        break;
      }
    }
    if (!touched)
      return ApplyResult::Ok;
    if (!large_touched++)
      substitute_stage_trace(k, "rewrite-large-first-touched", ref, c->size,
                             k.arena.capacity());
    // Every ledger update below locates this clause by its pre-rewrite pose,
    // and the kernel primitives (delete, collapse, in-place replace) run
    // before some of those updates. Materialize its literals' streams now,
    // while the watches and `Kernel::dense` still hold the source.
    if (session)
      session->prime_large(k, ref);
    const bool credundant = c->redundant();
    const uint32_t cglue = c->glue();
    const uint64_t cid = c->id();
    // Sweep's reference implementation rewrites this connected clause in
    // place (`sweep.c:substitute_connected_clauses`) and therefore leaves its
    // Vivify-leftover priority bit untouched.  The shared non-Sweep
    // substitution path has different source semantics, so preserve the bit
    // only for an active Sweep session.
    const bool cvivify = session && c->vivify();
    const bool sat = rewrite_lits(c->lits, c->size, out, chain);
    if (large_touched <= 5)
      substitute_stage_trace(k, "rewrite-large-first-image", out.size(),
                             out.capacity(), chain.capacity());
    if (sat) { // satisfied/tautology -> delete only (deletions are proof-free)
      if (session)
        session->remove_large(k, ref);
      kernel_delete_clause(k, c); // cid == c->id()
      changed = true;
      return ApplyResult::Ok;
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
    // Shared proof-rule replacement tail.  Sweep selects the general
    // stable-allocation representation for a surviving large image; ordinary
    // Substitute retains its existing append/collect representation.  The
    // choice changes neither the derived clause nor the proof transition.
    // Clamped on cadical's rule (decompose.cpp:629 -> shrink_clause); kissat's
    // substitute.c does not clamp here at all. Deviation:
    // docs/scheduling-parity-ledger.md section 13.3.
    Ref replacement_ref = INVALID_REF;
    const bool trace_replacement = !traced_large_replacement;
    if (trace_replacement) {
      substitute_stage_trace(k, "rewrite-large-first-replace", ref,
                             k.arena.size(), k.arena.capacity());
      traced_large_replacement = true;
    }
    const uint32_t replacement_glue =
        credundant
            ? kernel_shrunken_glue(k, cglue,
                                   static_cast<uint32_t>(out.size()))
            : 0;
    ReplaceOutcome replacement;
    if (session && out.size() >= 3) {
      session->rewrite_large_in_place(k, ref, out.data(),
                                      static_cast<uint32_t>(out.size()));
      kernel_replace_large_clause_in_place(
          k, ref, id, out.data(), static_cast<uint32_t>(out.size()),
          replacement_glue);
      replacement = ReplaceOutcome::Large;
      replacement_ref = ref;
    } else {
      replacement = kernel_replace_clause(
          k, c, id, out.data(), static_cast<uint32_t>(out.size()), credundant,
          replacement_glue,
          arena_order != SubstitutionArenaOrder::Policy ? &replacement_ref
                                                         : nullptr);
    }
    if (session && replacement == ReplaceOutcome::Large) {
      assert(replacement_ref != INVALID_REF);
      clause_at(k.arena, replacement_ref)->set_vivify(cvivify);
    }
    switch (replacement) {
    case ReplaceOutcome::Refuted: // all-false rewrite (unreachable at a
      return ApplyResult::Unsat;  // quiescent root)
    case ReplaceOutcome::Unit:
      if (record_unit(out[0], id))
        return ApplyResult::Unsat;
      break;
    default:
      break;
    }
    if (session) {
      if (replacement == ReplaceOutcome::Binary) {
        bool append_first = false, append_second = false;
        for (const Lit old_lit : *clause_at(k.arena, ref))
          if (substitutable(old_lit)) {
            append_first |= representative(old_lit) == out[0];
            append_second |= representative(old_lit) == out[1];
          }
        session->collapse_large_to_binary(k, ref, out[0], out[1],
                                          append_first, append_second);
      } else if (replacement == ReplaceOutcome::Unit ||
                 replacement == ReplaceOutcome::Refuted) {
        session->remove_large(k, ref);
      }
    }
    if (replacement_ref != INVALID_REF && replacement_ref != ref) {
      assert(arena_order != SubstitutionArenaOrder::Policy);
      if (session)
        session->inherit(k, replacement_ref, ref);
      else
        local_order.inherit(k, replacement_ref, ref);
    }
    if (trace_replacement)
      substitute_stage_trace(k, "rewrite-large-first-replaced", replacement_ref,
                             k.arena.size(), k.arena.capacity());
    return ApplyResult::Ok;
  };

  const auto make_large_image = [&](Ref ref) {
    const Clause *c = clause_at(k.arena, ref);
    bool touched = false;
    for (const Lit m : *c) {
      const Lit mp = substitutable(m) ? representative(m) : m;
      if (substitutable(m) || k.value(m) != 0 || k.value(mp) != 0) {
        touched = true;
        break;
      }
    }
    return touched && !rewrite_lits(c->lits, c->size, out, chain);
  };

  // Tactix represents a large replacement by appending a new arena record,
  // while Kissat shrinks the existing allocation in place. Derive the exact
  // append image before mutating anything so neither the clause arena nor its
  // two watch slices can hit an amortized doubling in the memory-critical
  // pass. `visit` must enumerate the same immutable refs in both calls.
  const auto reserve_large_replacements = [&](const auto &visit) {
    // Sweep owns an exact connected snapshot and defers one global collection
    // to the call boundary. Exact reservation here would copy the complete
    // clause arena and repack every watch list for each proved equivalence.
    // Normal geometric arena/list growth is the Tactix equivalent of Kissat's
    // connected-vector appends, and arena Refs survive vector relocation.
    if (session)
      return;
    size_t arena_words = 0, watch_words = 0;
    visit([&](Ref ref) {
      if (!make_large_image(ref))
        return;
      if (out.size() == 2)
        watch_words += 2;
      else if (out.size() >= 3) {
        arena_words += clause_words(static_cast<uint32_t>(out.size()));
        watch_words += 4;
      }
    });
    assert(k.arena.size() + arena_words <= UINT32_MAX);
    substitute_stage_trace(k, "rewrite-large-reserve-plan", arena_words,
                           watch_words, k.arena.capacity());
    if (k.arena.capacity() < k.arena.size() + arena_words)
      k.arena.reserve(k.arena.size() + arena_words);
    if (watch_words) {
      k.watches.begin_batch_reserve();
      visit([&](Ref ref) {
        if (!make_large_image(ref))
          return;
        if (out.size() == 2) {
          k.watches.reserve_batch_word(out[0]);
          k.watches.reserve_batch_word(out[1]);
        } else if (out.size() >= 3) {
          k.watches.reserve_batch_word(out[0]);
          k.watches.reserve_batch_word(out[0]);
          k.watches.reserve_batch_word(out[1]);
          k.watches.reserve_batch_word(out[1]);
        }
      });
      // Pack exactly while dense rewrite scratch is live. The first later
      // relocation establishes bounded global headroom on demand.
      k.watches.finish_batch_reserve(false);
    }
    substitute_stage_trace(k, "rewrite-large-reserved", k.arena.capacity(),
                           k.watches.arena.size(),
                           k.watches.arena.capacity());
  };

  auto rewrite_large_clauses = [&]() -> ApplyResult {
  substitute_stage_trace(k, "rewrite-large-begin", k.arena.size(),
                         k.arena.capacity());
  // Kissat's Sweep stays in dense mode and `substitute_connected_clauses`
  // visits only the retiring literal's unified binary/irredundant stream.
  // Redundant clauses are deliberately absent and are flushed after retired
  // variables leave the probing phase.  A search-time fallback to the complete
  // arena is therefore both semantically non-reference and O(F) per productive
  // environment.  The session names the exact retiring frontier; collect its
  // irredundant refs sparsely without an arena-sized membership bitmap.
  if (session && k.dense) {
    std::vector<Ref> refs;
    for (const Lit l : session->retiring_sources()) {
      for (const Ref ref : k.dense->large[l]) {
        const Clause *c = clause_at(k.arena, ref);
        if (!c->garbage())
          refs.push_back(ref);
      }
    }
    std::sort(refs.begin(), refs.end()); // preserve current arena traversal
    refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
    const auto visit = [&](const auto &fn) {
      for (const Ref ref : refs)
        fn(ref);
    };
    reserve_large_replacements(visit);
    for (const Ref ref : refs)
      if (rewrite_large_ref(ref) == ApplyResult::Unsat)
        return ApplyResult::Unsat;
  } else {
    const Ref original_end = static_cast<Ref>(k.arena.size());
    const auto visit = [&](const auto &fn) {
      for (Ref ref = 0; ref < original_end;) {
        const Clause *c = clause_at(k.arena, ref);
        const uint32_t words = clause_actual_words(c);
        if (!c->garbage())
          fn(ref);
        ref += words;
      }
    };
    reserve_large_replacements(visit);
    for (Ref ref = 0; ref < original_end;) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(c);
      if (!c->garbage() && rewrite_large_ref(ref) == ApplyResult::Unsat)
        return ApplyResult::Unsat;
      ref += words;
    }
  }
  return ApplyResult::Ok;
  };

  // (C) rewrite BINARY clauses.  Preserve the complete clause multiset and
  // import traversal order in both proof and no-proof modes.  Identical proof
  // clauses cannot all be named by `binary_ids` (the latest id wins), but they
  // do not need separate names: emit one proof image per distinct target pair
  // and let every identical internal watch copy share that live id.  This keeps
  // the formula seen by the following SCC/Sweep passes byte-for-byte aligned
  // with the no-proof/Kissat path without weakening DRAT or LRAT.
  std::vector<Var> variable_order;
  substitute_stage_trace(k, "rewrite-variable-order", variable_order.size(),
                         variable_order.capacity());
  if (!session) {
    variable_order.resize(k.nvars);
    kernel_vars_in_import_order(k, variable_order);
  }

  struct BinaryImage {
    Lit source_a, source_b;
  };
  struct ExpandedBinaryImage {
    Lit source_a, source_b;
    Lit target_a, target_b;
    bool tautological;
  };
  const auto expand_image = [&](const BinaryImage &source) {
    const Lit a = substitutable(source.source_a)
                      ? representative(source.source_a)
                      : source.source_a;
    const Lit b = substitutable(source.source_b)
                      ? representative(source.source_b)
                      : source.source_b;
    return ExpandedBinaryImage{source.source_a, source.source_b, a, b,
                               a == negate(b)};
  };
  // Targets are a pure function of the immutable representative map. Store
  // only the source pair, and use segmented storage so growing a giant image
  // does not momentarily hold both old and doubled contiguous buffers.
  std::deque<BinaryImage> images;
  // Substitution, canonical or Sweep-inline, rewrites only clauses incident to
  // a non-representative literal.  Unconnected binaries cannot change.  Keep
  // them in place instead of copying the complete binary formula into
  // BinaryImage/set/map structures and rebuilding every watch list.
  const bool connected_only = true;
  const auto snapshot_undirected = [&](Lit lit) {
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (!w.binary())
          continue;
        const Lit other = w.other();
        // A pair with two retiring endpoints appears in both source streams;
        // retain one orientation per physical copy. With one stable endpoint
        // only the retiring stream is traversed and therefore owns the image
        // regardless of the stable endpoint's import rank.
        if (substitutable(other) &&
            kernel_import_lit_order(k, lit) >=
                kernel_import_lit_order(k, other))
          continue;
        images.push_back({lit, other});
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        const Lit a = substitutable(lit) ? representative(lit) : lit;
        const Lit b = substitutable(other) ? representative(other) : other;
        rule_journal_note_binary_rewrite(k, lit, other, a, b);
#endif
      }
  };
  if (session) {
    // The session contract names every retiring literal. Its directed watch
    // streams are the exact connected binary frontier, matching Kissat's
    // `substitute_connected_clauses`; no unrelated imported owner is visited.
    for (const Lit lit : session->retiring_sources())
      snapshot_undirected(lit);
  } else {
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        for (const WatchEntry w : k.watches.entries(lit)) {
          if (!w.binary() || kernel_import_lit_order(k, lit) >=
                                 kernel_import_lit_order(k, w.other()))
            continue;
          const Lit other = w.other();
          if (connected_only && !substitutable(lit) && !substitutable(other))
            continue;
          images.push_back({lit, other});
#ifdef TACTIX_ENABLE_RULE_JOURNAL
          const Lit a = substitutable(lit) ? representative(lit) : lit;
          const Lit b = substitutable(other) ? representative(other) : other;
          rule_journal_note_binary_rewrite(k, lit, other, a, b);
#endif
        }
      }
  }
  const uint64_t subst_snapshot = substitute_time ? now_nanos() : 0;
  const size_t image_count = images.size();
  substitute_stage_trace(k, "rewrite-images", images.size(), 0,
                         sizeof(BinaryImage));

  // `seen` is otherwise clear between large-clause rewrites. Reuse it to
  // deduplicate collapsed binary units and retain only the touched literals
  // for O(produced units) restoration; a tree node per unit can exceed the
  // binary image itself on highly congruent formulas.
  std::vector<Lit> emitted_units;
  const auto first_emitted_unit = [&](Lit unit) {
    if (seen_contains(unit))
      return false;
    seen_mark(unit);
    emitted_units.push_back(unit);
    return true;
  };
  bool refuted = false;
  std::vector<uint64_t> target_keys, target_ids;
  if (!lrat) {
    // DRAT names clauses by their literals, not by ids.  Stream the transformed
    // multiset exactly as Kissat does instead of sorting two global key arrays
    // merely to recover the LRAT identity of each distinct target.  On giant
    // binary formulas those arrays and their comparison sorts dominate both
    // Substitute's memory and wall time.  Duplicate source clauses deliberately
    // produce duplicate target clauses, preserving the internal multiset.
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      const bool changed_image =
          image.tautological || image.target_a == image.target_b ||
          kernel_binary_key(image.source_a, image.source_b) !=
              kernel_binary_key(image.target_a, image.target_b);
      changed = changed || changed_image;
      if (image.tautological)
        continue;

      if (image.target_a == image.target_b) {
        if (!first_emitted_unit(image.target_a))
          continue;
        const uint64_t id = ++k.next_clause_id;
        if (k.proof)
          k.proof->add_rup(0, &image.target_a, 1, nullptr, 0);
        if (record_unit(image.target_a, id))
          refuted = true;
        continue;
      }

      if (changed_image && k.proof) {
        const Lit lits2[2] = {image.target_a, image.target_b};
        k.proof->add_rup(0, lits2, 2, nullptr, 0);
      }
    }

    if (k.proof)
      for (const BinaryImage &stored : images) {
        const ExpandedBinaryImage image = expand_image(stored);
        const bool changed_image =
            image.tautological || image.target_a == image.target_b ||
            kernel_binary_key(image.source_a, image.source_b) !=
                kernel_binary_key(image.target_a, image.target_b);
        if (!changed_image)
          continue;
        const Lit lits2[2] = {image.source_a, image.source_b};
        k.proof->del(k.next_clause_id, 0, lits2, 2);
      }
  } else {
    std::vector<uint64_t> source_keys;
    source_keys.reserve(images.size());
    target_keys.reserve(images.size());
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      const uint64_t source =
          kernel_binary_key(image.source_a, image.source_b);
      source_keys.push_back(source);
      if (!image.tautological && image.target_a != image.target_b)
        target_keys.push_back(
            kernel_binary_key(image.target_a, image.target_b));
    }
    const auto sort_unique = [](std::vector<uint64_t> &keys) {
      std::sort(keys.begin(), keys.end());
      keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    };
    sort_unique(source_keys);
    sort_unique(target_keys);
    const auto contains = [](const std::vector<uint64_t> &keys, uint64_t key) {
      return std::binary_search(keys.begin(), keys.end(), key);
    };
    const auto key_index = [](const std::vector<uint64_t> &keys, uint64_t key) {
      const auto it = std::lower_bound(keys.begin(), keys.end(), key);
      assert(it != keys.end() && *it == key);
      return static_cast<size_t>(it - keys.begin());
    };

    // LRAT needs one live named antecedent per distinct pair.  A target pair
    // already present as a source retains that proof clause; a genuinely new
    // target is derived once and duplicate watches share its id.
    target_ids.assign(target_keys.size(), 0);
    for (size_t i = 0; i < target_keys.size(); i++)
      if (contains(source_keys, target_keys[i]))
        target_ids[i] = k.binary_ids.at(target_keys[i]);

    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      const bool changed_image =
          image.tautological || image.target_a == image.target_b ||
          kernel_binary_key(image.source_a, image.source_b) !=
              kernel_binary_key(image.target_a, image.target_b);
      changed = changed || changed_image;
      if (image.tautological)
        continue;

      const uint64_t source =
          kernel_binary_key(image.source_a, image.source_b);
      std::vector<uint64_t> ch;
      if (substitutable(image.source_a))
        ch.push_back(lemma_id(image.source_a));
      if (substitutable(image.source_b))
        ch.push_back(lemma_id(image.source_b));
      ch.push_back(k.binary_ids.at(source));

      if (image.target_a == image.target_b) {
        if (!first_emitted_unit(image.target_a))
          continue;
        const uint64_t id = ++k.next_clause_id;
        k.proof->add_rup(id, &image.target_a, 1, ch.data(),
                         static_cast<uint32_t>(ch.size()));
        if (record_unit(image.target_a, id))
          refuted = true;
        continue;
      }

      const uint64_t target =
          kernel_binary_key(image.target_a, image.target_b);
      const size_t target_index = key_index(target_keys, target);
      if (target_ids[target_index])
        continue;
      const Lit lits2[2] = {image.target_a, image.target_b};
      const uint64_t id = ++k.next_clause_id;
      k.proof->add_rup(id, lits2, 2, ch.data(),
                       static_cast<uint32_t>(ch.size()));
      target_ids[target_index] = id;
    }

    // Retire only distinct source clauses absent from the final image.  Older
    // duplicate LRAT copies may stay in the checker because one id cannot name
    // several input copies.
    std::vector<uint8_t> deleted(source_keys.size(), 0);
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      const uint64_t source =
          kernel_binary_key(image.source_a, image.source_b);
      const size_t source_index = key_index(source_keys, source);
      if (contains(target_keys, source) || deleted[source_index])
        continue;
      deleted[source_index] = 1;
      const Lit lits2[2] = {image.source_a, image.source_b};
      k.proof->del(k.next_clause_id,
                   kernel_binary_id(k, image.source_a, image.source_b), lits2,
                   2);
    }
  }
  substitute_stage_trace(k, "rewrite-binary-proof", emitted_units.size(),
                         emitted_units.capacity());
  for (const Lit unit : emitted_units)
    seen_clear(unit);
  std::vector<Lit>().swap(emitted_units);

  // Kissat rewrites a binary direction in place when the literal owning that
  // watch list is already the class representative (substitute.c:328-340).
  // Only the mirror on the retired endpoint moves to the representative's
  // list.  Rebuilding both directions moves the stable direction to the tail;
  // on `sted5`, (-187 v -2983) -> (-187 v 186) then crossed a later Factor
  // binary and selected a different Vivify antecedent.  Rewrite exactly one
  // matching source copy now; the compact pass below will retain it because
  // both of its post-rewrite endpoints are representatives.
  for (const BinaryImage &stored : images) {
    const ExpandedBinaryImage image = expand_image(stored);
    if (image.tautological || image.target_a == image.target_b)
      continue;
    const bool stable_a = !substitutable(image.source_a);
    const bool stable_b = !substitutable(image.source_b);
    if (stable_a == stable_b)
      continue;
    const Lit stable = stable_a ? image.source_a : image.source_b;
    const Lit old_other = stable_a ? image.source_b : image.source_a;
    const Lit new_other = stable_a ? image.target_b : image.target_a;
    uint32_t *p = k.watches.begin(stable);
    uint32_t *const end = k.watches.end(stable);
    bool rewritten = false;
    while (p != end) {
      Watch watch{*p};
      if (watch.binary()) {
        if (!rewritten && watch.other() == old_other) {
          *p = binary_watch(new_other).raw;
          rewritten = true;
        }
        p++;
      } else {
        p += 2;
      }
    }
    assert(rewritten);
  }
  substitute_stage_trace(k, "rewrite-stable-directions");

#ifdef TACTIX_ENABLE_EVENT_TRACE
  for (const BinaryImage &stored : images) {
    const ExpandedBinaryImage image = expand_image(stored);
    const bool changed_image =
        image.tautological || image.target_a == image.target_b ||
        kernel_binary_key(image.source_a, image.source_b) !=
            kernel_binary_key(image.target_a, image.target_b);
    if (changed_image)
      event_trace_binary_lifecycle(
          k, image.tautological ? "sweep-physical-tautology"
                               : image.target_a == image.target_b
                                     ? "sweep-physical-unit"
                                     : "sweep-physical-rewrite",
          image.source_a, image.source_b,
          kernel_binary_id(k, image.source_a, image.source_b),
          kernel_binary_copies(k, image.source_a, image.source_b));
  }
#endif

  // Proof and clause identity use the one-image-per-undirected-clause order
  // above.  Sparse reconstruction is directed in Kissat: each retired
  // literal appends its own watch sequence to its representative only after
  // that complete source list has been visited (substitute.c:294-363).  Reuse
  // the released image storage for those directed moves so two retired
  // endpoints can land in their two independently correct destination orders.
  std::vector<Lit> affected_binary_owners;
  if (session) {
    affected_binary_owners.reserve(2 * images.size());
    for (const BinaryImage &image : images) {
      affected_binary_owners.push_back(image.source_a);
      affected_binary_owners.push_back(image.source_b);
    }
    std::sort(affected_binary_owners.begin(),
              affected_binary_owners.end());
    affected_binary_owners.erase(
        std::unique(affected_binary_owners.begin(),
                    affected_binary_owners.end()),
        affected_binary_owners.end());
  }
  std::deque<BinaryImage>().swap(images);
  const auto snapshot_directed = [&](Lit lit) {
    assert(substitutable(lit));
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary())
        images.push_back({lit, watch.other()});
  };
  if (session) {
    // Sweep retires one source at a time in equivalence-emission order.  The
    // session already computed that order to replay its logical dense stream;
    // use the same order for the physical watch mirrors that search will later
    // consume.  Import-order reconstruction gave the next Sweep environments
    // the right ledger but left search with a different binary propagation
    // order once sparse mode resumed.
    for (const Lit lit : session->retiring_sources())
      snapshot_directed(lit);
#ifndef NDEBUG
    // Contract, not an optimization: the compaction below drops EVERY binary
    // incident to a substitutable literal, and only a snapshotted direction is
    // rebuilt.  A literal the session fails to name therefore loses its mirror
    // while the opposite direction survives, rewritten in place — a
    // half-watched clause that still propagates but owns no proof identity.
    {
      std::vector<uint8_t> named(nlits, 0);
      for (const Lit lit : session->retiring_sources())
        named[lit] = 1;
      for (Lit l = 0; l < nlits; l++)
        assert((!substitutable(l) || named[l]) &&
               "sweep session omitted a substitutable literal");
    }
#endif
  } else {
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        if (substitutable(lit))
          snapshot_directed(lit);
      }
  }
  substitute_stage_trace(k, "rewrite-directed-images", images.size(), 0,
                         sizeof(BinaryImage));

  if (connected_only) {
    // Every snapshotted image is exactly one binary with at least one marked
    // endpoint. Remove moved/retired directions in one pass over each watch
    // list. A stable direction rewritten above now has representative
    // endpoints and stays at its original position.
    const auto compact_owner = [&](Lit lit) {
        k.watches.compact(lit, [&](WatchEntry watch) {
          return !watch.binary() ||
                 (!substitutable(lit) && !substitutable(watch.other()));
        });
    };
    if (session)
      for (const Lit lit : affected_binary_owners)
        compact_owner(lit);
    else
      for (const Var variable : variable_order)
        for (uint32_t sign = 0; sign < 2; sign++)
          compact_owner(make_lit(variable, sign != 0));
  } else {
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++)
        k.watches.compact(make_lit(variable, sign != 0),
                          [](WatchEntry w) { return !w.binary(); });
  }
  substitute_stage_trace(k, "rewrite-binary-compact", k.watches.arena.size(),
                         k.watches.holes);
  std::vector<Var>().swap(variable_order);
  if (session)
    sparse_seen.clear();
  else
    std::vector<uint8_t>().swap(seen);
  substitute_stage_trace(k, "rewrite-binary-scratch-released");
  // Every forthcoming mirror is known from the immutable image. Pack the
  // current prefixes once into exact final per-literal slices; ordinary pushes
  // below then fill reserved words without global arena growth or relocation.
  if (!session) {
    k.watches.begin_batch_reserve();
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      if (image.tautological || image.target_a == image.target_b)
        continue;
      assert(substitutable(image.source_a));
      k.watches.reserve_batch_word(image.target_a);
    }
    k.watches.finish_batch_reserve(false);
  }
  substitute_stage_trace(k, "rewrite-binary-reserved", k.watches.arena.size(),
                         k.watches.arena.capacity());
  size_t rebuilt_images = 0;
  for (const BinaryImage &stored : images) {
    const ExpandedBinaryImage image = expand_image(stored);
    if (image.tautological || image.target_a == image.target_b)
      continue;
    const bool moved_other = substitutable(image.source_b);
    const bool publish =
        !moved_other || kernel_import_lit_order(k, image.source_a) <
                            kernel_import_lit_order(k, image.source_b);
    uint64_t id = 0;
    if (lrat) {
      const uint64_t target =
          kernel_binary_key(image.target_a, image.target_b);
      const auto it = std::lower_bound(target_keys.begin(), target_keys.end(),
                                       target);
      assert(it != target_keys.end() && *it == target);
      id = target_ids[static_cast<size_t>(it - target_keys.begin())];
      assert(id);
    } else if (publish)
      id = ++k.next_clause_id;
    const bool already_live =
        kernel_binary_copies(k, image.target_a, image.target_b, 1) ||
        kernel_binary_copies(k, image.target_b, image.target_a, 1);
    k.watches.push_binary(image.target_a, image.target_b);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_binary_lifecycle(k, "sweep-physical-target", image.target_a,
                                 image.target_b, id,
                                 kernel_binary_copies(k, image.target_a,
                                                      image.target_b));
#endif
    if (publish)
      kernel_publish_binary_clause(k, image.target_a, image.target_b, id,
                                   !already_live);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (!(++rebuilt_images % 1000000))
      substitute_stage_trace(k, "rewrite-binary-progress", rebuilt_images,
                             k.watches.arena.size(), k.watches.holes);
#else
    ++rebuilt_images;
#endif
  }
  substitute_stage_trace(k, "rewrite-binary-end", rebuilt_images,
                         k.watches.arena.size(), k.watches.holes);
  std::deque<BinaryImage>().swap(images);
  if (session)
    sparse_seen.clear();
  else
    seen.assign(nlits, 0);
  substitute_stage_trace(k, "rewrite-large-scratch-restored", seen.size());
  const uint64_t subst_binaries = substitute_time ? now_nanos() : 0;
  if (refuted)
    return ApplyResult::Unsat;

  if (flush_units())
    return ApplyResult::Unsat;
  const uint64_t subst_binary_units = substitute_time ? now_nanos() : 0;
  if (rewrite_large_clauses() == ApplyResult::Unsat)
    return ApplyResult::Unsat;
  substitute_stage_trace(k, "rewrite-large-end", k.arena.size());
  const uint64_t subst_large = substitute_time ? now_nanos() : 0;
  if (flush_units())
    return ApplyResult::Unsat;
  std::vector<uint8_t>().swap(seen);
  sparse_seen.clear();
  std::vector<Lit>().swap(pending.units);
  std::vector<uint64_t>().swap(pending.ids);
  substitute_stage_trace(k, "rewrite-large-scratch-released");

  // (E) delete the transient equivalence lemmas (keep the core clean).
  if (k.proof)
    for_each_substitutable([&](Lit l) {
      const Lit lits2[2] = {negate(l), representative(l)};
      k.proof->del(k.next_clause_id, lrat ? lemma_id(l) : 0, lits2, 2);
    });

  // (F) reconstruction witnesses, once per substituted variable: the
  // order-independent symmetric pair for l ≡ s = repr[l], {¬l, s} witnessed by
  // ¬l and {l, ¬s} witnessed by l, so kernel_extend_model recovers l's value
  // from s regardless of any search assignment to l.
  std::vector<Var> witness_order;
  if (session) {
    witness_order.reserve(session->retiring_sources().size() / 2);
    for (const Lit l : session->retiring_sources())
      if (!negated(l))
        witness_order.push_back(var_of(l));
    std::sort(witness_order.begin(), witness_order.end(), [&](Var a, Var b) {
      return kernel_import_var_order(k, a) < kernel_import_var_order(k, b);
    });
    witness_order.erase(
        std::unique(witness_order.begin(), witness_order.end()),
        witness_order.end());
  } else {
    kernel_vars_in_import_order(k, witness_order);
  }
  size_t witness_vars = 0;
  for (const Var v : witness_order)
    witness_vars += substitutable(make_lit(v, false));
  k.retired_events.reserve(k.retired_events.size() + witness_vars);
  k.substituted_events.reserve(k.substituted_events.size() + witness_vars);
  k.reconstruction.reserve(k.reconstruction.size() + 6 * witness_vars);
  substitute_stage_trace(k, "rewrite-witness-reserved", witness_vars,
                         k.reconstruction.capacity(),
                         k.retired_events.capacity());
  for (const Var v : witness_order) {
    const Lit l = make_lit(v, false);
    if (!substitutable(l))
      continue;
    const Lit s = representative(l);
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
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    rule_journal_note_variable_status(k, v, "substituted");
#endif
    k.var_status[v] = VarStatus::Substituted;
    k.retired_events.push_back(v);
    k.substituted_events.push_back(v);
    kernel_push_witness(k, negate(l), rest_a, 1); // clause {s, ¬l}
    const Lit rest_b[1] = {negate(s)};
    kernel_push_witness(k, l, rest_b, 1); // clause {¬s, l}
  }
  const uint64_t subst_witnesses = substitute_time ? now_nanos() : 0;
  std::vector<Lit>().swap(r.repr);
  std::vector<Lit>().swap(r.failed_units);
  std::vector<Lit>().swap(r.failed_witness);
  std::vector<uint8_t>().swap(live_sub);
  std::vector<uint64_t>().swap(lemma);
  substitute_stage_trace(k, "rewrite-representatives-released");

  if (changed && session) {
    session->changed = true;
  } else if (changed) {
    if (arena_order != SubstitutionArenaOrder::Policy) {
      ArenaOrdering *const saved = k.arena_ordering;
      k.arena_ordering = position_order;
      kernel_collect_garbage(k); // reclaim without moving replacements to tail
      k.arena_ordering = saved;
      substitute_stage_trace(k, "rewrite-collected", k.arena.size(),
                             k.watches.arena.size());
    } else {
      kernel_collect_garbage(k); // reclaim the garbaged large clauses
      substitute_stage_trace(k, "rewrite-collected", k.arena.size(),
                             k.watches.arena.size());
    }
  }
  if (substitute_time) {
    const uint64_t subst_done = now_nanos();
    std::fprintf(stderr,
                 "[substitute-rewrite-time] ms: lemmas %.0f snapshot %.0f "
                 "binaries %.0f bin-units %.0f large %.0f post-large %.0f "
                 "collect %.0f total %.0f; images %zu\n",
                 (subst_lemmas - subst_t0) / 1e6,
                 (subst_snapshot - subst_lemmas) / 1e6,
                 (subst_binaries - subst_snapshot) / 1e6,
                 (subst_binary_units - subst_binaries) / 1e6,
                 (subst_large - subst_binary_units) / 1e6,
                 (subst_witnesses - subst_large) / 1e6,
                 (subst_done - subst_witnesses) / 1e6,
                 (subst_done - subst_t0) / 1e6, image_count);
  }
  substitute_stage_trace(k, "rewrite-exit", changed, k.arena.size(),
                         k.watches.arena.size());
  return changed ? ApplyResult::Ok : ApplyResult::NoOp;
}

} // namespace

// Canonical instance: one full round. Detect SCCs; on x ≡ ¬x refute, otherwise
// substitute every non-trivial SCC onto its representative.
ApplyResult substitute_apply(Kernel &k) {
  assert(substitute_guard(k));
  const uint64_t apply_t0 = substitute_time ? now_nanos() : 0;
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  const uint64_t root_t1 = substitute_time ? now_nanos() : 0;
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k, false);
  substitute_stage_trace(k, "scc-return", r.nontrivial_count,
                         r.failed_units.size(), r.repr.capacity());
#ifdef TACTIX_ENABLE_EVENT_TRACE
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    event_trace_substitute_map(k, l, r.repr[l]);
#endif
  substitute_stage_trace(k, "scc-event-map-end");
  const uint64_t scc_t1 = substitute_time ? now_nanos() : 0;
  if (r.contradiction) {
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  const ApplyResult failed = apply_failed_root_units(k, r);
  substitute_stage_trace(k, "scc-failed-units-end", r.failed_units.size());
  if (failed == ApplyResult::Unsat)
    return failed;
  k.stats.substitute_sccs += r.nontrivial_count; // counted before the
  // Every round also runs Kissat's `substitute_clauses`: even with no
  // non-trivial SCC, root units produced by the preceding Sweep can satisfy
  // large clauses or shrink them to binaries.  Returning here used to leave
  // those clauses stale and was the first FmlaEquivChain divergence after an
  // otherwise identical Sweep consequence set.
  std::vector<uint8_t> sub(2 * k.nvars, 0);
  substitute_stage_trace(k, "scc-sub-map", sub.size(), sub.capacity());
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    if (r.repr[l] != INVALID_LIT && r.repr[l] != l) {
      sub[l] = 1;
      k.stats.substitute_scc_lits++;
    }
  const uint64_t mark_t1 = substitute_time ? now_nanos() : 0;
  const ApplyResult rewritten = do_substitution(
      k, r, std::move(sub),
      SubstitutionArenaOrder::PreserveAllClausePositions);
  if (substitute_time) {
    const uint64_t rewrite_t1 = now_nanos();
    std::fprintf(stderr,
                 "[substitute-time] round %llu ms: root %.0f scc %.0f "
                 "mark %.0f rewrite %.0f total %.0f; sccs %llu\n",
                 (unsigned long long)k.stats.substitute_rounds,
                 (root_t1 - apply_t0) / 1e6, (scc_t1 - root_t1) / 1e6,
                 (mark_t1 - scc_t1) / 1e6,
                 (rewrite_t1 - mark_t1) / 1e6,
                 (rewrite_t1 - apply_t0) / 1e6,
                 (unsigned long long)r.nontrivial_count);
  }
  if (rewritten == ApplyResult::NoOp && failed == ApplyResult::Ok)
    return ApplyResult::Ok;
  return rewritten;
}

ApplyResult substitute_apply_complete(Kernel &k, uint32_t max_rounds) {
  const auto active = [&] {
    const uint64_t retired = static_cast<uint64_t>(kernel_root_fixed(k)) +
                             k.stats.bve_eliminated +
                             k.stats.substitute_vars;
    return uint64_t{k.nvars} > retired ? uint64_t{k.nvars} - retired : 0;
  };
  ApplyResult result = ApplyResult::NoOp;
  for (uint32_t round = 0; round < max_rounds; round++) {
    const uint64_t active_before = active();
    const ApplyResult current = substitute_apply(k);
    if (current == ApplyResult::Unsat)
      return current;
    if (current == ApplyResult::Ok)
      result = current;
    // Kissat's `substitute_round` continues only when this round actually
    // removed variables. Other root/formula cleanup can still report Ok, but
    // does not justify another whole SCC pass.
    if (active() == active_before)
      break;
  }
  return result;
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
  SccResult r = substitute_find_sccs(k, false);
  if (r.contradiction) {
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  k.stats.substitute_sccs += r.nontrivial_count;
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
      k, r, std::move(sub), SubstitutionArenaOrder::PreserveClausePosition);
  k.arena_ordering = saved_order;
  return result;
}

ApplyResult substitute_apply_equivalences(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &equivalences) {
  return substitute_apply_equivalences_impl(k, equivalences, true);
}

ApplyResult substitute_apply_sweep_equivalences(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &equivalences,
    SweepSubstitutionSession &session) {
  if (equivalences.empty())
    return ApplyResult::NoOp;
  assert(k.level == 0 && !k.conflict_valid);

  // Sweep published both implication binaries before calling in, so the ledger
  // owes them a stream position whatever the classes below decide.  Declining
  // every class is a normal outcome — a proved endpoint can be root-fixed by
  // the units of the same batch — and returning first left those binaries live
  // in the watch lists but invisible to every later environment.
  session.sync_new_binaries(k, equivalences);

  // Sweep has just inserted both implication binaries for every pair.  Its
  // resumable loop needs those identities substituted before choosing the next
  // seed, but a whole-formula Tarjan pass here makes that bridge O(seeds * F).
  // Build exactly the equivalence classes stated by this emission batch; any
  // older/larger SCC remains visible to the normal Substitute slot immediately
  // after Sweep.  This is complete for the requested rewrite and preserves the
  // same minimum-import-order representative as substitute_find_sccs.
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
  // Kissat proves one equivalence at a time and substitutes it before the next
  // is even attempted, so the retiring literal of each merge releases its whole
  // watch stream into the winner's, in merge order (sweep.c `sweep_equivalence`
  // -> `substitute_connected_clauses`). A literal that lost an earlier merge of
  // the same batch therefore travels inside the block of the literal it lost
  // to, not as a peer of it. Record the merge forest here, and read that order
  // back below; the final representative map alone cannot express it.
  std::vector<std::vector<Lit>> absorbed(members.size());
  const auto unite = [&](Lit a, Lit b) {
    uint32_t ra = find(index_of(a)), rb = find(index_of(b));
    if (ra == rb)
      return;
    if (kernel_import_lit_order(k, members[rb]) <
        kernel_import_lit_order(k, members[ra]))
      std::swap(ra, rb);
    parent[rb] = ra;
    absorbed[ra].push_back(members[rb]);
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
  std::vector<uint32_t> class_size(members.size(), 0);
  for (uint32_t i = 0; i < members.size(); i++)
    class_size[find(i)]++;
  for (uint32_t i = 0; i < members.size(); i++) {
    const uint32_t root = find(i);
    if (i == root && class_size[root] > 1)
      r.nontrivial_reps.push_back(members[root]);
  }
  std::sort(r.nontrivial_reps.begin(), r.nontrivial_reps.end(),
            [&](Lit a, Lit b) {
              return kernel_import_lit_order(k, a) <
                     kernel_import_lit_order(k, b);
            });
  r.nontrivial_count = r.nontrivial_reps.size();

  k.stats.substitute_rounds++;
  k.stats.substitute_sccs += r.nontrivial_count;
  std::vector<std::pair<Lit, Lit>> sparse_map;
  sparse_map.reserve(members.size());
  for (uint32_t i = 0; i < members.size(); i++) {
    const Lit l = members[i];
    const Lit image = members[find(i)];
    if (image != l) {
      sparse_map.emplace_back(l, image);
      k.stats.substitute_scc_lits++;
    }
  }

  // Pre-order walk of the merge forest: a retiring literal is followed by
  // everything it had already absorbed, which is exactly the stream Kissat
  // hands to `substitute_connected_clauses` at the moment that literal retires.
  // Both polarities are separate roots and their streams never mix, so the
  // relative order of the two halves is not observable.
  std::vector<Lit> sources;
  sources.reserve(members.size());
  {
    std::vector<uint32_t> pending;
    const auto descend = [&](uint32_t node) {
      for (auto it = absorbed[node].rbegin(); it != absorbed[node].rend(); ++it)
        pending.push_back(index_of(*it));
    };
    for (uint32_t i = 0; i < members.size(); i++) {
      if (find(i) != i)
        continue; // reached from the root that absorbed it
      descend(i);
      while (!pending.empty()) {
        const uint32_t node = pending.back();
        pending.pop_back();
        sources.push_back(members[node]);
        descend(node);
      }
    }
  }

  session.rewrite_binary_occurrences(k, sparse_map, sources);
  ArenaOrdering *const saved_order = k.arena_ordering;
  k.arena_ordering = nullptr;
  const ApplyResult result = do_substitution(
      k, r, {}, SubstitutionArenaOrder::PreserveClausePosition,
      &session);
  k.arena_ordering = saved_order;
  session.flush_moves(k);
  // `rewrite_binary_occurrences` predicts the dense stream before the physical
  // rewrite, while units and large-clause collapses inside `do_substitution`
  // can decline or replace those images. Restore the stated membership
  // invariant before the next seed consumes the ledger.
  session.sync_new_binaries(k, equivalences);
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
  SccResult r = substitute_find_sccs(k, false);
  if (r.contradiction) { // a global x ≡ ¬x refutes regardless of the request
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  // substitute only members whose representative is rep or its mirror ¬rep; a
  // dissolved SCC marks nothing and yields NoOp
  k.stats.substitute_sccs += r.nontrivial_count;
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
  return do_substitution(k, r, std::move(sub));
}

TACTIX_REGISTER_RULE(Substitute, RuleTier::Inprocess, substitute_guard,
                     substitute_apply, substitute_instances,
                     substitute_apply_instance);

} // namespace tactix

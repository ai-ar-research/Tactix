// See substitute.hpp for the rule's contract. The SCC pass is transliterated
// from kissat decompose.c/substitute.c and cadical decompose.cpp (iterative
// Tarjan), deliberately not CryptoMiniSat's recursive sccfinder: its
// max_scc_depth early-return silently drops deep SCCs, a completeness loss,
// whereas an explicit work stack survives arbitrarily deep chains.
#include "rules/substitute/substitute.hpp"

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
      ref += clause_words(c->size);
    }
    assert(written == refs.size());
    refs.resize(written);
  }
};

// Sweep rewrites each connected clause in place (`sweep.c`'s
// `substitute_connected_clauses`).  Tactix clauses cannot shrink in place: the
// arena record has no separate allocation-size field, so a replacement is
// appended and the old record is garbaged.  Preserve the same *logical* pose at
// collection time by assigning every large replacement the ordinal of its old
// record.  This is a general representation bridge, not a candidate-order
// heuristic: all untouched clauses keep their ordinal and binary/unit
// collapses simply leave a hole.
} // namespace

void SweepSubstitutionSession::begin(const Kernel &k) {
  original_words = static_cast<Ref>(k.arena.size());
  changed = false;
  origin.clear();
  latest.clear();
  dense_occurrences.clear();
  if (k.dense) {
    dense_occurrences.resize(2 * k.nvars);
    for (Lit lit = 0; lit < 2 * k.nvars; lit++) {
      std::vector<DenseOccurrence> &ordered = dense_occurrences[lit];
      ordered.reserve(k.watches.size(lit) + k.dense->large[lit].size());
      for (const WatchEntry watch : k.watches.entries(lit))
        if (watch.binary())
          ordered.push_back({true, watch.other()});
      for (const Ref ref : k.dense->large[lit]) {
        const Clause *clause = clause_at(k.arena, ref);
        if (!clause->garbage() && !clause->redundant())
          ordered.push_back({false, ref});
      }
    }
  }
}

void SweepSubstitutionSession::sync_new_binaries(
    const Kernel &k,
    const std::vector<std::pair<Lit, Lit>> &equivalences) {
  if (dense_occurrences.empty())
    return;
  std::vector<Lit> touched;
  touched.reserve(4 * equivalences.size());
  for (const auto &eq : equivalences)
    touched.insert(touched.end(),
                   {eq.first, negate(eq.first), eq.second, negate(eq.second)});
  std::sort(touched.begin(), touched.end());
  touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
  for (const Lit lit : touched) {
    std::unordered_map<Lit, size_t> known;
    for (const DenseOccurrence occurrence : dense_occurrences[lit])
      if (occurrence.binary)
        known[static_cast<Lit>(occurrence.payload)]++;
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary())
        continue;
      const auto found = known.find(watch.other());
      if (found != known.end() && found->second) {
        if (!--found->second)
          known.erase(found);
        continue;
      }
      // The physical Tactix watch sub-list can have a different order from the
      // logical Kissat-order ledger after a prior inline substitution. Match
      // the existing multiset, not a prefix; only unmatched watches are new
      // equivalence binaries, in the creation order observed here.
      dense_occurrences[lit].push_back({true, watch.other()});
    }
    assert(known.empty());
  }
}

void SweepSubstitutionSession::rewrite_binary_occurrences(
    const Kernel &k, const std::vector<Lit> &repr,
    const std::vector<uint8_t> &sub) {
  if (dense_occurrences.empty())
    return;
  struct Move {
    Lit destination;
    Lit other;
  };
  std::vector<Move> moved;
  std::vector<Var> variable_order(k.nvars);
  kernel_vars_in_import_order(k, variable_order);
  for (const Var variable : variable_order)
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit owner = make_lit(variable, sign != 0);
      const Lit target_owner = sub[owner] ? repr[owner] : owner;
      std::vector<DenseOccurrence> &ordered = dense_occurrences[owner];
      size_t kept = 0;
      for (DenseOccurrence occurrence : ordered) {
        if (!occurrence.binary) {
          ordered[kept++] = occurrence;
          continue;
        }
        const Lit other = static_cast<Lit>(occurrence.payload);
        const Lit target_other = sub[other] ? repr[other] : other;
        if (target_owner == target_other ||
            target_owner == negate(target_other))
          continue;
        if (target_owner == owner) {
          occurrence.payload = target_other;
          ordered[kept++] = occurrence;
        } else {
          moved.push_back({target_owner, target_other});
        }
      }
      ordered.resize(kept);
    }
  for (const Move move : moved)
    dense_occurrences[move.destination].push_back({true, move.other});
}

void SweepSubstitutionSession::remove_large(Kernel &k, Ref source) {
  const Clause *const old = clause_at(k.arena, source);
  if (old->redundant())
    return; // dense Sweep indexes irredundant clauses only
  for (const Lit lit : *old) {
    if (k.dense) {
      std::vector<Ref> &occ = k.dense->large[lit];
      const auto at = std::find(occ.begin(), occ.end(), source);
      assert(at != occ.end());
      occ.erase(at);
    }
    if (!dense_occurrences.empty()) {
      std::vector<DenseOccurrence> &ordered = dense_occurrences[lit];
      const auto at = std::find_if(
          ordered.begin(), ordered.end(), [&](DenseOccurrence occurrence) {
            return !occurrence.binary && occurrence.payload == source;
          });
      assert(at != ordered.end());
      ordered.erase(at);
    }
  }
}

void SweepSubstitutionSession::collapse_large_to_binary(Kernel &k, Ref source,
                                                         Lit a, Lit b,
                                                         bool append_a,
                                                         bool append_b) {
  const Clause *const old = clause_at(k.arena, source);
  if (old->redundant())
    return; // dense Sweep indexes irredundant clauses only
  for (const Lit lit : *old) {
    if (k.dense) {
      std::vector<Ref> &occ = k.dense->large[lit];
      const auto at = std::find(occ.begin(), occ.end(), source);
      assert(at != occ.end());
      occ.erase(at);
    }
    if (!dense_occurrences.empty()) {
      std::vector<DenseOccurrence> &ordered = dense_occurrences[lit];
      const auto at = std::find_if(
          ordered.begin(), ordered.end(), [&](DenseOccurrence occurrence) {
            return !occurrence.binary && occurrence.payload == source;
          });
      assert(at != ordered.end());
      if (lit == a && !append_a)
        *at = {true, b};
      else if (lit == b && !append_b)
        *at = {true, a};
      else
        ordered.erase(at);
    }
  }
  if (!dense_occurrences.empty()) {
    const auto contains = [&](Lit lit) {
      return std::find(old->begin(), old->end(), lit) != old->end();
    };
    if (append_a || !contains(a))
      dense_occurrences[a].push_back({true, b});
    if (append_b || !contains(b))
      dense_occurrences[b].push_back({true, a});
  }
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
  // semantic.
  if (k.dense && !old->redundant()) {
    // Remove the append performed by `kernel_add_large_clause`; the loop below
    // puts each image occurrence at its Kissat position.
    for (const Lit lit : *image) {
      std::vector<Ref> &occ = k.dense->large[lit];
      const auto appended = std::find(occ.rbegin(), occ.rend(), replacement);
      assert(appended != occ.rend());
      occ.erase(std::next(appended).base());
    }

    for (const Lit lit : *old) {
      std::vector<Ref> &occ = k.dense->large[lit];
      const auto at = std::find(occ.begin(), occ.end(), source);
      assert(at != occ.end());
      if (std::find(image->begin(), image->end(), lit) != image->end())
        *at = replacement;
      else
        occ.erase(at);
    }
    for (const Lit lit : *image)
      if (std::find(old->begin(), old->end(), lit) == old->end())
        k.dense->large[lit].push_back(replacement);
  }
  if (!old->redundant() && !dense_occurrences.empty()) {
    for (const Lit lit : *old) {
      std::vector<DenseOccurrence> &ordered = dense_occurrences[lit];
      const auto at = std::find_if(
          ordered.begin(), ordered.end(), [&](DenseOccurrence occurrence) {
            return !occurrence.binary && occurrence.payload == source;
          });
      assert(at != ordered.end());
      if (std::find(image->begin(), image->end(), lit) != image->end())
        at->payload = replacement;
      else
        ordered.erase(at);
    }
    for (const Lit lit : *image)
      if (std::find(old->begin(), old->end(), lit) == old->end())
        dense_occurrences[lit].push_back({false, replacement});
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
    ref += clause_words(clause->size);
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
  const uint64_t words = k.arena.size();
  ArenaOrdering *const saved = k.arena_ordering;
  k.arena_ordering = this;
  kernel_collect_garbage(k);
  k.arena_ordering = saved;
  changed = false;
  return words;
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
  std::vector<uint8_t> live_sub = std::move(sub);
  for (Lit l = 0; l < nlits; l++)
    live_sub[l] = static_cast<uint8_t>(live_sub[l] && k.values[l] == 0 &&
                                       !kernel_var_retired(k, var_of(l)));
  substitute_stage_trace(k, "rewrite-live-sub", live_sub.size(),
                         live_sub.capacity());
  auto substitutable = [&](Lit l) { return l < nlits && live_sub[l]; };

  // (B) transient equivalence lemmas {¬l, repr[l]}, one per member literal,
  // derived before any binary mutation so big_path_chain reads the intact
  // graph. RUP: assume l, ¬repr[l]; the path l→…→repr[l] forces repr[l].
  // Clause ids are antecedents only in LRAT. DRAT ignores them, so avoid a
  // 16-byte-per-variable zero table on the production proof path.
  std::vector<uint64_t> lemma;
  if (lrat)
    lemma.assign(nlits, 0);
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
    if (lrat)
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
  const uint64_t subst_lemmas = substitute_time ? now_nanos() : 0;
  substitute_stage_trace(k, "rewrite-lemmas", lemma.size(), lemma.capacity());

  std::vector<uint8_t> seen(nlits, 0); // dedup scratch for a rewritten clause
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
      const Lit mp = substitutable(m) ? r.repr[m] : m;
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
            chain.push_back(lemma[m]);
        }
        continue;
      }
      if (substitutable(m) && lrat)
        chain.push_back(lemma[m]);
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
      const Lit mp = substitutable(m) ? r.repr[m] : m;
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
    const bool credundant = c->redundant();
    const uint32_t cglue = c->glue();
    const uint64_t cid = c->id();
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
    // shared replacement tail; glue 0 for irredundant (review-aligned);
    // c dangles after (arena may reallocate) and is not touched again
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
    const ReplaceOutcome replacement = kernel_replace_clause(
        k, c, id, out.data(), static_cast<uint32_t>(out.size()), credundant,
        credundant ? kernel_shrunken_glue(
                         k, cglue, static_cast<uint32_t>(out.size()))
                   : 0,
        arena_order != SubstitutionArenaOrder::Policy ? &replacement_ref
                                                       : nullptr);
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
            append_first |= r.repr[old_lit] == out[0];
            append_second |= r.repr[old_lit] == out[1];
          }
        session->collapse_large_to_binary(k, ref, out[0], out[1],
                                          append_first, append_second);
      } else if (replacement == ReplaceOutcome::Unit ||
                 replacement == ReplaceOutcome::Refuted) {
        session->remove_large(k, ref);
      }
    }
    if (replacement_ref != INVALID_REF) {
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
      const Lit mp = substitutable(m) ? r.repr[m] : m;
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
  // During preprocessing the dense index covers every live large clause
  // (there are no learned clauses yet).  Sweep substitutes tiny, freshly
  // proved classes one seed at a time; collecting just their connected clauses
  // avoids rescanning the complete input arena after every seed.  Later rounds
  // can contain redundant clauses, which are intentionally absent from the
  // dense index, so they retain the complete arena snapshot.
  if (arena_order == SubstitutionArenaOrder::PreserveClausePosition &&
      k.stats.conflicts == 0 && k.dense) {
    std::vector<Ref> refs;
    // One bit per arena word plus one Ref per selected clause is much smaller
    // than a node-based unordered_set and yields the same arena-order list.
    std::vector<uint64_t> connected((k.arena.size() + 63) / 64, 0);
    for (Lit l = 0; l < nlits; l++) {
      if (!substitutable(l))
        continue;
      for (const Ref ref : k.dense->large[l]) {
        const Clause *c = clause_at(k.arena, ref);
        const uint64_t bit = UINT64_C(1) << (ref & 63);
        uint64_t &word = connected[ref >> 6];
        if (!c->garbage() && !(word & bit)) {
          word |= bit;
          refs.push_back(ref);
        }
      }
    }
    std::sort(refs.begin(), refs.end()); // preserve current arena traversal
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
        const uint32_t words = clause_words(c->size);
        if (!c->garbage())
          fn(ref);
        ref += words;
      }
    };
    reserve_large_replacements(visit);
    for (Ref ref = 0; ref < original_end;) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_words(c->size);
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
  std::vector<Var> variable_order(k.nvars);
  substitute_stage_trace(k, "rewrite-variable-order", variable_order.size(),
                         variable_order.capacity());
  kernel_vars_in_import_order(k, variable_order);

  struct BinaryImage {
    Lit source_a, source_b;
  };
  struct ExpandedBinaryImage {
    Lit source_a, source_b;
    Lit target_a, target_b;
    bool tautological;
  };
  const auto expand_image = [&](const BinaryImage &source) {
    const Lit a = substitutable(source.source_a) ? r.repr[source.source_a]
                                                  : source.source_a;
    const Lit b = substitutable(source.source_b) ? r.repr[source.source_b]
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
  for (const Var variable : variable_order) {
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit lit = make_lit(variable, sign != 0);
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (!w.binary() || kernel_import_lit_order(k, lit) >=
                               kernel_import_lit_order(k, w.other()))
          continue;
        const Lit other = w.other();
        // Sweep substitutes only the clauses connected to its freshly proved
        // class.  Snapshotting every unrelated binary on a 48-million-variable
        // formula consumed several GiB and changed their watch positions even
        // though their literals were untouched.
        if (connected_only && !substitutable(lit) && !substitutable(other))
          continue;
        images.push_back({lit, other});
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        const Lit a = substitutable(lit) ? r.repr[lit] : lit;
        const Lit b = substitutable(other) ? r.repr[other] : other;
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
    if (seen[unit])
      return false;
    seen[unit] = 1;
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
        ch.push_back(lemma[image.source_a]);
      if (substitutable(image.source_b))
        ch.push_back(lemma[image.source_b]);
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
    seen[unit] = 0;
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

  // Proof and clause identity use the one-image-per-undirected-clause order
  // above.  Sparse reconstruction is directed in Kissat: each retired
  // literal appends its own watch sequence to its representative only after
  // that complete source list has been visited (substitute.c:294-363).  Reuse
  // the released image storage for those directed moves so two retired
  // endpoints can land in their two independently correct destination orders.
  std::deque<BinaryImage>().swap(images);
  for (const Var variable : variable_order)
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit lit = make_lit(variable, sign != 0);
      if (!substitutable(lit))
        continue;
      for (const WatchEntry watch : k.watches.entries(lit))
        if (watch.binary())
          images.push_back({lit, watch.other()});
    }
  substitute_stage_trace(k, "rewrite-directed-images", images.size(), 0,
                         sizeof(BinaryImage));

  if (connected_only) {
    // Every snapshotted image is exactly one binary with at least one marked
    // endpoint. Remove moved/retired directions in one pass over each watch
    // list. A stable direction rewritten above now has representative
    // endpoints and stays at its original position.
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        k.watches.compact(lit, [&](WatchEntry watch) {
          return !watch.binary() ||
                 (!substitutable(lit) && !substitutable(watch.other()));
        });
      }
  } else {
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++)
        k.watches.compact(make_lit(variable, sign != 0),
                          [](WatchEntry w) { return !w.binary(); });
  }
  substitute_stage_trace(k, "rewrite-binary-compact", k.watches.arena.size(),
                         k.watches.holes);
  std::vector<Var>().swap(variable_order);
  std::vector<uint8_t>().swap(seen);
  substitute_stage_trace(k, "rewrite-binary-scratch-released");
  // Every forthcoming mirror is known from the immutable image. Pack the
  // current prefixes once into exact final per-literal slices; ordinary pushes
  // below then fill reserved words without global arena growth or relocation.
  k.watches.begin_batch_reserve();
  for (const BinaryImage &stored : images) {
    const ExpandedBinaryImage image = expand_image(stored);
    if (image.tautological || image.target_a == image.target_b)
      continue;
    assert(substitutable(image.source_a));
    k.watches.reserve_batch_word(image.target_a);
  }
  k.watches.finish_batch_reserve(false);
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
  std::vector<Lit>().swap(pending.units);
  std::vector<uint64_t>().swap(pending.ids);
  substitute_stage_trace(k, "rewrite-large-scratch-released");

  // (E) delete the transient equivalence lemmas (keep the core clean).
  if (k.proof)
    for (Lit l = 0; l < nlits; l++)
      if (substitutable(l)) {
        const Lit lits2[2] = {negate(l), r.repr[l]};
        k.proof->del(k.next_clause_id, lrat ? lemma[l] : 0, lits2, 2);
      }

  // (F) reconstruction witnesses, once per substituted variable: the
  // order-independent symmetric pair for l ≡ s = repr[l], {¬l, s} witnessed by
  // ¬l and {l, ¬s} witnessed by l, so kernel_extend_model recovers l's value
  // from s regardless of any search assignment to l.
  size_t witness_vars = 0;
  for (Var v = 0; v < k.nvars; v++)
    witness_vars += substitutable(make_lit(v, false));
  k.retired_events.reserve(k.retired_events.size() + witness_vars);
  k.substituted_events.reserve(k.substituted_events.size() + witness_vars);
  k.reconstruction.reserve(k.reconstruction.size() + 6 * witness_vars);
  substitute_stage_trace(k, "rewrite-witness-reserved", witness_vars,
                         k.reconstruction.capacity(),
                         k.retired_events.capacity());
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
  ApplyResult result = ApplyResult::NoOp;
  for (uint32_t round = 0; round < max_rounds; round++) {
    const ApplyResult current = substitute_apply(k);
    if (current == ApplyResult::Unsat)
      return current;
    if (current == ApplyResult::NoOp)
      break;
    result = ApplyResult::Ok;
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
  const auto unite = [&](Lit a, Lit b) {
    uint32_t ra = find(index_of(a)), rb = find(index_of(b));
    if (ra == rb)
      return;
    if (kernel_import_lit_order(k, members[rb]) <
        kernel_import_lit_order(k, members[ra]))
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
  std::sort(r.nontrivial_reps.begin(), r.nontrivial_reps.end(),
            [&](Lit a, Lit b) {
              return kernel_import_lit_order(k, a) <
                     kernel_import_lit_order(k, b);
            });
  r.nontrivial_count = r.nontrivial_reps.size();

  k.stats.substitute_rounds++;
  k.stats.substitute_sccs += r.nontrivial_count;
  std::vector<uint8_t> sub(nlits, 0);
  for (const Lit l : members)
    if (r.repr[l] != l) {
      sub[l] = 1;
      k.stats.substitute_scc_lits++;
    }

  session.sync_new_binaries(k, equivalences);
  session.rewrite_binary_occurrences(k, r.repr, sub);
  ArenaOrdering *const saved_order = k.arena_ordering;
  k.arena_ordering = nullptr;
  const ApplyResult result = do_substitution(
      k, r, std::move(sub), SubstitutionArenaOrder::PreserveClausePosition,
      &session);
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

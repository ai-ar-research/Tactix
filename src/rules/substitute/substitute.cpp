// See substitute.hpp for the contract. The SCC pass is transliterated from
// kissat decompose.c/substitute.c and cadical decompose.cpp (iterative Tarjan
// with an explicit work stack, so deep SCCs are complete).
#include "rules/substitute/substitute.hpp"
#include "strategy/substitute_execution.hpp"

#include "diagnostics/event_trace.hpp"
#include "diagnostics/rule_journal.hpp"
#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"

#include "kernel/watch.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace tactix {

RuleEligibility substitute_guard(const Kernel &k) {
  // Quiescent root. No ¬conflict_valid conjunct: a pure F-rewrite is sound
  // regardless of a recorded conflict (D-SPLIT-3, `docs/dev/calculus.md` §7).
  return root_level_guard(k);
}

SccResult substitute_find_sccs(const Kernel &k, bool collect_instances,
                               bool account_work) {
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  SccResult res;
  res.repr.assign(nlits, INVALID_LIT);
  if (nlits == 0)
    return res;

  // DFS index and low-link share a record: both are read at every step.
  struct SccNode {
    uint32_t dfs = 0; // 1-based DFS index; 0 = unvisited
    uint32_t low = 0;
  };
  std::vector<SccNode> node(nlits);
  std::vector<Lit> tstack; // Tarjan's component stack
  uint32_t next = 1;

  // explicit DFS work stack: (node, resume point in the node's watch list)
  struct Frame {
    Lit v;
    WatchIter it;
  };
  std::vector<Frame> work;
  const auto charge = [&](uint64_t ticks) {
    if (account_work)
      k.stats.substitute_ticks += ticks;
  };

  // Kissat substitute.c builds the SCC graph from ACTIVE variables only;
  // retired identities must not be substituted twice.
  auto active = [&](Lit l) {
    return k.values[l] == 0 && !kernel_var_retired(k, var_of(l));
  };
  // Out-edges of p are the binary watches of ¬p; the arena is not mutated, so a
  // suspended iterator stays valid across the DFS.
  auto out_edges = [&](Lit p) { return k.watches.entries(negate(p)); };

  // `kernel_vars_in_import_order` clears and refills, so pre-sizing here only
  // pays for a zero fill the call throws away.
  std::vector<Var> roots;
  kernel_vars_in_import_order(k, roots);
  for (const Var root_var : roots) {
    for (uint32_t root_sign = 0; root_sign < 2; root_sign++) {
      const Lit root = make_lit(root_var, root_sign != 0);
      if (node[root].dfs != 0 || !active(root))
        continue;
      node[root].dfs = node[root].low = next++;
      const uint32_t mark_root = node[root].dfs;
      bool failed_root = false;
      tstack.push_back(root);
      charge(1 + kernel_cache_lines(k.watches.size(negate(root))));
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
          if (node[q].dfs == 0) {
            // tree edge: save v's resume point (past this edge), descend into q
            work.back().it = ++it;
            node[q].dfs = node[q].low = next++;
            tstack.push_back(q);
            charge(1 + kernel_cache_lines(k.watches.size(negate(q))));
            // may reallocate `work`; touch it no more
            work.push_back({q, out_edges(q).begin()});
            descended = true;
            break;
          }
          // `repr` is written when a component leaves the stack, so a visited
          // literal without a representative is still on-stack.
          if (res.repr[q] == INVALID_LIT && node[q].dfs < node[v].low)
            node[v].low = node[q].dfs; // frond to an on-stack node
        }
        if (descended)
          continue;
        work.back().it = it;

        // Charge Kissat's second watch scan of a finished literal
        // (substitute.c, the INVALID_LIT pop branch), so substitute_ticks and
        // the 10-per-mille second-round test match kissat's.
        charge(1 + kernel_cache_lines(k.watches.size(negate(v))));
        if (node[v].low == node[v].dfs) {
          // v roots an SCC: pop the component off tstack
          size_t start = tstack.size();
          do {
            --start;
          } while (tstack[start] != v);
          Lit rep = tstack[start];
          uint64_t rep_order = kernel_import_lit_order(k, rep);
          for (size_t i = start + 1; i < tstack.size(); i++) {
            const uint64_t order = kernel_import_lit_order(k, tstack[i]);
            if (order < rep_order) {
              rep = tstack[i];
              rep_order = order;
            }
          }
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
            // If the mirror SCC completed during this DFS, root reaches m and
            // ¬m, so ¬root is a unit. Kissat records at most one witness per
            // DFS root.
            if (!failed_root && mirror_rep != INVALID_LIT &&
                node[negate(m)].dfs >= mark_root) {
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
          if (node[done].low < node[parent].low)
            node[parent].low = node[done].low;
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
  return res;
}

namespace {

// x ≡ ¬x: refutation (`docs/dev/calculus.md` V3). {¬x} and {x} are RUP over the
// two paths and resolve to the empty clause; add_rup keeps them visible to the
// checker.
void emit_unsat(Kernel &k, Lit x) {
  const Lit nx = negate(x);
  const uint64_t id_nx = ++k.next_clause_id;
  const uint64_t id_x = ++k.next_clause_id;
  const uint64_t id_e = ++k.next_clause_id;
  if (k.proof) {
    k.proof->add_rup(id_nx, &nx, 1);
    k.proof->add_rup(id_x, &x, 1);
    k.proof->add_empty(id_e);
  }
  k.inconsistent = true;
}

// Emit and close the failed-root units from the SCC DFS. All proof lines are
// emitted before assignments so every path is read from the same graph.
ApplyResult apply_failed_root_units(Kernel &k, const SccResult &r) {
  if (r.failed_units.empty())
    return ApplyResult::NoOp;
  PendingUnits pending;
  pending.init(k);
  for (size_t pos = r.failed_units.size(); pos-- > 0;) {
    const Lit unit = r.failed_units[pos]; // reference pops its unit stack
    if (k.value(unit) > 0)
      continue;
    assert(k.value(unit) == 0 && "failed DFS roots are active at discovery");
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, &unit, 1);
    if (pending.record(k, unit) == PendingUnits::Outcome::Refuted)
      return ApplyResult::Unsat;
  }
  for (const Lit unit : pending.units) {
    if (k.value(unit) > 0)
      continue;
    if (k.value(unit) < 0) {
      kernel_emit_empty(k); // the preceding propagation already refuted
      return ApplyResult::Unsat;
    }
    kernel_assign_unit(k, unit);
    if (inprocess_reach_root(k))
      return ApplyResult::Unsat;
  }
  return pending.units.empty() ? ApplyResult::NoOp : ApplyResult::Ok;
}

// Equivalent-literal rewrite. `sub[l]` marks the non-representative literals to
// rewrite (l → r.repr[l]). Emits transient lemmas {¬l, r.repr[l]}, rewrites
// every occurrence add-before-delete, deletes the lemmas, pushes witnesses once
// per variable and asserts collapsed units un-propagated (M1 owns the cascade).
// Returns Unsat on an empty rewrite, Ok if anything changed, NoOp otherwise.

// Members eligible for equivalence lemmas and retirement: root-fixed or retired
// members are declined (Kissat's `add_representative_equivalences`); the frozen
// SCC map still governs physical rewriting.
inline bool substitution_member_live(const Kernel &k,
                                     const std::vector<uint8_t> &members,
                                     Lit l) {
  return l < members.size() && members[l] && k.values[l] == 0 &&
         !kernel_var_retired(k, var_of(l));
}

// A persistent affine owner certifies the aliases before their endpoint bodies
// disappear. False when it declines or the run stops.
template <class Collect>
bool preserve_affine_aliases(Kernel &k, uint64_t scanned,
                             const Collect &collect) {
  AffineMatrixSearchSession *const affine = k.affine_matrix_search_session;
  if (!affine || !affine->persistent() || !affine->attached_to(k))
    return true;
  if (kernel_terminating(k)) return false;
  std::vector<std::pair<Lit, Lit>> aliases;
  collect(aliases);
  k.stats.substitute_ticks += std::min<uint64_t>(scanned + aliases.size(),
      UINT64_MAX - k.stats.substitute_ticks);
  const auto preserved = affine_matrix_search_preserve_aliases(
      k, *affine, aliases);
  // Expose preservation work to the existing Substitute action meter.
  k.stats.substitute_ticks += std::min(preserved.work,
      UINT64_MAX - k.stats.substitute_ticks);
  return preserved.status == AffineMatrixServiceStatus::Applied ||
         preserved.status == AffineMatrixServiceStatus::NoEvent ||
         preserved.status == AffineMatrixServiceStatus::Disabled;
}

} // namespace

// Ledger slot naming `ref` in `lit`'s stream, or null if never indexed; callers
// must test the result (a miss is a session invariant violation). The stream
// must already be materialized (`prime_large`).
SweepSubstitutionSession::DenseOccurrence *
SweepSubstitutionSession::find_large(const Kernel &k, Lit lit, Ref ref) {
  if (!dense_enabled)
    return nullptr;
  const auto found = dense_occurrences.find(lit);
  assert(found != dense_occurrences.end() && found->second.initialized &&
         "sweep ledger stream materialized after the kernel rewrite");
  if (found == dense_occurrences.end() || !found->second.initialized)
    return nullptr;
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
  std::vector<DenseOccurrence> &ordered = dense_occurrences[lit].entries;
  ordered.erase(ordered.begin() + (at - ordered.data()));
}

void SweepSubstitutionSession::begin(const Kernel &k) {
  changed = false;
  reset_transaction();
  for (const Lit lit : dense_initialized_lits) {
    DenseOccurrenceList &list = dense_occurrences[lit];
    list.entries.clear();
    list.initialized = false;
  }
  dense_initialized_lits.clear();
  for (const Lit lit : ledger_dirty)
    ledger_dirty_mark[lit] = 0;
  ledger_dirty.clear();
  dirty_binary_owners.clear();
  dense_enabled = k.dense;
}

// One scan of each distinct owner's binary prefix answers every image pushed
// onto it.
void SweepSubstitutionSession::classify_existing_binaries(
    const Kernel &k, const std::vector<Lit> &owners,
    const std::vector<Lit> &others, std::vector<uint8_t> &out) {
  const size_t n = owners.size();
  out.assign(n, 0);
  if (!n)
    return;
  const size_t nlits = 2 * static_cast<size_t>(k.nvars);
  if (live_epoch.size() < nlits)
    live_epoch.resize(nlits, 0);
  std::vector<uint32_t> order(n);
  for (size_t i = 0; i < n; i++)
    order[i] = static_cast<uint32_t>(i);
  std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    return owners[a] < owners[b];
  });
  size_t at = 0;
  while (at < n) {
    const Lit owner = owners[order[at]];
    size_t end = at;
    while (end < n && owners[order[end]] == owner)
      end++;
    if (++live_generation == 0) {
      std::fill(live_epoch.begin(), live_epoch.end(), 0);
      live_generation = 1;
    }
    const bool stop_at_large = k.watches.heads[owner].binaries_first();
    for (const WatchEntry w : k.watches.entries(owner)) {
      if (!w.binary()) {
        if (stop_at_large)
          break;
        continue;
      }
      live_epoch[w.other()] = live_generation;
    }
    for (size_t i = at; i < end; i++) {
      const uint32_t image = order[i];
      out[image] =
          static_cast<uint8_t>(live_epoch[others[image]] == live_generation);
    }
    at = end;
  }
}

void SweepSubstitutionSession::mark_ledger_dirty(Lit lit) {
  if (ledger_dirty_mark.size() <= lit)
    ledger_dirty_mark.resize(static_cast<size_t>(lit) + 1, 0);
  if (ledger_dirty_mark[lit])
    return;
  ledger_dirty_mark[lit] = 1;
  ledger_dirty.push_back(lit);
}

std::vector<SweepSubstitutionSession::DenseOccurrence> &
SweepSubstitutionSession::occurrences(const Kernel &k, Lit lit) {
  assert(dense_enabled && k.dense);
  assert(lit < 2 * static_cast<size_t>(k.nvars));
  mark_ledger_dirty(lit);
  DenseOccurrenceList &list = dense_occurrences[lit];
  std::vector<DenseOccurrence> &ordered = list.entries;
  if (list.initialized)
    return ordered;
  assert(ordered.empty());
  list.initialized = true;
  dense_initialized_lits.push_back(lit);
  ordered.reserve(k.watches.size(lit) + k.dense->large_size(lit));
  if (k.dense->track_order) {
    // Reuse Eliminate's exact mixed connected stream when an N1 schedule runs
    // Sweep/Substitute and BVE in one dense scope.
    for (const OccIndex::Entry occurrence : k.dense->ordered[lit]) {
      if (occurrence.binary) {
        ordered.push_back({true, occurrence.payload});
        continue;
      }
      const Clause *clause = clause_at(k.arena, occurrence.payload);
      if (clause->garbage() || clause->redundant() ||
          !clause_contains(clause, lit))
        continue;
      ordered.push_back({false, occurrence.payload});
    }
  } else {
    for (const WatchEntry watch : k.watches.entries(lit))
      if (watch.binary()) {
        ordered.push_back({true, watch.other()});
      }
    for (const Ref ref : k.dense->large[lit]) {
      const Clause *clause = clause_at(k.arena, ref);
      if (!clause->garbage() && !clause->redundant())
        ordered.push_back({false, ref});
    }
  }
  return ordered;
}

void SweepSubstitutionSession::publish_dense_order(Kernel &k,
                                                   const std::vector<Lit> &lits) {
  if (!dense_enabled || !k.dense || !k.dense->track_order)
    return;
  assert(!k.bve_occurrence_count_image);
  for (const Lit lit : lits) {
    const DenseOccurrenceList &source = dense_occurrences[lit];
    assert(source.initialized);
    k.dense->ordered.clear(lit);
    for (const DenseOccurrence occurrence : source.entries) {
      if (occurrence.binary) {
        k.dense->ordered.push(lit, OccIndex::Entry::from_binary(
                                       static_cast<Lit>(occurrence.payload)));
      } else {
        const Ref ref = static_cast<Ref>(occurrence.payload);
        k.dense->ordered.push(lit, OccIndex::Entry::from_large(ref));
      }
    }
  }
}

void SweepSubstitutionSession::prime_equivalence(const Kernel &k, Lit a,
                                                  Lit b) {
  if (!dense_enabled)
    return;
  for (const Lit lit : {a, negate(a), b, negate(b)})
    (void)occurrences(k, lit);
}

// Debug oracle: the dense index must equal a full republication of every
// initialized stream.
bool SweepSubstitutionSession::sync_publication_is_complete(const Kernel &k) {
  if (!dense_enabled || !k.dense || !k.dense->track_order)
    return true;
  for (const Lit lit : dense_initialized_lits) {
    const DenseOccurrenceList &source = dense_occurrences[lit];
    const auto published = k.dense->ordered[lit];
    const auto large = k.dense->ordered.large_refs(lit);
    auto next_large = large.begin();
    size_t at = 0, at_large = 0;
    for (const DenseOccurrence occurrence : source.entries) {
      if (at == published.size())
        return false;
      const OccIndex::Entry &entry = published[at++];
      if (entry.binary != occurrence.binary ||
          entry.payload != occurrence.payload)
        return false;
      if (!occurrence.binary) {
        if (next_large == large.end() ||
            *next_large++ != static_cast<Ref>(occurrence.payload))
          return false;
        ++at_large;
      }
    }
    if (at != published.size() || at_large != large.size() ||
        next_large != large.end())
      return false;
  }
  return true;
}

void SweepSubstitutionSession::sync_new_binaries(
    Kernel &k,
    const std::vector<std::pair<Lit, Lit>> &equivalences) {
  if (!dense_enabled) {
    dirty_binary_owners.clear(); // no ledger to re-pose; end the transaction
    return;
  }
  // Only a dirty owner's binary substream may hold a ledger-only binary (a
  // superseded prediction).
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
  touched.reserve(4 * equivalences.size() + dirty_binary_owners.size() +
                  ledger_dirty.size());
  for (const auto &eq : equivalences)
    touched.insert(touched.end(),
                   {eq.first, negate(eq.first), eq.second, negate(eq.second)});
  touched.insert(touched.end(), dirty_binary_owners.begin(),
                 dirty_binary_owners.end());
  if (k.dense->track_order)
    touched.insert(touched.end(), ledger_dirty.begin(), ledger_dirty.end());
  std::sort(touched.begin(), touched.end());
  touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
  // Provenance already marks every moved, rewritten and collapsed owner; no
  // neighbor expansion is needed.
  for (const Lit lit : touched) {
    assert(lit < 2 * static_cast<size_t>(k.nvars));
    // Watch lists decide which binaries exist; the ledger adds Kissat's dense
    // order. Reconcile both directions as multisets.
    const size_t nlits = 2 * static_cast<size_t>(k.nvars);
    if (sync_epoch.size() < nlits) {
      sync_epoch.resize(nlits, 0);
      sync_count.resize(nlits, 0);
    }
    if (++sync_generation == 0) {
      std::fill(sync_epoch.begin(), sync_epoch.end(), 0);
      sync_generation = 1;
    }
    sync_touched.clear();
    std::vector<DenseOccurrence> &stream = occurrences(k, lit);
    size_t stale = 0;
    for (const DenseOccurrence occurrence : stream)
      if (occurrence.binary) {
        const Lit other = static_cast<Lit>(occurrence.payload);
        if (sync_epoch[other] != sync_generation) {
          sync_epoch[other] = sync_generation;
          sync_count[other] = 0;
          sync_touched.push_back(other);
          stale++;
        }
        sync_count[other]++;
      }
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary())
        continue;
      const Lit other = watch.other();
      if (sync_epoch[other] == sync_generation && sync_count[other]) {
        if (!--sync_count[other])
          stale--;
        continue;
      }
      // Only unmatched watches are new, in the creation order observed here.
      stream.push_back({true, watch.other()});
    }
    // A residue on an unmutated stream means a binary bypassed the session; on
    // a dirty stream it is the superseded prediction. Purge it either way.
    assert((!stale || dirty(lit)) &&
           "sweep ledger holds a binary absent from the watches");
    if (stale) {
      purge_stale_binaries(lit);
    }
    // Publish the same binary order to the physical watches. Only binary words
    // are permuted; nothing allocates.
    auto next_binary = stream.begin();
    uint32_t *word = k.watches.begin(lit);
    const uint32_t *const end = k.watches.end(lit);
    while (word != end) {
      const Watch watch{*word};
      if (!watch.binary()) {
        word += 2;
        continue;
      }
      while (next_binary != stream.end() && !next_binary->binary)
        ++next_binary;
      assert(next_binary != stream.end());
      *word++ = binary_watch(static_cast<Lit>(next_binary->payload)).raw;
      ++next_binary;
    }
    while (next_binary != stream.end() && !next_binary->binary)
      ++next_binary;
    assert(next_binary == stream.end());
  }
  publish_dense_order(k, touched);
  // Every stream this batch could have changed has been reconciled and
  // republished, so the next batch starts from an empty changed set.
  for (const Lit lit : ledger_dirty)
    ledger_dirty_mark[lit] = 0;
  ledger_dirty.clear();
  assert(sync_publication_is_complete(k) &&
         "a ledger stream changed outside the reconciled set");
  dirty_binary_owners.clear();
}

void SweepSubstitutionSession::repose_emitted_equivalence(
    const Kernel &k, Lit a, Lit b) {
  if (!dense_enabled)
    return;
  const auto repose = [&](Lit owner, Lit other) {
    auto &stream = occurrences(k, owner);
    for (size_t i = stream.size(); i; --i) {
      const auto entry = stream[i - 1];
      if (!entry.binary || entry.payload != other)
        continue;
      std::rotate(stream.begin() + (i - 1), stream.begin() + i, stream.end());
      break;
    }
  };
  repose(negate(a), b);
  repose(b, negate(a));
  repose(a, negate(b));
  repose(negate(b), a);
}

void SweepSubstitutionSession::purge_stale_binaries(Lit lit) {
  std::vector<DenseOccurrence> &ordered = dense_occurrences[lit].entries;
  size_t kept = 0;
  for (const DenseOccurrence occurrence : ordered) {
    if (occurrence.binary) {
      const Lit other = static_cast<Lit>(occurrence.payload);
      if (sync_epoch[other] == sync_generation && sync_count[other]) {
        sync_count[other]--;
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
  equivalence_bridges.clear();
}

// Stream index of a retiring literal's large occurrence, keyed by (lit, Ref)
// while the stream is intact.
static uint64_t sweep_stream_key(Lit lit, Ref source) {
  return (static_cast<uint64_t>(lit) << 32) | source;
}

// The retiring literal whose stream releases this clause to `representative`:
// as in Kissat's first `substitute_connected_clauses` call that rewrites one of
// the clause's literals to it, the lowest-ranked such member.
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
    // A surviving large occurrence belongs to the first retiring member that
    // introduces the representative; a final binary is born when the last
    // duplicate goes, so it belongs to the latest member's stream.
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
    const std::vector<Lit> &sources,
    const std::vector<EquivalenceBridge> &bridges) {
  reset_transaction();
  for (const EquivalenceBridge &bridge : bridges) {
    assert(bridge.a != INVALID_LIT && bridge.b != INVALID_LIT &&
           bridge.target_a != INVALID_LIT && bridge.target_b != INVALID_LIT);
    assert(bridge.target_a != bridge.target_b &&
           bridge.target_a != negate(bridge.target_b));
    equivalence_bridges.emplace(kernel_binary_key(bridge.a, bridge.b),
                                bridge);
  }
  dirty_binary_owners.clear();
  // Use the rewrite's own member test: members `do_substitution` declines keep
  // their ledger entries.
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
  // The retiring list is the transaction's membership; `do_substitution` reads
  // it to snapshot watch lists. Record it even without a ledger, or a mirror is
  // lost.
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
  // A binary changes iff an endpoint retires, so the affected owners are the
  // retiring literals and their binary neighbors (Kissat's connected streams).
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
      Lit target_owner, target_other;
      if (!equivalence_bridge_targets(owner, other, &target_owner,
                                      &target_other)) {
        target_owner = member_live(owner) ? requested_image(owner) : owner;
        target_other = member_live(other) ? requested_image(other) : other;
      }
      if (target_owner == target_other ||
          target_owner == negate(target_other)) {
        // Only removed occurrences are dirty; identity entries are not.
        dirty_binary_owners.push_back(owner);
        continue;
      }
      if (target_owner == owner) {
        if (target_other != other) {
          dirty_binary_owners.push_back(owner);
        }
        occurrence.payload = target_other;
        ordered[kept++] = occurrence;
      } else {
        // Every retiring literal is a caller source; keep an unranked move
        // reachable.
        assert(retires && "sweep ledger moved an unranked source stream");
        dirty_binary_owners.push_back(owner);
        dirty_binary_owners.push_back(target_owner);
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

bool SweepSubstitutionSession::equivalence_bridge_targets(Lit a, Lit b,
                                                          Lit *target_a,
                                                          Lit *target_b) const {
  const auto found = equivalence_bridges.find(kernel_binary_key(a, b));
  if (found == equivalence_bridges.end())
    return false;
  const EquivalenceBridge &bridge = found->second;
  // The undirected key is injective on literal pairs, so the query names the
  // stored pair; only its orientation can differ.
  if (a == bridge.a) {
    assert(b == bridge.b);
    *target_a = bridge.target_a;
    *target_b = bridge.target_b;
  } else {
    assert(a == bridge.b && b == bridge.a);
    *target_a = bridge.target_b;
    *target_b = bridge.target_a;
  }
  return true;
}

void SweepSubstitutionSession::flush_moves(const Kernel &k) {
#ifndef TACTIX_ENABLE_EVENT_TRACE
  (void)k;
#endif
  // Flush each retiring literal's `delayed` stack before the next, so a
  // representative receives each source's stream contiguously in encounter
  // order.
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
  // Membership leaves `large` and the mixed stream at once; kernel primitives
  // read both before the ledger publishes order.
  for (const Lit lit : *old) {
    if (k.dense)
      kernel_dense_remove_large_occurrence(k, lit, source);
    if (dense_enabled)
      erase_large(k, lit, source);
  }
}

void SweepSubstitutionSession::collapse_large_to_binary(Kernel &k, Ref source,
                                                         Lit a, Lit b,
                                                         bool append_a,
                                                         bool append_b) {
  const Clause *const old = clause_at(k.arena, source);
  // Kissat's dense stream holds every binary but only irredundant large
  // clauses; the binary just published is still recorded.
  const bool indexed = !old->redundant();
  dirty_binary_owners.push_back(a);
  dirty_binary_owners.push_back(b);
  if (indexed)
    for (const Lit lit : *old) {
      if (k.dense)
        kernel_dense_remove_large_occurrence(k, lit, source);
      if (!dense_enabled)
        continue;
      DenseOccurrence *const at = find_large(k, lit, source);
      if (!at)
        continue;
      // The introduced endpoint takes Kissat's appended `delayed` watch, the
      // surviving one its in-place replacement
      // (`kissat_substitute_large_watch`).
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
    return clause_contains(old, lit);
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
  // Private cursor over retained runs, wrapping on misses for full membership.
  const auto membership = [](const Lit *begin, const Lit *end) {
    return [begin, end, next = begin](Lit lit) mutable {
      const Lit *found = std::find(next, end, lit);
      if (found == end) {
        found = std::find(begin, next, lit);
        if (found == next)
          return false;
      }
      next = found + 1;
      return true;
    };
  };
  auto image_holds = membership(image, image + size);
  auto source_held = membership(old->begin(), old->end());
  // Only literals leaving the image lose their stream slot; retained literals
  // are never searched.
  for (const Lit lit : *old) {
    if (image_holds(lit))
      continue;
    DenseOccurrence *const at = find_large(k, lit, source);
    if (!at)
      continue;
    std::vector<DenseOccurrence> &ordered = occurrences(k, lit);
    ordered.erase(ordered.begin() + (at - ordered.data()));
  }
  for (uint32_t i = 0; i < size; i++)
    if (!source_held(image[i]))
      push_move(k, *old, image[i], source, {false, source});
}

uint64_t SweepSubstitutionSession::collect(Kernel &k) {
  // Publish even for a NoOp call: garbage cleanup and compaction change the
  // representation the next BVE/Walk consumer reads.
  publish_dense_order(k, dense_initialized_lits);
  if (!changed)
    return 0;
  // Surviving large clauses keep their Ref and span; collapsed clauses are lazy
  // garbage. No representation debt remains at the call boundary.
  (void)k;
  changed = false;
  return 0;
}

namespace {

ApplyResult do_substitution(Kernel &k, SccResult &r,
                            std::vector<uint8_t> &&sub,
                            SweepSubstitutionSession *session = nullptr) {
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  // Canonical substitution edits binary watches in place
  // (substitute.c:294-363). When an N1 schedule keeps Eliminate's mixed stream
  // attached, republish it from the physical result at the rule boundary, as
  // Congruence does in kernel_canonicalize_binary_watches. Sweep sessions
  // republish per batch.
  struct RepublishAttachedMixedStream {
    Kernel &k;
    const bool active;
    ~RepublishAttachedMixedStream() {
      if (active)
        kernel_rebuild_dense_occurrences(k);
    }
  } republish{k, !session && k.dense && k.dense->track_order};
  // The caller's map can predate this round's units. Kissat skips root-fixed
  // members for proof equivalences and retirement
  // (`add_representative_equivalences` checks ACTIVE) but
  // `substitute_binaries`/`substitute_clauses` apply the whole map:
  // `mapped_sub` owns rewriting, `live_sub` lemmas, witnesses and retirement. A
  // snapshot: a live test could change before (F) owes a witness.
  std::vector<uint8_t> mapped_sub;
  std::vector<uint8_t> live_sub;
  if (!session) {
    mapped_sub = std::move(sub);
    live_sub = mapped_sub;
    for (Lit l = 0; l < nlits; l++) {
      // A policy may select a subset of the valid SCC substitution. An
      // explicit standalone invocation has no selection and stays complete.
      if (mapped_sub[l] && kernel_startup_retained(k, var_of(l))) {
        mapped_sub[l]=mapped_sub[negate(l)]=0;
        live_sub[l]=live_sub[negate(l)]=0;
      }
      live_sub[l]=static_cast<uint8_t>(substitution_member_live(k,live_sub,l));
    }
  }
  const auto substitutable = [&](Lit l) {
    return session ? session->substitutes(l)
                   : l < nlits && static_cast<bool>(live_sub[l]);
  };
  const auto mapped = [&](Lit l) {
    return session ? session->substitutes(l)
                   : l < nlits && static_cast<bool>(mapped_sub[l]);
  };
  const auto representative = [&](Lit l) {
    return session ? session->representative(l) : r.repr[l];
  };
  // Combined image for the non-session path: l maps to its representative if
  // substituted, else itself. Sweep sessions use the lambda path.
  std::vector<Lit> image;
  if (!session) {
    image.resize(nlits);
    for (Lit l = 0; l < nlits; l++)
      image[l] = static_cast<bool>(mapped_sub[l]) ? r.repr[l] : l;
  }
  const auto for_each_substitutable = [&](const auto &fn) {
    if (session)
      for (const Lit l : session->retiring_sources())
        fn(l);
    else
      for (Lit l = 0; l < nlits; l++)
        if (substitutable(l))
          fn(l);
  };

  // Certify persistent affine aliases while the binary graph is intact.
  if (!session &&
      !preserve_affine_aliases(k, uint64_t(nlits), [&](auto &aliases) {
        for_each_substitutable([&](Lit l) {
          if (!negated(l)) aliases.emplace_back(l, representative(l));
        });
      }))
    return ApplyResult::NoOp;

  // (B) transient lemmas {¬l, repr[l]}, RUP along l→…→repr[l], derived before
  // any binary mutation.
  for_each_substitutable([&](Lit l) {
    const Lit s = representative(l);
    const uint64_t id = ++k.next_clause_id;
    const Lit lits2[2] = {negate(l), s};
    emit_rup_lemma(k, id, lits2, 2);
  });

  // Translate the query's constrained coordinates while the sources and bridges
  // still exist.
  if (k.query_projection) {
    std::vector<std::pair<Lit, Lit>> aliases;
    for (const int32_t external : k.query_projection->constraints()) {
      const Lit from = kernel_internalize(k, external);
      if (from != INVALID_LIT && mapped(from))
        aliases.emplace_back(from, representative(from));
    }
    k.query_projection->substitute(k, aliases);
  }

  // Canonical Substitute uses a literal-indexed byte table; Sweep a sparse set.
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
  // Batched root units + opposite-sign refutation: the shared kernel idiom
  // (PendingUnits, kernel.hpp).
  PendingUnits pending;
  pending.init(k);
  bool changed = false;

  // Record collapsed root unit `u`, deduplicated per variable. Returns true iff
  // `u` contradicts an earlier unit of this round (caller returns Unsat).
  auto record_unit = [&](Lit u) -> bool {
    if (k.value(u) > 0)
      return false;
    if (k.value(u) < 0) {
      kernel_emit_empty(k);
      return true;
    }
    return pending.record(k, u) == PendingUnits::Outcome::Refuted;
  };

  // Kissat assigns and propagates binary-pass units before the large pass:
  // process in stack-pop order, propagating after each, then reset the table.
  auto flush_units = [&]() -> bool {
    for (auto it = pending.units.rbegin(); it != pending.units.rend(); ++it) {
      const Lit u = *it;
      if (k.value(u) > 0)
        continue;
      if (k.value(u) < 0) {
        kernel_emit_empty(k); // ¬u became root-true since the record
        return true;
      }
      kernel_assign_unit(k, u);
      if (inprocess_reach_root(k))
        return true;
    }
    pending.init(k);
    return false;
  };

  // Rewrite one literal sequence: apply repr, strip root-false literals, detect
  // root-true/tautology. Returns true iff satisfied or tautological.
  std::vector<uint32_t> sweep_origin_rank;
  auto rewrite_lits = [&](const Lit *lits, uint32_t size,
                          std::vector<Lit> &out) -> bool {
    out.clear();
    sweep_origin_rank.clear();
    const bool connected_large = session && size > 2;
    bool satisfied = false;
    for (uint32_t i = 0; i < size; i++) {
      const Lit m = lits[i];
      // Non-session reads the precomputed image (one load); the ternary
      // short-circuits so the lambdas are not evaluated off the session path.
      const Lit mp = session ? (mapped(m) ? representative(m) : m) : image[m];
      if (k.value(mp) > 0) { // root-true → clause satisfied
        satisfied = true;
        break;
      }
      if (k.value(mp) < 0) // root-false → strip
        continue;
      const uint32_t origin_rank = connected_large && mapped(m)
                                       ? session->retiring_index(m) + 1
                                       : 0;
      if (seen_contains(mp)) { // duplicate literal
        // Connected substitution keeps the retiring occurrence's position;
        // later merge-forest origins win collisions.
        if (connected_large) {
          const auto prior = std::find(out.begin(), out.end(), mp);
          const size_t at = static_cast<size_t>(prior - out.begin());
          if (origin_rank > sweep_origin_rank[at]) {
            out.erase(prior);
            sweep_origin_rank.erase(sweep_origin_rank.begin() + at);
            out.push_back(mp);
            sweep_origin_rank.push_back(origin_rank);
          }
        }
        continue;
      }
      if (seen_contains(negate(mp))) { // mp and ¬mp both present → tautology
        satisfied = true;
        break;
      }
      seen_mark(mp);
      out.push_back(mp);
      if (connected_large)
        sweep_origin_rank.push_back(origin_rank);
    }
    for (const Lit l : out)
      seen_clear(l); // unmark
    if (satisfied) {
      out.clear();
      return true;
    }
    return false;
  };

  // (D) rewrite large clauses up to the captured arena end, like Kissat's
  // `all_clauses`. Survivors are rewritten in place; only collapses retire.
  std::vector<Lit> out;
  const auto rewrite_large_ref = [&](Ref ref) -> ApplyResult {
    Clause *c = clause_at(k.arena, ref);
    // Touched iff some literal is substituted or root-assigned.
    bool touched = false;
    if (session) {
      for (const Lit m : *c)
        if (mapped(m) || k.value(m) != 0) {
          touched = true;
          break;
        }
    } else {
      for (const Lit m : *c)
        if (mapped_sub[m] || k.value(m) != 0) {
          touched = true;
          break;
        }
    }
    if (!touched)
      return ApplyResult::Ok;
    // Materialize the literals' streams while watches and `Kernel::dense` still
    // hold the source.
    if (session)
      session->prime_large(k, ref);
    const bool credundant = c->redundant();
    const uint32_t cglue = c->glue();
    const bool sat = rewrite_lits(c->lits, c->size, out);
    if (sat) { // satisfied/tautology -> delete only (deletions are proof-free)
      if (session)
        session->remove_large(k, ref);
      kernel_delete_clause(k, c);
      changed = true;
      return ApplyResult::Ok;
    }
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, out);
    changed = true;
    // Both Kissat paths overwrite surviving images in place
    // (`substitute.c:substitute_clauses`,
    // `sweep.c:substitute_connected_clauses`), keeping the Ref and header bits.
    // Canonical keeps glue; Sweep promotes an oversized glue to new_size-1
    // (sweep.c:1004-1008).
    const uint32_t replacement_glue =
        credundant
            ? (session ? kernel_shrunken_glue(
                             k, cglue, static_cast<uint32_t>(out.size()))
                       : cglue)
            : 0;
    ReplaceOutcome replacement;
    if (out.size() >= 3) {
      if (session)
        session->rewrite_large_in_place(k, ref, out.data(),
                                        static_cast<uint32_t>(out.size()));
      // sweep.c::substitute_connected_clauses marks surviving literals of an
      // irredundant image; substitute.c::substitute_clauses does not.
      kernel_replace_large_clause_in_place(
          k, ref, id, out.data(), static_cast<uint32_t>(out.size()),
          replacement_glue, session ? "sweep_substitute" : "substitute",
          /*mark_added_material=*/session != nullptr);
      replacement = ReplaceOutcome::Large;
    } else {
      // Canonical substitution retires the original on a binary collapse;
      // complete removal advice for retained free endpoints.
      if (!session && !credundant && out.size() == 2)
        for (const Lit old : *c)
          if (!k.value(old) &&
              std::find(out.begin(), out.end(), old) != out.end())
            kernel_mark_bve_retry(k, var_of(old),
                                  "substitute-collapse-binary", c);
      replacement = kernel_replace_clause(
          k, c, id, out.data(), static_cast<uint32_t>(out.size()), credundant,
          replacement_glue, nullptr);
    }
    switch (replacement) {
    case ReplaceOutcome::Refuted: // all-false rewrite (unreachable at a
      return ApplyResult::Unsat;  // quiescent root)
    case ReplaceOutcome::Unit:
      if (record_unit(out[0]))
        return ApplyResult::Unsat;
      break;
    default:
      break;
    }
    if (session) {
      if (replacement == ReplaceOutcome::Binary) {
        bool append_first = false, append_second = false;
        for (const Lit old_lit : *clause_at(k.arena, ref))
          if (mapped(old_lit)) {
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
    return ApplyResult::Ok;
  };

  const auto make_large_image = [&](Ref ref) {
    const Clause *c = clause_at(k.arena, ref);
    bool touched = false;
    for (const Lit m : *c) {
      const Lit mp = mapped(m) ? representative(m) : m;
      if (mapped(m) || k.value(m) != 0 || k.value(mp) != 0) {
        touched = true;
        break;
      }
    }
    return touched && !rewrite_lits(c->lits, c->size, out);
  };

  // Derive each image before mutating, so watch slices cannot double mid-pass.
  const auto reserve_large_replacements = [&](const auto &visit) {
    // Sweep publishes at the call boundary; only binary watch growth is
    // reserved.
    if (session)
      return;
    size_t watch_words = 0;
    visit([&](Ref ref) {
      if (!make_large_image(ref))
        return;
      if (out.size() == 2)
        watch_words += 2;
      else if (out.size() >= 3) {
        watch_words += 4;
      }
    });
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
  };

  auto rewrite_large_clauses = [&]() -> ApplyResult {
  // Kissat's `substitute_connected_clauses` visits only the retiring literal's
  // binary/irredundant stream; redundant clauses wait for canonical Substitute.
  // Collect irredundant refs sparsely; those literals' dense lists are consumed
  // whole and cleared once at the end.
  const auto bulk_clear_dense = [&](const auto &literals) {
    if (!k.dense)
      return;
    for (const Lit l : literals)
      k.dense->clear_large(l);
  };
  if (session && k.dense) {
    std::vector<uint8_t> &bulk = k.dense_bulk_scratch;
    if (bulk.size() < static_cast<size_t>(nlits))
      bulk.assign(static_cast<size_t>(nlits), 0);
    for (const Lit l : session->retiring_sources())
      bulk[l] = 1;
    k.dense_bulk_cleared = &bulk;
    struct Restore {
      Kernel &k;
      std::vector<uint8_t> &bulk;
      const std::vector<Lit> &sources;
      ~Restore() {
        k.dense_bulk_cleared = nullptr;
        for (const Lit l : sources)
          bulk[l] = 0;
      }
    } restore{k, bulk, session->retiring_sources()};
    std::vector<Ref> refs;
    for (const Lit l : session->retiring_sources()) {
      k.dense->with_large(l, [&](auto occurrences) {
        for (const Ref ref : occurrences) {
          const Clause *c = clause_at(k.arena, ref);
          if (!c->garbage())
            refs.push_back(ref);
        }
      });
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
    bulk_clear_dense(session->retiring_sources());
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
    struct Restore {
      Kernel &k;
      ~Restore() { k.dense_bulk_cleared = nullptr; }
    } restore{k};
    if (k.dense)
      k.dense_bulk_cleared = &mapped_sub;
    for (Ref ref = 0; ref < original_end;) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(c);
      if (!c->garbage() && rewrite_large_ref(ref) == ApplyResult::Unsat)
        return ApplyResult::Unsat;
      ref += words;
    }
    if (k.dense) {
      std::vector<Lit> retired;
      for (Lit l = 0; l < nlits; l++)
        if (mapped_sub[l])
          retired.push_back(l);
      bulk_clear_dense(retired);
    }
  }
  return ApplyResult::Ok;
  };

  // (C) rewrite binaries, preserving the clause multiset and import traversal
  // order with or without a proof. Identical copies share one proof image.
  std::vector<Var> variable_order;
  std::vector<uint8_t> canonical_binary_affected;
  if (!session) {
    kernel_vars_in_import_order(k, variable_order);
    // Only binaries incident to a retiring literal change: name their owners
    // and mirrors, then keep the global import-order traversal over that
    // population.
    canonical_binary_affected.assign(nlits, 0);
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        if (!mapped(lit))
          continue;
        canonical_binary_affected[lit] = 1;
        for (const WatchEntry watch : k.watches.entries(lit))
          if (watch.binary())
            canonical_binary_affected[watch.other()] = 1;
      }
  }

  struct BinaryImage {
    Lit source_a, source_b;
    // Bridge endpoints when the source is a preserved equivalence binary:
    // the emitted pair under every later merge, not the final representative.
    Lit bridge_target_a = INVALID_LIT;
    Lit bridge_target_b = INVALID_LIT;
  };
  struct ExpandedBinaryImage {
    Lit source_a, source_b;
    Lit target_a, target_b;
    bool tautological;
    // The image differs from its source pair: tautological, a unit, or moved.
    bool changed() const {
      return tautological || target_a == target_b ||
             kernel_binary_key(source_a, source_b) !=
                 kernel_binary_key(target_a, target_b);
    }
  };
  const auto make_image = [&](Lit lit, Lit other) {
    BinaryImage image{lit, other, INVALID_LIT, INVALID_LIT};
    if (session)
      (void)session->equivalence_bridge_targets(
          lit, other, &image.bridge_target_a, &image.bridge_target_b);
    return image;
  };
  const auto expand_image = [&](const BinaryImage &source) {
    if (source.bridge_target_a != INVALID_LIT)
      return ExpandedBinaryImage{
          source.source_a, source.source_b, source.bridge_target_a,
          source.bridge_target_b,
          source.bridge_target_a == negate(source.bridge_target_b)};
    const Lit a = mapped(source.source_a)
                      ? representative(source.source_a)
                      : source.source_a;
    const Lit b = mapped(source.source_b)
                      ? representative(source.source_b)
                      : source.source_b;
    return ExpandedBinaryImage{source.source_a, source.source_b, a, b,
                               a == negate(b)};
  };
  bool refuted = false;
  // Once the unit batch derives the empty clause, no later image may add a
  // proof line; the physical rewrite still completes.
  const auto pbp_proof_finished = [&] {
    return refuted && k.proof;
  };
  // Retain disappearing copies until the unit batch reaches root closure, then
  // publish removal advice in import order (Kissat substitute_binaries
  // delayed).
  std::vector<std::pair<Lit, Lit>> removed_binaries;
  // The non-session path ports Kissat's in-place `substitute_binaries`
  // (kissat/src/substitute.c:276); Sweep sessions use the image path.
  const bool kissat_single_pass = !session;
  uint64_t binary_population_transaction = 0;
  struct DeferredBinaryPopulationDelta {
    SolverPopulationEventKind kind;
    uint64_t id;
    Lit a, b;
  };
  std::vector<DeferredBinaryPopulationDelta> deferred_single_pass_deltas;
  const auto emit_binary_population =
      [&](SolverPopulationEventKind kind, uint64_t id, Lit a, Lit b) {
        if (!k.population_observer || !k.population_observe_solver)
          return;
        if (!binary_population_transaction)
          binary_population_transaction =
              kernel_begin_solver_population_transaction(k);
        const Lit clause[2] = {a, b};
        kernel_observe_solver_clause(
            k, kind, SolverClauseKind::Binary, id, clause, 2, 1, false,
            binary_population_transaction);
      };
  const auto stage_binary_population =
      [&](SolverPopulationEventKind kind, uint64_t id, Lit a, Lit b) {
        if (!k.population_observer || !k.population_observe_solver)
          return;
        if (kissat_single_pass) {
          if (deferred_single_pass_deltas.size() ==
              deferred_single_pass_deltas.capacity())
            std::abort();
          deferred_single_pass_deltas.push_back({kind, id, a, b});
          return;
        }
        emit_binary_population(kind, id, a, b);
      };
  if (kissat_single_pass) {
    // Kissat substitute_binaries: `lit==repr` rewrites in place; otherwise the
    // pair moves to `repr` via a `delayed` stack appended after the walk (raw
    // pointers stay valid). Proof lines and identities are emitted once per
    // undirected pair under the import-order owner; metadata mirrors
    // WatchLists::compact.
    const auto import_owner = [&](Lit a, Lit b) {
      return kernel_import_lit_order(k, a) < kernel_import_lit_order(k, b);
    };
    if (k.population_observer && k.population_observe_solver) {
      // Reserve twice the directed population so buffering never allocates
      // while the mirror invariant is transient.
      size_t directed_binaries = 0;
      for (Lit owner = 0; owner < nlits; ++owner) {
        if (!canonical_binary_affected[owner])
          continue;
        for (const WatchEntry watch : k.watches.entries(owner))
          directed_binaries += watch.binary();
      }
      if (directed_binaries >
          std::numeric_limits<size_t>::max() / 2)
        std::abort();
      deferred_single_pass_deltas.reserve(2 * directed_binaries);
    }
    std::vector<Lit> delayed_targets; // repr_other words to move to repr_lit
    std::vector<uint8_t> delayed_publish;
    std::vector<Lit> touched_units;   // seen marks to clear at the end
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        if (!canonical_binary_affected[lit])
          continue;
        const Lit repr_lit = image[lit];
        const Lit not_repr_lit = negate(repr_lit);
        delayed_targets.clear();
        delayed_publish.clear();
        uint32_t *const base = k.watches.begin(lit);
        uint32_t *q = base;
        const uint32_t *p = base;
        const uint32_t *const stop = k.watches.end(lit);
        const uint32_t old_size = k.watches.heads[lit].size;
        uint32_t kept_binaries = 0;
        bool kept_has_large = false, kept_binaries_first = true;
        while (p != stop) {
          const Watch w{*p};
          if (!w.binary()) { // large watch: two words, keep verbatim
            *q++ = *p++;
            *q++ = *p++;
            kept_has_large = true;
            continue;
          }
          const Lit other = w.other();
          ++p;
          const Lit repr_other = image[other];
          const bool is_owner = import_owner(lit, other);
          const auto retire_source = [&] {
            if (is_owner) {
              const Lit lits2[2] = {lit, other};
              const bool observe =
                  k.population_observer && k.population_observe_solver;
              const uint64_t source_id = (k.proof || observe)
                                             ? kernel_binary_id(k, lit, other)
                                             : 0;
              if (k.proof)
                k.proof->del(k.next_clause_id, source_id, lits2, 2);
              if (observe)
                stage_binary_population(SolverPopulationEventKind::StageDelete,
                                        source_id, lit, other);
            }
          };
          if (repr_other == not_repr_lit) { // tautology: drop this direction
            changed = true;
            if (is_owner)
              removed_binaries.emplace_back(lit, other);
            retire_source();
            continue;
          }
          if (repr_other == repr_lit) { // collapses to the unit repr_lit
            changed = true;
            if (is_owner)
              removed_binaries.emplace_back(lit, other);
            // Add the unit while the source is live (its RUP cites it), then
            // delete.
            if (is_owner && !seen_contains(repr_lit) &&
                !pbp_proof_finished()) {
              seen_mark(repr_lit);
              touched_units.push_back(repr_lit);
              ++k.next_clause_id;
              if (k.proof)
                k.proof->add_rup(0, &repr_lit, 1);
              if (record_unit(repr_lit))
                refuted = true;
            }
            retire_source();
            continue;
          }
          if (lit == repr_lit && other == repr_other) { // unchanged: keep
            *q++ = w.raw;
            ++kept_binaries;
            kept_binaries_first = kept_binaries_first && !kept_has_large;
            continue;
          }
          // Genuine surviving rewrite to (repr_lit, repr_other).
          changed = true;
          if (is_owner && k.proof && !pbp_proof_finished()) {
            const Lit lits2[2] = {repr_lit, repr_other};
            k.proof->add_rup(0, lits2, 2);
          }
          retire_source();
          if (lit == repr_lit) { // stable owner: rewrite in place, survives
            kernel_rearm_binary_clause(k, repr_lit, repr_other);
            *q++ = binary_watch(repr_other).raw;
            ++kept_binaries;
            kept_binaries_first = kept_binaries_first && !kept_has_large;
          } else { // mapped owner: move to repr_lit's list, publish once/pair
            delayed_targets.push_back(repr_other);
            const bool moved_other = (other != repr_other);
            delayed_publish.push_back(
                static_cast<uint8_t>(!moved_other || is_owner));
          }
        }
        // Finalize lit's compacted list (WatchLists::compact bookkeeping).
        WatchHead &head = k.watches.heads[lit];
        const uint32_t new_size = static_cast<uint32_t>(q - base);
        head.size = new_size;
        k.watches.holes += old_size - new_size;
        head.set_layout(kept_binaries, kept_binaries_first);
        // Append the moved directions to repr_lit's list (may relocate that
        // slice only; lit's list is already finalized and re-read by offset).
        for (size_t i = 0; i < delayed_targets.size(); i++) {
          const Lit repr_other = delayed_targets[i];
          if (delayed_publish[i]) {
            // The exact binary-liveness index is a no-false-negative prefilter:
            // a reject skips both watch scans.
            const bool already_live =
                (k.binary_liveness &&
                 !k.binary_liveness->maybe(
                     kernel_binary_key(repr_lit, repr_other)))
                    ? false
                    : (kernel_binary_copies(k, repr_lit, repr_other, 1) ||
                       kernel_binary_copies(k, repr_other, repr_lit, 1));
            k.watches.push_binary(repr_lit, repr_other);
            const uint64_t id = ++k.next_clause_id;
            stage_binary_population(SolverPopulationEventKind::StageAdd, id,
                                    repr_lit, repr_other);
            kernel_publish_binary_clause(k, repr_lit, repr_other, id,
                                         !already_live,
                                         BinaryPopulationEffect::RepresentationOnly);
          } else {
            k.watches.push_binary(repr_lit, repr_other);
          }
        }
      }
    for (const Lit unit : touched_units)
      seen_clear(unit);
    // Publish the buffered population delta once every list and mirror is
    // final.
    for (const DeferredBinaryPopulationDelta &delta :
         deferred_single_pass_deltas)
      emit_binary_population(delta.kind, delta.id, delta.a, delta.b);
  } else {
  // Targets are a function of the representative map, so store only the source
  // pair, in segmented storage.
  std::vector<BinaryImage> images;
  // Only binaries incident to a non-representative literal change; others stay
  // in place.
  const auto snapshot_undirected = [&](Lit lit) {
      // The owner's import rank is constant across its whole watch list.
      const uint64_t lit_order = kernel_import_lit_order(k, lit);
      for (const WatchEntry w : k.watches.entries(lit)) {
        if (!w.binary())
          continue;
        const Lit other = w.other();
        // A pair with two retiring endpoints keeps one orientation per copy;
        // with one, the retiring stream owns the image.
        if (mapped(other) && lit_order >= kernel_import_lit_order(k, other))
          continue;
        images.push_back(make_image(lit, other));
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        const ExpandedBinaryImage noted = expand_image(images.back());
        rule_journal_note_binary_rewrite(k, lit, other, noted.target_a,
                                         noted.target_b);
#endif
      }
  };
  if (session) {
    // Retiring sources' watch streams are Kissat's connected frontier.
    for (const Lit lit : session->retiring_sources())
      snapshot_undirected(lit);
  } else {
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        if (!canonical_binary_affected[lit])
          continue;
        const uint64_t lit_order = kernel_import_lit_order(k, lit);
        for (const WatchEntry w : k.watches.entries(lit)) {
          if (!w.binary() ||
              lit_order >= kernel_import_lit_order(k, w.other()))
            continue;
          const Lit other = w.other();
          if (!mapped(lit) && !mapped(other))
            continue;
          images.push_back({lit, other});
#ifdef TACTIX_ENABLE_RULE_JOURNAL
          const Lit a = mapped(lit) ? representative(lit) : lit;
          const Lit b = mapped(other) ? representative(other) : other;
          rule_journal_note_binary_rewrite(k, lit, other, a, b);
#endif
        }
      }
  }
  if (!session)
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      if (image.tautological || image.target_a == image.target_b)
        removed_binaries.emplace_back(image.source_a, image.source_b);
    }

  // Reuse `seen` to deduplicate collapsed binary units.
  std::vector<Lit> emitted_units;
  const auto first_emitted_unit = [&](Lit unit) {
    if (seen_contains(unit))
      return false;
    seen_mark(unit);
    emitted_units.push_back(unit);
    return true;
  };
  // Stream the transformed multiset as Kissat does; duplicates stay duplicates.
  {
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      const bool changed_image = image.changed();
      changed = changed || changed_image;
      if (image.tautological)
        continue;

      if (image.target_a == image.target_b) {
        if (!first_emitted_unit(image.target_a))
          continue;
        if (pbp_proof_finished())
          continue;
        ++k.next_clause_id;
        if (k.proof)
          k.proof->add_rup(0, &image.target_a, 1);
        if (record_unit(image.target_a))
          refuted = true;
        continue;
      }

      if (changed_image && k.proof && !pbp_proof_finished()) {
        const Lit lits2[2] = {image.target_a, image.target_b};
        k.proof->add_rup(0, lits2, 2);
      }
    }

    if (k.proof)
      for (const BinaryImage &stored : images) {
        const ExpandedBinaryImage image = expand_image(stored);
        if (!image.changed())
          continue;
        const Lit lits2[2] = {image.source_a, image.source_b};
        k.proof->del(k.next_clause_id, 0, lits2, 2);
      }
  }
  // Retire one logical source copy per changed undirected image.
  if (k.population_observer && k.population_observe_solver)
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      if (!image.changed())
        continue;
      const Lit source[2] = {image.source_a, image.source_b};
      stage_binary_population(
          SolverPopulationEventKind::StageDelete,
          kernel_binary_id(k, image.source_a, image.source_b), source[0],
          source[1]);
    }
  for (const Lit unit : emitted_units)
    seen_clear(unit);
  std::vector<Lit>().swap(emitted_units);

  // A representative-owned direction is rewritten in place
  // (substitute.c:328-340); only the retired endpoint's mirror moves. The
  // session path keeps per-image rewriting because its bridge targets depend on
  // the image.
  if (!session) {
    size_t stable_rewrites = 0;
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit stable = make_lit(variable, sign != 0);
        if (!canonical_binary_affected[stable] || mapped(stable))
          continue;
        uint32_t *p = k.watches.begin(stable);
        uint32_t *const end = k.watches.end(stable);
        while (p != end) {
          const Watch watch{*p};
          if (!watch.binary()) {
            p += 2;
            continue;
          }
          const Lit other = watch.other();
          if (mapped(other)) {
            const Lit target = representative(other);
            // The unit and tautological images are retired elsewhere and must
            // keep their source entry until then.
            if (target != stable && target != negate(stable)) {
              kernel_rearm_binary_clause(k, stable, target);
              *p = binary_watch(target).raw;
              stable_rewrites++;
            }
          }
          p++;
        }
      }
#ifndef NDEBUG
    // The pass must rewrite exactly the images the per-image scans did.
    size_t stable_images = 0;
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      if (image.tautological || image.target_a == image.target_b)
        continue;
      stable_images +=
          (image.source_a == image.target_a) != (image.source_b == image.target_b);
    }
    assert(stable_rewrites == stable_images &&
           "stable-direction pass and the binary image disagree");
#endif
    (void)stable_rewrites;
  } else {
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      if (image.tautological || image.target_a == image.target_b)
        continue;
      const bool stable_a = image.source_a == image.target_a;
      const bool stable_b = image.source_b == image.target_b;
      if (stable_a == stable_b)
        continue;
      const Lit stable = stable_a ? image.source_a : image.source_b;
      const Lit old_other = stable_a ? image.source_b : image.source_a;
      const Lit new_other = stable_a ? image.target_b : image.target_a;
      uint32_t *p = k.watches.begin(stable);
      uint32_t *const end = k.watches.end(stable);
      [[maybe_unused]] bool rewritten = false;
      while (p != end) {
        Watch watch{*p};
        if (watch.binary()) {
          if (watch.other() == old_other) {
            // Preserve the first physical copy's position and leave later
            // duplicates untouched (docs/dev/implementation-notes.md §5).
            kernel_rearm_binary_clause(k, stable, new_other);
            *p = binary_watch(new_other).raw;
            rewritten = true;
            break;
          }
          p++;
        } else {
          p += 2;
        }
      }
      assert(rewritten);
    }
  }


  // Sparse reconstruction is directed in Kissat: each retired literal appends
  // its watch sequence to its representative after its list is visited
  // (substitute.c:294-363).
  std::vector<Lit> affected_binary_owners;
  // Bridge entries with final endpoints survive the snapshot and compaction,
  // counted per directed entry (one per bridge image), never by literal pair:
  // an ordinary binary matching a bridge target was already rewritten or
  // deleted.

  // Sorted (key, count) pairs, not a hash map: Sweep calls this once per
  // productive environment, so a per-call map allocates per distinct bridge.
  std::vector<std::pair<uint64_t, uint32_t>> bridge_keep_snapshot;
  std::vector<std::pair<uint64_t, uint32_t>> bridge_keep_compact;
  const auto bridge_keep_add = [](std::vector<std::pair<uint64_t, uint32_t>> &v,
                                  uint64_t key) { v.emplace_back(key, 1u); };
  const auto bridge_keep_coalesce =
      [](std::vector<std::pair<uint64_t, uint32_t>> &v) {
        std::sort(v.begin(), v.end(),
                  [](const std::pair<uint64_t, uint32_t> &a,
                     const std::pair<uint64_t, uint32_t> &b) {
                    return a.first < b.first;
                  });
        size_t kept = 0;
        for (size_t i = 0; i < v.size(); i++) {
          if (kept && v[kept - 1].first == v[i].first)
            v[kept - 1].second += v[i].second;
          else
            v[kept++] = v[i];
        }
        v.resize(kept);
      };
  const auto directed_entry_key = [](Lit from, Lit to) {
    return (static_cast<uint64_t>(from) << 32) | to;
  };
  // Consume one protected copy of the directed entry `from -> to`; false
  // when none is left.
  const auto bridge_keep_take =
      [&](std::vector<std::pair<uint64_t, uint32_t>> &v, Lit from, Lit to) {
    const uint64_t key = directed_entry_key(from, to);
    const auto it = std::lower_bound(
        v.begin(), v.end(), key,
        [](const std::pair<uint64_t, uint32_t> &a, uint64_t b) {
          return a.first < b;
        });
    if (it == v.end() || it->first != key || !it->second)
      return false;
    --it->second;
    return true;
  };
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
    for (const BinaryImage &stored : images) {
      if (stored.bridge_target_a == INVALID_LIT)
        continue;
      const ExpandedBinaryImage image = expand_image(stored);
      assert(!image.tautological && image.target_a != image.target_b);
      const bool stable_a = image.source_a == image.target_a;
      const bool stable_b = image.source_b == image.target_b;
      if (stable_a && stable_b) {
        // Untouched bridge: both physical directions stay in place.
        bridge_keep_add(bridge_keep_snapshot,
                        directed_entry_key(image.target_a, image.target_b));
        bridge_keep_add(bridge_keep_snapshot,
                        directed_entry_key(image.target_b, image.target_a));
      } else if (stable_a != stable_b) {
        // The in-place pass rewrote the stable direction to the bridge
        // endpoints; only that entry exists before the rebuild below.
        const Lit stable = stable_a ? image.target_a : image.target_b;
        const Lit moved = stable_a ? image.target_b : image.target_a;
        bridge_keep_add(bridge_keep_snapshot,
                        directed_entry_key(stable, moved));
      }
      // Both endpoints moved: nothing survives compaction; both directions
      // are rebuilt from the directed images after it.
    }
    bridge_keep_coalesce(bridge_keep_snapshot);
    bridge_keep_compact = bridge_keep_snapshot;
  }
  std::vector<BinaryImage>().swap(images);
  const auto snapshot_directed = [&](Lit lit) {
    assert(mapped(lit));
    for (const WatchEntry watch : k.watches.entries(lit)) {
      if (!watch.binary())
        continue;
      if (session &&
          bridge_keep_take(bridge_keep_snapshot, lit, watch.other()))
        continue;
      images.push_back(make_image(lit, watch.other()));
    }
  };
  if (session) {
    // Rebuild physical mirrors in the session's equivalence-emission order, so
    // search sees Sweep's binary propagation order.
    for (const Lit lit : session->retiring_sources())
      snapshot_directed(lit);
#ifndef NDEBUG
    // The compaction drops every binary incident to a substitutable literal and
    // rebuilds only snapshotted directions, so every retiring literal must be
    // named.
    {
      std::vector<uint8_t> named(nlits, 0);
      for (const Lit lit : session->retiring_sources())
        named[lit] = 1;
      for (Lit l = 0; l < nlits; l++)
        assert((!mapped(l) || named[l]) &&
               "sweep session omitted a substitutable literal");
    }
#endif
  } else {
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++) {
        const Lit lit = make_lit(variable, sign != 0);
        if (mapped(lit))
          snapshot_directed(lit);
      }
  }

  // Remove moved/retired directions in one pass per list; rewritten stable
  // directions keep their position.
  const auto compact_owner = [&](Lit lit) {
    k.watches.compact(lit, [&](WatchEntry watch) {
      if (!watch.binary())
        return true;
      // Same first-N consumption order as the snapshot.
      if (session &&
          bridge_keep_take(bridge_keep_compact, lit, watch.other()))
        return true;
      return !mapped(lit) && !mapped(watch.other());
    });
  };
  if (session)
    for (const Lit lit : affected_binary_owners)
      compact_owner(lit);
  else
    for (const Var variable : variable_order)
      for (uint32_t sign = 0; sign < 2; sign++)
        if (canonical_binary_affected[make_lit(variable, sign != 0)])
          compact_owner(make_lit(variable, sign != 0));
  std::vector<Var>().swap(variable_order);
  std::vector<uint8_t>().swap(canonical_binary_affected);
  if (session)
    sparse_seen.clear();
  else
    std::vector<uint8_t>().swap(seen);
  // Pack exact final per-literal slices so the pushes below never relocate.
  if (!session) {
    k.watches.begin_batch_reserve();
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      if (image.tautological || image.target_a == image.target_b)
        continue;
      assert(mapped(image.source_a));
      k.watches.reserve_batch_word(image.target_a);
    }
    k.watches.finish_batch_reserve(false);
  }
  // Pre-pass: pair presence before the first push, and each pair's first image.
  std::vector<uint8_t> preexisting;
  std::vector<uint8_t> mirror_seen;
  if (session) {
    std::vector<Lit> owners, others;
    std::vector<uint64_t> keys;
    owners.reserve(images.size());
    others.reserve(images.size());
    keys.reserve(images.size());
    for (const BinaryImage &stored : images) {
      const ExpandedBinaryImage image = expand_image(stored);
      const bool skip = image.tautological || image.target_a == image.target_b;
      owners.push_back(skip ? 0 : image.target_a);
      others.push_back(skip ? 0 : image.target_b);
      keys.push_back(skip ? 0
                          : kernel_binary_key(image.target_a, image.target_b));
    }
    // A mapped binary can survive in one endpoint's list only, so ask both
    // directions.
    std::vector<uint8_t> mirror_existing;
    session->classify_existing_binaries(k, owners, others, preexisting);
    session->classify_existing_binaries(k, others, owners, mirror_existing);
    for (size_t i = 0; i < preexisting.size(); i++)
      preexisting[i] = static_cast<uint8_t>(preexisting[i] | mirror_existing[i]);
    mirror_seen.assign(images.size(), 0);
    std::vector<uint32_t> by_key(images.size());
    for (size_t i = 0; i < by_key.size(); i++)
      by_key[i] = static_cast<uint32_t>(i);
    std::sort(by_key.begin(), by_key.end(), [&](uint32_t a, uint32_t b) {
      return keys[a] != keys[b] ? keys[a] < keys[b] : a < b;
    });
    for (size_t i = 1; i < by_key.size(); i++)
      if (keys[by_key[i]] == keys[by_key[i - 1]])
        mirror_seen[by_key[i]] = 1;
  }
  size_t image_index = 0;
  struct DeferredBinaryPopulationAdd {
    uint64_t id;
    Lit a, b;
  };
  std::vector<DeferredBinaryPopulationAdd> deferred_population_adds;
  if (k.population_observer && k.population_observe_solver)
    deferred_population_adds.reserve(images.size());
  for (const BinaryImage &stored : images) {
    const ExpandedBinaryImage image = expand_image(stored);
    const size_t this_image = image_index++;
    if (image.tautological || image.target_a == image.target_b)
      continue;
    const bool moved_other = image.source_b != image.target_b;
    const bool publish =
        !moved_other || kernel_import_lit_order(k, image.source_a) <
                            kernel_import_lit_order(k, image.source_b);
    uint64_t id = 0;
    if (publish)
      id = ++k.next_clause_id;
    const bool already_live =
        session ? (preexisting[this_image] || mirror_seen[this_image])
                : (kernel_binary_copies(k, image.target_a, image.target_b, 1) ||
                   kernel_binary_copies(k, image.target_b, image.target_a, 1));
    assert(!session ||
           already_live ==
               (kernel_binary_copies(k, image.target_a, image.target_b, 1) ||
                kernel_binary_copies(k, image.target_b, image.target_a, 1)));
    k.watches.push_binary(image.target_a, image.target_b);
    if (publish) {
      const BinaryPopulationEffect population_effect =
          k.population_observer && k.population_observe_solver &&
                  kernel_binary_key(image.source_a, image.source_b) !=
                      kernel_binary_key(image.target_a, image.target_b)
              ? BinaryPopulationEffect::LogicalAdd
              : BinaryPopulationEffect::RepresentationOnly;
      if (population_effect == BinaryPopulationEffect::LogicalAdd)
        deferred_population_adds.push_back(
            {id, image.target_a, image.target_b});
      kernel_publish_binary_clause(k, image.target_a, image.target_b, id,
                                   !already_live,
                                   BinaryPopulationEffect::RepresentationOnly);
    }
  }
  // Every directed watch has now reached its final mirrored image.  Deliver
  // the staged logical additions only at this stable callback boundary.
  for (const DeferredBinaryPopulationAdd &addition :
       deferred_population_adds)
    stage_binary_population(SolverPopulationEventKind::StageAdd, addition.id,
                            addition.a, addition.b);
  std::vector<BinaryImage>().swap(images);
  } // end image-path binary substitution (else of kissat_single_pass)
  kernel_commit_solver_population_transaction(
      k, binary_population_transaction);
  if (session)
    sparse_seen.clear();
  else
    seen.assign(nlits, 0);
  const bool binary_refuted = refuted || flush_units();
  for (const auto &[a, b] : removed_binaries) {
    for (const Lit lit : {a, b}) {
      // In this root-only transformation, a value now present means the
      // unit batch fixed the endpoint. Native removal marks skip fixed vars.
      if (k.value(lit))
        continue;
      kernel_mark_bve_retry(k, var_of(lit), "substitute-delete-binary");
      kernel_removed_var(k, var_of(lit));
    }
  }
  std::vector<std::pair<Lit, Lit>>().swap(removed_binaries);
  if (binary_refuted)
    return ApplyResult::Unsat;
  if (rewrite_large_clauses() == ApplyResult::Unsat)
    return ApplyResult::Unsat;
  if (flush_units())
    return ApplyResult::Unsat;
  std::vector<uint8_t>().swap(seen);
  sparse_seen.clear();
  std::vector<Lit>().swap(pending.units);

  // (E) Retire every transient lemma: a later direct SR step needs exact
  // checker population counts.
  if (k.proof)
    for_each_substitutable([&](Lit l) {
      const Lit lits2[2] = {negate(l), representative(l)};
      k.proof->del(k.next_clause_id, 0, lits2, 2);
    });

  // (F) witnesses per substituted variable: {¬l, s} by ¬l and {l, ¬s} by l, so
  // kernel_extend_model recovers l from s = repr[l].
  std::vector<Var> witness_order;
  if (!session) {
    kernel_vars_in_import_order(k, witness_order);
  }
  size_t witness_vars = 0;
  for (const Var v : witness_order)
    witness_vars += substitutable(make_lit(v, false));
  k.retired_events.reserve(k.retired_events.size() + witness_vars);
  k.substituted_events.reserve(k.substituted_events.size() + witness_vars);
  k.reconstruction.reserve(k.reconstruction.size() + 6 * witness_vars);
  for (const Var v : witness_order) {
    const Lit l = make_lit(v, false);
    if (!substitutable(l))
      continue;
    const Lit s = representative(l);
    const Lit rest_a[1] = {s};
    k.stats.substitute_vars++; // one per substituted variable (see Stats)
    // Retire v with its witness: a retired variable without one gives a wrong
    // SAT (`docs/design/variable-retirement-design-v2.md` §3).
    retire_variable(k, v, VarStatus::Substituted, "substituted");
    k.substituted_events.push_back(v);
    size_t repair_begin = k.reconstruction.size();
    kernel_push_witness(k, negate(l), rest_a, 1); // clause {s, ¬l}
    kernel_record_reconstruction(k, WeakeningFamily::Substitute, repair_begin);
    const Lit rest_b[1] = {negate(s)};
    repair_begin = k.reconstruction.size();
    kernel_push_witness(k, l, rest_b, 1); // clause {¬s, l}
    kernel_record_reconstruction(k, WeakeningFamily::Substitute, repair_begin);
  }
  std::vector<Lit>().swap(r.repr);
  std::vector<Lit>().swap(r.failed_units);
  std::vector<Lit>().swap(r.failed_witness);
  std::vector<uint8_t>().swap(live_sub);

  if (changed && session) {
    session->changed = true;
  }
  return changed ? ApplyResult::Ok : ApplyResult::NoOp;
}

} // namespace

// Canonical instance: one full round. Detect SCCs; on x ≡ ¬x refute, otherwise
// substitute every non-trivial SCC onto its representative.
ApplyResult substitute_reconnect_large_watches(Kernel &k,
                                               uint32_t trail_begin) {
  assert(!k.large_watches_connected);
  assert(trail_begin <= k.trail.size());
  kernel_rebuild_large_watches(k);
  // Replay units through the reconnected large clauses (dense.c / substitute.c:
  // kissat_reset_propagate then kissat_probing_propagate).
  k.propagate_head = std::min<uint32_t>(k.propagate_head, trail_begin);
  k.binary_only_head = UINT32_MAX;
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  return ApplyResult::Ok;
}

ApplyResult substitute_apply(Kernel &k) {
  return execution::execute_action(k, execution::SubstituteRoundRequest{});
}

ApplyResult substitute_apply_round(Kernel &k, bool reconnect_large_watches) {
  assert(substitute_guard(k));
  const bool connected_at_entry = k.large_watches_connected;
  const uint32_t trail_at_entry = static_cast<uint32_t>(k.trail.size());
  const auto finish = [&](ApplyResult result) {
    // Restore an owned watch session before publishing even a refutation.
    // The root service recognizes inconsistent state without further reasoning.
    if (!connected_at_entry || !reconnect_large_watches ||
        k.large_watches_connected)
      return result;
    const ApplyResult closure =
        substitute_reconnect_large_watches(k, trail_at_entry);
    return closure == ApplyResult::Unsat ? closure : result;
  };
  // D-LEVEL: drain the root closure (no-op at a fixpoint); a conflict refutes.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.substitute_rounds++;
  SccResult r = substitute_find_sccs(k, false);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    event_trace_substitute_map(k, l, r.repr[l]);
#endif
  if (r.contradiction) {
    assert(r.contradiction_lit != INVALID_LIT);
    emit_unsat(k, r.contradiction_lit);
    return ApplyResult::Unsat;
  }
  const ApplyResult failed = apply_failed_root_units(k, r);
  if (failed == ApplyResult::Unsat)
    return failed;
  k.stats.substitute_sccs += r.nontrivial_count; // counted before the
  // Every round runs Kissat's `substitute_clauses`: root units can satisfy or
  // shrink large clauses even without an SCC.
  std::vector<uint8_t> sub(2 * k.nvars, 0);
  for (Lit l = 0; l < static_cast<Lit>(2 * k.nvars); l++)
    if (r.repr[l] != INVALID_LIT && r.repr[l] != l) {
      sub[l] = 1;
      k.stats.substitute_scc_lits++;
    }
  // Kissat's round-one SCC pass charges the large watches;
  // `substitute_binaries` then drops them.
  if (k.large_watches_connected)
    kernel_drop_large_watches(k);
  const ApplyResult rewritten = do_substitution(k, r, std::move(sub));
  if (rewritten == ApplyResult::NoOp && failed == ApplyResult::Ok)
    return finish(ApplyResult::Ok);
  return finish(rewritten);
}

ApplyResult substitute_apply_sweep_equivalences(
    Kernel &k, const std::vector<std::pair<Lit, Lit>> &equivalences,
    SweepSubstitutionSession &session,
    std::vector<std::pair<Lit, Lit>> *applied_images) {
  if (applied_images) applied_images->clear();
  if (equivalences.empty())
    return ApplyResult::NoOp;
  assert(k.level == 0 && !k.conflict_valid);

  // Sync Sweep's implication binaries into the ledger even if every class is
  // declined.
  session.sync_new_binaries(k, equivalences);

  // Build only the classes stated by this emission batch (older SCCs are left
  // to the Substitute slot after Sweep), with substitute_find_sccs's
  // minimum-import-order representative.
  const Lit nlits = static_cast<Lit>(2 * k.nvars);
  // A pair is admitted iff both endpoints are in range, open and distinct
  // variables.
  const auto admitted = [&](Lit a, Lit b) {
    return a < nlits && b < nlits && a != b && a != negate(b) &&
           k.value(a) == 0 && k.value(b) == 0 &&
           !kernel_startup_retained(k, var_of(a)) &&
           !kernel_startup_retained(k, var_of(b));
  };
  std::vector<Lit> members;
  members.reserve(4 * equivalences.size());
  for (const auto &eq : equivalences) {
    const Lit a = eq.first, b = eq.second;
    if (!admitted(a, b))
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
  // Kissat substitutes each equivalence before the next (sweep.c
  // `sweep_equivalence` -> `substitute_connected_clauses`), so a merge's loser
  // carries everything it absorbed. Record that merge forest.
  std::vector<std::vector<Lit>> absorbed(members.size());
  const auto unite = [&](Lit a, Lit b) -> int {
    uint32_t ra = find(index_of(a)), rb = find(index_of(b));
    if (ra == rb)
      return -1;
    const uint32_t root_a = ra;
    if (kernel_import_lit_order(k, members[rb]) <
        kernel_import_lit_order(k, members[ra]))
      std::swap(ra, rb);
    const int losing_side = rb == root_a ? 0 : 1;
    parent[rb] = ra;
    absorbed[ra].push_back(members[rb]);
    return losing_side;
  };
  struct ProductivePair {
    Lit a, b;
    int losing_side;
  };
  std::vector<ProductivePair> productive;
  productive.reserve(equivalences.size());
  for (const auto &eq : equivalences) {
    const Lit a = eq.first, b = eq.second;
    if (!admitted(a, b))
      continue;
    const int losing_side = unite(a, b);
    const int mirror_losing_side = unite(negate(a), negate(b));
    assert(losing_side == mirror_losing_side);
    (void)mirror_losing_side;
    if (losing_side < 0)
      continue;
    productive.push_back({a, b, losing_side});
  }
  // Kissat exempts the emitted bridges only from their own pair's substitution
  // (`other == NOT (repr)`); replay later merges backwards so each bridge
  // carries the endpoints the sequential process leaves.
  std::vector<SweepSubstitutionSession::EquivalenceBridge> bridges;
  bridges.reserve(2 * productive.size());
  {
    std::unordered_map<Lit, Lit> suffix; // identity where absent
    const auto suffix_image = [&](Lit l) {
      const auto it = suffix.find(l);
      return it == suffix.end() ? l : it->second;
    };
    for (size_t i = productive.size(); i--;) {
      const ProductivePair &pair = productive[i];
      bridges.push_back({negate(pair.a), pair.b,
                         suffix_image(negate(pair.a)),
                         suffix_image(pair.b)});
      bridges.push_back({pair.a, negate(pair.b), suffix_image(pair.a),
                         suffix_image(negate(pair.b))});
      const Lit loser = pair.losing_side ? pair.b : pair.a;
      const Lit winner = pair.losing_side ? pair.a : pair.b;
      const Lit winner_image = suffix_image(winner);
      const Lit winner_mirror = suffix_image(negate(winner));
      suffix[loser] = winner_image;
      suffix[negate(loser)] = winner_mirror;
    }
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

  // Pre-order walk of the merge forest: a retiring literal, then everything it
  // absorbed (Kissat's `substitute_connected_clauses` stream). Polarities never
  // mix.
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

  // Preserve affine aliases while the graph certifies both directions; a
  // decline leaves the transaction unopened.
  if (!preserve_affine_aliases(k, sparse_map.size(), [&](auto &aliases) {
        for (const auto &[source, target] : sparse_map)
          if (!negated(source)) aliases.emplace_back(source, target);
      }))
    return ApplyResult::NoOp;
  session.rewrite_binary_occurrences(k, sparse_map, sources, bridges);
  const ApplyResult result = do_substitution(k, r, {}, &session);
  // Report the rewrite image, with polarity, for composing callers.
  if (applied_images)
    for (const Lit lit : session.retiring_sources())
      applied_images->emplace_back(lit, session.representative(lit));
  session.flush_moves(k);
  // Units and collapses inside `do_substitution` can change predicted images;
  // restore the membership invariant before the next seed.
  session.sync_new_binaries(k, equivalences);
  return result;
}

// Instance payload: an SCC representative literal; one instance substitutes
// that class and its mirror ¬rep.
static uint32_t substitute_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  // Public enumeration observes the current frontier. It must not spend the
  // solver's work currency before the caller chooses an instance.
  const SccResult r = substitute_find_sccs(k, true, false);
  return copy_literal_instance_prefix(k, r.nontrivial_reps, out, cap);
}

// One class, behind the shared root-literal instance prelude (D-LEVEL: the
// root closure is drained first; a conflict refutes).
static ApplyResult substitute_apply_class(Kernel &k, Lit rep) {
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
                     (apply_root_literal_instance<substitute_apply,
                                                  substitute_apply_class>));

} // namespace tactix

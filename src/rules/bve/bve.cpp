// BVE, rule E1: the occurrence index, the candidate schedule, the elimination
// round and the gate reduction. Transliterated idioms from
// kissat/src/eliminate.c + resolve.c + definition.c; the add-before-delete /
// root-false-strip / record_unit / witness mechanics follow V3 Substitute.
// See docs/design/e1-bve-design.md and bve.hpp.
#include "rules/bve/bve.hpp"

#include "diagnostics/event_trace.hpp"
#include "engine/subsolver.hpp"
#include "kernel/watch.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <utility>

namespace tactix {

// Growth slack: eliminate iff #non-tautological resolvents ≤ occ⁺ + occ⁻ +
// bound. `Kernel::bve_bound` is the one channel for it — no in-rule env pin
// may override a policy's write — and DefaultPolicy ramps it cheap-first, so
// the eliminations that do not grow the formula happen first (kissat
// eliminate.c set_next_elimination_bound).
static uint32_t bve_bound(const Kernel &k) { return k.bve_bound; }

RuleEligibility bve_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

void bve_build_occurrences(const Kernel &k, BveOccurrences &occ) {
  const uint32_t nlits = 2 * k.nvars;
  occ.counts.assign(nlits, 0);
  // Large irredundant clauses only: BVE eliminates over F, not over R.
  if (k.dense) {
    // dense fast-path: the shared index already lists each active literal's
    // irredundant large clauses — O(occ), no arena scan
    for (Lit l = 0; l < nlits; l++) {
      if (k.values[l] != 0) // active-variable literals only
        continue;
      k.dense->with_large(l, [&](auto refs) {
        for (const Ref ref : refs)
          if (!clause_at(k.arena, ref)->garbage())
            occ.counts[l]++;
      });
    }
  } else {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage() && !c->redundant())
        for (const Lit l : *c)
          if (k.values[l] == 0) // active-variable literals only
            occ.counts[l]++;
      ref += clause_actual_words(c);
    }
  }
  // binaries: one watch-list scan per active literal. They have an
  // irredundant lifecycle in this kernel, so all of them count.
  for (Lit l = 0; l < nlits; l++) {
    if (k.values[l] != 0)
      continue;
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && k.values[w.other()] == 0)
        occ.counts[l]++;
  }
}

void bve_flush_units_while_connected(Kernel &k, uint32_t trail_from) {
  assert(k.dense && k.dense->track_order);
  assert(trail_from <= k.propagate_head && k.propagate_head <= k.trail.size());
  for (uint32_t i = trail_from; i < k.propagate_head; ++i) {
    const Lit unit = k.trail[i];
    assert(k.value(unit) > 0 && k.assigned[var_of(unit)].level == 0);
    const auto ordered = k.dense->ordered[unit];
    for (const OccIndex::Entry entry : ordered) {
      if (entry.binary) {
        // eliminate.c::kissat_flush_units_while_connected keeps the satisfied
        // binary but republishes its unassigned partner's current score.
        if (k.value(entry.payload) == 0)
          kernel_notify_bve_score(k, var_of(entry.payload), true);
        continue;
      }
      Clause *c = clause_at(k.arena, entry.payload);
      if (c->garbage())
        continue;
      bool has_unit = false;
      for (const Lit l : *c)
        has_unit |= l == unit;
      if (!has_unit)
        continue;

      // `kissat_eliminate_clause (c, unit)` marks every non-fixed literal for
      // future elimination, then updates/reinserts every clause variable except
      // the satisfying unit.  Suppress the generic all-literal notifications
      // while retaining its proof, garbage, retry and removal bookkeeping.
      const BveScoreUpdateHook hook = k.bve_score_update_hook;
      void *const state = k.bve_score_update_state;
      k.bve_score_update_hook = nullptr;
      kernel_delete_clause(k, c);
      k.bve_score_update_hook = hook;
      k.bve_score_update_state = state;
      for (const Lit l : *c)
        if (l != unit && k.value(l) == 0)
          kernel_notify_bve_score(k, var_of(l), true);
    }

    // Kissat compacts only the satisfied unit's connected stack here.  Other
    // literal stacks retain their garbage references until inspected later.
    const uint32_t before = k.dense->large_size(unit);
    k.dense->compact_large(unit, [&](Ref ref) {
      const Clause *c = clause_at(k.arena, ref);
      if (c->garbage())
        return false;
      for (const Lit l : *c)
        if (l == unit)
          return true;
      return false;
    });
    const uint32_t removed = before - k.dense->large_size(unit);
    if (removed && k.bve_occurrence_count_image)
      kernel_bve_occurrence_remove(k, unit, removed);
  }
}

uint32_t bve_schedule_occurrence_count(const Kernel &k, Lit lit) {
  assert(lit < 2 * k.nvars);
  if (k.bve_occurrence_count_image) {
    assert(k.dense && k.bve_occurrence_count_image->size() == 2 * k.nvars);
    return (*k.bve_occurrence_count_image)[lit];
  }
  uint64_t count = 0;
  if (k.dense) {
    count = k.dense->large_size(lit);
  } else {
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage() && !c->redundant())
        for (const Lit other : *c)
          count += other == lit;
      ref += clause_actual_words(c);
    }
  }
  for (const WatchEntry w : k.watches.entries(lit))
    count += w.binary();
  return static_cast<uint32_t>(std::min<uint64_t>(count, UINT32_MAX));
}

void bve_schedule_from_occurrences(const Kernel &k, const BveOccurrences &occ,
                                   std::vector<Var> &out) {
  std::vector<std::pair<int64_t, Var>> ranked; // structural score, ascending
  for (Var v = 0; v < k.nvars; v++) {
    // The retry bitmap (Kernel::bve_retry) is deliberately not consulted here:
    // this rule maintains the advice, the policy reads it. See bve.hpp and
    // docs/implementation-notes.md §6.
    const Lit pl = make_lit(v, false);
    if (k.values[pl] != 0) // assigned variable: not an elimination candidate
      continue;
    const uint32_t p = occ.count(pl), n = occ.count(make_lit(v, true));
    // Kissat schedules every active variable whose `eliminate` flag is set
    // (eliminate.c schedule_variables), occurrences or not: a variable in no
    // elimination clause is popped, treated as pure by generate_resolvents,
    // and eliminated, which lowers `active` and shapes the schedule heap.
    // Kissat's variable_score uses p*n-p-n before adding search relevancy.
    // Keep that policy-independent structural component here; DefaultPolicy
    // supplies the current EVSIDS/VMTF term and the dynamic heap pose.
    ranked.emplace_back(static_cast<int64_t>(p) * n - p - n, v);
  }
  std::stable_sort(ranked.begin(), ranked.end());
  out.clear();
  out.reserve(ranked.size());
  for (const auto &pr : ranked)
    out.push_back(pr.second);
}

void bve_schedule(Kernel &k, std::vector<Var> &out) {
  BveOccurrences occ;
  bve_build_occurrences(k, occ);
  bve_schedule_from_occurrences(k, occ, out);
}

namespace {

// A slice into one candidate's reusable flat storage.  Kissat keeps pivot
// clauses and resolvents in raw stacks; giving every small clause its own
// std::vector instead made allocator ownership part of BVE's hot path.  The
// vector object is stable while its backing store grows, so slices retain
// offsets rather than pointers.  Logical order is unchanged.
template <typename T> struct PoolSlice {
  const std::vector<T> *pool = nullptr;
  uint32_t offset = 0;
  uint32_t length = 0;

  using const_iterator = typename std::vector<T>::const_iterator;
  const_iterator begin() const {
    assert(pool && offset <= pool->size());
    return pool->cbegin() + offset;
  }
  const_iterator end() const { return begin() + length; }
  size_t size() const { return length; }
  bool empty() const { return !length; }
  const T *data() const { return length ? pool->data() + offset : nullptr; }
  const T &operator[](size_t i) const {
    assert(i < length);
    return (*pool)[offset + i];
  }
};

template <typename T>
PoolSlice<T> pool_slice(const std::vector<T> &pool, size_t begin) {
  assert(begin <= pool.size());
  assert(begin <= UINT32_MAX && pool.size() - begin <= UINT32_MAX);
  return PoolSlice<T>{&pool, static_cast<uint32_t>(begin),
                      static_cast<uint32_t>(pool.size() - begin)};
}

// A materialized clause on the pivot: its active literals (root-false dropped,
// their unit ids kept in `absorbed` for LRAT), its proof id, and a delete
// handle. `active` includes the pivot.
struct MClause {
  PoolSlice<Lit> active;
  PoolSlice<uint64_t> absorbed;
  uint64_t id = 0;
  Ref ref = INVALID_REF; // INVALID_REF => binary
  Lit ba = INVALID_LIT, bb = INVALID_LIT;
};

struct BveResolvent {
  PoolSlice<Lit> lits;
  PoolSlice<uint64_t> chain;
  // Logical source identity is needed even when no LRAT hint chain is built:
  // FastEliminate can replace either parent during its feasibility scan and
  // must discard any earlier tentative image of that now-stale pair in every
  // proof mode.
  Ref first_parent = INVALID_REF;
  Ref second_parent = INVALID_REF;
};

// Sparse literal-to-clause lookup for one candidate. The dense value image is
// allocated once per BVE round; only entries written by the preceding
// candidate are reset. On multi-million-variable formulas, constructing two
// fresh 2*nvars integer vectors for every cheap candidate made zero-fill, not
// resolution, the dominant cost.
struct SparseBinaryIndex {
  std::vector<int32_t> values;
  std::vector<Lit> touched;

  void begin(size_t nlits) {
    for (const Lit lit : touched)
      values[lit] = -1;
    touched.clear();
    if (values.size() < nlits)
      values.resize(nlits, -1);
  }
  void set(Lit lit, int32_t value) {
    if (values[lit] < 0) {
      touched.push_back(lit);
      // Kissat's literal mark represents the first physical binary watch.
      // Retaining that first occurrence also lets AND/OR extraction consume
      // exactly one antecedent copy of an unrelated duplicated partner.
      values[lit] = value;
    }
  }
  int32_t get(Lit lit) const { return values[lit]; }
};

// Reusable candidate state, owned by `bve_eliminate`'s caller: a round creates
// one and every candidate reuses it, so no per-attempt O(nvars) fill-init
// happens inside a per-variable loop. A null pointer retains the standalone
// fallback used by direct rule consumers.
struct BveDefScratch {
  SubSolver solver;
  // Candidate materialization is one reusable append-only image.  MClause and
  // BveResolvent keep slices into these pools, avoiding one heap allocation per
  // antecedent/resolvent without changing any observable order.
  std::vector<MClause> pos, neg;
  std::vector<Lit> clause_lits;
  std::vector<uint64_t> clause_absorbed;
  std::vector<BveResolvent> resolvents;
  std::vector<Lit> resolvent_lits;
  std::vector<uint64_t> resolvent_chains;
  // Distinct non-pivot variables in first clause/literal occurrence order.
  // SubSolver's Kitten mode imports by that same first occurrence, so this is
  // both the direct reversible proof map and the source search identity.
  std::vector<Var> vars;
  std::vector<uint32_t> var_to_sub; // kernel var -> compact sub var + 1
  std::vector<Var> var_touched; // entries to restore in var_to_sub
  std::vector<SubLit> lits; // one reduced clause, being encoded
  SubDerivation deriv;
  SparseBinaryIndex pos_bin, neg_bin;
  // Per-candidate working sets of `bve_eliminate`.  Kept here so a
  // ten-million-candidate elimination phase allocates them once instead of
  // once per attempt; each is cleared or re-assigned at the point of use.
  std::vector<Ref> del_large;
  std::vector<std::pair<Lit, Lit>> del_bin;
  std::vector<uint8_t> pos_gate;
  std::vector<uint8_t> neg_gate;
  std::vector<uint32_t> pos_gate_order;
  std::vector<uint32_t> neg_gate_order;
  std::vector<uint32_t> pos_antecedent_order;
  std::vector<uint32_t> neg_antecedent_order;
  std::vector<uint32_t> pos_all;
  std::vector<uint32_t> neg_all;
  std::vector<Ref> fresh;
  std::vector<std::pair<Lit, Lit>> fresh_bin;
  std::vector<Lit> connected_lits;
  std::vector<uint64_t> connected_chain;
  std::vector<uint64_t> connected_extra_units;
  std::vector<uint8_t> literal_marks;
  uint64_t ticks = 0;    // ticks the last attempt spent (0 if none was made)
  Lit unit = INVALID_LIT; // one-sided core's failed pivot literal, if any
  bool checked = false;  // an attempt was made at all
  bool exhausted = false; // ...and the tick ceiling cut it short
  uint64_t session = UINT64_MAX; // strategy-owned embedded-Kitten lifetime
};

// ---- gate detection (equivalence, AND/OR, ITE, XOR, general definition) ----
// Mark the clauses of a functional gate defining x in pos_gate/neg_gate
// (1 = gate, 0 = antecedent). Every detector must establish condition (★) of
// docs/calculus.md §6b E1 exactly; a false gate drops needed resolvents. Only
// The definition type and a shortened ITE set `resolve_gate`: their gate×gate
// resolvents are real.  The ITE matcher below follows Kissat's
// `ifthenelse.c`, including its binary/root-shortened opposite-side clauses.

// Sorted signature of a clause's non-pivot active literals, for the exact
// lookups ITE and XOR need. Only clauses within the configured XOR arity
// literals and no repeated variable are indexed: a repeat means a duplicated
// literal or a tautology, and neither may stand in for a gate clause.
constexpr uint32_t BVE_SIG_MAX = BVE_XOR_ARITY_CAPACITY;
struct GateSig {
  uint32_t n = 0;
  Lit l[BVE_SIG_MAX] = {0};
  bool operator<(const GateSig &o) const {
    if (n != o.n)
      return n < o.n;
    // Fixed trip count over the whole array, not over `n`: with n == o.n the
    // tail is zero on both sides and cannot change the order, and the fixed
    // bound keeps std::sort's by-value pivot copy provably initialized to the
    // optimizer (-Wmaybe-uninitialized at -O3 otherwise).
    for (uint32_t i = 0; i < BVE_SIG_MAX; i++)
      if (l[i] != o.l[i])
        return l[i] < o.l[i];
    return false;
  }
};
using SigIndex = std::vector<std::pair<GateSig, uint32_t>>; // sorted by sig

static void bve_sort_signature(Lit *lits, uint32_t size) {
  assert(size <= BVE_SIG_MAX);
  // std::sort's 16-element unguarded insertion tail makes GCC diagnose an
  // impossible out-of-bounds access on this deliberately five-slot array.
  // Its actual small-range algorithm is insertion sort; spell that bounded
  // loop directly so the representation invariant stays visible.
  for (uint32_t i = 1; i < size; ++i) {
    const Lit value = lits[i];
    uint32_t j = i;
    while (j && value < lits[j - 1]) {
      lits[j] = lits[j - 1];
      --j;
    }
    lits[j] = value;
  }
}

void bve_build_sig_index(const std::vector<MClause> &side, Lit pivot,
                         uint32_t arity_limit, SigIndex &out) {
  arity_limit = std::min<uint32_t>(arity_limit, BVE_SIG_MAX);
  out.clear();
  for (size_t i = 0; i < side.size(); ++i) {
    const PoolSlice<Lit> &a = side[i].active;
    if (a.size() < 3 || a.size() > arity_limit + 1)
      continue;
    GateSig s;
    bool ok = true;
    for (const Lit l : a) {
      if (l == pivot)
        continue;
      if (s.n == BVE_SIG_MAX || var_of(l) == var_of(pivot)) {
        ok = false;
        break;
      }
      s.l[s.n++] = l;
    }
    if (!ok || s.n + 1 != a.size())
      continue;
    bve_sort_signature(s.l, s.n);
    for (uint32_t j = 1; j < s.n; j++)
      if (var_of(s.l[j]) == var_of(s.l[j - 1]))
        ok = false;
    if (ok)
      out.emplace_back(s, static_cast<uint32_t>(i));
  }
  std::sort(out.begin(), out.end(),
            [](const std::pair<GateSig, uint32_t> &a,
               const std::pair<GateSig, uint32_t> &b) {
              return a.first < b.first;
            });
}

// index of a clause whose non-pivot literals are exactly `s` (already sorted),
// or -1; duplicate signatures are copies of one logical clause, either of
// which serves as the gate clause
int bve_find_sig(const SigIndex &idx, const GateSig &s) {
  const auto it = std::lower_bound(
      idx.begin(), idx.end(), s,
      [](const std::pair<GateSig, uint32_t> &e, const GateSig &q) {
        return e.first < q;
      });
  if (it == idx.end() || s < it->first)
    return -1;
  return static_cast<int>(it->second);
}

inline GateSig bve_sig2(Lit a, Lit b) {
  GateSig s;
  s.n = 2;
  s.l[0] = std::min(a, b);
  s.l[1] = std::max(a, b);
  return s;
}

// General definition x ↔ φ, the one type that is not a shape match. Hand the
// pivot's occurrence lists, each clause minus the pivot literal, to the
// sub-solver: refuting that set is exactly (★) for G⁺ = P, G⁻ = N, and the
// core names the sub-multiset the refutation used, so (★) holds for the core
// alone and the core is the gate (kissat `definition.c`). The sub-solver uses
// Kitten's first-import VMTF order and tick currency. Nothing from it normally
// enters the proof except through the core selecting
// resolvents. A one-sided core is different: it proves a failed pivot literal,
// and its learned clauses are lifted below by appending that literal, exactly
// as Kissat's `traverse_one_sided_core_lemma` does. Kissat's default second
// core shrinks to the first core, deterministically shuffles Kitten's carried
// VMTF/watch/unit state, and resolves under a tenfold allowance below.
uint32_t bve_find_definition(const Kernel &k, Lit px,
                             const std::vector<MClause> &pos,
                             const std::vector<MClause> &neg,
                             std::vector<uint8_t> &pos_gate,
                             std::vector<uint8_t> &neg_gate,
                             BveDefScratch &s) {
  // One-sided pivots already have an empty cross product: nothing for a gate
  // to reduce, and (★) would be asking to refute one side alone.
  if (pos.empty() || neg.empty())
    return 0;
  const Var pv = var_of(px);
  for (const Var v : s.var_touched)
    s.var_to_sub[v] = 0;
  s.var_touched.clear();
  if (s.var_to_sub.size() < k.nvars)
    s.var_to_sub.resize(k.nvars, 0);
  s.vars.clear();
  // definition.c exports each physical occurrence clause with only the pivot
  // excepted. In particular, root-false literals remain Kitten variables;
  // resolution's active image has already stripped them and is not the same
  // input. Binaries cannot have a root-false partner at the drained fixpoint.
  const auto for_definition_lits = [&](const MClause &m, const auto &visit) {
    if (m.ref == INVALID_REF) {
      for (const Lit l : m.active)
        visit(l);
    } else {
      const Clause *physical = clause_at(k.arena, m.ref);
      assert(!physical->garbage());
      for (const Lit l : *physical)
        visit(l);
    }
  };
  for (const std::vector<MClause> *side : {&pos, &neg})
    for (const MClause &m : *side)
      for_definition_lits(m, [&](const Lit l) {
        if (const Var v = var_of(l); v != pv && !s.var_to_sub[v]) {
          // UINT32_MAX is only a seen marker until first-occurrence compact
          // ids are assigned below. Every touched entry is restored at the
          // next attempt, matching the persistent signed-mark discipline.
          s.var_to_sub[v] = UINT32_MAX;
          s.var_touched.push_back(v);
          s.vars.push_back(v);
        }
      });
  if (s.vars.empty())
    return 0; // every clause reduces to the empty clause: not a real pivot
  for (uint32_t i = 0; i < s.vars.size(); ++i)
    s.var_to_sub[s.vars[i]] = i + 1;
#ifndef NDEBUG
  for (uint32_t i = 0; i < s.vars.size(); ++i) {
    assert(s.var_to_sub[s.vars[i]] == i + 1);
  }
#endif
  const auto sub_of = [&s](Lit l) {
    const uint32_t mapped = s.var_to_sub[var_of(l)];
    assert(mapped && mapped != UINT32_MAX);
    return sub_lit(mapped - 1, negated(l));
  };

  // The one-instance rule entry keeps allocation scratch thread-local across
  // Kernel objects. Kitten's initialized counter is solver-session state, not
  // allocator state: reset it at the first real definition attempt of each
  // fresh Kernel so API clients cannot inherit another solver's shuffle seed.
  if (!k.stats.bve_definitions_checked ||
      s.session != k.bve_definition_session) {
    s.solver.start_kitten_session(/*clear_before_first_environment=*/true);
    s.session = k.bve_definition_session;
  }
  s.checked = true;
  // Kissat imports external variables on their first clause occurrence and
  // decides from the newest end of Kitten's VMTF queue. The first-occurrence
  // compact external ids above are a reversible proof map; Kitten mode assigns
  // the same internal ids directly from this unchanged clause/literal stream.
  // The reversible outer map is already dense in first clause/literal
  // occurrence order. Let the shared engine consume that compact identity
  // directly instead of rebuilding the same map once per definition attempt.
  s.solver.reset_compact_kitten(static_cast<uint32_t>(s.vars.size()));
  // Ids are handed out from 1 in add order, so id ∈ [1, |pos|] names
  // pos[id-1] and the rest neg[id-|pos|-1]. That mapping holds because
  // `add_clause` consumes an id even for a clause it drops as a tautology.
  for (const std::vector<MClause> *side : {&pos, &neg}) {
    for (const MClause &m : *side) {
      s.lits.clear();
      for_definition_lits(m, [&](const Lit l) {
        if (var_of(l) != pv)
          s.lits.push_back(sub_of(l));
      });
      s.solver.add_clause(s.lits);
    }
  }
  const uint64_t attempt = k.stats.bve_definitions_checked + 1;
  event_trace_bve_definition(
      k, pv, attempt, 0, "imported", s.solver.kitten_ticks(),
      k.bve_definition_budget, s.solver.kitten_generator(),
      static_cast<uint32_t>(s.vars.size()), static_cast<uint32_t>(pos.size()),
      static_cast<uint32_t>(neg.size()), nullptr, 0);
  SubResult res = s.solver.solve_interruptible(
      {}, k.bve_definition_budget, SubTickMode::Kitten, k.stop);
  s.ticks = s.solver.kitten_ticks();
  s.exhausted = res == SubResult::Budget;
  if (res != SubResult::Unsat || !s.solver.derivation(s.deriv) ||
      s.deriv.core.empty()) {
    event_trace_bve_definition(
        k, pv, attempt, 1,
        res == SubResult::Budget ? "budget" :
        res == SubResult::Sat ? "sat" : "no-derivation",
        s.ticks, k.bve_definition_budget, s.solver.kitten_generator(),
        static_cast<uint32_t>(s.vars.size()),
        static_cast<uint32_t>(pos.size()), static_cast<uint32_t>(neg.size()),
        nullptr, 0);
    return 0;
  }
  event_trace_bve_definition(
      k, pv, attempt, 1, "unsat", s.ticks, k.bve_definition_budget,
      s.solver.kitten_generator(), static_cast<uint32_t>(s.vars.size()),
      static_cast<uint32_t>(pos.size()), static_cast<uint32_t>(neg.size()),
      s.deriv.core.data(), static_cast<uint32_t>(s.deriv.core.size()));
  const auto refinement_budget = [&] {
    if (!k.bve_definition_budget)
      return uint64_t{0};
    const uint64_t multiplier = k.bve_definition_core_budget_multiplier;
    return multiplier && k.bve_definition_budget > UINT64_MAX / multiplier
               ? UINT64_MAX
               : multiplier * k.bve_definition_budget;
  };
  for (uint32_t core = 2; core <= k.bve_definition_cores; ++core) {
    s.solver.shrink_to_clausal_core(s.deriv.core);
    s.solver.shuffle_clauses();
    uint64_t ceiling = 0; // SubSolver's public zero means unlimited.
    if (k.bve_definition_budget) {
      const uint64_t delta = refinement_budget();
      const uint64_t current = s.solver.kitten_ticks();
      ceiling = UINT64_MAX - delta <= current ? UINT64_MAX : current + delta;
    }
    res = s.solver.solve_interruptible({}, ceiling, SubTickMode::Kitten,
                                       k.stop);
    s.ticks = s.solver.kitten_ticks();
    s.exhausted = res == SubResult::Budget;
    assert(res != SubResult::Sat && "an UNSAT core subset became SAT");
    if (res != SubResult::Unsat || !s.solver.derivation(s.deriv) ||
        s.deriv.core.empty()) {
      event_trace_bve_definition(
          k, pv, attempt, core,
          res == SubResult::Budget ? "budget" :
          res == SubResult::Sat ? "sat" : "no-derivation",
          s.ticks, refinement_budget(),
          s.solver.kitten_generator(),
          static_cast<uint32_t>(s.vars.size()),
          static_cast<uint32_t>(pos.size()),
          static_cast<uint32_t>(neg.size()), nullptr, 0);
      return 0;
    }
    event_trace_bve_definition(
        k, pv, attempt, core, "unsat", s.ticks, refinement_budget(),
        s.solver.kitten_generator(), static_cast<uint32_t>(s.vars.size()),
        static_cast<uint32_t>(pos.size()), static_cast<uint32_t>(neg.size()),
        s.deriv.core.data(), static_cast<uint32_t>(s.deriv.core.size()));
  }
  const uint32_t np = static_cast<uint32_t>(pos.size());
  uint32_t cp = 0, cn = 0;
  for (const uint32_t id : s.deriv.core) {
    if (id <= np) {
      pos_gate[id - 1] = 1;
      cp++;
    } else {
      neg_gate[id - np - 1] = 1;
      cn++;
    }
  }
  if (!cp)
    s.unit = negate(px);
  else if (!cn)
    s.unit = px;
  return BVE_GATE_DEFINITION;
}

// Lift the embedded refutation of one reduced pivot side into the main proof.
// Each sub-lemma L becomes (unit v L). Original sub-clause ids map to the
// corresponding live pivot clauses; LRAT additionally cites root units whose
// false literals were stripped on import. The final empty sub-lemma becomes
// the failed-literal unit. These proof-only lemmas are retired after the unit
// is installed. LRAT uses exact ids; DSR/DRAT remove one literal-multiset copy
// per emitted intermediate. Identical clauses are interchangeable in the
// latter formats, while retaining an extra copy would make a later direct-SR
// validator observe fewer clauses than the checker.
static void bve_emit_definition_unit(Kernel &k, Lit unit,
                                     const std::vector<MClause> &pos,
                                     const std::vector<MClause> &neg,
                                     BveDefScratch &s) {
  assert(unit != INVALID_LIT && k.value(unit) == 0);
  assert(!s.deriv.steps.empty() && s.deriv.steps.back().lits.empty());
  const bool lrat = k.proof && k.proof->lrat();
  const uint32_t np = static_cast<uint32_t>(pos.size());
  const uint32_t originals = np + static_cast<uint32_t>(neg.size());
  uint32_t max_sub_id = originals;
  for (const SubStep &step : s.deriv.steps)
    max_sub_id = std::max(max_sub_id, step.id);
  std::vector<uint64_t> id_of(static_cast<size_t>(max_sub_id) + 1, 0);
  for (uint32_t i = 0; i < np; i++)
    id_of[i + 1] = pos[i].id;
  for (uint32_t i = 0; i < neg.size(); i++)
    id_of[np + i + 1] = neg[i].id;

  const auto kernel_lit = [&](SubLit sl) {
    assert(sub_var_of(sl) < s.vars.size());
    return make_lit(s.vars[sub_var_of(sl)], (sl & 1u) != 0);
  };

  std::vector<Lit> lifted;
  std::vector<uint64_t> chain;
  struct Transient {
    uint64_t id;
    std::vector<Lit> lits;
  };
  std::vector<Transient> transients;
  uint64_t last_id = 0;
  for (size_t si = 0; si < s.deriv.steps.size(); si++) {
    const SubStep &step = s.deriv.steps[si];
    lifted.clear();
    lifted.push_back(unit);
    for (const SubLit sl : step.lits)
      lifted.push_back(kernel_lit(sl));
    assert(!Proof::tautological(lifted.data(),
                               static_cast<uint32_t>(lifted.size())));
    chain.clear();
    if (lrat)
      for (const uint32_t antecedent : step.chain) {
        assert(antecedent < id_of.size() && id_of[antecedent]);
        // Definition Kitten imports the physical clause, including root-false
        // literals. Do not cite the root units that resolution would use to
        // derive its stripped active image: the lifted sub-lemma retains those
        // literals and starts directly from the physical original.
        chain.push_back(id_of[antecedent]);
      }
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, lifted.data(),
                       static_cast<uint32_t>(lifted.size()),
                       lrat ? chain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(chain.size()) : 0);
    id_of[step.id] = id;
    last_id = id;
    if (k.proof && si + 1 != s.deriv.steps.size())
      transients.push_back({id, lifted});
  }
  assert(last_id);
  k.unit_ids[var_of(unit)] = last_id;
  kernel_assign_unit(k, unit);
  k.stats.bve_definition_units++;
  if (k.proof)
    for (size_t i = transients.size(); i-- > 0;)
      k.proof->del(k.next_clause_id, lrat ? transients[i].id : 0,
                   transients[i].lits.data(),
                   static_cast<uint32_t>(transients[i].lits.size()));
}

// Returns the BVE_GATE_* bit of the type found, or 0 for "no gate". `k` is
// const by design: detection is read-only and the caller owns the counters,
// with the definition attempt reporting its cost back through `def`.
uint32_t bve_detect_gate(const Kernel &k, Lit px, Lit nx,
                         const std::vector<MClause> &pos,
                         const std::vector<MClause> &neg,
                         std::vector<uint8_t> &pos_gate,
                         std::vector<uint8_t> &neg_gate, bool &resolve_gate,
                         BveDefScratch *def) {
  resolve_gate = false;
  // binary partner lookup: side_bin[lit] = index of a binary {pivot, lit}, or -1.
  SparseBinaryIndex local_pos_bin, local_neg_bin;
  SparseBinaryIndex &pos_bin = def ? def->pos_bin : local_pos_bin;
  SparseBinaryIndex &neg_bin = def ? def->neg_bin : local_neg_bin;
  pos_bin.begin(2 * static_cast<size_t>(k.nvars));
  neg_bin.begin(2 * static_cast<size_t>(k.nvars));
  for (size_t i = 0; i < pos.size(); ++i)
    if (pos[i].ref == INVALID_REF) {
      assert(pos[i].active.size() == 2);
      const Lit o = pos[i].active[0] == px ? pos[i].active[1] : pos[i].active[0];
      pos_bin.set(o, static_cast<int32_t>(i));
    }
  for (size_t j = 0; j < neg.size(); ++j)
    if (neg[j].ref == INVALID_REF) {
      assert(neg[j].active.size() == 2);
      const Lit o = neg[j].active[0] == nx ? neg[j].active[1] : neg[j].active[0];
      neg_bin.set(o, static_cast<int32_t>(j));
    }

  // (1) equivalence x↔¬w: pos binary (x∨w) + neg binary (¬x∨¬w) ⟹ ¬w→x, x→¬w.
  // kissat equivalences.c marks the physical binaries of the positive side and
  // takes the FIRST physical binary of the negative side whose partner's
  // complement is marked; large clauses shortened to two open literals are
  // never candidates on either side.
  if (k.bve_gate_mask & BVE_GATE_EQUIV)
  for (size_t j = 0; j < neg.size(); ++j)
    if (neg[j].ref == INVALID_REF) {
      const Lit o = neg[j].active[0] == nx ? neg[j].active[1] : neg[j].active[0];
      const int i = pos_bin.get(negate(o));
      if (i >= 0) {
        pos_gate[size_t(i)] = 1;
        neg_gate[j] = 1;
        return BVE_GATE_EQUIV;
      }
    }

  // (2) AND/OR: a base clause on one side whose every input has the matching
  // binary on the other; the base plus those binaries fully define x.
  auto try_and_or = [&](const std::vector<MClause> &base_side, Lit base_pivot,
                        const std::vector<MClause> &opp_side,
                        const SparseBinaryIndex &opp_bin,
                        std::vector<uint8_t> &base_gate,
                        std::vector<uint8_t> &opp_gate) -> bool {
    // kissat_find_and_gate declines before scanning bases unless the binary
    // side contains at least two distinct live partners.
    if (opp_bin.touched.size() < 2)
      return false;
    for (size_t b = 0; b < base_side.size(); ++b) {
      const MClause &base = base_side[b];
      // The base is a physical large clause, but root-false literals may have
      // shortened its active image to pivot plus one input. Kissat still uses
      // that base here; its equivalence detector only sees physical binaries.
      if (base.ref == INVALID_REF || base.active.size() < 2)
        continue;
      bool all = true;
      for (const Lit o : base.active)
        if (o != base_pivot && opp_bin.get(negate(o)) < 0) {
          all = false;
          break;
        }
      if (!all)
        continue;
      base_gate[b] = 1;
      for (const Lit o : base.active) {
        if (o == base_pivot)
          continue;
        opp_gate[size_t(opp_bin.get(negate(o)))] = 1;
      }
      // Kissat marks every distinct partner, clears the required inputs, then
      // walks the physical binary-watch stream.  Thus every required copy is
      // a gate.  For an unrelated duplicated partner its first watch consumes
      // the mark as the one antecedent and each later copy becomes a gate.
      // Reproduce that literal-mark consumption in one linear side scan.
      for (size_t i = 0; i < opp_side.size(); ++i) {
        // ands.c pushes physical binary watches only; a large clause whose
        // root-false literals leave two open ones is not a gate member.
        if (opp_side[i].ref != INVALID_REF)
          continue;
        const PoolSlice<Lit> &clause = opp_side[i].active;
        if (clause.size() != 2)
          continue;
        const Lit other = clause[0] == negate(base_pivot)
                              ? clause[1]
                              : clause[0];
        const int representative = opp_bin.get(other);
        if (representative >= 0 &&
            (opp_gate[size_t(representative)] ||
             i != static_cast<size_t>(representative)))
          opp_gate[i] = 1;
      }
      return true;
    }
    return false;
  };
  if (k.bve_gate_mask & BVE_GATE_AND_OR) {
    if (try_and_or(neg, nx, pos, pos_bin, neg_gate, pos_gate))
      return BVE_GATE_AND_OR;
    if (try_and_or(pos, px, neg, neg_bin, pos_gate, neg_gate))
      return BVE_GATE_AND_OR;
  }

  // the general definition is the LAST resort (kissat gates.c's own order): it
  // costs a sub-solve, where every shape above costs a scan.
  const auto definition = [&]() -> uint32_t {
    if (!def || !(k.bve_gate_mask & BVE_GATE_DEFINITION))
      return 0;
    const uint32_t kind =
        bve_find_definition(k, px, pos, neg, pos_gate, neg_gate, *def);
    // the ONLY place `resolve_gate` is set; see the argument above
    resolve_gate = kind != 0;
    return kind;
  };

  if (!(k.bve_gate_mask & (BVE_GATE_ITE | BVE_GATE_XOR)))
    return definition();
  // exact-match index over both sides, built once and shared by ITE and XOR;
  // the pos/neg MClause vectors are already materialized, so it is O(P+N) and
  // needs no O(nvars) scratch
  SigIndex psig, nsig;
  const uint32_t signature_arity =
      std::min<uint32_t>(BVE_SIG_MAX,
                         std::max<uint32_t>(BVE_XOR_ARITY_MIN,
                                            k.bve_xor_arity_limit));
  bve_build_sig_index(pos, px, signature_arity, psig);
  bve_build_sig_index(neg, nx, signature_arity, nsig);

  // (3) ITE, transliterated from pinned Kissat `ifthenelse.c`.  Two live
  // ternaries on one pivot side name the condition and branches.  Each
  // opposite-side clause may already have shortened to a binary (or a large
  // clause with fewer than three active literals); Kissat accepts that shape
  // and asks resolve.c to generate the otherwise non-tautological gate×gate
  // pairs.  Both pivot orientations are tried, in that order.
  auto try_ite_side = [&](Lit pivot, const std::vector<MClause> &side,
                          std::vector<uint8_t> &side_gate, Lit other_pivot,
                          const std::vector<MClause> &other,
                          std::vector<uint8_t> &other_gate) -> bool {
    uint64_t large_clauses = 0;
    for (const MClause &m : side)
      large_clauses += m.ref != INVALID_REF;
    const uint64_t limit = k.bve_occurrence_limit;
    if (large_clauses * large_clauses > limit)
      return false;

    const auto ternary = [](const MClause &m, Lit pivot, Lit &a, Lit &b,
                            Lit &c) {
      if (m.ref == INVALID_REF || m.active.size() != 3)
        return false;
      a = m.active[0], b = m.active[1], c = m.active[2];
      if (b == pivot)
        std::swap(a, b);
      if (c == pivot)
        std::swap(a, c);
      return a == pivot;
    };
    const auto find_match = [&](uint64_t &steps, Lit a, Lit b,
                                Lit c) -> int32_t {
      assert(a == other_pivot);
      for (size_t i = 0; i < other.size(); ++i) {
        steps++;
        const MClause &m = other[i];
        if (m.ref == INVALID_REF) {
          assert(m.active.size() == 2);
          const Lit o = m.active[0] == a ? m.active[1] : m.active[0];
          if (o == b || o == c) {
            resolve_gate = true;
            return static_cast<int32_t>(i);
          }
          continue;
        }
        uint32_t found = 0;
        bool matches = true;
        for (const Lit lit : m.active) {
          if (lit != a && lit != b && lit != c) {
            matches = false;
            break;
          }
          found++;
        }
        if (matches) {
          if (found < 3)
            resolve_gate = true;
          return static_cast<int32_t>(i);
        }
      }
      return -1;
    };

    uint64_t steps = 0;
    for (size_t i = 0; i + 1 < side.size() && steps < limit; ++i) {
      Lit a1, b1, c1;
      if (!ternary(side[i], pivot, a1, b1, c1))
        continue;
      for (size_t j = i + 1; j < side.size() && steps < limit; ++j) {
        Lit a2, b2, c2;
        if (!ternary(side[j], pivot, a2, b2, c2))
          continue;
        if (var_of(b1) == var_of(c2))
          std::swap(b2, c2);
        if (var_of(c1) == var_of(c2) || b1 != negate(b2))
          continue;
        resolve_gate = false;
        const int32_t p3 =
            find_match(steps, other_pivot, b1, negate(c1));
        if (p3 < 0)
          continue;
        const int32_t p4 =
            find_match(steps, other_pivot, b2, negate(c2));
        if (p4 < 0)
          continue;
        side_gate[i] = side_gate[j] = 1;
        other_gate[static_cast<size_t>(p3)] = 1;
        other_gate[static_cast<size_t>(p4)] = 1;
        return true;
      }
    }
    return false;
  };
  if (k.bve_gate_mask & BVE_GATE_ITE) {
    if (try_ite_side(px, pos, pos_gate, nx, neg, neg_gate) ||
        try_ite_side(nx, neg, neg_gate, px, pos, pos_gate))
      return BVE_GATE_ITE;
  }

  // (4) XOR: all 2^m parity clauses over the pivot and m other variables must
  //     be present (cadical gates.cpp find_xor_gate). A base clause fixes the
  //     variable set and the parity R, and the 2^m falsified assignments then
  //     name the 2^m clauses exactly, so this is a fixed enumeration.
  auto try_xor = [&]() -> bool {
    uint32_t gate_pos[1u << BVE_XOR_ARITY_CAPACITY];
    uint32_t gate_neg[1u << BVE_XOR_ARITY_CAPACITY];
    for (const std::pair<GateSig, uint32_t> &base : psig) {
      const uint32_t m = base.first.n; // arity
      if (m < 2)                       // m == 1 is an equivalence (case 1)
        continue;
      if (m > k.bve_xor_arity_limit)
        continue;
      // The relation is x ⊕ a₁ ⊕ … ⊕ aₘ = R. `base` is a pos-side clause, so
      // x = 0 in its falsifying assignment "aⱼ true iff base.l[j] is
      // negative", of parity p₀; a relation clause forbids exactly the
      // parity-(1⊕R) assignments, hence R = 1⊕p₀.
      uint32_t p0 = 0;
      for (uint32_t j = 0; j < m; j++)
        p0 ^= static_cast<uint32_t>(negated(base.first.l[j]));
      const uint32_t r = 1u ^ (p0 & 1u);
      uint32_t np = 0, nn = 0;
      bool all = true;
      GateSig probe;
      probe.n = m;
      for (uint32_t a = 0; a < (1u << m) && all; a++) {
        uint32_t par = 0;
        for (uint32_t j = 0; j < m; j++) {
          const uint32_t bit = (a >> j) & 1u;
          par ^= bit;
          // aⱼ true ⇒ the clause carries its NEGATIVE literal
          probe.l[j] = make_lit(var_of(base.first.l[j]), bit != 0);
        }
        bve_sort_signature(probe.l, m);
        // x's value in this falsified assignment: total parity must be 1⊕R
        const uint32_t xv = (1u ^ r) ^ par;
        const int hit = xv ? bve_find_sig(nsig, probe) : bve_find_sig(psig, probe);
        if (hit < 0)
          all = false;
        else if (xv)
          gate_neg[nn++] = static_cast<uint32_t>(hit);
        else
          gate_pos[np++] = static_cast<uint32_t>(hit);
      }
      if (!all)
        continue;
      for (uint32_t i = 0; i < np; i++)
        pos_gate[gate_pos[i]] = 1;
      for (uint32_t i = 0; i < nn; i++)
        neg_gate[gate_neg[i]] = 1;
      return true;
    }
    return false;
  };
  // A half-matched ITE pair leaves `resolve_gate` set (ifthenelse.c resets it
  // per pair only); kissat's next consumer is the definition, which owns the
  // flag.  The XOR shape's gate×gate resolvents are tautologies, so clear it.
  resolve_gate = false;
  if ((k.bve_gate_mask & BVE_GATE_XOR) && try_xor())
    return BVE_GATE_XOR;
  return definition();
}

// --- eager backward subsumption from fresh resolvents -----------------------
// CaDiCaL `elimbackward` (backward.cpp), drained at the tail of
// try_to_eliminate_variable (elim.cpp:731). Each fresh resolvent is the
// SUBSUMER over the occurrence list of its least-occurring literal: a victim
// `d ⊇ c` is deleted, one resolving with `c` on a single literal is shortened,
// and shortened victims re-enter the queue so a pivot reaches a fixpoint.
// kissat has no analogue; see `docs/implementation-notes.md`'s BVE section.

// Direction is the easy thing to invert: C1/C2's `check_marked` answers "does
// the partner subsume the candidate", the reverse of what is needed here.
// Deleting on that predicate retires the STRONGER clause, and no checker
// catches it — a `del` is unchecked, so the certificate verifies while the
// solver answers SAT on an UNSAT formula.
static void bve_backward(Kernel &k, std::vector<Ref> &queue,
                         std::vector<std::pair<Lit, Lit>> &fresh_bin,
                         std::vector<uint8_t> &marks) {
  if (!k.dense || k.inconsistent)
    return; // N1: the index is an acceleration, and the rule runs without it
  // The candidate gather/resolution pass has already restored this persistent
  // literal image. Reuse it and clear only each subsumer's touched literals:
  // allocating and zeroing 2*nvars bytes once per successful elimination can
  // dominate its local work; see docs/implementation-notes.md section 18.
  assert(marks.size() >= 2 * static_cast<size_t>(k.nvars));
  // Per-call working sets; BVE is not re-entrant and one elimination phase
  // calls this once per eliminated variable.
  static thread_local std::vector<Lit> clits, dlits;
  static thread_local std::vector<Ref> victims;
  static thread_local std::vector<Lit> bin_victims;
  clits.clear();
  dlits.clear();
  victims.clear();
  bin_victims.clear();
  const bool lrat = k.proof && k.proof->lrat();
  // Least-occurring-literal count, cadical's occs parity: dense->large is
  // large-only, so the binary share comes from the watch list. Counting stops
  // one past the configured limit — a count beyond it can only disqualify,
  // so the cap changes neither the argmin (a capped literal never beats an
  // under-limit one) nor the guard (all capped means the minimum is over).
  auto occ_len = [&](Lit l) {
    size_t n = k.dense->large_size(l);
    if (n > k.bve_backward_occurrence_limit)
      return n;
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && ++n > k.bve_backward_occurrence_limit)
        break;
    return n;
  };
  // One subsumer pass over `clits` (the snapshot; its proof id is `cid`).
  // `cref` names the arena copy of a large subsumer and is INVALID_REF for a
  // binary one, whose identity is the pair {clits[0], clits[1]}.
  auto backward_one = [&](uint64_t cid, Ref cref) {
    uint32_t size = 0;
    Lit best = INVALID_LIT;
    size_t len = 0;
    bool satisfied = false;
    for (const Lit l : clits) {
      const int8_t v = k.value(l);
      if (v > 0) { satisfied = true; break; }
      if (v < 0)
        continue; // root-false: absorbed, and never marked
      assert(!marks[l] && "BVE backward scratch leaked a prior mark");
      marks[l] = 1;
      size++;
      const size_t n = occ_len(l);
      if (best == INVALID_LIT || n < len) { best = l; len = n; }
    }
    if (satisfied || best == INVALID_LIT || size == 0 ||
        len > k.bve_backward_occurrence_limit) {
      // A root-satisfied subsumer is garbage, not merely unusable — a fresh
      // resolvent goes root-true the moment a sibling unit resolvent is
      // asserted (cadical backward.cpp:66-69 marks it garbage here too). A
      // duplicated binary pair refuses deletion by name (kernel.hpp) and
      // stays, the hazard-safe outcome.
      if (satisfied) {
        if (cref != INVALID_REF)
          kernel_delete_clause(k, clause_at(k.arena, cref));
        else
          kernel_delete_binary(k, clits[0], clits[1]);
      }
      for (const Lit l : clits) marks[l] = 0;
      return;
    }
    k.stats.bve_bwd_queued++;
    bool unit_derived = false;
    k.dense->copy_large(best, victims);
    for (const Ref dref : victims) {
      if (dref == cref || k.inconsistent)
        continue;
      Clause *d = clause_at(k.arena, dref);
      if (d->garbage() || d->redundant() || d->size < size)
        continue;
      // c ⊆ d (subsume), or c ⊆ d with exactly one literal flipped (strengthen).
      uint32_t found = 0;
      Lit negated = INVALID_LIT;
      bool dsat = false, bail = false;
      for (const Lit l : *d) {
        const int8_t v = k.value(l);
        if (v > 0) { dsat = true; break; }
        if (v < 0)
          continue;
        if (marks[l]) { found++; continue; }
        if (marks[negate(l)]) {
          if (negated != INVALID_LIT) { bail = true; break; }
          negated = l;
          found++;
        }
      }
      if (bail || (!dsat && found != size))
        continue;
      if (dsat || negated == INVALID_LIT) {
        // satisfied, or every literal of c occurs in d with the same sign.
        // `d` is never a trail reason, which is what makes the deletion safe
        // without a reason check: `d` comes from `occs(best)` so it contains
        // `best`, and `best` is unassigned by construction — a reason has all
        // its literals assigned. `kernel_delete_clause` does not retire a
        // reason, and the dosed path does not collect until phase exit, so a
        // garbage reason would survive to be cited by a later chain.
        kernel_delete_clause(k, d);
        if (!dsat)
          k.stats.bve_bwd_subsumed++;
        continue;
      }
      // self-subsuming resolution: d resolved with c on `negated` gives
      // d \ {negated}, which strictly subsumes d. `negated != best`, since d
      // holds `best` and a clause holding both polarities is a tautology — so
      // the strengthened clause keeps an unassigned literal and the
      // not-a-reason argument above survives the rewrite (cadical asserts the
      // same thing at backward.cpp:201).
      dlits.clear();
      std::vector<uint64_t> chain;
      for (const Lit l : *d) {
        if (l == negated)
          continue;
        if (k.value(l) < 0) {
          if (lrat)
            chain.push_back(k.unit_ids[var_of(l)]);
          continue;
        }
        dlits.push_back(l);
      }
      if (lrat) {
        // The SUBSUMER's root-false literals need their unit ids too, or `c`
        // is not unit under the RUP assumption and the line is unverifiable —
        // a fresh resolvent can carry a literal falsified by a sibling unit
        // resolvent asserted before this drain (cadical collects units over
        // {c\d} ∪ {d\c}, backward.cpp:149-182). Dedup against d's, which are
        // already in the chain.
        for (const Lit l : clits)
          if (k.value(l) < 0) {
            const uint64_t uid = k.unit_ids[var_of(l)];
            bool dup = false;
            for (const uint64_t have : chain)
              dup |= (have == uid);
            if (!dup)
              chain.push_back(uid);
          }
      }
      const uint64_t id = ++k.next_clause_id;
      if (k.proof) {
        if (lrat) {
          chain.push_back(cid);      // the subsumer propagates `negated` false
          chain.push_back(d->id());  // then the victim conflicts
          k.proof->add_rup(id, dlits.data(),
                           static_cast<uint32_t>(dlits.size()), chain.data(),
                           static_cast<uint32_t>(chain.size()));
        } else {
          k.proof->add_rup(0, dlits.data(),
                           static_cast<uint32_t>(dlits.size()), nullptr, 0);
        }
      }
      k.stats.bve_bwd_strengthened++;
      Ref nref = INVALID_REF;
      switch (kernel_replace_clause(k, d, id, dlits.data(),
                                    static_cast<uint32_t>(dlits.size()),
                                    /*redundant=*/false, 0, &nref)) {
      case ReplaceOutcome::Refuted:
        k.inconsistent = true;
        break;
      case ReplaceOutcome::Unit:
        // kernel_replace_clause owns only the deletion — unit semantics stay
        // with the caller (kernel.hpp), and the victim is already gone, so
        // dropping the literal here silently weakens the formula (wrong-SAT
        // class, invisible to every proof check of an UNSAT run). Root-false
        // literals were stripped into `dlits` above, so the literal is
        // unassigned or already root-true; assert it un-propagated — M1 owns
        // the cascade, the same contract as bve_eliminate's pending drain.
        k.stats.bve_bwd_units++;
        if (k.value(dlits[0]) == 0) {
          k.unit_ids[var_of(dlits[0])] = id;
          kernel_assign_unit(k, dlits[0]);
        }
        // A unit invalidates this subsumer's marks/size snapshot (its own
        // literals may now be assigned): abandon the walk as cadical does
        // (backward.cpp:195) and let the queue re-check at the next entry.
        unit_derived = true;
        break;
      case ReplaceOutcome::Binary:
        // Leaves the dense index, but re-enters as a binary subsumer under
        // the id kernel_replace_clause installed (cadical re-enqueues every
        // strengthened clause, backward.cpp:202).
        fresh_bin.emplace_back(dlits[0], dlits[1]);
        break;
      case ReplaceOutcome::Large:
        if (nref != INVALID_REF)
          queue.push_back(nref); // fixpoint, cadical backward.cpp:202
        break;
      }
      if (unit_derived)
        break;
    }
    // --- binary victims (the watch lists are their only index) ------------
    // A pair {best, o} is victim material only under an effective subsumer
    // size <= 2, the binary mirror of the `d->size < size` skip above. A
    // delete compacts the watch list of `best`, so the walk runs over a
    // snapshot of its pairs, never the live range.
    if (!unit_derived && !k.inconsistent && size <= 2) {
      const Lit self_other =
          cref != INVALID_REF ? INVALID_LIT
                              : (clits[0] == best ? clits[1] : clits[0]);
      bin_victims.clear();
      for (const WatchEntry w : k.watches.entries(best))
        if (w.binary())
          bin_victims.push_back(w.other());
      for (const Lit o : bin_victims) {
        if (k.inconsistent)
          break;
        // A binary subsumer skips its own pair by identity — the mirror of
        // `dref == cref` — including a root-false partner, which the
        // absorbed case below would otherwise misread as a subsumed victim,
        // deleting the subsumer through itself.
        if (o == self_other)
          continue;
        const int8_t vo = k.value(o);
        if (vo > 0)
          continue; // satisfied: not a victim, and its cleanup belongs to
                    // the root drain, not this walk
        uint32_t found = 1; // `best` is a marked subsumer literal
        Lit neg_lit = INVALID_LIT;
        if (vo == 0) {
          if (marks[o]) {
            found++;
          } else if (marks[negate(o)]) {
            neg_lit = o;
            found++;
          }
        } // vo < 0: root-false partner, absorbed exactly as in the walk above
        if (found != size)
          continue;
        // One id cannot name two live copies (kernel_delete_binary's
        // contract): a duplicated pair is neither deletable nor
        // strengthenable by name, so it is left in place.
        if (kernel_binary_copies(k, best, o, 2) != 1)
          continue;
        if (neg_lit == INVALID_LIT) {
          // Every marked literal occurs in {best, o} with the same sign. A
          // binary subsumer can match this way only through its own pair
          // (exact duplicates are dropped at add), skipped above; a large
          // subsumer whose extra literals are all absorbed root-false
          // genuinely subsumes the pair.
          const bool ok = kernel_delete_binary(k, best, o);
          assert(ok);
          (void)ok;
          k.stats.bve_bwd_subsumed++;
          continue;
        }
        // Self-subsuming resolution on `o`: {best, o} strengthens to the
        // unit {best}. `neg_lit != best` as above — a stored pair never
        // carries both polarities of one variable.
        const uint64_t vid = kernel_binary_id(k, best, o);
        std::vector<uint64_t> chain;
        if (lrat) {
          // Both victim literals are unassigned here (value-checked above),
          // so only the subsumer contributes absorbed-literal unit ids; its
          // variables are distinct, so the ids need no dedup.
          for (const Lit l : clits)
            if (k.value(l) < 0)
              chain.push_back(k.unit_ids[var_of(l)]);
        }
        const uint64_t id = ++k.next_clause_id;
        Lit unit = best;
        if (k.proof) {
          if (lrat) {
            chain.push_back(cid); // the subsumer propagates `o` false
            chain.push_back(vid); // then the victim pair conflicts
            k.proof->add_rup(id, &unit, 1, chain.data(),
                             static_cast<uint32_t>(chain.size()));
          } else {
            k.proof->add_rup(0, &unit, 1, nullptr, 0);
          }
        }
        // Add-before-delete: the unit's add precedes the victim's del.
        const bool ok = kernel_delete_binary(k, best, o);
        assert(ok);
        (void)ok;
        k.stats.bve_bwd_strengthened++;
        k.stats.bve_bwd_units++;
        if (k.value(unit) == 0) {
          k.unit_ids[var_of(unit)] = id;
          kernel_assign_unit(k, unit);
        }
        // A unit invalidates this subsumer's marks/size snapshot: abandon
        // the walk, the same contract as the Unit outcome above.
        unit_derived = true;
        break;
      }
    }
    for (const Lit l : clits) marks[l] = 0;
  };
  // Drain order: the large queue first, then the size-2 resolvents as binary
  // subsumers. cadical enqueues resolvents of every size into one FIFO
  // (elim.cpp:592-594); the split queues meet in this outer fixpoint, since a
  // binary subsumer can strengthen a large victim back into `queue` just as a
  // large one pushes pairs into `fresh_bin`.
  size_t qi = 0, bi = 0;
  while ((qi < queue.size() || bi < fresh_bin.size()) && !k.inconsistent) {
    for (; qi < queue.size() && !k.inconsistent; ++qi) {
      const Ref cref = queue[qi];
      {
        const Clause *c = clause_at(k.arena, cref);
        if (c->garbage())
          continue;
        // Snapshot: adding a strengthened victim can reallocate the arena,
        // so a Clause* held across the victim walk would dangle.
        clits.assign(c->begin(), c->end());
      }
      backward_one(clause_at(k.arena, cref)->id(), cref);
    }
    for (; bi < fresh_bin.size() && !k.inconsistent; ++bi) {
      const auto [a, b] = fresh_bin[bi]; // copy: the pass may grow the vector
      if (!kernel_binary_live(k, a, b))
        continue; // deleted since it was queued (subsumed or strengthened)
      clits.assign({a, b});
      backward_one(kernel_binary_id(k, a, b), INVALID_REF);
    }
  }
}

// Eliminate variable x by resolution at a drained root.  The physical
// candidate-stack scan can delete satisfied binaries and compact inspected
// garbage exactly as Kissat does before the growth bound is known; resolution
// clauses are not committed until that bound passes. Unsat on an empty
// resolvent or contradictory collapsed units.
// `NoOp` here means only that the pivot was kept: Kissat's resolve.c retires
// root-satisfied antecedents and installs unit resolvents as it meets them,
// before the candidate's growth decision, and those effects survive a decline.
// `bve_eliminate` turns that into the registry's result.
ApplyResult bve_attempt(Kernel &k, Var x, BveDefScratch *def) {
  assert(def && "BVE candidate scratch is required by every registered entry");
  const bool lrat = k.proof && k.proof->lrat();
  Lit px = make_lit(x, false), nx = make_lit(x, true);
  // Candidate contract, owed by every registered entry: the pivot is
  // unassigned and the root closure is complete.  Antecedent gathering reads
  // root values as final (a binary's partner cannot be root-false, a dropped
  // false literal already owns its unit id), and the definition refutation
  // derives a pivot unit the kernel must not already hold.
  assert(kernel_propagation_fixpoint(k));
  assert(k.value(px) == 0);
  // Retire the retry bit before doing the work, so a variable whose resolvents
  // blow the bound is not re-tried until its clauses change (kissat
  // eliminate.c eliminate_variable, same position).
  k.bve_retry[x] = 0;

  std::vector<MClause> &pos = def->pos; // live irredundant: resolved + witnessed
  std::vector<MClause> &neg = def->neg;
  pos.clear();
  neg.clear();
  def->clause_lits.clear();
  def->clause_absorbed.clear();
  def->resolvents.clear();
  def->resolvent_lits.clear();
  def->resolvent_chains.clear();
  std::vector<Ref> &del_large = def->del_large; // redundant/satisfied/taut → delete-only
  std::vector<std::pair<Lit, Lit>> &del_bin = def->del_bin; // satisfied binaries → delete-only
  del_large.clear();
  del_bin.clear();
  bool antecedent_oversized = false;
  bool rescore_popped_candidate = false;

  auto has_lit = [](const Clause *c, Lit target) {
    for (const Lit l : *c)
      if (l == target)
        return true;
    return false;
  };

#ifdef TACTIX_ENABLE_EVENT_TRACE
  // Pure diagnostic: its only consumers are the two trace calls below, which
  // are empty inline functions in a production build.  Left compiled in, it
  // walked both polarities' whole occurrence streams per candidate,
  // dereferenced every clause, ran a membership scan twice over each, and
  // built a literal vector for each one, all discarded.  Kissat's eliminate
  // has no counterpart.
  if (k.dense && k.dense->track_order) {
    struct OccurrenceAudit {
      uint32_t ordered = 0;
      uint32_t true_binary = 0;
      uint32_t satisfied_large = 0;
      uint32_t satisfied_effective_binary = 0;
      uint32_t tautological = 0;
    };
    uint32_t satisfied_ordinal = 0;
    const auto audit = [&](Lit pivot, Lit opposite) {
      OccurrenceAudit result;
      for (const OccIndex::Entry &entry : k.dense->ordered[pivot]) {
        result.ordered++;
        if (entry.binary) {
          result.true_binary += k.value(entry.payload) > 0;
          continue;
        }
        const Clause *c = clause_at(k.arena, entry.payload);
        if (c->garbage() || !has_lit(c, pivot))
          continue;
        bool satisfied = false;
        uint32_t root_nonfalse = 0;
        std::vector<Lit> nonfalse;
        for (const Lit l : *c)
          if (k.value(l) >= 0) {
            root_nonfalse++;
            satisfied |= k.value(l) > 0;
            nonfalse.push_back(l);
          }
        result.satisfied_large += satisfied;
        result.satisfied_effective_binary += satisfied && root_nonfalse == 2;
        if (satisfied)
          event_trace_bve_pair(
              k, x, 0x80000000u + satisfied_ordinal++, c->lits, c->size,
              entry.payload, c->id(), nonfalse.data(), nonfalse.size(),
              entry.payload, c->id(), "satisfied-occurrence-audit");
        result.tautological += has_lit(c, opposite);
      }
      return result;
    };
    const OccurrenceAudit positive = audit(px, nx);
    const OccurrenceAudit negative = audit(nx, px);
    event_trace_bve_occurrences(
        k, x, positive.ordered, negative.ordered, positive.true_binary,
        negative.true_binary, positive.satisfied_large,
        negative.satisfied_large, positive.satisfied_effective_binary,
        negative.satisfied_effective_binary, positive.tautological,
        negative.tautological);
  }
#endif

  // Exact `resolve.c::occurrences_literal` contract: scan each polarity in
  // connected order, deleting satisfied binaries and compacting garbage only
  // until the first live clause above `eliminateclslim`.  The oversized entry
  // and suffix remain physical occurrences, and a failed scan does not set the
  // popped-candidate update flag.  Tactix drives this from its mixed connected
  // order and mirrors each inspected garbage removal into the large index.
  // Test only garbage and physical size, never current clause membership.
  if (k.dense && k.dense->track_order) {
    for (const Lit pivot : {px, nx}) {
      // First identify the mutable prefix without copying the occurrence
      // stack.  Apply its deletions before inspecting the opposite polarity,
      // preserving Kissat's two sequential `occurrences_literal` calls.
      // Garbage has no score side effect of its own, so it can be compacted in
      // one stable pass after the satisfied binaries have been retired.
      std::vector<Lit> satisfied_binaries;
      std::vector<Ref> garbage_prefix;
      bool failed = false;
      for (const OccIndex::Entry entry : k.dense->ordered[pivot]) {
        if (entry.binary) {
          if (k.value(entry.payload) > 0)
            satisfied_binaries.push_back(entry.payload);
          continue;
        }
        const Ref ref = entry.payload;
        const Clause *c = clause_at(k.arena, ref);
        if (!c->garbage()) {
          if (c->size > k.bve_clause_size_limit) {
            failed = true;
            break;
          }
          continue;
        }
        garbage_prefix.push_back(ref);
      }
      for (const Lit other : satisfied_binaries) {
        const bool removed =
            kernel_delete_binary_one_copy_bve_scan(k, pivot, other);
        assert(removed);
        (void)removed;
      }
      if (!garbage_prefix.empty()) {
        size_t garbage = 0;
        k.dense->compact_large(pivot, [&](Ref ref) {
          if (garbage < garbage_prefix.size() &&
              ref == garbage_prefix[garbage]) {
            garbage++;
            return false;
          }
          return true;
        });
        assert(garbage == garbage_prefix.size());
        kernel_bve_occurrence_remove(
            k, pivot, static_cast<uint32_t>(garbage_prefix.size()));
      }
      const bool changed =
          !satisfied_binaries.empty() || !garbage_prefix.empty();
      antecedent_oversized |= failed;
      if (!failed && changed)
        rescore_popped_candidate = true;
    }
  }

  // `resolve.c::kissat_generate_resolvents` re-scores a popped candidate when
  // `occurrences_literal` compacted either polarity and the elimination later
  // fails.  `kissat_update_heap` deliberately leaves an absent item absent:
  // the candidate's eliminate flag was cleared before this attempt, so this
  // is an update-only notification, not a retry/reinsertion.  A successful
  // candidate needs neither because it is about to retire.
  const auto decline_after_compaction = [&]() {
    if (rescore_popped_candidate)
      kernel_notify_bve_score(k, x, false);
    return ApplyResult::NoOp;
  };

  // Kissat performs both polarity scans, then declines before gate detection,
  // antecedent gathering, or the failed-generation score update if either
  // returned UINT_MAX for an oversized physical clause.  Thus even successful
  // compaction on the other polarity does not rescore the popped variable.
  // In particular neither scan continues cleaning after its oversized entry.
  if (antecedent_oversized)
    return ApplyResult::NoOp;

  // ---- gather x's connected antecedents (duplicates retained) ----
  // `definition.c` and `resolve.c` import one mixed dense-watch stream.  A
  // binary created during the phase is appended after the arena occurrences
  // already connected at dense entry; hoisting it back into a binary prefix
  // preserves the formula but changes resolvent and later propagation order.
  std::vector<uint8_t> local_literal_marks;
  std::vector<uint8_t> &literal_marks =
      def ? def->literal_marks : local_literal_marks;
  if (literal_marks.size() < 2 * static_cast<size_t>(k.nvars))
    literal_marks.resize(2 * static_cast<size_t>(k.nvars), 0);
  auto gather_bin = [&](Lit lit, Lit o, std::vector<MClause> &live) {
      if (k.value(o) > 0) { // satisfied
        del_bin.emplace_back(lit, o);
        return;
      }
      // o root-false is impossible at a drained fixpoint (it would force `lit`).
      MClause m;
      m.ref = INVALID_REF;
      m.ba = lit;
      m.bb = o;
      m.id = kernel_binary_id(k, lit, o);
      const size_t begin = def->clause_lits.size();
      def->clause_lits.push_back(lit);
      def->clause_lits.push_back(o);
      m.active = pool_slice(def->clause_lits, begin);
      m.absorbed = pool_slice(def->clause_absorbed,
                              def->clause_absorbed.size());
      live.push_back(std::move(m));
  };

  // ---- gather x's large clauses ----
  // tautology / redundant / root-satisfied → delete-only; otherwise
  // materialize the active literals into pos (has px) or neg (has nx)
  auto classify = [&](Ref ref, const Clause *c, bool has_px, bool has_nx) {
    if (c->redundant()) {
      del_large.push_back(ref);
      return;
    }
    // Kissat resolve.c::occurrences_literal applies `eliminateclslim` to each
    // live irredundant antecedent before inspecting assignments or extracting
    // a gate/definition.  This is candidate admission owned by policy, not a
    // proof transformation: an unrestricted public-rule call keeps UINT_MAX.
    if (c->size > k.bve_clause_size_limit) {
      antecedent_oversized = true;
      return;
    }
    if (has_px && has_nx) {
      del_large.push_back(ref);
      return;
    }
    MClause m;
    m.id = c->id();
    m.ref = ref;
    const size_t active_begin = def->clause_lits.size();
    const size_t absorbed_begin = def->clause_absorbed.size();
    bool satisfied = false;
    for (const Lit l : *c) {
      const int8_t v = k.value(l);
      if (v > 0) { // root-true → clause satisfied → delete-only, no witness
        satisfied = true;
        break;
      }
      if (v < 0) { // root-false → drop (cite its unit)
        if (lrat)
          def->clause_absorbed.push_back(k.unit_ids[var_of(l)]);
        continue;
      }
      def->clause_lits.push_back(l);
    }
    if (satisfied) {
      def->clause_lits.resize(active_begin);
      def->clause_absorbed.resize(absorbed_begin);
      del_large.push_back(ref);
    } else {
      m.active = pool_slice(def->clause_lits, active_begin);
      m.absorbed = pool_slice(def->clause_absorbed, absorbed_begin);
      (has_px ? pos : neg).push_back(std::move(m));
    }
  };
  if (k.dense && k.dense->track_order) {
    // Consume the exact connected stream. Entries can be stale after deletion
    // or an in-place membership rewrite; filter them against current kernel
    // state without disturbing survivor order.
    const auto gather = [&](Lit pivot, Lit opposite,
                            std::vector<MClause> &live, bool positive) {
      for (const OccIndex::Entry &entry : k.dense->ordered[pivot]) {
        if (entry.binary) {
          // Dense ordered entries are maintained eagerly by every kernel
          // binary add/delete path.  They are therefore the same live stream
          // Kissat consumes, not a hint that needs an O(degree) watch-list
          // membership probe for every entry.  Large entries remain lazy and
          // are validated below because garbage and membership rewrites can
          // leave their records stale.
#ifndef NDEBUG
          assert(kernel_binary_live(k, pivot, entry.payload));
#endif
          gather_bin(pivot, entry.payload, live);
          continue;
        }
        const Ref ref = entry.payload;
        const Clause *c = clause_at(k.arena, ref);
        if (c->garbage() || !has_lit(c, pivot))
          continue;
        const bool has_opposite = has_lit(c, opposite);
        if (!positive && has_opposite)
          continue; // positive side already handled the tautology
        if (positive)
          classify(ref, c, true, has_opposite);
        else
          classify(ref, c, false, true);
      }
    };
    gather(px, nx, pos, true);
    gather(nx, px, neg, false);
  } else if (k.dense) {
    // Probe and public composition may supply the ordinary large-only dense
    // image without Eliminate's mixed binary/large order. Consume its exact
    // per-literal large slices, then the authoritative binary watches. This
    // keeps a PureOnly pass linear in the clauses it removes instead of
    // rescanning the whole arena once per pure variable. There is no mixed
    // ordering authority in this state; the deterministic large-then-binary
    // pose is therefore the complete available representation.
    const auto gather_large = [&](Lit pivot, Lit opposite,
                                  bool positive) {
      k.dense->with_large(pivot, [&](auto refs) {
        for (const Ref ref : refs) {
          const Clause *c = clause_at(k.arena, ref);
          if (c->garbage() || !has_lit(c, pivot))
            continue;
          const bool has_opposite = has_lit(c, opposite);
          if (!positive && has_opposite)
            continue;
          classify(ref, c, positive, !positive || has_opposite);
        }
      });
    };
    gather_large(px, nx, true);
    gather_large(nx, px, false);
    for (const WatchEntry w : k.watches.entries(px))
      if (w.binary())
        gather_bin(px, w.other(), pos);
    for (const WatchEntry w : k.watches.entries(nx))
      if (w.binary())
        gather_bin(nx, w.other(), neg);
  } else {
    // N1 fallback: one arena scan in the public no-index state.
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(c);
      if (!c->garbage()) {
        bool has_px = false, has_nx = false;
        for (const Lit l : *c) {
          has_px |= (l == px);
          has_nx |= (l == nx);
        }
        if (has_px || has_nx)
          classify(ref, c, has_px, has_nx);
      }
      ref += words;
    }
    // The public non-dense path has no reference connected-list pose; retain
    // its historical arena-first order and append binary watches afterward.
    for (const WatchEntry w : k.watches.entries(px))
      if (w.binary())
        gather_bin(px, w.other(), pos);
    for (const WatchEntry w : k.watches.entries(nx))
      if (w.binary())
        gather_bin(nx, w.other(), neg);
  }

  if (antecedent_oversized)
    return decline_after_compaction();

  // Reapplying Davis-Putnam to an already-retired, occurrence-free pivot has
  // no formula, reconstruction or active-population effect. In particular it
  // must not report another elimination and trap a canonical caller on it.
  // Keep every explicit identity selectable: retirement alone does not bypass
  // work when actual clauses remain on the requested pivot.
  if (kernel_var_retired(k, x) && pos.empty() && neg.empty() &&
      del_large.empty() && del_bin.empty())
    return decline_after_compaction();

  // Occurrence blow-up (kissat `eliminateocclim` = 2000, resolve.c:282-290):
  // resolving a variable whose two polarities occur together this often costs
  // more than the elimination can repay. The cap sits HERE, on the apply, where
  // kissat's does — deliberately NOT in `bve_schedule`, because that is what
  // `bve_instances` enumerates, and a COST cap there withholds an applicable
  // instance from the public API (N1, `RV-S17`). The rule offers the candidate
  // and declines it; it never fails to offer. `pos`/`neg` are x's occurrence
  // lists, so this needs no second index.
  if (!pos.empty() && !neg.empty() &&
      pos.size() + neg.size() > k.bve_occurrence_limit)
    return decline_after_compaction();

  // `resolve.c::kissat_generate_resolvents` makes the smaller physical
  // polarity `lit` before gate detection, antecedent extraction and pair
  // generation.  Equal populations retain the positive/internal-first side.
  // Keeping the side vectors and their pivot literals aligned makes the rest
  // of this symmetric transaction follow that exact orientation.
  if (!k.bve_fastel_traversal && pos.size() > neg.size()) {
    std::swap(pos, neg);
    std::swap(px, nx);
  }

  // Kissat resolve.c::kissat_generate_resolvents treats a candidate with an
  // empty polarity as pure and commits it immediately, before gate detection.
  // This includes a candidate whose last occurrence disappeared after it was
  // inserted into the mutable elimination heap: zero/zero is still retired,
  // rather than silently surviving as an active decision variable.
  const bool pure = pos.empty() || neg.empty();

  // ---- syntactic gate detection: restrict resolvents to gate×antecedent ----
  std::vector<uint8_t> &pos_gate = def->pos_gate;
  std::vector<uint8_t> &neg_gate = def->neg_gate;
  pos_gate.assign(pos.size(), 0);
  neg_gate.assign(neg.size(), 0);
  bool resolve_gate = false;
  if (def) {
    def->checked = false;
    def->exhausted = false;
    def->ticks = 0;
    def->unit = INVALID_LIT;
  }
  const uint32_t gate_kind =
      pure ? 0 : bve_detect_gate(k, px, nx, pos, neg, pos_gate, neg_gate,
                                 resolve_gate, def);
  const bool gate = gate_kind != 0;
  if (def && def->checked) {
    k.stats.bve_definitions_checked++;
    k.stats.bve_definition_ticks += def->ticks;
    k.stats.bve_definition_budget += def->exhausted ? 1 : 0;
  }
  if (def && def->unit != INVALID_LIT) {
    assert(gate_kind == BVE_GATE_DEFINITION);
    k.stats.bve_gates_definition++;
    bve_emit_definition_unit(k, def->unit, pos, neg, *def);
    return ApplyResult::Ok;
  }
  // (★) needs every pair holding a gate clause. A syntactic shape may drop the
  // gate×gate pairs only because they are tautologies; pinning which types may
  // makes an inexact matcher a deliberate edit rather than a silent one.
  assert((!resolve_gate || gate_kind == BVE_GATE_DEFINITION ||
          gate_kind == BVE_GATE_ITE) &&
         "a syntactic gate asked for its gate x gate resolvents back: its "
         "detection is no longer exact, so (star) must be re-argued");
  uint32_t gp = 0, gn = 0;
  uint64_t charged = static_cast<uint64_t>(pos.size()) * neg.size();
  if (gate) {
    // The gate shrinks the resolvent set the growth bound is applied to, so
    // the pairs it removes are what the bound's meaning changes by. Counted
    // per attempt: an over-budget attempt is where the lever matters.
    gp = static_cast<uint32_t>(
        std::count(pos_gate.begin(), pos_gate.end(), 1));
    gn = static_cast<uint32_t>(
        std::count(neg_gate.begin(), neg_gate.end(), 1));
    charged = static_cast<uint64_t>(gp) * (neg.size() - gn) +
              static_cast<uint64_t>(pos.size() - gp) * gn +
              (resolve_gate ? static_cast<uint64_t>(gp) * gn : 0);
    k.stats.bve_gate_pairs_saved += pos.size() * neg.size() - charged;
    switch (gate_kind) {
    case BVE_GATE_EQUIV: k.stats.bve_gates_equiv++; break;
    case BVE_GATE_AND_OR: k.stats.bve_gates_and_or++; break;
    case BVE_GATE_ITE: k.stats.bve_gates_ite++; break;
    case BVE_GATE_XOR: k.stats.bve_gates_xor++; break;
    default: k.stats.bve_gates_definition++; break;
    }
  }
  // The reference returns from resolvent generation at the pure branch, so it
  // has no gate/resolvent plan event for this candidate either.
  if (!pure)
    event_trace_bve_plan(k, x, static_cast<uint32_t>(pos.size()),
                         static_cast<uint32_t>(neg.size()), gate_kind, gp, gn,
                         charged, resolve_gate);

  // Preserve the extracted lists as ordered lists, not only membership bits.
  // Kissat's gates.c/get_antecedents keep each list in connected-watch order,
  // and resolve.c consumes those lists in three distinct stages.  Flattening
  // this plan into a filtered Cartesian product preserves the resolvent set but
  // changes clause insertion and dynamic elimination-heap update order.
  std::vector<uint32_t> &pos_gate_order = def->pos_gate_order;
  std::vector<uint32_t> &neg_gate_order = def->neg_gate_order;
  std::vector<uint32_t> &pos_antecedent_order = def->pos_antecedent_order;
  std::vector<uint32_t> &neg_antecedent_order = def->neg_antecedent_order;
  pos_gate_order.clear();
  neg_gate_order.clear();
  pos_antecedent_order.clear();
  neg_antecedent_order.clear();
  // gates.c get_antecedents does not consult the extractor's marks: it walks
  // the connected stream with one cursor over the gate stack and takes an
  // entry as a gate exactly when its raw watch equals the cursor's.  A binary
  // watch is raw-equal to every copy of the same pair, so an earlier duplicate
  // of a gate copy is consumed in its place and the flagged copy itself falls
  // through as an antecedent at its own (later) position.  Reproduce that
  // consumption over the flag bits instead of splitting on them directly.
  const auto split = [&](const std::vector<MClause> &side, Lit pivot,
                         const std::vector<uint8_t> &flags,
                         std::vector<uint32_t> &gates,
                         std::vector<uint32_t> &antecedents) {
    const auto raw_equal = [&](const MClause &a, const MClause &b) {
      if (a.ref != INVALID_REF || b.ref != INVALID_REF)
        return a.ref == b.ref;
      const Lit oa = a.ba == pivot ? a.bb : a.ba;
      const Lit ob = b.ba == pivot ? b.bb : b.ba;
      return oa == ob;
    };
    uint32_t cursor = 0; // next flagged index, in stream order
    for (uint32_t i = 0; i < side.size(); ++i) {
      while (cursor < side.size() && !flags[cursor])
        cursor++;
      if (cursor < side.size() && raw_equal(side[i], side[cursor])) {
        gates.push_back(i);
        cursor++;
      } else {
        antecedents.push_back(i);
      }
    }
    // every gate entry is consumed at or before its own index
    while (cursor < side.size() && !flags[cursor])
      cursor++;
    assert(cursor == side.size());
  };
  split(pos, px, pos_gate, pos_gate_order, pos_antecedent_order);
  split(neg, nx, neg_gate, neg_gate_order, neg_antecedent_order);

  // ---- generate resolvents (ordered gate stages or full cross product) + bound ----
  std::vector<BveResolvent> &resolvents = def->resolvents;
  const size_t base = pos.size() + neg.size();
  const size_t commit_limit = base + bve_bound(k); // eliminate iff ≤ this
  // Kissat aborts at the FIRST non-tautological resolvent beyond the current
  // bound.  Do not resolve ahead to a later ramp ceiling: those extra attempts
  // spend shared effort now and alter the dynamic heap trajectory.  The first
  // overflow still supplies the only sound retry hint available at this level
  // (`current bound + 1`); a later ramp level recomputes, as the reference does.
  const size_t gen_limit =
      std::min(commit_limit,
               static_cast<size_t>(k.bve_resolvent_count_limit));
  // SC2024 enters its mutating feasibility scan only when the raw cross
  // product exceeds fastelim.  At or below that absolute cap it proceeds
  // straight to ordinary resolution, so self-strengthening there would be a
  // broader transformation rather than a port of FastEliminate.
  const bool fastel_feasibility =
      k.bve_fastel_traversal && !neg.empty() &&
      pos.size() > static_cast<size_t>(k.bve_resolvent_count_limit) /
                       neg.size();
  bool oversized = false; // configured resolvent-size limit was exceeded
  bool over_gen = false;  // resolvent count passed gen_limit → needs > ceiling
  bool immediate_unsat = false;
  uint64_t resolved = 0;
  uint32_t pair_ordinal = 0;
  // The binary gather above restored every touched mark. Reuse the same
  // round-owned literal image for resolvent deduplication and tautology tests.
  std::vector<uint8_t> &seen = literal_marks;

  // Resolution can learn roots before the candidate's growth decision is
  // known.  Kissat installs those units immediately and never puts them in the
  // tentative resolvent stack, so they survive a later bound failure.  The
  // proof id becomes the root reason at the same point.
  const auto learn_unit = [&](Lit unit, const uint64_t *chain,
                              uint32_t chain_size) {
    assert(k.value(unit) == 0);
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, &unit, 1,
                       lrat ? chain : nullptr, lrat ? chain_size : 0);
    if (lrat)
      k.unit_ids[var_of(unit)] = id;
    kernel_assign_unit(k, unit);
  };
  const auto learn_empty = [&](const uint64_t *chain, uint32_t chain_size) {
    assert(!k.inconsistent);
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_empty(lrat ? id : 0, lrat ? chain : nullptr,
                         lrat ? chain_size : 0);
    k.inconsistent = true;
  };

  // resolve.c removes a newly root-satisfied large antecedent immediately,
  // even if this candidate later declines.  Binary temporaries are deliberately
  // retained here and are handled by the connected-unit flush after the
  // candidate.  Reproduce eliminate.c's score notifications: every unassigned
  // variable except the satisfying literal is updated/reinserted.
  const auto delete_satisfied_large = [&](const MClause &m, Lit satisfying) {
    if (m.ref == INVALID_REF)
      return;
    Clause *c = clause_at(k.arena, m.ref);
    if (c->garbage())
      return;
    const BveScoreUpdateHook hook = k.bve_score_update_hook;
    void *const state = k.bve_score_update_state;
    k.bve_score_update_hook = nullptr;
    kernel_delete_clause(k, c);
    k.bve_score_update_hook = hook;
    k.bve_score_update_state = state;
    c = clause_at(k.arena, m.ref);
    for (const Lit l : *c)
      if (l != satisfying && k.value(l) == 0)
        kernel_notify_bve_score(k, var_of(l), true);
  };

  const auto append_resolution_chain = [&](const MClause &c, Lit c_pivot,
                                           const MClause &d, Lit d_pivot) {
    const size_t begin = def->resolvent_chains.size();
    if (lrat) {
      // A root-false literal can occur on both resolution parents. Its named
      // unit is a propagation hint exactly once, even when both parents need
      // it before they become unit/conflicting (AM358-v2 H3).
      const auto add_root_hint = [&](uint64_t id) {
        assert(id != 0);
        if (std::find(def->resolvent_chains.begin() + begin,
                      def->resolvent_chains.end(), id) ==
            def->resolvent_chains.end())
          def->resolvent_chains.push_back(id);
      };
      for (const uint64_t id : c.absorbed)
        add_root_hint(id);
      for (const uint64_t id : d.absorbed)
        add_root_hint(id);
      // These literals were unassigned when the antecedents were gathered but
      // became root-false through an earlier immediate resolvent unit.
      for (const Lit l : c.active)
        if (l != c_pivot && k.value(l) < 0)
          add_root_hint(k.unit_ids[var_of(l)]);
      for (const Lit l : d.active)
        if (l != d_pivot && k.value(l) < 0)
          add_root_hint(k.unit_ids[var_of(l)]);
      def->resolvent_chains.push_back(c.id);
      def->resolvent_chains.push_back(d.id);
    }
    return begin;
  };

  const auto generate_pairs = [&](const std::vector<MClause> &first,
                                  Lit first_pivot,
                                  const std::vector<uint32_t> &first_order,
                                  const std::vector<MClause> &second,
                                  Lit second_pivot,
                                  const std::vector<uint32_t> &second_order) {
    for (const uint32_t ci : first_order) {
      if (immediate_unsat || oversized || over_gen)
        break;
      const MClause &c = first[ci];
      if (c.ref != INVALID_REF && clause_at(k.arena, c.ref)->garbage())
        continue;
      bool first_satisfied = false;
      for (const Lit l : c.active) {
        if (l == first_pivot || k.value(l) <= 0)
          continue;
        first_satisfied = true;
        delete_satisfied_large(c, l);
        break;
      }
      if (first_satisfied)
        continue;
      for (const Lit l : c.active)
        if (l != first_pivot)
          seen[l] = 1;
      for (const uint32_t di : second_order) {
        const MClause &d = second[di];
        if (d.ref != INVALID_REF && clause_at(k.arena, d.ref)->garbage())
          continue;
        k.stats.bve_resolutions++; // one attempt (kissat resolve.c:157)
        const uint32_t ordinal = pair_ordinal++;
        const size_t lits_begin = def->resolvent_lits.size();
        bool satisfied_or_tautological = false;
        // Ordinary Kissat resolve.c copies the second antecedent then the
        // first. SC2024 fastel keeps positive before negative for large/large
        // pairs and puts the binary literal first for mixed pairs. Build the
        // same logical second+first image, then rotate only when the posed
        // traversal owns first+second physical order.
        for (const Lit l : d.active) {
          if (l == second_pivot)
            continue;
          const int8_t value = k.value(l);
          if (value < 0)
            continue;
          if (value > 0) {
            delete_satisfied_large(d, l);
            satisfied_or_tautological = true;
            break;
          }
          if (seen[l]) // duplicate with c
            continue;
          if (seen[negate(l)]) { // l and ¬l both present → tautology
            satisfied_or_tautological = true;
            break;
          }
          def->resolvent_lits.push_back(l);
        }
        const size_t second_size = def->resolvent_lits.size() - lits_begin;
        if (satisfied_or_tautological) {
          def->resolvent_lits.resize(lits_begin);
          event_trace_bve_pair(k, x, ordinal, c.active.data(), c.active.size(),
                               c.ref, c.id, d.active.data(), d.active.size(),
                               d.ref, d.id, "satisfied-or-tautology");
          continue;
        }
        // Ordinary eliminate counts every surviving pair before materializing
        // it.  SC2024 fastel first lets a mixed or large/large resolution
        // shorten a large antecedent; such a pair is not part of its absolute
        // resolvent count.
        // Defer that traversal's count until after the checked strengthening
        // below (fastel.c::can_fast_resolve_large_large).
        if (!fastel_feasibility && ++resolved > gen_limit) {
          def->resolvent_lits.resize(lits_begin);
          event_trace_bve_pair(k, x, ordinal, c.active.data(), c.active.size(),
                               c.ref, c.id, d.active.data(), d.active.size(),
                               d.ref, d.id, "bound");
          over_gen = true;
          break;
        }
        for (const Lit l : c.active) {
          if (l == first_pivot)
            continue;
          const int8_t value = k.value(l);
          assert(value <= 0);
          if (!value)
            def->resolvent_lits.push_back(l);
        }
        if (k.bve_fastel_traversal) {
          const bool first_binary = c.ref == INVALID_REF;
          const bool second_binary = d.ref == INVALID_REF;
          const bool first_then_second = first_binary || !second_binary;
          if (first_then_second && second_size) {
            auto begin = def->resolvent_lits.begin() + lits_begin;
            std::rotate(begin, begin + second_size,
                        def->resolvent_lits.end());
          }
        }
        const size_t lits_size = def->resolvent_lits.size() - lits_begin;
        const size_t chain_begin =
            append_resolution_chain(c, first_pivot, d, second_pivot);
        const uint32_t chain_size = static_cast<uint32_t>(
            def->resolvent_chains.size() - chain_begin);
        if (!lits_size) {
          event_trace_bve_pair(k, x, ordinal, c.active.data(), c.active.size(),
                               c.ref, c.id, d.active.data(), d.active.size(),
                               d.ref, d.id, "empty-immediate");
          learn_empty(def->resolvent_chains.data() + chain_begin, chain_size);
          immediate_unsat = true;
          break;
        }
        if (lits_size == 1) {
          const Lit unit = def->resolvent_lits[lits_begin];
          event_trace_bve_pair(k, x, ordinal, c.active.data(), c.active.size(),
                               c.ref, c.id, d.active.data(), d.active.size(),
                               d.ref, d.id, "unit-immediate");
          learn_unit(unit, def->resolvent_chains.data() + chain_begin,
                     chain_size);
          def->resolvent_lits.resize(lits_begin);
          def->resolvent_chains.resize(chain_begin);
          if (seen[unit]) {
            first_satisfied = true;
            break;
          }
          continue;
        }
        if (fastel_feasibility &&
            (c.ref != INVALID_REF || d.ref != INVALID_REF)) {
          // Reuse the resolution marks with a second bit for the materialized
          // resolvent.  This keeps containment linear in the two parents and
          // resolvent (the same work covered by this resolution tick), rather
          // than performing an uncharged quadratic membership search.
          for (size_t i = lits_begin; i < def->resolvent_lits.size(); ++i)
            seen[def->resolvent_lits[i]] |= 2;
          const auto subsumes_parent = [&](const MClause &parent) {
            if (parent.ref == INVALID_REF)
              return false;
            const Clause *live = clause_at(k.arena, parent.ref);
            if (live->garbage() || lits_size >= live->size)
              return false;
            size_t contained = 0;
            for (const Lit l : parent.active)
              contained += !!(seen[l] & 2);
            return contained == lits_size;
          };
          const bool c_subsumed = subsumes_parent(c);
          const bool d_subsumed = subsumes_parent(d);
          for (size_t i = lits_begin; i < def->resolvent_lits.size(); ++i)
            seen[def->resolvent_lits[i]] &= 1;
          if (c_subsumed || d_subsumed) {
            event_trace_bve_pair(k, x, ordinal, c.active.data(), c.active.size(),
                                 c.ref, c.id, d.active.data(), d.active.size(),
                                 d.ref, d.id, "fastel-strengthened");
            const uint64_t id = ++k.next_clause_id;
            const Lit *const lits = def->resolvent_lits.data() + lits_begin;
            if (k.proof) {
              if (lrat)
                k.proof->add_rup(id, lits, static_cast<uint32_t>(lits_size),
                                 def->resolvent_chains.data() + chain_begin,
                                 chain_size);
              else
                k.proof->add_rup(0, lits, static_cast<uint32_t>(lits_size),
                                 nullptr, 0);
            }

            // A pending resolvent whose parent is replaced is still entailed,
            // but native fastel recomputes the final resolvents after this
            // feasibility pass and therefore does not publish that stale
            // image.  Remove it by its retained two-parent identity (the same
            // pair is the LRAT chain tail when present) while retaining the
            // append-only pools backing every surviving slice.
            const auto uses_replaced_parent = [&](const BveResolvent &r) {
              return (c_subsumed &&
                      (r.first_parent == c.ref || r.second_parent == c.ref)) ||
                     (d_subsumed &&
                      (r.first_parent == d.ref || r.second_parent == d.ref));
            };
            resolvents.erase(
                std::remove_if(resolvents.begin(), resolvents.end(),
                               uses_replaced_parent),
                resolvents.end());

            const MClause &victim = c_subsumed ? c : d;
            Ref replacement = INVALID_REF;
            const ReplaceOutcome outcome = kernel_replace_clause(
                k, clause_at(k.arena, victim.ref), id, lits,
                static_cast<uint32_t>(lits_size), /*redundant=*/false, 0,
                &replacement);
            assert(outcome == ReplaceOutcome::Binary ||
                   outcome == ReplaceOutcome::Large);
            (void)outcome;
            (void)replacement;
            if (c_subsumed && d_subsumed) {
              const Ref other = victim.ref == c.ref ? d.ref : c.ref;
              Clause *const clause = clause_at(k.arena, other);
              if (!clause->garbage())
                kernel_delete_clause(k, clause);
            }
            k.stats.strengthened++;
            def->resolvent_lits.resize(lits_begin);
            def->resolvent_chains.resize(chain_begin);
            // Replacing the outer antecedent ends its row.  The next outer
            // entry observes the compacted connected stream; replacing only
            // the inner antecedent continues with the next live entry.
            if (c_subsumed) {
              first_satisfied = true;
              break;
            }
            continue;
          }
        }
        if (fastel_feasibility && ++resolved > gen_limit) {
          def->resolvent_lits.resize(lits_begin);
          def->resolvent_chains.resize(chain_begin);
          event_trace_bve_pair(k, x, ordinal, c.active.data(), c.active.size(),
                               c.ref, c.id, d.active.data(), d.active.size(),
                               d.ref, d.id, "bound");
          over_gen = true;
          break;
        }
        if (lits_size > k.bve_resolvent_size_limit) {
          def->resolvent_lits.resize(lits_begin);
          def->resolvent_chains.resize(chain_begin);
          event_trace_bve_pair(k, x, ordinal, c.active.data(), c.active.size(),
                               c.ref, c.id, d.active.data(), d.active.size(),
                               d.ref, d.id, "oversized");
          oversized = true;
          break;
        }
        BveResolvent r;
        r.lits = pool_slice(def->resolvent_lits, lits_begin);
        r.chain = pool_slice(def->resolvent_chains, chain_begin);
        r.first_parent = c.ref;
        r.second_parent = d.ref;
        resolvents.push_back(r);
        event_trace_bve_pair(k, x, ordinal, c.active.data(), c.active.size(),
                             c.ref, c.id, d.active.data(), d.active.size(),
                             d.ref, d.id, "kept");
      }
      for (const Lit l : c.active)
        if (l != first_pivot)
          seen[l] = 0;
      if (first_satisfied)
        continue;
    }
  };

  std::vector<uint32_t> &pos_all = def->pos_all;
  std::vector<uint32_t> &neg_all = def->neg_all;
  pos_all.clear();
  neg_all.clear();
  if (gate) {
    // resolve.c: gates[0]×antecedents[1], gates[1]×antecedents[0], then the
    // gate cross-product only for an incomplete/general definition.
    generate_pairs(pos, px, pos_gate_order, neg, nx,
                   neg_antecedent_order);
    generate_pairs(neg, nx, neg_gate_order, pos, px,
                   pos_antecedent_order);
    if (resolve_gate)
      generate_pairs(pos, px, pos_gate_order, neg, nx, neg_gate_order);
  } else {
    pos_all.resize(pos.size());
    neg_all.resize(neg.size());
    std::iota(pos_all.begin(), pos_all.end(), 0u);
    std::iota(neg_all.begin(), neg_all.end(), 0u);
    generate_pairs(pos, px, pos_all, neg, nx, neg_all);
  }
  if (immediate_unsat)
    return ApplyResult::Unsat;
  // Growth decision: eliminate iff not oversized and within the current
  // commit slack. Kissat records only the cleared per-variable eliminate bit;
  // a later bound increase blanket-rearms every variable.
  if (oversized || over_gen) {
    return decline_after_compaction();
  }

  // ---- commit: add resolvents (RUP, add-before-delete), witnesses, delete ---
  std::vector<Ref> &fresh = def->fresh; // large resolvents, the backward pass's subsumers
  std::vector<std::pair<Lit, Lit>> &fresh_bin = def->fresh_bin; // size-2 resolvents, ditto
  std::vector<Lit> &connected_lits = def->connected_lits;
  std::vector<uint64_t> &connected_chain = def->connected_chain;
  std::vector<uint64_t> &connected_extra_units = def->connected_extra_units;
  fresh.clear();
  fresh_bin.clear();
  connected_lits.clear();
  connected_chain.clear();

  for (const BveResolvent &r : resolvents) {
    connected_lits.clear();
    bool satisfied = false;
    // A literal unassigned when the resolvent was generated can be root-false
    // now, falsified by a sibling immediate unit resolved after it. Its unit
    // id must join the chain BEFORE the two resolution antecedents, not after:
    // the RUP replay reaches those clauses only once every literal they carry
    // is false, so a unit cited past them leaves the clause non-unit and the
    // line unverifiable (report 3.69). `append_resolution_chain` ends r.chain
    // with exactly `c.id, d.id`, so the units belong ahead of that pair.
    connected_extra_units.clear();
    for (const Lit l : r.lits) {
      const int8_t value = k.value(l);
      if (value > 0) {
        satisfied = true;
        break;
      }
      if (value < 0) {
        if (lrat)
          connected_extra_units.push_back(k.unit_ids[var_of(l)]);
      } else {
        connected_lits.push_back(l);
      }
    }
    if (satisfied)
      continue;
    connected_chain.clear();
    if (lrat) {
      assert(r.chain.size() >= 2 &&
             "append_resolution_chain always ends with c.id, d.id");
      const size_t clause_tail = r.chain.size() - 2;
      connected_chain.assign(r.chain.begin(), r.chain.begin() + clause_tail);
      connected_chain.insert(connected_chain.end(),
                             connected_extra_units.begin(),
                             connected_extra_units.end());
      connected_chain.push_back(r.chain[clause_tail]);
      connected_chain.push_back(r.chain[clause_tail + 1]);
    }
    if (connected_lits.empty()) {
      learn_empty(connected_chain.data(),
                  static_cast<uint32_t>(connected_chain.size()));
      return ApplyResult::Unsat;
    }
    if (connected_lits.size() == 1) {
      learn_unit(connected_lits[0], connected_chain.data(),
                 static_cast<uint32_t>(connected_chain.size()));
      continue;
    }
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) {
      if (lrat)
        k.proof->add_rup(id, connected_lits.data(),
                         static_cast<uint32_t>(connected_lits.size()),
                         connected_chain.data(),
                         static_cast<uint32_t>(connected_chain.size()));
      else
        k.proof->add_rup(0, connected_lits.data(),
                         static_cast<uint32_t>(connected_lits.size()), nullptr,
                         0);
    }
    if (connected_lits.size() == 2) {
      kernel_add_binary_clause(k, connected_lits[0], connected_lits[1], id);
      kernel_defrag_vectors(k);
      fresh_bin.emplace_back(connected_lits[0], connected_lits[1]);
    } else {
      fresh.push_back(kernel_add_large_clause(
          k, connected_lits.data(),
          static_cast<uint32_t>(connected_lits.size()),
          /*redundant=*/false, /*glue=*/0, id));
      kernel_defrag_vectors(k);
      // Adding a large clause may reallocate the arena, but the Refs held in
      // pos/neg/del_large are offsets and survive a resize; only a Clause*
      // would dangle, and every use below re-fetches through clause_at.
    }
  }

  // reconstruction witnesses: each removed live clause, pivot as its blocking
  // literal, both sides (design §3.4). Satisfied, redundant and tautological
  // clauses need none — they are entailed or already true.
  for (const std::vector<MClause> *side : {&pos, &neg}) {
    const Lit pivot = (side == &pos) ? px : nx;
    for (const MClause &m : *side) {
      if (m.ref != INVALID_REF && clause_at(k.arena, m.ref)->garbage())
        continue;
      bool satisfied = false;
      for (const Lit l : m.active)
        if (l != pivot && k.value(l) > 0) {
          satisfied = true;
          break;
        }
      if (satisfied)
        continue;
      std::vector<Lit> rest;
      rest.reserve(m.active.size());
      for (const Lit l : m.active)
        if (l != pivot)
          rest.push_back(l);
      kernel_push_witness(k, pivot, rest.data(),
                          static_cast<uint32_t>(rest.size()));
    }
  }
  // Deactivate the pivot with its witness, the V3 Substitute idiom (status and
  // ρ entry created together): both references retire it here — cadical
  // elim.cpp:723 flags.cpp:34-46, kissat eliminate.c:405 flags.c:77-100 — so
  // it leaves the decision structures and the active-variable count instead of
  // burning propagation-free decisions, and the phase fixpoint test sees BVE
  // progress. Reconstruction supplies its value either way.
  if (k.var_status.size() < k.nvars)
    k.var_status.resize(k.nvars, VarStatus::Active);
  if (k.var_status[x] == VarStatus::Active && k.values[px] == 0) {
    assert(k.unassigned_active > 0);
    k.unassigned_active--;
  }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
  rule_journal_note_variable_status(k, x, "eliminated");
#endif
  kernel_affine_note_namespace_change(k);
  k.var_status[x] = VarStatus::Eliminated;
  k.retired_events.push_back(x); // the policy deactivates its own H from this

  // delete every clause on x: live (pos/neg) plus delete-only (redundant,
  // satisfied, tautology). Large → proof del + garbage; each connected binary
  // occurrence → one physical-copy deletion in the same side/list order.
  auto del_large_ref = [&](Ref ref) {
    Clause *c = clause_at(k.arena, ref);
    if (c->garbage())
      return;
    kernel_delete_clause(k, c);
  };
  auto del_all = [&](const std::vector<MClause> &side) {
    for (const MClause &m : side) {
      if (m.ref != INVALID_REF) {
        del_large_ref(m.ref);
      } else {
        // One connected occurrence removes one physical copy.  This preserves
        // Kissat's per-copy score-update sequence while the kernel helper
        // withholds an ambiguous LRAT deletion until the final copy.
        const bool ok = kernel_delete_binary_one_copy(k, m.ba, m.bb);
        assert(ok);
        (void)ok;
      }
    }
  };
  del_all(pos);
  del_all(neg);
  for (const Ref ref : del_large)
    del_large_ref(ref);
  for (const auto &pr : del_bin) {
    const bool ok =
        kernel_delete_binary_one_copy(k, pr.first, pr.second);
    assert(ok);
    (void)ok;
  }

  // cadical drains here: after the resolvents are connected and the pivot's
  // own clauses are garbage, so the antecedents are skipped (elim.cpp:731).
  if (k.bve_backward)
    bve_backward(k, fresh, fresh_bin, literal_marks);

  k.stats.bve_eliminated++;
  // The drain can refute (a strengthened victim's unit contradicting a root
  // value). An N1 API consumer trusts ApplyResult alone — returning Ok on an
  // inconsistent kernel invites a wrong SAT downstream (cadical's drain loop
  // guards on unsat for the same reason, backward.cpp:224).
  return k.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok;
}

// `Ok` means the application changed solver state.  That is read from the
// kernel's own record of clause transitions and root assignments rather than
// reported by each mutation in the attempt, so a kept pivot whose attempt
// retired an antecedent or learned a unit is `Ok` by construction.  Whether
// the pivot was eliminated is read from retirement and `bve_eliminated`.
ApplyResult bve_eliminate(Kernel &k, Var x, BveDefScratch *def) {
  const KernelChangeMark before = kernel_change_mark(k);
  const ApplyResult result = bve_attempt(k, x, def);
  return result == ApplyResult::NoOp && kernel_change_mark(k) != before
             ? ApplyResult::Ok
             : result;
}

} // namespace

// canonical instance = a round: eliminate the best schedulable candidate
// within the bound, draining the root closure at entry
ApplyResult bve_apply(Kernel &k) {
  assert(bve_guard(k));
  k.stats.bve_rounds++;
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  std::vector<Var> cand;
  bve_schedule(k, cand);
  BveDefScratch def; // one per ROUND: every candidate reuses its allocations
  for (const Var x : cand) {
    const ApplyResult r = bve_eliminate(k, x, &def);
    if (r == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    // The schedule and the root closure describe the state at round entry, so
    // the round ends at the first candidate which changes that state, whether
    // it eliminated its pivot or declined after installing a unit resolvent
    // or retiring a satisfied antecedent.  The next application drains the
    // new roots and schedules from current occurrences; no later candidate is
    // ever attempted against a stale schedule or an incomplete root closure.
    // Kissat keeps its round going instead and pays for that with a unit
    // flush and a `can_eliminate_variable` re-test per candidate
    // (eliminate.c); DefaultPolicy's dosing loop is the port of that shape.
    if (r == ApplyResult::Ok) {
      // Reclamation is the caller's cost choice, not part of the rule's
      // effect: deletes are lazy and every reader skips garbage. The canonical
      // round stays self-contained; the dosing path leaves it to the
      // scheduler, which reclaims at the end of its authored elimination
      // round.
      kernel_collect_garbage(k);
      return ApplyResult::Ok; // the round changed solver state
    }
    // unchanged state (bound / hazard): try the next candidate
  }
  return ApplyResult::NoOp;
}

// instance surface: payload = a candidate variable via its positive DIMACS
// literal (lit_instance); INSTANCE_CANONICAL = the full round.
static uint32_t bve_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Var> cand;
  bve_schedule(k, cand);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(cand.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = lit_instance(k, make_lit(cand[i], false));
  return n;
}

static ApplyResult bve_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return bve_apply(k);
  Lit l;
  if (!lit_of_instance(k, inst, &l))
    return ApplyResult::NoOp;
  if (var_of(l) >= k.nvars || k.values[l] != 0)
    return ApplyResult::NoOp;
  assert(bve_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (k.values[l] != 0) // the requested var got assigned by the drain
    return ApplyResult::NoOp;
  // The scheduler doses BVE through this one-instance entry, so stack-local
  // scratch here would still allocate and zero 2*nvars maps once per
  // candidate even though the canonical-round entry reuses them. Keep the
  // rule-scoped cache across admissions, matching the persistent sparse
  // scratch used by Instantiate, Congruence, and TransitiveReduce. BVE is not
  // re-entrant; thread-local storage keeps independent solver threads apart.
  static thread_local BveDefScratch def;
  return bve_eliminate(k, var_of(l), &def);
}

TACTIX_REGISTER_RULE(BVE, RuleTier::Inprocess, bve_guard, bve_apply,
                     bve_instances, bve_apply_instance);

} // namespace tactix

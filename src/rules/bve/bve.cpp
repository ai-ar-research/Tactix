// BVE, rule E1: occurrence index, candidate schedule, elimination round and
// gate reduction, from kissat eliminate.c + resolve.c + definition.c. See
// docs/design/e1-bve-design.md and bve.hpp.
#include "rules/bve/bve.hpp"

#include "diagnostics/event_trace.hpp"
#include "engine/subsolver.hpp"
#include "kernel/watch.hpp"
#include "proof/proof.hpp"
#include "rules/instance_helpers.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <utility>

namespace tactix {

RuleEligibility bve_guard(const Kernel &k) { return root_level_guard(k); }

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
    for (const auto [ref, c] : arena_records(k.arena)) {
      if (!c->garbage() && !c->redundant())
        for (const Lit l : *c)
          if (k.values[l] == 0) // active-variable literals only
            occ.counts[l]++;
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

// `kissat_eliminate_clause (c, satisfying)`: delete with the generic score
// notifications suppressed, then update every unassigned clause variable except
// the satisfying literal.
static void bve_eliminate_satisfied(Kernel &k, Ref ref, Lit satisfying) {
  const BveScoreUpdateHook hook = k.bve_score_update_hook;
  void *const state = k.bve_score_update_state;
  k.bve_score_update_hook = nullptr;
  kernel_delete_clause(k, clause_at(k.arena, ref));
  k.bve_score_update_hook = hook;
  k.bve_score_update_state = state;
  for (const Lit l : *clause_at(k.arena, ref))
    if (l != satisfying && k.value(l) == 0)
      kernel_notify_bve_score(k, var_of(l), true);
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
      if (clause_contains(c, unit))
        bve_eliminate_satisfied(k, entry.payload, unit);
    }

    // Kissat compacts only the satisfied unit's connected stack here.  Other
    // literal stacks retain their garbage references until inspected later.
    const uint32_t before = k.dense->large_size(unit);
    k.dense->compact_large(unit, [&](Ref ref) {
      const Clause *c = clause_at(k.arena, ref);
      return !c->garbage() && clause_contains(c, unit);
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
    for (const auto [ref, c] : arena_records(k.arena)) {
      if (!c->garbage() && !c->redundant())
        for (const Lit other : *c)
          count += other == lit;
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
    // Kernel::bve_retry is advice for the policy and is not read here.
    const Lit pl = make_lit(v, false);
    if (k.values[pl] != 0) // assigned variable: not an elimination candidate
      continue;
    const uint32_t p = occ.count(pl), n = occ.count(make_lit(v, true));
    // Kissat schedules every active variable with its `eliminate` flag set
    // (eliminate.c schedule_variables), occurrences or not. Structural score
    // p*n-p-n as in variable_score; DefaultPolicy adds the search term.
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

// Offset slice into one candidate's reusable flat storage.
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

// A materialized clause on the pivot: its active literals (root-false dropped),
// its proof id, and a delete handle. `active` includes the pivot.
struct MClause {
  PoolSlice<Lit> active;
  uint64_t id = 0;
  Ref ref = INVALID_REF; // INVALID_REF => binary
  Lit ba = INVALID_LIT, bb = INVALID_LIT;
};

struct BveResolvent {
  PoolSlice<Lit> lits;
  // Source identity: FastEliminate may replace either parent, discarding
  // earlier tentative images of that pair.
  Ref first_parent = INVALID_REF;
  Ref second_parent = INVALID_REF;
};

// Sparse literal-to-clause lookup; only the previous candidate's entries are
// reset.
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
      // Kissat's mark is the first physical binary watch, so AND/OR extraction
      // uses one copy of a duplicated partner.
      values[lit] = value;
    }
  }
  int32_t get(Lit lit) const { return values[lit]; }
};

// Reusable candidate state owned by `bve_eliminate`'s caller.
struct BveDefScratch {
  SubSolver solver;
  // Append-only candidate pools; MClause and BveResolvent hold slices.
  std::vector<MClause> pos, neg;
  std::vector<Lit> clause_lits;
  std::vector<BveResolvent> resolvents;
  std::vector<Lit> resolvent_lits;
  // Distinct non-pivot variables in first occurrence order: the proof map and
  // Kitten's import order.
  std::vector<Var> vars;
  std::vector<uint32_t> var_to_sub; // kernel var -> compact sub var + 1
  std::vector<Var> var_touched; // entries to restore in var_to_sub
  std::vector<SubLit> lits; // one reduced clause, being encoded
  SubDerivation deriv;
  SparseBinaryIndex pos_bin, neg_bin;
  // Per-candidate working sets, cleared or reassigned at use.
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
  std::vector<uint8_t> literal_marks;
  uint64_t ticks = 0;    // ticks the last attempt spent (0 if none was made)
  Lit unit = INVALID_LIT; // one-sided core's failed pivot literal, if any
  bool checked = false;  // an attempt was made at all
  bool exhausted = false; // ...and the tick ceiling cut it short
  uint64_t session = UINT64_MAX; // strategy-owned embedded-Kitten lifetime
};

// ---- gate detection (equivalence, AND/OR, ITE, XOR, definition) ----
// Mark a gate defining x in pos_gate/neg_gate (1 = gate, 0 = antecedent). Every
// detector must establish (★) of docs/dev/calculus.md §6b E1 exactly. Only the
// definition type and a shortened ITE set `resolve_gate`. ITE follows Kissat's
// `ifthenelse.c`.

// Sorted signature of a clause's non-pivot active literals for ITE/XOR lookups.
// Clauses with a repeated variable are not indexed.
constexpr uint32_t BVE_SIG_MAX = BVE_XOR_ARITY_CAPACITY;
struct GateSig {
  uint32_t n = 0;
  Lit l[BVE_SIG_MAX] = {0};
  bool operator<(const GateSig &o) const {
    if (n != o.n)
      return n < o.n;
    // Fixed trip count keeps std::sort's pivot copy initialized for the
    // optimizer.
    for (uint32_t i = 0; i < BVE_SIG_MAX; i++)
      if (l[i] != o.l[i])
        return l[i] < o.l[i];
    return false;
  }
};
using SigIndex = std::vector<std::pair<GateSig, uint32_t>>; // sorted by sig

static void bve_sort_signature(Lit *lits, uint32_t size) {
  assert(size <= BVE_SIG_MAX);
  // Bounded insertion sort (std::sort trips -Warray-bounds on five slots).
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

// Index of a clause whose non-pivot literals are exactly `s`, or -1.
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

// General definition x ↔ φ (kissat `definition.c`): refuting the pivot's
// occurrence lists minus the pivot is (★) for G⁺ = P, G⁻ = N, and the core is
// the gate. Kitten order and tick currency. A one-sided core proves a failed
// pivot literal; its lemmas are lifted by appending that literal
// (`traverse_one_sided_core_lemma`). The second core shrinks the first after
// Kissat's deterministic shuffle, with a tenfold allowance.
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
  // definition.c exports physical clauses minus the pivot, so root-false
  // literals stay Kitten variables.
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
          // UINT32_MAX marks seen until compact ids are assigned; restored next
          // attempt.
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

  // Reset Kitten's initialized counter at the first definition attempt of each
  // fresh Kernel, so clients do not inherit another solver's shuffle seed.
  if (!k.stats.bve_definitions_checked ||
      s.session != k.bve_definition_session) {
    s.solver.start_kitten_session(/*clear_before_first_environment=*/true);
    s.session = k.bve_definition_session;
  }
  s.checked = true;
  // Kissat imports variables on first occurrence and decides from the newest
  // end of Kitten's VMTF queue; the compact first-occurrence ids are used
  // directly.
  s.solver.reset_compact_kitten(static_cast<uint32_t>(s.vars.size()));
  // Ids from 1 in add order: [1, |pos|] names pos[id-1], the rest
  // neg[id-|pos|-1] (`add_clause` consumes an id for a dropped tautology too).
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
  // Account one definition solve under `budget` and trace its outcome; true
  // iff it refuted the sides with a non-empty core.
  const auto settle = [&](SubResult res, uint32_t core, uint64_t budget) {
    s.ticks = s.solver.kitten_ticks();
    s.exhausted = res == SubResult::Budget;
    if (res != SubResult::Unsat || !s.solver.derivation(s.deriv) ||
        s.deriv.core.empty()) {
      event_trace_bve_definition(
          k, pv, attempt, core,
          res == SubResult::Budget ? "budget" :
          res == SubResult::Sat ? "sat" : "no-derivation",
          s.ticks, budget, s.solver.kitten_generator(),
          static_cast<uint32_t>(s.vars.size()),
          static_cast<uint32_t>(pos.size()), static_cast<uint32_t>(neg.size()),
          nullptr, 0);
      return false;
    }
    event_trace_bve_definition(
        k, pv, attempt, core, "unsat", s.ticks, budget,
        s.solver.kitten_generator(), static_cast<uint32_t>(s.vars.size()),
        static_cast<uint32_t>(pos.size()), static_cast<uint32_t>(neg.size()),
        s.deriv.core.data(), static_cast<uint32_t>(s.deriv.core.size()));
    return true;
  };
  if (!settle(s.solver.solve_interruptible({}, k.bve_definition_budget,
                                           SubTickMode::Kitten, k.stop),
              1, k.bve_definition_budget))
    return 0;
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
    const SubResult res = s.solver.solve_interruptible(
        {}, ceiling, SubTickMode::Kitten, k.stop);
    assert(res != SubResult::Sat && "an UNSAT core subset became SAT");
    if (!settle(res, core, refinement_budget()))
      return 0;
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

// Lift the embedded refutation into the main proof: each sub-lemma L becomes
// (unit v L), the empty one the failed-literal unit. The lemmas are retired
// after the unit (DSR deletes one copy per intermediate).
static void bve_emit_definition_unit(Kernel &k, Lit unit,
                                     BveDefScratch &s) {
  assert(unit != INVALID_LIT && k.value(unit) == 0);
  assert(!s.deriv.steps.empty() && s.deriv.steps.back().lits.empty());
  const auto kernel_lit = [&](SubLit sl) {
    assert(sub_var_of(sl) < s.vars.size());
    return make_lit(s.vars[sub_var_of(sl)], (sl & 1u) != 0);
  };

  std::vector<Lit> lifted;
  std::vector<TransientLemma> transients;
  // The writer tracks each transient under its id (emit_rup_lemma), so it
  // deletes them by that id.
  uint64_t last_id = 0;
  for (size_t si = 0; si < s.deriv.steps.size(); si++) {
    const SubStep &step = s.deriv.steps[si];
    lifted.clear();
    lifted.push_back(unit);
    for (const SubLit sl : step.lits)
      lifted.push_back(kernel_lit(sl));
    assert(!Proof::tautological(lifted.data(),
                               static_cast<uint32_t>(lifted.size())));
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, lifted);
    last_id = id;
    if (k.proof && si + 1 != s.deriv.steps.size())
      transients.push_back({id, lifted});
  }
  assert(last_id);
  k.unit_ids[var_of(unit)] = last_id;
  kernel_assign_unit(k, unit);
  k.stats.bve_definition_units++;
  delete_transients(k, transients);
}

// Returns the BVE_GATE_* bit found, or 0. Read-only; `def` reports cost.
uint32_t bve_detect_gate(const Kernel &k, Lit px, Lit nx,
                         const std::vector<MClause> &pos,
                         const std::vector<MClause> &neg,
                         std::vector<uint8_t> &pos_gate,
                         std::vector<uint8_t> &neg_gate, bool &resolve_gate,
                         BveDefScratch &def) {
  resolve_gate = false;
  // binary partner lookup: side_bin[lit] = index of a binary {pivot, lit}, or
  // -1.
  SparseBinaryIndex &pos_bin = def.pos_bin;
  SparseBinaryIndex &neg_bin = def.neg_bin;
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

  // (1) equivalence x↔¬w: (x∨w) + (¬x∨¬w). kissat equivalences.c marks the
  // positive physical binaries and takes the first negative binary whose
  // partner's complement is marked; shortened large clauses never qualify.
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
      // A root-shortened physical large base still serves as in Kissat.
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
      // Kissat marks every distinct partner, clears the inputs, then walks the
      // binary-watch stream: required copies are gates and a duplicated
      // partner's first watch is the antecedent.
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
    if (!(k.bve_gate_mask & BVE_GATE_DEFINITION))
      return 0;
    const uint32_t kind =
        bve_find_definition(k, px, pos, neg, pos_gate, neg_gate, def);
    // the ONLY place `resolve_gate` is set; see the argument above
    resolve_gate = kind != 0;
    return kind;
  };

  if (!(k.bve_gate_mask & (BVE_GATE_ITE | BVE_GATE_XOR)))
    return definition();
  // Exact-match index over both sides, shared by ITE and XOR; O(P+N).
  SigIndex psig, nsig;
  const uint32_t signature_arity =
      std::min<uint32_t>(BVE_SIG_MAX,
                         std::max<uint32_t>(BVE_XOR_ARITY_MIN,
                                            k.bve_xor_arity_limit));
  bve_build_sig_index(pos, px, signature_arity, psig);
  bve_build_sig_index(neg, nx, signature_arity, nsig);

  // (3) ITE from Kissat `ifthenelse.c`: two ternaries on one side name the
  // condition and branches; opposite clauses may be shortened. Both pivot
  // orientations are tried in order.
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

  // (4) XOR: all 2^m parity clauses over the pivot and m variables (cadical
  // gates.cpp find_xor_gate), a fixed enumeration from one base clause.
  auto try_xor = [&]() -> bool {
    uint32_t gate_pos[1u << BVE_XOR_ARITY_CAPACITY];
    uint32_t gate_neg[1u << BVE_XOR_ARITY_CAPACITY];
    for (const std::pair<GateSig, uint32_t> &base : psig) {
      const uint32_t m = base.first.n; // arity
      if (m < 2)                       // m == 1 is an equivalence (case 1)
        continue;
      if (m > k.bve_xor_arity_limit)
        continue;
      // Relation x ⊕ a₁ ⊕ … ⊕ aₘ = R with R = 1⊕p₀, p₀ the parity of the base's
      // falsifying assignment.
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
  // ifthenelse.c resets `resolve_gate` per pair only; XOR gate×gate resolvents
  // are tautologies, so clear it.
  resolve_gate = false;
  if ((k.bve_gate_mask & BVE_GATE_XOR) && try_xor())
    return BVE_GATE_XOR;
  return definition();
}

// --- eager backward subsumption from fresh resolvents ---
// CaDiCaL `elimbackward` (backward.cpp, elim.cpp:731): each fresh resolvent
// subsumes over its least-occurring literal's list; shortened victims re-enter
// the queue. See the BVE section of `docs/dev/implementation-notes.md`.

// Direction matters: C1/C2's `check_marked` tests the reverse. Deleting the
// stronger clause is unchecked by the proof and gives a wrong SAT.
static void bve_backward(Kernel &k, std::vector<Ref> &queue,
                         std::vector<std::pair<Lit, Lit>> &fresh_bin,
                         std::vector<uint8_t> &marks) {
  if (!k.dense || k.inconsistent)
    return; // N1: the index is an acceleration, and the rule runs without it
  // Reuse the restored literal image; clear only touched literals
  // (implementation-notes §18).
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
  // Least-occurring-literal count (cadical occs: binaries from the watch list).
  // Counting stops one past the limit, which changes neither argmin nor guard.
  auto occ_len = [&](Lit l) {
    size_t n = k.dense->large_size(l);
    if (n > k.bve_backward_occurrence_limit)
      return n;
    for (const WatchEntry w : k.watches.entries(l))
      if (w.binary() && ++n > k.bve_backward_occurrence_limit)
        break;
    return n;
  };
  // One subsumer pass over `clits`; `cref` is INVALID_REF for a binary
  // subsumer.
  auto backward_one = [&](Ref cref) {
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
      // A root-satisfied subsumer is garbage (cadical backward.cpp:66-69); a
      // duplicated binary refuses deletion by name and stays.
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
      // c ⊆ d (subsume), or c ⊆ d with exactly one literal flipped
      // (strengthen).
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
        // `d` contains the unassigned `best`, so it is never a trail reason and
        // can be deleted without a reason check.
        kernel_delete_clause(k, d);
        if (!dsat)
          k.stats.bve_bwd_subsumed++;
        continue;
      }
      // Self-subsuming resolution: d \ {negated} strictly subsumes d; `negated
      // != best`, so the result keeps `best` unassigned (cadical
      // backward.cpp:201).
      dlits.clear();
      for (const Lit l : *d) {
        if (l == negated)
          continue;
        if (k.value(l) < 0)
          continue;
        dlits.push_back(l);
      }
      const uint64_t id = ++k.next_clause_id;
      emit_rup_lemma(k, id, dlits);
      k.stats.bve_bwd_strengthened++;
      Ref nref = INVALID_REF;
      switch (kernel_replace_clause(k, d, id, dlits.data(),
                                    static_cast<uint32_t>(dlits.size()),
                                    /*redundant=*/false, 0, &nref)) {
      case ReplaceOutcome::Refuted:
        k.inconsistent = true;
        break;
      case ReplaceOutcome::Unit:
        // kernel_replace_clause owns only the deletion; the caller must assert
        // the unit (un-propagated, M1 owns the cascade) or the formula weakens.
        k.stats.bve_bwd_units++;
        if (k.value(dlits[0]) == 0) {
          k.unit_ids[var_of(dlits[0])] = id;
          kernel_assign_unit(k, dlits[0]);
        }
        // A unit invalidates the snapshot: abandon the walk (cadical
        // backward.cpp:195).
        unit_derived = true;
        break;
      case ReplaceOutcome::Binary:
        // Re-enters as a binary subsumer under its new id (backward.cpp:202).
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
    // --- binary victims (indexed only by watch lists) ---
    // Only for effective subsumer size <= 2; walk a snapshot because deletes
    // compact the list.
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
        // A binary subsumer skips its own pair, including a root-false partner.
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
        // A duplicated pair cannot be deleted or strengthened by name.
        if (kernel_binary_copies(k, best, o, 2) != 1)
          continue;
        if (neg_lit == INVALID_LIT) {
          // Every marked literal occurs in {best, o}; a large subsumer with
          // absorbed root-false extras genuinely subsumes the pair.
          const bool ok = kernel_delete_binary(k, best, o);
          assert(ok);
          (void)ok;
          k.stats.bve_bwd_subsumed++;
          continue;
        }
        // Self-subsuming resolution on `o`: {best, o} becomes the unit {best}.
        const uint64_t id = ++k.next_clause_id;
        Lit unit = best;
        emit_rup_lemma(k, id, &unit, 1);
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
  // Drain the large queue, then size-2 resolvents as binary subsumers, to an
  // outer fixpoint (cadical uses one FIFO, elim.cpp:592-594).
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
      backward_one(cref);
    }
    for (; bi < fresh_bin.size() && !k.inconsistent; ++bi) {
      const auto [a, b] = fresh_bin[bi]; // copy: the pass may grow the vector
      if (!kernel_binary_live(k, a, b))
        continue; // deleted since it was queued (subsumed or strengthened)
      clits.assign({a, b});
      backward_one(INVALID_REF);
    }
  }
}

// Eliminate x by resolution at a drained root. Unsat on an empty resolvent or
// contradictory units. `NoOp` means the pivot was kept; like resolve.c,
// retired antecedents and unit resolvents met earlier survive a decline.
ApplyResult bve_attempt(Kernel &k, Var x, BveDefScratch &def) {
  Lit px = make_lit(x, false), nx = make_lit(x, true);
  // Candidate contract: pivot unassigned and root closure complete.
  assert(kernel_propagation_fixpoint(k));
  assert(k.value(px) == 0);
  // Clear the retry bit first (kissat eliminate.c eliminate_variable).
  k.bve_retry[x] = 0;

  std::vector<MClause> &pos = def.pos; // live irredundant: resolved + witnessed
  std::vector<MClause> &neg = def.neg;
  pos.clear();
  neg.clear();
  def.clause_lits.clear();
  def.resolvents.clear();
  def.resolvent_lits.clear();
  std::vector<Ref> &del_large = def.del_large; // redundant/satisfied/taut → delete-only
  std::vector<std::pair<Lit, Lit>> &del_bin = def.del_bin; // satisfied binaries → delete-only
  del_large.clear();
  del_bin.clear();
  bool antecedent_oversized = false;
  bool rescore_popped_candidate = false;

  // `resolve.c::occurrences_literal`: scan each polarity in connected order,
  // deleting satisfied binaries and compacting garbage until the first clause
  // above `eliminateclslim`; a failed scan does not set the update flag. Test
  // only garbage and physical size.
  if (k.dense && k.dense->track_order) {
    for (const Lit pivot : {px, nx}) {
      // Apply the prefix's deletions before the opposite polarity (two
      // sequential `occurrences_literal` calls); garbage is compacted in one
      // stable pass.
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

  // `kissat_generate_resolvents` re-scores a popped candidate whose occurrences
  // were compacted when elimination fails; update-only (`kissat_update_heap`
  // leaves absent items absent).
  const auto decline_after_compaction = [&]() {
    if (rescore_popped_candidate)
      kernel_notify_bve_score(k, x, false);
    return ApplyResult::NoOp;
  };

  // Kissat declines after both scans if either hit an oversized clause, before
  // gate detection and without rescoring.
  if (antecedent_oversized)
    return ApplyResult::NoOp;

  // ---- gather x's connected antecedents (duplicates retained) ----
  // `definition.c` and `resolve.c` import one mixed dense-watch stream; binaries
  // created in the phase stay after earlier occurrences.
  std::vector<uint8_t> &literal_marks = def.literal_marks;
  if (literal_marks.size() < 2 * static_cast<size_t>(k.nvars))
    literal_marks.resize(2 * static_cast<size_t>(k.nvars), 0);
  auto gather_bin = [&](Lit lit, Lit o, std::vector<MClause> &live) {
      if (k.value(o) > 0) { // satisfied
        del_bin.emplace_back(lit, o);
        return;
      }
      // o root-false is impossible at a drained fixpoint (it would force
      // `lit`).
      MClause m;
      m.ref = INVALID_REF;
      m.ba = lit;
      m.bb = o;
      m.id = kernel_binary_id(k, lit, o);
      const size_t begin = def.clause_lits.size();
      def.clause_lits.push_back(lit);
      def.clause_lits.push_back(o);
      m.active = pool_slice(def.clause_lits, begin);
      live.push_back(std::move(m));
  };

  // ---- gather x's large clauses ----
  // Tautology / redundant / root-satisfied → delete-only; else materialize into
  // pos or neg.
  auto classify = [&](Ref ref, const Clause *c, bool has_px, bool has_nx) {
    if (c->redundant()) {
      del_large.push_back(ref);
      return;
    }
    // resolve.c::occurrences_literal applies `eliminateclslim` before
    // assignments or gates; an unrestricted rule call keeps UINT_MAX.
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
    const size_t active_begin = def.clause_lits.size();
    bool satisfied = false;
    for (const Lit l : *c) {
      const int8_t v = k.value(l);
      if (v > 0) { // root-true → clause satisfied → delete-only, no witness
        satisfied = true;
        break;
      }
      if (v < 0) // root-false → drop
        continue;
      def.clause_lits.push_back(l);
    }
    if (satisfied) {
      def.clause_lits.resize(active_begin);
      del_large.push_back(ref);
    } else {
      m.active = pool_slice(def.clause_lits, active_begin);
      (has_px ? pos : neg).push_back(std::move(m));
    }
  };
  // Classify one indexed large occurrence of `pivot`, skipping a stale entry;
  // a tautology belongs to the positive side.
  const auto gather_large_occurrence = [&](Ref ref, Lit pivot, Lit opposite,
                                           bool positive) {
    const Clause *c = clause_at(k.arena, ref);
    if (c->garbage() || !clause_contains(c, pivot))
      return;
    const bool has_opposite = clause_contains(c, opposite);
    if (!positive && has_opposite)
      return; // positive side already handled the tautology
    classify(ref, c, positive, !positive || has_opposite);
  };
  const auto gather_watched_binaries = [&] {
    for (const WatchEntry w : k.watches.entries(px))
      if (w.binary())
        gather_bin(px, w.other(), pos);
    for (const WatchEntry w : k.watches.entries(nx))
      if (w.binary())
        gather_bin(nx, w.other(), neg);
  };
  if (k.dense && k.dense->track_order) {
    // Filter stale entries against kernel state, keeping survivor order.
    const auto gather = [&](Lit pivot, Lit opposite,
                            std::vector<MClause> &live, bool positive) {
      for (const OccIndex::Entry &entry : k.dense->ordered[pivot]) {
        if (entry.binary) {
          // Dense binary entries are exact (maintained eagerly); large entries
          // are lazy and validated below.
#ifndef NDEBUG
          assert(kernel_binary_live(k, pivot, entry.payload));
#endif
          gather_bin(pivot, entry.payload, live);
          continue;
        }
        gather_large_occurrence(entry.payload, pivot, opposite, positive);
      }
    };
    gather(px, nx, pos, true);
    gather(nx, px, neg, false);
  } else if (k.dense) {
    // Without Eliminate's mixed order, consume the large-only dense slices,
    // then the binary watches (large-then-binary).
    const auto gather_large = [&](Lit pivot, Lit opposite,
                                  bool positive) {
      k.dense->with_large(pivot, [&](auto refs) {
        for (const Ref ref : refs)
          gather_large_occurrence(ref, pivot, opposite, positive);
      });
    };
    gather_large(px, nx, true);
    gather_large(nx, px, false);
    gather_watched_binaries();
  } else {
    // N1 fallback: one arena scan in the public no-index state.
    for (const auto [ref, c] : arena_records(k.arena)) {
      if (!c->garbage()) {
        bool has_px = false, has_nx = false;
        for (const Lit l : *c) {
          has_px |= (l == px);
          has_nx |= (l == nx);
        }
        if (has_px || has_nx)
          classify(ref, c, has_px, has_nx);
      }
    }
    // The non-dense path has no connected-list pose: arena order, then
    // binaries.
    gather_watched_binaries();
  }

  if (antecedent_oversized)
    return decline_after_compaction();

  // An already retired, occurrence-free pivot is a no-op and must not count as
  // another elimination.
  if (kernel_var_retired(k, x) && pos.empty() && neg.empty() &&
      del_large.empty() && del_bin.empty())
    return decline_after_compaction();

  // Occurrence blow-up (kissat `eliminateocclim` = 2000, resolve.c:282-290).
  // The cap is applied here, not in `bve_schedule`, so `bve_instances` still
  // offers every candidate (N1, `RV-S17`).
  if (!pos.empty() && !neg.empty() &&
      pos.size() + neg.size() > k.bve_occurrence_limit)
    return decline_after_compaction();

  // `kissat_generate_resolvents` makes the smaller polarity `lit` (ties keep
  // the positive side); keep the sides and pivots aligned with that
  // orientation.
  if (!k.bve_fastel_traversal && pos.size() > neg.size()) {
    std::swap(pos, neg);
    std::swap(px, nx);
  }

  // CaDiCaL sorts both lists by size before gates and the bound scan
  // (elim.cpp:714-718); coupled to the OTF pose, the off arm keeps Kissat
  // order.
  if (k.bve_on_the_fly_strengthening && !k.bve_fastel_traversal) {
    const auto physical_size = [&](const MClause &m) {
      return m.ref == INVALID_REF ? 2u : clause_at(k.arena, m.ref)->size;
    };
    const auto smaller = [&](const MClause &a, const MClause &b) {
      return physical_size(a) < physical_size(b);
    };
    std::stable_sort(pos.begin(), pos.end(), smaller);
    std::stable_sort(neg.begin(), neg.end(), smaller);
  }

  // Kissat commits a candidate with an empty polarity as pure before gate
  // detection, including zero/zero.
  const bool pure = pos.empty() || neg.empty();

  // ---- syntactic gate detection: restrict resolvents to gate×antecedent ----
  std::vector<uint8_t> &pos_gate = def.pos_gate;
  std::vector<uint8_t> &neg_gate = def.neg_gate;
  pos_gate.assign(pos.size(), 0);
  neg_gate.assign(neg.size(), 0);
  bool resolve_gate = false;
  def.checked = false;
  def.exhausted = false;
  def.ticks = 0;
  def.unit = INVALID_LIT;
  const uint32_t gate_kind =
      pure ? 0 : bve_detect_gate(k, px, nx, pos, neg, pos_gate, neg_gate,
                                 resolve_gate, def);
  const bool gate = gate_kind != 0;
  if (def.checked) {
    k.stats.bve_definitions_checked++;
    k.stats.bve_definition_ticks += def.ticks;
    k.stats.bve_definition_budget += def.exhausted ? 1 : 0;
  }
  if (def.unit != INVALID_LIT) {
    assert(gate_kind == BVE_GATE_DEFINITION);
    k.stats.bve_gates_definition++;
    bve_emit_definition_unit(k, def.unit, def);
    return ApplyResult::Ok;
  }
  // (★) needs every gate pair; only tautological shapes may drop gate×gate
  // pairs.
  assert((!resolve_gate || gate_kind == BVE_GATE_DEFINITION ||
          gate_kind == BVE_GATE_ITE) &&
         "a syntactic gate asked for its gate x gate resolvents back: its "
         "detection is no longer exact, so (star) must be re-argued");
  uint32_t gp = 0, gn = 0;
  uint64_t charged = static_cast<uint64_t>(pos.size()) * neg.size();
  if (gate) {
    // Pairs removed by the gate, counted per attempt.
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

  // Keep the extracted lists ordered: gates.c/get_antecedents keeps
  // connected-watch order and resolve.c consumes them in three stages.
  std::vector<uint32_t> &pos_gate_order = def.pos_gate_order;
  std::vector<uint32_t> &neg_gate_order = def.neg_gate_order;
  std::vector<uint32_t> &pos_antecedent_order = def.pos_antecedent_order;
  std::vector<uint32_t> &neg_antecedent_order = def.neg_antecedent_order;
  pos_gate_order.clear();
  neg_gate_order.clear();
  pos_antecedent_order.clear();
  neg_antecedent_order.clear();
  // gates.c get_antecedents takes an entry as a gate when its raw watch equals
  // the gate cursor's, so an earlier duplicate of a gate copy is consumed in
  // its place and the flagged copy becomes an antecedent.
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
  std::vector<BveResolvent> &resolvents = def.resolvents;
  const size_t base = pos.size() + neg.size();
  // Eliminate iff #non-tautological resolvents ≤ occ⁺ + occ⁻ +
  // `Kernel::bve_bound` (set only by the policy; kissat eliminate.c
  // set_next_elimination_bound).
  const size_t commit_limit = base + k.bve_bound; // eliminate iff ≤ this
  // Kissat aborts at the first resolvent beyond the current bound; the retry
  // hint is `current bound + 1`.
  const size_t gen_limit =
      std::min(commit_limit,
               static_cast<size_t>(k.bve_resolvent_count_limit));
  // SC2024 runs its mutating feasibility scan only when the raw product exceeds
  // fastelim.
  const bool fastel_feasibility =
      k.bve_fastel_traversal && !neg.empty() &&
      pos.size() > static_cast<size_t>(k.bve_resolvent_count_limit) /
                       neg.size();
  // CaDiCaL OTF strengthening is not layered on FastEliminate's feasibility
  // scan.
  const bool otf_feasibility =
      k.bve_on_the_fly_strengthening && !fastel_feasibility;
  const bool deferred_resolvent_count =
      fastel_feasibility || otf_feasibility;
  bool oversized = false; // configured resolvent-size limit was exceeded
  bool over_gen = false;  // resolvent count passed gen_limit → needs > ceiling
  bool immediate_unsat = false;
  uint64_t resolved = 0;
  // The binary gather above restored every touched mark. Reuse the same
  // round-owned literal image for resolvent deduplication and tautology tests.
  std::vector<uint8_t> &seen = literal_marks;

  // Tentative resolvents of a replaced or deleted parent are dropped.
  const auto prune_parent_resolvents = [&](Ref first, Ref second) {
    resolvents.erase(
        std::remove_if(
            resolvents.begin(), resolvents.end(),
            [&](const BveResolvent &r) {
              return (first != INVALID_REF &&
                      (r.first_parent == first || r.second_parent == first)) ||
                     (second != INVALID_REF &&
                      (r.first_parent == second || r.second_parent == second));
            }),
        resolvents.end());
  };

  // Units learned during resolution are installed immediately as in Kissat and
  // survive a decline.
  const auto learn_unit = [&](Lit unit) {
    assert(k.value(unit) == 0);
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, &unit, 1);
    kernel_assign_unit(k, unit);
  };
  const auto learn_empty = [&]() {
    assert(!k.inconsistent);
    ++k.next_clause_id;
    if (k.proof)
      k.proof->add_empty(0);
    k.inconsistent = true;
  };

  // resolve.c removes a root-satisfied large antecedent at once; binaries wait
  // for the unit flush. Notifications skip the satisfying literal
  // (eliminate.c).
  const auto delete_satisfied_large = [&](const MClause &m, Lit satisfying) {
    if (m.ref == INVALID_REF)
      return;
    if (!clause_at(k.arena, m.ref)->garbage())
      bve_eliminate_satisfied(k, m.ref, satisfying);
  };

  // CaDiCaL elim_update_removed_clause(c, pivot): retire the strengthened
  // antecedent's memberships now and update every other active variable without
  // reinserting the pivot.
  const auto delete_otf_parent = [&](const MClause &m, Lit pivot) {
    assert(m.ref != INVALID_REF);
    Clause *clause = clause_at(k.arena, m.ref);
    if (clause->garbage())
      return;
    if (k.dense) {
      for (const Lit l : *clause) {
        kernel_dense_remove_large_occurrence(k, l, m.ref);
        kernel_bve_occurrence_remove(k, l);
      }
    }
    kernel_delete_clause(k, clause, false, "bve-otf-strengthen");
    kernel_removed_var(k, var_of(pivot));
    for (const Lit l : *clause) {
      if (l == pivot || k.value(l) != 0)
        continue;
      kernel_mark_bve_retry(k, var_of(l), "bve-otf-strengthen", clause);
      kernel_removed_var(k, var_of(l));
      kernel_notify_bve_score(k, var_of(l), true);
    }
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
        const size_t lits_begin = def.resolvent_lits.size();
        bool satisfied_or_tautological = false;
        bool mixed_shared_literal = false;
        // Kissat resolve.c copies the second antecedent then the first; SC2024
        // fastel keeps positive first for large/large and the binary literal
        // first for mixed. Rotate only when the posed traversal owns
        // first+second order.
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
          if (seen[l]) { // duplicate with c
            mixed_shared_literal |=
                (c.ref == INVALID_REF) != (d.ref == INVALID_REF);
            continue;
          }
          if (seen[negate(l)]) { // l and ¬l both present → tautology
            satisfied_or_tautological = true;
            break;
          }
          def.resolvent_lits.push_back(l);
        }
        const size_t second_size = def.resolvent_lits.size() - lits_begin;
        if (satisfied_or_tautological) {
          def.resolvent_lits.resize(lits_begin);
          continue;
        }
        // fastel strengthening pairs are not counted; defer the count until
        // after the check (fastel.c::can_fast_resolve_large_large).
        if (!deferred_resolvent_count && ++resolved > gen_limit) {
          def.resolvent_lits.resize(lits_begin);
          over_gen = true;
          break;
        }
        for (const Lit l : c.active) {
          if (l == first_pivot)
            continue;
          const int8_t value = k.value(l);
          assert(value <= 0);
          if (!value)
            def.resolvent_lits.push_back(l);
        }
        if (k.bve_fastel_traversal) {
          const bool first_binary = c.ref == INVALID_REF;
          const bool second_binary = d.ref == INVALID_REF;
          const bool first_then_second = first_binary || !second_binary;
          if (first_then_second && second_size) {
            auto begin = def.resolvent_lits.begin() + lits_begin;
            std::rotate(begin, begin + second_size,
                        def.resolvent_lits.end());
          }
        }
        const size_t lits_size = def.resolvent_lits.size() - lits_begin;
        // Emit the self-subsuming proof step, prune tentative resolvents of
        // replaced parents, and add the resolvent as a new irredundant clause.
        const auto publish_strengthened = [&](bool c_subsumed,
                                              bool d_subsumed) {
          const uint64_t id = ++k.next_clause_id;
          const Lit *const lits = def.resolvent_lits.data() + lits_begin;
          emit_rup_lemma(k, id, lits, static_cast<uint32_t>(lits_size));
          prune_parent_resolvents(c_subsumed ? c.ref : INVALID_REF,
                                   d_subsumed ? d.ref : INVALID_REF);
          assert(lits_size >= 2);
          if (lits_size == 2)
            kernel_add_binary_clause(k, lits[0], lits[1], id);
          else
            kernel_add_large_clause(k, lits, static_cast<uint32_t>(lits_size),
                                    /*redundant=*/false, 0, id);
        };

        // SC2024 binary×large: a shared partner is self-subsumption even when
        // the result collapses to a unit; the large×large collapsed path keeps
        // both parents.
        const auto collapsed_fastel_victim = [&]() -> Ref {
          if (!fastel_feasibility || lits_size > 1)
            return INVALID_REF;
          if (!mixed_shared_literal)
            return INVALID_REF;
          return c.ref == INVALID_REF ? d.ref : c.ref;
        };
        const Ref collapsed_victim = collapsed_fastel_victim();
        const auto finish_collapsed_strengthening = [&]() {
          if (collapsed_victim == INVALID_REF)
            return;
          prune_parent_resolvents(collapsed_victim, INVALID_REF);
          Clause *const victim = clause_at(k.arena, collapsed_victim);
          if (!victim->garbage())
            kernel_delete_clause(k, victim);
          k.stats.strengthened++;
        };
        if (!lits_size) {
          learn_empty();
          finish_collapsed_strengthening();
          immediate_unsat = true;
          break;
        }
        if (lits_size == 1) {
          const Lit unit = def.resolvent_lits[lits_begin];
          learn_unit(unit);
          finish_collapsed_strengthening();
          def.resolvent_lits.resize(lits_begin);
          if (seen[unit]) {
            first_satisfied = true;
            break;
          }
          continue;
        }
        if (otf_feasibility &&
            (c.ref != INVALID_REF || d.ref != INVALID_REF)) {
          // Generation-time self-subsuming resolution (Han--Somenzi/CaDiCaL): a
          // strict subclause of an antecedent replaces it and is not counted.
          // Root-false literals are absent from both.
          for (size_t i = lits_begin; i < def.resolvent_lits.size(); ++i)
            seen[def.resolvent_lits[i]] |= 2;
          const auto subsumes_parent = [&](const MClause &parent) {
            if (parent.ref == INVALID_REF)
              return false;
            const Clause *live = clause_at(k.arena, parent.ref);
            if (live->garbage())
              return false;
            size_t active_size = 0;
            size_t contained = 0;
            for (const Lit l : parent.active) {
              if (k.value(l) < 0)
                continue;
              active_size++;
              contained += !!(seen[l] & 2);
            }
            return lits_size < active_size && contained == lits_size;
          };
          const bool c_subsumed = subsumes_parent(c);
          const bool d_subsumed = subsumes_parent(d);
          for (size_t i = lits_begin; i < def.resolvent_lits.size(); ++i)
            seen[def.resolvent_lits[i]] &= 1;
          if (c_subsumed || d_subsumed) {

            // CaDiCaL uses the first strengthened antecedent's literal order.
            const MClause &victim = c_subsumed ? c : d;
            const Lit victim_pivot = c_subsumed ? first_pivot : second_pivot;
            def.resolvent_lits.resize(lits_begin);
            for (const Lit l : victim.active)
              if (l != victim_pivot && k.value(l) == 0)
                def.resolvent_lits.push_back(l);
            assert(def.resolvent_lits.size() - lits_begin == lits_size);

            publish_strengthened(c_subsumed, d_subsumed);
            if (c_subsumed)
              delete_otf_parent(c, first_pivot);
            if (d_subsumed)
              delete_otf_parent(d, second_pivot);
            k.stats.strengthened++;
            k.stats.bve_otf_strengthened++;
            if (c_subsumed && d_subsumed)
              k.stats.bve_otf_subsumed++;
            def.resolvent_lits.resize(lits_begin);
            if (c_subsumed) {
              first_satisfied = true;
              break;
            }
            continue;
          }
        }
        if (fastel_feasibility &&
            (c.ref != INVALID_REF || d.ref != INVALID_REF)) {
          // A second mark bit keeps containment linear.
          for (size_t i = lits_begin; i < def.resolvent_lits.size(); ++i)
            seen[def.resolvent_lits[i]] |= 2;
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
          for (size_t i = lits_begin; i < def.resolvent_lits.size(); ++i)
            seen[def.resolvent_lits[i]] &= 1;
          if (c_subsumed || d_subsumed) {
            // Mixed pair: the large antecedent without its pivot, in its own
            // order (fastel.c::can_fast_resolve_binary_large).
            if ((c.ref == INVALID_REF) != (d.ref == INVALID_REF)) {
              const bool large_first = c.ref != INVALID_REF;
              const MClause &large = large_first ? c : d;
              const Lit large_pivot = large_first ? first_pivot : second_pivot;
              def.resolvent_lits.resize(lits_begin);
              for (const Lit l : large.active)
                if (l != large_pivot && k.value(l) == 0)
                  def.resolvent_lits.push_back(l);
              assert(def.resolvent_lits.size() - lits_begin == lits_size);
            }
            // fastel.c publishes the resolvent as a new irredundant clause,
            // then garbage-marks each subsumed antecedent; not an in-place
            // shrink.
            publish_strengthened(c_subsumed, d_subsumed);
            for (const Ref antecedent :
                 {c_subsumed ? c.ref : INVALID_REF,
                  d_subsumed ? d.ref : INVALID_REF}) {
              if (antecedent == INVALID_REF)
                continue;
              Clause *const clause = clause_at(k.arena, antecedent);
              if (!clause->garbage())
                kernel_delete_clause(k, clause, true, "fastel-strengthen");
            }
            k.stats.strengthened++;
            def.resolvent_lits.resize(lits_begin);
            // Replacing the outer antecedent ends its row.
            if (c_subsumed) {
              first_satisfied = true;
              break;
            }
            continue;
          }
        }
        if (deferred_resolvent_count && ++resolved > gen_limit) {
          def.resolvent_lits.resize(lits_begin);
          over_gen = true;
          break;
        }
        if (lits_size > k.bve_resolvent_size_limit) {
          def.resolvent_lits.resize(lits_begin);
          oversized = true;
          break;
        }
        BveResolvent r;
        r.lits = pool_slice(def.resolvent_lits, lits_begin);
        r.first_parent = c.ref;
        r.second_parent = d.ref;
        resolvents.push_back(r);
      }
      for (const Lit l : c.active)
        if (l != first_pivot)
          seen[l] = 0;
      if (first_satisfied)
        continue;
    }
  };

  std::vector<uint32_t> &pos_all = def.pos_all;
  std::vector<uint32_t> &neg_all = def.neg_all;
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
  // Eliminate iff not oversized and within the commit slack.
  if (oversized || over_gen) {
    return decline_after_compaction();
  }

  // ---- commit: add resolvents (RUP, add-before-delete), witnesses, delete ---
  std::vector<Ref> &fresh = def.fresh; // large resolvents, the backward pass's subsumers
  std::vector<std::pair<Lit, Lit>> &fresh_bin = def.fresh_bin; // size-2 resolvents, ditto
  std::vector<Lit> &connected_lits = def.connected_lits;
  fresh.clear();
  fresh_bin.clear();
  connected_lits.clear();

  for (const BveResolvent &r : resolvents) {
    connected_lits.clear();
    bool satisfied = false;
    // A literal unassigned when the resolvent was generated can be root-false
    // now, falsified by a sibling immediate unit resolved after it.
    for (const Lit l : r.lits) {
      const int8_t value = k.value(l);
      if (value > 0) {
        satisfied = true;
        break;
      }
      if (value == 0)
        connected_lits.push_back(l);
    }
    if (satisfied)
      continue;
    if (connected_lits.empty()) {
      learn_empty();
      return ApplyResult::Unsat;
    }
    if (connected_lits.size() == 1) {
      learn_unit(connected_lits[0]);
      continue;
    }
    const uint64_t id = ++k.next_clause_id;
    emit_rup_lemma(k, id, connected_lits);
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
      // Refs survive an arena resize; Clause* is re-fetched below.
    }
  }

  // Witnesses: each removed live clause, pivot as blocking literal (design
  // §3.4).
  const bool capture_weakening = kernel_records_weakening(k);
  std::vector<uint64_t> weakening_tokens;
  if (capture_weakening) weakening_tokens.resize(pos.size() + neg.size(), 0);
  size_t witness_index = 0;
  for (const std::vector<MClause> *side : {&pos, &neg}) {
    const Lit pivot = (side == &pos) ? px : nx;
    for (const MClause &m : *side) {
      const size_t source_index = witness_index++;
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
      if (capture_weakening) {
        const Lit binary[2] = {m.ba, m.bb};
        const Clause *full = m.ref == INVALID_REF ? nullptr : clause_at(k.arena, m.ref);
        weakening_tokens[source_index] = kernel_prepare_weakening(k, WeakeningFamily::Bve,
            kernel_population_clause(k, full ? full->lits : binary, full ? full->size : 2),
            kernel_population_clause(k, &pivot, 1), full ? full->id() : 0, !full, false,
            uint64_t(rest.size()) + 2);
      }
      kernel_push_witness(k, pivot, rest.data(),
                          static_cast<uint32_t>(rest.size()));
    }
  }
  // Retire the pivot with its witness (cadical elim.cpp:723 flags.cpp:34-46,
  // kissat eliminate.c:405 flags.c:77-100).
  retire_variable(k, x, VarStatus::Eliminated, "eliminated");

  // Delete every clause on x: large → proof del + garbage; each binary copy in
  // side/list order.
  auto del_large_ref = [&](Ref ref) {
    Clause *c = clause_at(k.arena, ref);
    if (c->garbage())
      return false;
    kernel_delete_clause(k, c);
    return true;
  };
  size_t deletion_index = 0;
  auto del_all = [&](const std::vector<MClause> &side) {
    for (const MClause &m : side) {
      const uint64_t token = capture_weakening ? weakening_tokens[deletion_index++] : 0;
      bool deleted;
      if (m.ref != INVALID_REF) {
        deleted = del_large_ref(m.ref);
      } else {
        // One connected occurrence removes one physical copy.  This preserves
        // Kissat's per-copy score-update sequence.
        const bool ok = kernel_delete_binary_one_copy(k, m.ba, m.bb);
        assert(ok);
        deleted = ok;
      }
      if (token) kernel_finish_weakening(k, token, deleted);
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
  // The drain can refute; report Unsat (cadical backward.cpp:224).
  return k.inconsistent ? ApplyResult::Unsat : ApplyResult::Ok;
}

// `Ok` is read from the kernel's clause-transition and root-assignment record;
// elimination from retirement and `bve_eliminated`.
ApplyResult bve_eliminate(Kernel &k, Var x, BveDefScratch &def) {
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
    const ApplyResult r = bve_eliminate(k, x, def);
    if (r == ApplyResult::Unsat)
      return ApplyResult::Unsat;
    // The round ends at the first candidate that changes state (elimination,
    // unit or retired antecedent); the next application reschedules from
    // current occurrences. DefaultPolicy's dosing loop ports Kissat's
    // continuing round.
    if (r == ApplyResult::Ok) {
      // Reclamation here is the canonical round's choice; the dosing path
      // reclaims at the end of its round.
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
  return copy_positive_variable_instance_prefix(k, cand, out, cap);
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
  // Rule-scoped scratch kept across admissions; thread-local per solver thread.
  static thread_local BveDefScratch def;
  return bve_eliminate(k, var_of(l), def);
}

TACTIX_REGISTER_RULE(BVE, RuleTier::Inprocess, bve_guard, bve_apply,
                     bve_instances, bve_apply_instance);

} // namespace tactix

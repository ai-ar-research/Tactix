// Sweep: candidate selection, environment construction and emission. Contract,
// soundness argument and deviation ledger: sweep.hpp.
#include "proof/proof.hpp"
#include "diagnostics/rule_journal.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/substitute/substitute.hpp"
#include "rules/sweep/sweep.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace tactix {

namespace {

#ifdef TACTIX_ENABLE_CONFLICT_STOP
static const bool sweep_time = std::getenv("TACTIX_SWEEP_TIME") != nullptr;
#endif

SubResult sweep_solve(SweepEnv &e, const std::vector<SubLit> &assumptions,
                      uint64_t budget) {
  e.solver.randomize_phases();
  return e.solver.solve(assumptions, budget, SubTickMode::Kitten);
}

ApplyResult sweep_emit_round(Kernel &, SweepRound &, uint32_t);
ApplyResult sweep_emit_pending_units(Kernel &, SweepRound &);

// Dedup keys for the environment's clause set: a binary by its literal pair
// (so both endpoint scans encode it once), a large clause by its arena ref, a
// unit by its variable. The tag bits keep the three spaces disjoint.
constexpr uint64_t KEY_LARGE = 1ull << 63;
constexpr uint64_t KEY_UNIT = 1ull << 62;

// At a quiescent root (the guard) an assigned variable is a fixed one.
bool root_fixed(const Kernel &k, Var v) {
  return k.values[make_lit(v, false)] != 0;
}

// Is (a ∨ b) live in the watch lists? Not `kernel_binary_id != 0`: `binary_ids`
// is size-monotone, so the id map would report a binary TransitiveReduce has
// already deleted. The pair is mirrored, so scan the shorter list.
bool binary_live(const Kernel &k, Lit a, Lit b) {
  const Lit scan = k.watches.size(a) <= k.watches.size(b) ? a : b;
  return kernel_binary_live(k, scan, scan == a ? b : a);
}

// --- the round's representative map (kissat sweep.c `sweep_repr`) -----------
// Union-find over literals with the mirror invariant repr[¬l] = ¬repr[l], so a
// class and its negation move together. Round-scoped and durable of nothing —
// V3 Substitute owns the durable substitution.
Lit repr_find(SweepRound &r, Lit l) {
  Lit root = l;
  while (r.repr[root] != root)
    root = r.repr[root];
  while (r.repr[l] != root) { // path compression, mirrored
    const Lit next = r.repr[l];
    r.repr[l] = root;
    r.repr[negate(l)] = negate(root);
    l = next;
  }
  return root;
}

void repr_unite(const Kernel &k, SweepRound &r, Lit a, Lit b) {
  Lit ra = repr_find(r, a), rb = repr_find(r, b);
  if (ra == rb)
    return;
  // `rb == ¬ra` means this round proved both x ≡ y and x ≡ ¬y, possible only
  // when F is unsatisfiable. Merging would make repr a two-cycle that hangs
  // `repr_find` forever, so detection draws no conclusion and the classes stay
  // apart. Emission must treat the case as a refutation, not as two binaries.
  if (rb == negate(ra))
    return;
  if (kernel_import_lit_order(k, rb) < kernel_import_lit_order(k, ra))
    std::swap(ra, rb);
  r.repr[rb] = ra;
  r.repr[negate(rb)] = negate(ra);
}

// --- environment assembly ---------------------------------------------------

void env_reset(const Kernel &k, SweepEnv &e) {
  if (e.sub_of_var.size() != k.nvars) {
    e.sub_of_var.assign(k.nvars, SWEEP_NO_SUB_VAR);
    e.touched.clear();
  } else {
    for (const Var v : e.touched) // touched-list restore, never a refill
      e.sub_of_var[v] = SWEEP_NO_SUB_VAR;
    e.touched.clear();
  }
  e.vars.clear();
  e.depth.clear();
  e.clause_ids.clear();
  e.clause_lits.clear();
  e.clause_off.assign(1, 0);
  e.strip_ids.clear();
  e.strip_off.assign(1, 0);
}

// Start large enough for the configured clause cap. Kissat admits the whole
// clause which crosses a structural boundary, so `keyset_grow` preserves the
// half-full invariant if that admitted clause crosses the initial cap.
void keyset_reset(SweepEnv &e, uint32_t cap) {
  size_t n = 16;
  while (n < 2 * static_cast<size_t>(cap) + 2)
    n <<= 1;
  if (e.key_slots.size() != n)
    e.key_slots.assign(n, UINT64_MAX);
  else
    std::fill(e.key_slots.begin(), e.key_slots.end(), UINT64_MAX);
  e.key_mask = n - 1;
}

// The slot `key` occupies or would occupy. Recording is a separate step: a
// clause that passes the dedup test can still be declined by the variable cap,
// and must stay re-addable, since `fresh` shrinks as its other variables enter
// the environment.
size_t keyset_slot(const SweepEnv &e, uint64_t key) {
  size_t i = ((key * 0x9E3779B97F4A7C15ull) >> 32) & e.key_mask;
  while (e.key_slots[i] != UINT64_MAX && e.key_slots[i] != key)
    i = (i + 1) & e.key_mask;
  return i;
}

void keyset_grow(SweepEnv &e) {
  const std::vector<uint64_t> old = std::move(e.key_slots);
  e.key_slots.assign(old.size() * 2, UINT64_MAX);
  e.key_mask = e.key_slots.size() - 1;
  for (const uint64_t key : old) {
    if (key == UINT64_MAX)
      continue;
    e.key_slots[keyset_slot(e, key)] = key;
  }
}

uint32_t env_var(SweepEnv &e, Var v, uint32_t depth) {
  uint32_t s = e.sub_of_var[v];
  if (s != SWEEP_NO_SUB_VAR)
    return s;
  s = static_cast<uint32_t>(e.vars.size());
  e.sub_of_var[v] = s;
  e.touched.push_back(v);
  e.vars.push_back(v);
  e.depth.push_back(depth);
  return s;
}

// Add one clause of F to the environment. Kissat checks its structural caps at
// variable-expansion boundaries, so the clause that crosses a cap lands whole;
// declining or truncating that clause changes the local formula. `strip` (may
// be null) are the proof ids justifying literals the caller already removed.
bool env_add(Kernel &k, SweepEnv &e, const Lit *lits, uint32_t n, uint64_t key,
             uint64_t id, uint32_t depth, const SweepLimits &,
             uint64_t *ticks, const std::vector<uint64_t> *strip = nullptr,
             bool deduplicate = true) {
  *ticks += 1;
  k.stats.sweep_ticks_dedup += 1;
  size_t slot = 0;
  if (deduplicate) {
    if (2 * (static_cast<size_t>(e.nclauses()) + 1) >= e.key_slots.size())
      keyset_grow(e);
    slot = keyset_slot(e, key);
    if (e.key_slots[slot] == key)
      return false;
  }
  for (uint32_t i = 0; i < n; i++) {
    e.clause_lits.push_back(lits[i]);
    env_var(e, var_of(lits[i]), depth);
  }
  e.clause_off.push_back(static_cast<uint32_t>(e.clause_lits.size()));
  if (strip)
    e.strip_ids.insert(e.strip_ids.end(), strip->begin(), strip->end());
  e.strip_off.push_back(static_cast<uint32_t>(e.strip_ids.size()));
  if (deduplicate)
    e.key_slots[slot] = key;
  e.clause_ids.push_back(id);
  return true;
}

// Add one clause of F with its root-false literals removed — justified, not
// assumed: `C \ {l}` follows from `C` and the root unit `{¬l}`, whose id travels
// in `SweepEnv::strip_ids` for emission to cite ahead of `C`. An unjustifiable
// removal is not made (`docs/calculus.md` V5).
void env_add_clause(Kernel &k, SweepEnv &e, const Lit *lits, uint32_t n,
                    uint64_t key, uint64_t id, uint32_t depth,
                    const SweepLimits &lim, uint64_t *ticks,
                    bool deduplicate = true) {
  // Chains are cited only under LRAT; DRAT and the no-proof mode need no id.
  const bool cite = k.proof && k.proof->lrat();
  e.kept_scratch.clear();
  e.strip_scratch.clear();
  for (uint32_t i = 0; i < n; i++) {
    const Lit l = lits[i];
    if (k.values[l] >= 0) {
      e.kept_scratch.push_back(l);
      continue;
    }
    k.stats.sweep_env_false++;
    const uint64_t uid = k.unit_ids[var_of(l)];
    if (!lim.strip_false || (cite && !uid)) {
      if (!uid)
        k.stats.sweep_env_freefalse++;
      e.kept_scratch.push_back(l); // unjustifiable removal (or the off arm)
      continue;
    }
    if (uid)
      e.strip_scratch.push_back(uid);
    k.stats.sweep_env_stripped++;
  }
  // At a quiescent root a live clause has at least two non-false literals, but
  // the detector is callable from a policy that has not reached one (N1) — so
  // decline rather than assert; a smaller environment is still entailed by F.
  if (e.kept_scratch.size() < 2)
    return;
  if (!env_add(k, e, e.kept_scratch.data(),
               static_cast<uint32_t>(e.kept_scratch.size()), key, id, depth,
               lim, ticks, &e.strip_scratch, deduplicate))
    return;
  k.stats.sweep_env_lits += e.kept_scratch.size();
  if (lim.strip_false)
    return;
  // Off arm (`TACTIX_SWEEP_STRIP=0`): the root-false literal stays, and its
  // unit joins the environment as a clause of F whenever the proof holds one.
  for (uint32_t i = 0; i < n; i++) {
    const Lit l = lits[i];
    if (k.values[l] >= 0)
      continue;
    const Var v = var_of(l);
    e.depth[e.sub_of_var[v]] = UINT32_MAX; // fixed: never a BFS frontier
    if (!k.unit_ids[v])
      continue;
    const Lit unit = negate(l);
    if (env_add(k, e, &unit, 1, KEY_UNIT | v, k.unit_ids[v], UINT32_MAX, lim,
                ticks))
      k.stats.sweep_env_units++;
  }
}

} // namespace

RuleEligibility sweep_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0);
}

// The variable's large-clause + watch degree, both polarities. Without the
// phase occurrence index only the watch lists are readable, which still ranks
// binaries and the two watched literals of every large clause.
static uint64_t seed_literal_occurrences(const Kernel &k, Lit lit) {
  if (!k.dense ||
      k.dense->large.size() < 2 * static_cast<size_t>(k.nvars))
    return k.watches.size(lit);
  // Kissat ranks in dense mode, where its watch lists contain each large
  // occurrence exactly once. Tactix keeps the sparse two-watch entries beside
  // its all-literal dense index, so adding both whole lists double-counts a
  // large clause precisely when `lit` is one of its watched literals.
  uint64_t binary = 0;
  for (const WatchEntry w : k.watches.entries(lit))
    binary += w.binary();
  return binary + k.dense->large[lit].size();
}

void sweep_seeds(const Kernel &k, std::vector<Var> &out) {
  out.clear();
  for (Var v = 0; v < k.nvars; v++) {
    // Fixed variables belong to V1 Probe / V2 Backbone (their equivalences are
    // units), retired ones are no longer in F. Everything else is a seed, with
    // no occurrence filter: empty watch lists do not mean no occurrences, since
    // a large clause is watched on two literals only.
    if (root_fixed(k, v) || kernel_var_retired(k, v))
      continue;
    out.push_back(v);
  }
  // Order, not membership (kissat `scheduable_variable` + its radix sort,
  // sweep.c:1478-1528). Ascending occurrence degree, single-polarity variables
  // last; kissat drops both classes, but ranking them to the back keeps
  // `instances()` total (N1, rule-dev §instances). Rank computed once per
  // variable, since a recomputing comparator walks four watch lists per call.
  struct RankedSeed {
    uint64_t degree;
    uint64_t import_order;
    Var var;
  };
  std::vector<RankedSeed> ranked;
  ranked.reserve(out.size());
  for (const Var v : out) {
    const Lit pos = make_lit(v, false), neg = make_lit(v, true);
    const uint64_t dp = seed_literal_occurrences(k, pos);
    const uint64_t dn = seed_literal_occurrences(k, neg);
    const bool both = dp && dn;
    const uint64_t d = dp + dn;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_sweep_schedule(k, v, dp, dn, both ? d : UINT64_MAX,
                               kernel_import_var_order(k, v));
#endif
    ranked.push_back(
        {both ? d : UINT64_MAX, kernel_import_var_order(k, v), v});
  }
  std::sort(ranked.begin(), ranked.end(), [](const RankedSeed &a,
                                             const RankedSeed &b) {
    if (a.degree != b.degree)
      return a.degree < b.degree;
    if (a.import_order != b.import_order)
      return a.import_order < b.import_order;
    return a.var < b.var;
  });
  for (size_t i = 0; i < ranked.size(); i++)
    out[i] = ranked[i].var;
}

static bool sweep_schedulable(const Kernel &k, Var v,
                              const SweepLimits &lim) {
  if (v >= k.nvars || root_fixed(k, v) || kernel_var_retired(k, v))
    return false;
  const Lit pos = make_lit(v, false), neg = make_lit(v, true);
  const uint64_t dp = seed_literal_occurrences(k, pos);
  const uint64_t dn = seed_literal_occurrences(k, neg);
  return dp && dn && dp <= lim.max_env_clauses &&
         dn <= lim.max_env_clauses;
}

static uint32_t grown_limit(uint32_t base, uint32_t reference_max,
                            uint32_t completed) {
  const uint64_t ceiling = std::max(base, reference_max);
  const uint32_t shift = std::min(completed, 32u);
  const uint64_t grown = shift == 32 ? UINT64_MAX
                                     : static_cast<uint64_t>(base) << shift;
  return static_cast<uint32_t>(std::min(grown, ceiling));
}

static void sweep_completed_limits(Kernel &k, SweepLimits &lim) {
  lim.max_env_vars = grown_limit(lim.max_env_vars,
                                 lim.max_env_vars_ceiling,
                                 k.sweep_passes_completed);
  lim.max_env_clauses = grown_limit(lim.max_env_clauses,
                                    lim.max_env_clauses_ceiling,
                                    k.sweep_passes_completed);
  const uint64_t depth =
      static_cast<uint64_t>(lim.max_depth) + k.sweep_passes_completed;
  lim.max_depth = static_cast<uint32_t>(
      std::min(depth, static_cast<uint64_t>(
                          std::max(lim.max_depth, lim.max_depth_ceiling))));
  k.stats.sweep_limit_vars = lim.max_env_vars;
  k.stats.sweep_limit_clauses = lim.max_env_clauses;
  k.stats.sweep_limit_depth = lim.max_depth;
}

// Occurrence index over irredundant large clauses. Irredundant only, because
// `Reduce` may delete a learned clause between detection and emission and a
// chain citing a deleted id is a rejected proof. Outside a simplification
// phase a single arena pass builds the same thing, so the detector stays
// usable at any state (N1).
static void sweep_build_occurrences(Kernel &k, SweepRound &r) {
  if (k.dense && k.dense->large.size() >= 2 * static_cast<size_t>(k.nvars)) {
    r.occ = &k.dense->large;
  } else {
    r.occ_owned.assign(2 * k.nvars, {});
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_actual_words(c);
      if (!c->garbage() && !c->redundant())
        for (const Lit l : *c)
          r.occ_owned[l].push_back(ref);
      k.stats.sweep_ticks += words;
      ref += words;
    }
    r.occ = &r.occ_owned;
  }
  r.occ_generation = k.arena_generation;
}

// Kissat enters dense mode immediately before scheduling Sweep.  Its
// null-stack `kissat_flush_large_watches` deduplicates binaries (and, unlike
// Congruence's non-null dense entry, retains root-satisfied ones);
// `connect_irredundant_large_clauses` then garbages root-satisfied large clauses
// before constructing the full occurrence lists.  Reusing Tactix's
// older dense index without this transition leaves clauses satisfied by the
// preceding Backbone pass in the degree ranks and changes the very first seed.
static void sweep_enter_dense(Kernel &k) {
  kernel_deduplicate_binary_watches(k);
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(c);
    // `kissat_connect_irredundant_large_clauses` stops at the irredundant
    // prefix and explicitly skips redundant clauses.  Root-satisfied learned
    // clauses remain for the following late Substitute; retiring them here
    // moves a semantic delta from Substitute into Sweep and changes exact
    // per-rule accounting.
    if (!c->garbage() && !c->redundant()) {
      bool satisfied = false;
      for (const Lit lit : *c)
        if (k.value(lit) > 0) {
          satisfied = true;
          break;
        }
      if (satisfied) {
        // dense entry runs at a quiescent root; a reason may not name a
        // deleted clause (kernel.hpp kernel_retire_root_reason)
        kernel_retire_root_reason(k, c, ref);
        kernel_delete_clause(k, c);
      }
    }
    ref += words;
  }
  if (k.dense) {
    k.dense->large.assign(2 * k.nvars, {});
    if (k.dense->track_order) {
      k.dense->ordered.assign(2 * k.nvars, {});
      for (Lit lit = 0; lit < 2 * k.nvars; ++lit)
        for (const WatchEntry watch : k.watches.entries(lit))
          if (watch.binary())
            k.dense->ordered[lit].push_back(
                OccIndex::Entry::from_binary(watch.other()));
    }
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      if (!c->garbage() && !c->redundant())
        for (const Lit lit : *c) {
          k.dense->large[lit].push_back(ref);
          if (k.dense->track_order)
            k.dense->ordered[lit].push_back(
                OccIndex::Entry::from_large(ref));
        }
      ref += clause_actual_words(c);
    }
  }
}

// A productive seed substitutes its consequences inline, and that collects the
// variable-width arena. `Kernel::dense` is rebuilt by the collector; a
// round-local snapshot is not, so its Ref offsets would keep naming the
// pre-collection arena. Checked per seed rather than at the emission site so
// no future collector inside a round can reintroduce the stale traversal.
static void sweep_refresh_occurrences(Kernel &k, SweepRound &r) {
  if (r.occ == &r.occ_owned && r.occ_generation != k.arena_generation)
    sweep_build_occurrences(k, r);
}

void sweep_round_begin(Kernel &k, SweepRound &r) {
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  uint64_t t0 = sweep_time ? now_nanos() : 0;
#endif
  r.env.solver.start_kitten_session();
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  uint64_t t1 = sweep_time ? now_nanos() : 0;
#endif
  sweep_enter_dense(k);
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  uint64_t t2 = sweep_time ? now_nanos() : 0;
#endif
  sweep_build_occurrences(k, r);
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  uint64_t t3 = sweep_time ? now_nanos() : 0;
#endif
  r.substitution.begin(k);
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  uint64_t t4 = sweep_time ? now_nanos() : 0;
#endif
#ifdef TACTIX_ENABLE_EVENT_TRACE
  for (Var v = 0; v < k.nvars; v++) {
    uint32_t ordinal = 0;
    for (uint32_t sign = 0; sign < 2; sign++)
      for (const WatchEntry w : k.watches.entries(make_lit(v, sign != 0)))
        if (w.binary())
          event_trace_sweep_watch(k, v, ordinal++, w.other());
  }
#endif
  r.repr.resize(2 * k.nvars);
  for (Lit l = 0; l < 2 * k.nvars; l++)
    r.repr[l] = l; // O(nlits) once per ROUND, never per environment
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  const uint64_t t5 = sweep_time ? now_nanos() : 0;
  if (sweep_time) {
    r.time_kitten += t1 - t0;
    r.time_dense += t2 - t1;
    r.time_occ += t3 - t2;
    r.time_ledger += t4 - t3;
    r.time_repr += t5 - t4;
  }
#endif
  r.found.clear();
  // `proofs` is parallel to `found` (`sweep_apply` indexes both by the same
  // `idx`), so it must be reset with it: clearing one and not the other leaves
  // every later pair reading the previous round's refutation.
  r.proofs.clear();
  r.units.clear();
  r.units_emitted = 0;
  r.equivalences_emitted = 0;
  r.emission_batches = 0;
}

bool sweep_env_build(Kernel &k, Var seed, const SweepLimits &lim,
                     SweepRound &r) {
  assert(r.occ && "sweep_round_begin must run first");
  sweep_refresh_occurrences(k, r);
  SweepEnv &e = r.env;
  env_reset(k, e);
  keyset_reset(e, lim.max_env_clauses);
  if (root_fixed(k, seed) || kernel_var_retired(k, seed))
    return false;
  env_var(e, seed, 0);
  uint64_t ticks = 0;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  uint32_t binary_attempt_ordinal = 0;
#endif
  // Breadth-first over the clause neighbourhood (kissat sweep.c
  // sweep_binary/sweep_reference). `vars` doubles as the BFS queue, appended in
  // non-decreasing depth; fixed variables carry UINT32_MAX, so the frontier
  // never expands through one.
  bool limit_reached = false;
  for (uint32_t i = 0; i < e.vars.size() && !limit_reached; i++) {
    if (e.nclauses() >= lim.max_env_clauses)
      break;
    const uint32_t d = e.depth[i];
    if (d && d >= lim.max_depth)
      break;
    const Var v = e.vars[i];
    for (uint32_t pol = 0; pol < 2 && !limit_reached; pol++) {
      const Lit l = make_lit(v, pol != 0);
      ticks += k.watches.size(l);
      const auto process_binary = [&](Lit o) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
        if (repr_find(r, l) != l) {
          event_trace_sweep_binary_attempt(
              k, seed, binary_attempt_ordinal++, l, o,
              static_cast<int32_t>(d), -1, "skip-lit-repr");
          return;
        }
        if (repr_find(r, o) != o) {
          event_trace_sweep_binary_attempt(
              k, seed, binary_attempt_ordinal++, l, o,
              static_cast<int32_t>(d), -1, "skip-other-repr");
          return;
        }
#else
        if (repr_find(r, l) != l || repr_find(r, o) != o)
          return;
#endif
        const uint32_t other_sub = e.sub_of_var[var_of(o)];
#ifdef TACTIX_ENABLE_EVENT_TRACE
        const int32_t other_depth =
            other_sub == SWEEP_NO_SUB_VAR ? -1
                                          : static_cast<int32_t>(e.depth[other_sub]);
#endif
        if (k.values[l] > 0 || k.values[o] > 0) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
          event_trace_sweep_binary_attempt(
              k, seed, binary_attempt_ordinal++, l, o,
              static_cast<int32_t>(d), other_depth, "skip-satisfied");
#endif
          return;
        }
        if (other_sub != SWEEP_NO_SUB_VAR && e.depth[other_sub] < d) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
          event_trace_sweep_binary_attempt(
              k, seed, binary_attempt_ordinal++, l, o,
              static_cast<int32_t>(d), other_depth, "skip-shallower");
#endif
          return;
        }
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_sweep_binary_attempt(
            k, seed, binary_attempt_ordinal++, l, o,
            static_cast<int32_t>(d), other_depth, "add");
#endif
        const Lit pair[2] = {l, o};
        env_add_clause(k, e, pair, 2, kernel_binary_key(l, o),
                       kernel_binary_id(k, l, o), d + 1, lim, &ticks, false);
      };
      const auto process_large = [&](Ref ref) {
        // Scan work remains diagnostic; Kissat spends the embedded-solver tick
        // currency and bounds the resulting environment structurally.
        ticks += 1;
        Clause *c = clause_at(k.arena, ref);
        if (c->garbage() || c->redundant())
          return; // the index may hold stale refs (OccIndex contract)
        bool satisfied = false;
        for (const Lit m : *c)
          satisfied = satisfied || k.values[m] > 0;
        ticks += c->size;
        if (satisfied) {
          // Kissat `sweep_reference` retires a root-satisfied large clause as
          // soon as the dense traversal encounters it. Merely skipping it
          // leaves a different live formula and changes which later clauses
          // fit into capped environments (the seed-89 startup divergence).
          // Same quiescent-root deletion contract as the dense entry above: a
          // trail reason may not name a dropped clause
          // (kernel.hpp kernel_retire_root_reason).
          kernel_retire_root_reason(k, c, ref);
          kernel_delete_clause(k, c);
          return;
        }
        env_add_clause(k, e, c->begin(), c->size, KEY_LARGE | ref, c->id(),
                       d + 1, lim, &ticks);
      };
      if (r.substitution.has_dense_occurrences()) {
        for (const auto occurrence : r.substitution.occurrences(k, l)) {
          if (occurrence.binary)
            process_binary(static_cast<Lit>(occurrence.payload));
          else
            process_large(static_cast<Ref>(occurrence.payload));
          if (e.vars.size() >= lim.max_env_vars) {
            limit_reached = true;
            break;
          }
        }
      } else {
        for (const WatchEntry watch : k.watches.entries(l)) {
          if (watch.binary())
            process_binary(watch.other());
          if (e.vars.size() >= lim.max_env_vars) {
            limit_reached = true;
            break;
          }
        }
        if (!limit_reached)
          for (const Ref ref : (*r.occ)[l]) {
            process_large(ref);
            if (e.vars.size() >= lim.max_env_vars) {
              limit_reached = true;
              break;
            }
          }
      }
    }
  }
  k.stats.sweep_ticks += ticks;
  // Root-satisfied clauses can be retired while the environment is built, so
  // take the snapshot only after the BFS has finished mutating the formula.
  e.formula_epoch = k.touch_epoch;
  // Closure diagnostic: an environment is a ball around the seed, so its outer
  // variables arrive with only the clause that introduced them and every pair
  // they join costs a solve to learn that nothing about them can be proved.
  if (!lim.measure_closure)
    return e.nclauses() > 0;
  e.deg_scratch.assign(e.vars.size(), 0);
  for (uint32_t c = 0; c < e.nclauses(); c++) {
    if (e.clause_off[c + 1] - e.clause_off[c] <= 2)
      continue; // binaries are not in the occurrence index either
    for (uint32_t j = e.clause_off[c]; j < e.clause_off[c + 1]; j++)
      e.deg_scratch[e.sub_of_var[var_of(e.clause_lits[j])]]++;
  }
  for (uint32_t i = 0; i < e.vars.size(); i++) {
    const Var v = e.vars[i];
    const size_t deg_f = (*r.occ)[make_lit(v, false)].size() +
                         (*r.occ)[make_lit(v, true)].size();
    k.stats.sweep_occ_f += deg_f;
    k.stats.sweep_occ_env += e.deg_scratch[i];
    k.stats.sweep_vars_closed += e.deg_scratch[i] >= deg_f;
    k.stats.sweep_vars_total++;
  }
  return e.nclauses() > 0;
}

#ifdef TACTIX_ENABLE_EVENT_TRACE
static uint64_t sweep_trace_mix(uint64_t x) {
  x += 0x9e3779b97f4a7c15ull;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}

static uint64_t sweep_environment_hash(const Kernel &k, const SweepEnv &e) {
  uint64_t result = 0;
  for (uint32_t i = 0; i < e.nclauses(); i++) {
    const uint32_t begin = e.clause_off[i], end = e.clause_off[i + 1];
    uint64_t clause = sweep_trace_mix(end - begin);
    for (uint32_t j = begin; j < end; j++)
      clause += sweep_trace_mix(static_cast<uint64_t>(
          static_cast<int64_t>(kernel_externalize(k, e.clause_lits[j]))));
    result += sweep_trace_mix(clause);
  }
  return result;
}
#endif

namespace {

// The value the sub-solver's model gives a kernel literal of the environment.
int8_t model_value(const SweepEnv &e, const std::vector<int8_t> &model, Lit l) {
  const int8_t v = model[e.sub_of_var[var_of(l)]];
  return negated(l) ? static_cast<int8_t>(-v) : v;
}

// Drop every candidate the new model separates from the seed: kissat's
// sweep_refine_partition in seed-anchored form. The candidate that produced the
// model is always among the dropped ones, which is what terminates the loop.
uint32_t refine(const SweepEnv &e, const std::vector<int8_t> &model,
                Lit seed_lit, std::vector<Lit> &candidates) {
  const int8_t sv = model_value(e, model, seed_lit);
  size_t j = 0;
  for (const Lit y : candidates)
    if (model_value(e, model, y) == sv)
      candidates[j++] = y;
  const uint32_t dropped = static_cast<uint32_t>(candidates.size() - j);
  candidates.resize(j);
  return dropped;
}

} // namespace

// Sub-solver literal -> kernel literal, the single decode site: every
// translation goes through it rather than respelling it, because getting the
// parity wrong is silent — the two encodings share the `2*v | negated` shape,
// so an inverted decode yields the negation of the derived clause, and that is
// unverifiable (`benchmarks/reports/2026-08-05-sweep-se-polarity-fix.md`).
static inline Lit sweep_kernel_lit(const std::vector<Var> &vars, SubLit sl) {
  return make_lit(vars[sub_var_of(sl)], (sl & 1u) != 0);
}

// Copy one direction's refutation out with the translation tables it will need:
// `steps` names sub-solver clause ids, and the environment giving them meaning
// is rebuilt for the next seed. Failure just means no general-path proof. The
// target is read off the derivation, never predicted from the assumptions — a
// wrong prediction silently swaps the two directions.
static void sweep_capture_dir(const SweepEnv &e, SweepDirProof &out) {
  out.ok = false;
  out.t0 = out.t1 = INVALID_LIT;
  out.steps.clear();
  SubDerivation d;
  if (!e.solver.derivation(d) || d.steps.empty())
    return; // no refutation to hand back (e.g. directly contradictory assumptions)
  if (d.steps.size() > SWEEP_MAX_PROOF_STEPS)
    return; // pathological: decline rather than allocate without bound
  const std::vector<SubLit> &tl = d.steps.back().lits;
  if (tl.empty() || tl.size() > 2)
    return;
  out.t0 = sweep_kernel_lit(e.vars, tl[0]);
  out.t1 = tl.size() == 2 ? sweep_kernel_lit(e.vars, tl[1]) : INVALID_LIT;
  out.steps = std::move(d.steps);
  out.orig_ids = e.clause_ids;
  out.strip_ids = e.strip_ids;
  out.strip_off = e.strip_off;
  out.vars = e.vars;
  out.formula_epoch = e.formula_epoch;
  out.ok = true;
}

// Kissat's `save_add_clear_core` does not retain only the failed assumption's
// final target.  It walks every learned lemma in the extracted core and a lemma
// which is unit under the live root assignment is committed immediately.  A
// later SAT refinement in this same environment can therefore discard that
// literal before ever posing it as an equivalence candidate.  Keep the same
// ordered semantic surface here: each unit gets the derivation prefix ending
// at the lemma which proves it, rather than borrowing the final target's proof.
static void sweep_capture_core_units(const Kernel &k,
                                     const SweepDirProof &proof,
                                     SweepRound &round) {
  if (!proof.ok)
    return;
  for (size_t i = 0; i < proof.steps.size(); i++) {
    const SubStep &step = proof.steps[i];
    Lit unit = INVALID_LIT;
    bool satisfied = false;
    for (const SubLit sl : step.lits) {
      if (sub_var_of(sl) >= proof.vars.size()) {
        satisfied = true; // malformed captures decline in the real evaluator
        break;
      }
      const Lit lit = sweep_kernel_lit(proof.vars, sl);
      const int8_t value = k.value(lit);
      if (value > 0) {
        satisfied = true;
        break;
      }
      if (!value) {
        if (unit != INVALID_LIT) {
          unit = INVALID_LIT;
          satisfied = true; // more than one non-false literal: not a unit
          break;
        }
        unit = lit;
      }
    }
    if (satisfied || unit == INVALID_LIT)
      continue;
    if (std::any_of(round.units.begin(), round.units.end(),
                    [&](const SweepUnitProof &known) {
                      return known.unit == unit;
                    }))
      continue;
    SweepUnitProof captured;
    captured.unit = unit;
    captured.proof = proof;
    captured.proof.steps.resize(i + 1);
    captured.proof.t0 = unit;
    captured.proof.t1 = INVALID_LIT;
    round.units.push_back(std::move(captured));
  }
}

static uint32_t partition_size(const SweepRound &r) {
  uint32_t n = 0;
  for (const auto &c : r.partition)
    n += static_cast<uint32_t>(c.size());
  return n;
}

static void partition_refine(const Kernel &k, SweepRound &r) {
  SweepEnv &e = r.env;
  std::vector<std::vector<Lit>> next;
  for (const auto &c : r.partition) {
    std::vector<Lit> positive, negative;
    for (const Lit l : c) {
      // `sweep_refine_partition` drops both merged representatives and every
      // literal fixed in the main solver. The latter matters for a unit proved
      // earlier in this same seed: it is assigned immediately, so retaining it
      // here manufactures a later equivalence query Kissat never asks.
      if (repr_find(r, l) != l || k.values[l] != 0)
        continue;
      (model_value(e, e.solver.model(), l) > 0 ? positive : negative)
          .push_back(l);
    }
    if (positive.size() > 1)
      next.push_back(std::move(positive));
    if (negative.size() > 1)
      next.push_back(std::move(negative));
  }
  r.partition = std::move(next);
}

// Kissat removes the non-representative endpoint from the current partition
// immediately after merging an equivalence (`sweep.c:1220-1248`).  A generic
// refinement here would be wrong because the last sub-solver result is UNSAT
// and therefore has no model; update only the class membership that the merge
// itself settled.
static void partition_remove_merged_endpoint(SweepRound &r, Lit a, Lit b) {
  assert(!r.partition.empty());
  std::vector<Lit> &c = r.partition.back();
  assert(c.size() >= 2 && c[c.size() - 2] == a && c.back() == b);
  if (c.size() == 2) {
    r.partition.pop_back();
    return;
  }
  const Lit drop = repr_find(r, a) == a ? b : a;
  if (drop == b)
    c.pop_back();
  else
    c.erase(c.end() - 2);
}

static void backbone_refine(const Kernel &k, SweepRound &r) {
  SweepEnv &e = r.env;
  size_t keep = 0;
  for (const Lit lit : r.backbone)
    if (k.values[lit] == 0 && model_value(e, e.solver.model(), lit) > 0)
      r.backbone[keep++] = lit;
  r.backbone.resize(keep);
}

static void backbone_flip(Kernel &k,
#ifdef TACTIX_ENABLE_EVENT_TRACE
                          Var seed,
#endif
                          const SweepLimits &lim, SweepRound &r) {
  SweepEnv &e = r.env;
  if (!e.solver.has_model())
    return;
  for (uint32_t round = 0; round < lim.flip_rounds; round++) {
    uint32_t flipped = 0;
    size_t keep = 0;
    for (const Lit lit : r.backbone) {
      k.stats.sweep_backbone_candidates++;
      const SubLit sl = sub_lit(e.sub_of_var[var_of(lit)], negated(lit));
      const bool did_flip = e.solver.flip_model_literal(sl);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_flip(k, seed, "backbone_round", lit, did_flip,
                             e.solver.kitten_ticks());
#endif
      if (did_flip) {
        k.stats.sweep_backbone_flipped++;
        flipped++;
      } else {
        r.backbone[keep++] = lit;
      }
      if (lim.env_ticks && e.solver.kitten_ticks() > lim.env_ticks)
        break;
    }
    r.backbone.resize(keep);
    if (!flipped ||
        (lim.env_ticks && e.solver.kitten_ticks() > lim.env_ticks))
      break;
  }
}

static void partition_flip(Kernel &k,
#ifdef TACTIX_ENABLE_EVENT_TRACE
                           Var seed,
#endif
                           const SweepLimits &lim, SweepRound &r) {
  SweepEnv &e = r.env;
  if (!e.solver.has_model())
    return;
  for (uint32_t round = 0; round < lim.flip_rounds; round++) {
    uint32_t flipped = 0;
    for (auto &c : r.partition) {
      size_t keep = 0;
      size_t live = c.size();
      for (size_t i = 0; i < c.size(); i++) {
        const Lit l = c[i];
        k.stats.sweep_flip_attempts++;
        const SubLit sl = sub_lit(e.sub_of_var[var_of(l)], negated(l));
        const bool did_flip = e.solver.flip_model_literal(sl);
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_sweep_flip(k, seed, "partition_round", l, did_flip,
                               e.solver.kitten_ticks());
#endif
        if (did_flip) {
          k.stats.sweep_flipped++;
          flipped++;
          // Kissat abandons the rest of this class as soon as a successful
          // flip leaves fewer than two possible members.  Trying the unscanned
          // tail cannot produce another equivalence, but it does mutate the
          // embedded model and watch lists seen by the next query.
          if (--live < 2)
            break;
        } else {
          c[keep++] = l;
        }
        if (lim.env_ticks && e.solver.kitten_ticks() > lim.env_ticks)
          break;
      }
      c.resize(live < 2 ? 0 : keep);
      if (lim.env_ticks && e.solver.kitten_ticks() > lim.env_ticks)
        break;
    }
    r.partition.erase(
        std::remove_if(r.partition.begin(), r.partition.end(),
                       [](const auto &c) { return c.size() < 2; }),
        r.partition.end());
    if (!flipped ||
        (lim.env_ticks && e.solver.kitten_ticks() > lim.env_ticks))
      break;
  }
}

static uint32_t detect_full_partition(
    Kernel &k,
#ifdef TACTIX_ENABLE_EVENT_TRACE
    Var seed,
#endif
    const SweepLimits &lim, SweepRound &r, bool commit_inline
#ifdef TACTIX_ENABLE_EVENT_TRACE
    , uint32_t &query_ordinal
#endif
) {
  SweepEnv &e = r.env;
  r.partition.clear();
  r.partition.emplace_back();
  r.backbone.clear();
  auto &initial = r.partition.back();
  for (const Var v : e.vars) {
    if (root_fixed(k, v) || kernel_var_retired(k, v))
      continue;
    const uint32_t u = e.sub_of_var[v];
    const Lit l = make_lit(v, e.solver.model()[u] < 0);
    if (repr_find(r, l) == l) {
      initial.push_back(l);
      r.backbone.push_back(l);
    }
  }
  if (initial.size() < 2)
    r.partition.clear();
  k.stats.sweep_cand_seeded += partition_size(r);

  std::vector<SubLit> &assumptions = r.assumptions;
  while (!r.backbone.empty()) {
    // Kissat's outer refinement poll is inclusive even though the inner flip
    // loop permits an overrun (`kitten_ticks_limit_hit`, sweep.c:1369-1382).
    if (lim.env_ticks && e.solver.kitten_ticks() >= lim.env_ticks)
      break;
    // As in the equivalence loop below, Kissat runs the complete flip pass at
    // the top of every outer iteration.  In particular, a successful retry of
    // one popped candidate returns here before the next candidate is chosen;
    // the changed model can eliminate or reorder much of the remaining
    // backbone without another embedded solve.
    backbone_flip(k,
#ifdef TACTIX_ENABLE_EVENT_TRACE
                  seed,
#endif
                  lim, r);
    if (r.backbone.empty() ||
        (lim.env_ticks && e.solver.kitten_ticks() >= lim.env_ticks))
      break;
    const Lit lit = r.backbone.back();
    r.backbone.pop_back();
    const SubLit candidate =
        sub_lit(e.sub_of_var[var_of(lit)], negated(lit));
    // `kitten_fixed` is checked before the retry flip. A root-fixed candidate
    // has already been proved inside the incremental environment; querying it
    // again changes both the learned-clause stream and all later partitions.
    if (lim.skip_embedded_fixed && e.solver.fixed_literal(candidate))
      continue;
    // Kissat retries the popped candidate even though the immediately
    // preceding round retained it after a failed flip.  Failure is not
    // side-effect free: earlier watched clauses may already have relocated
    // before a later clause blocks the flip.
    k.stats.sweep_backbone_candidates++;
    const bool did_flip =
        e.solver.has_model() && e.solver.flip_model_literal(candidate);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_sweep_flip(k, seed, "backbone_candidate", lit, did_flip,
                           e.solver.kitten_ticks());
#endif
    if (did_flip) {
      k.stats.sweep_backbone_flipped++;
      continue;
    }
    assumptions = {sub_lit(e.sub_of_var[var_of(lit)], !negated(lit))};
    k.stats.sweep_solves++;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const uint64_t ticks_before = e.solver.kitten_ticks();
#endif
    const SubResult res = sweep_solve(e, assumptions, lim.env_ticks);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_sweep_query(k, seed, query_ordinal++, "backbone", lit,
                            INVALID_LIT, static_cast<int>(res),
                            e.solver.kitten_ticks() - ticks_before,
                            e.solver.conflicts());
    if (res == SubResult::Sat)
      event_trace_sweep_model(k, seed, query_ordinal - 1, e.vars.data(),
                              e.solver.model().data(), e.vars.size());
#endif
    if (res == SubResult::Budget) {
      k.stats.sweep_budget++;
      break;
    }
    if (res == SubResult::Sat) {
      k.stats.sweep_dir_sat++;
      backbone_refine(k, r);
      partition_refine(k, r);
      continue;
    }
    k.stats.sweep_dir_unsat++;
    SweepDirProof proof;
    sweep_capture_dir(e, proof);
    sweep_capture_core_units(k, proof, r);
    if (commit_inline &&
        sweep_emit_pending_units(k, r) == ApplyResult::Ok)
      r.emission_batches++;
  }

  uint32_t found = 0, solved = 0;
  while (!r.partition.empty() &&
         (!lim.max_candidates || solved < lim.max_candidates)) {
    if (lim.env_ticks && e.solver.kitten_ticks() >= lim.env_ticks)
      break;
    // Kissat runs the whole partition-flip pass at the top of *every*
    // equivalence-loop iteration, not only after SAT refinement.  A successful
    // endpoint flip, or a proved equivalence, mutates the embedded model and
    // watch lists before returning here.  Omitting this interstitial pass keeps
    // candidates alive which Kissat can discard without another solve.
    partition_flip(k,
#ifdef TACTIX_ENABLE_EVENT_TRACE
                   seed,
#endif
                   lim, r);
    if (r.partition.empty() ||
        (lim.env_ticks && e.solver.kitten_ticks() >= lim.env_ticks))
      break;
    std::vector<Lit> &c = r.partition.back();
    const Lit a = c[c.size() - 2], b = c.back();
    // Kissat first asks the current model whether either endpoint can be
    // flipped. A successful flip disproves this candidate pair without a SAT
    // query and removes that endpoint from the live class.
    const auto flip_candidate = [&](Lit lit) {
      k.stats.sweep_flip_attempts++;
      const SubLit sl = sub_lit(e.sub_of_var[var_of(lit)], negated(lit));
      const bool did_flip = e.solver.flip_model_literal(sl);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_flip(k, seed, "pair", lit, did_flip,
                             e.solver.kitten_ticks());
#endif
      if (!did_flip)
        return false;
      k.stats.sweep_flipped++;
      return true;
    };
    if (flip_candidate(a)) {
      if (c.size() == 2)
        r.partition.pop_back();
      else
        c.erase(c.end() - 2);
      continue;
    }
    if (flip_candidate(b)) {
      if (c.size() == 2)
        r.partition.pop_back();
      else
        c.pop_back();
      continue;
    }

    SweepPairProof pair;
    // Kissat asks ¬a ∧ b first, then a ∧ ¬b.  Reversing these two sound
    // implication checks changes the first SAT model and therefore every
    // subsequent partition refinement and model-flip trajectory.
    assumptions = {sub_lit(e.sub_of_var[var_of(a)], !negated(a)),
                   sub_lit(e.sub_of_var[var_of(b)], negated(b))};
    solved++;
    k.stats.sweep_candidates++;
    k.stats.sweep_solves++;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const uint64_t fwd_ticks_before = e.solver.kitten_ticks();
#endif
    SubResult res = sweep_solve(e, assumptions, lim.env_ticks);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_sweep_query(k, seed, query_ordinal++, "equiv_fwd", a, b,
                            static_cast<int>(res),
                            e.solver.kitten_ticks() - fwd_ticks_before,
                            e.solver.conflicts());
    if (res == SubResult::Sat)
      event_trace_sweep_model(k, seed, query_ordinal - 1, e.vars.data(),
                              e.solver.model().data(), e.vars.size());
#endif
    if (res == SubResult::Budget) {
      k.stats.sweep_budget++;
      break;
    }
    if (res == SubResult::Sat) {
      k.stats.sweep_dir_sat++;
      const uint32_t before = partition_size(r);
      partition_refine(k, r);
      k.stats.sweep_cand_refined += before - partition_size(r);
      continue;
    }
    k.stats.sweep_dir_unsat++;
    assert(!e.solver.inconsistent());
    sweep_capture_dir(e, pair.fwd);

    assumptions = {sub_lit(e.sub_of_var[var_of(a)], negated(a)),
                   sub_lit(e.sub_of_var[var_of(b)], !negated(b))};
    solved++;
    k.stats.sweep_candidates++;
    k.stats.sweep_solves++;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const uint64_t rev_ticks_before = e.solver.kitten_ticks();
#endif
    res = sweep_solve(e, assumptions, lim.env_ticks);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_sweep_query(k, seed, query_ordinal++, "equiv_rev", a, b,
                            static_cast<int>(res),
                            e.solver.kitten_ticks() - rev_ticks_before,
                            e.solver.conflicts());
    if (res == SubResult::Sat)
      event_trace_sweep_model(k, seed, query_ordinal - 1, e.vars.data(),
                              e.solver.model().data(), e.vars.size());
#endif
    if (res == SubResult::Budget) {
      k.stats.sweep_budget++;
      break;
    }
    if (res == SubResult::Sat) {
      k.stats.sweep_dir_sat++;
      const uint32_t before = partition_size(r);
      partition_refine(k, r);
      k.stats.sweep_cand_refined += before - partition_size(r);
      continue;
    }
    k.stats.sweep_dir_unsat++;
    assert(!e.solver.inconsistent());
    sweep_capture_dir(e, pair.rev);
    sweep_capture_core_units(k, pair.fwd, r);
    if (commit_inline &&
        sweep_emit_pending_units(k, r) == ApplyResult::Ok)
      r.emission_batches++;
    sweep_capture_core_units(k, pair.rev, r);
    if (commit_inline &&
        sweep_emit_pending_units(k, r) == ApplyResult::Ok)
      r.emission_batches++;
    r.found.push_back({std::min(a, b), std::max(a, b)});
    r.proofs.push_back(std::move(pair));
    k.stats.sweep_equivalences++;
    found++;
    repr_unite(k, r, a, b);
    partition_remove_merged_endpoint(r, a, b);
  }
  return found;
}

static uint32_t sweep_process_seed(Kernel &k, Var seed, const SweepLimits &lim,
                                   SweepRound &r, bool commit_inline) {
  assert(sweep_guard(k));
  assert(r.repr.size() == 2 * k.nvars && "sweep_round_begin must run first");
  SweepEnv &e = r.env;
  const Lit seed_lit = make_lit(seed, false);
  if (repr_find(r, seed_lit) != seed_lit)
    return 0; // already merged into a class this round: its sweep is that
              // class representative's (kissat schedule_inner)
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_sweep_seed(k, seed);
#endif
  if (!sweep_env_build(k, seed, lim, r)) {
    e.solver.reset(0, SubSearchMode::Kitten);
    return 0;
  }
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_sweep_environment(k, seed, static_cast<uint32_t>(e.vars.size()),
                                e.nclauses(), sweep_environment_hash(k, e));
  for (uint32_t c = 0; c < e.nclauses(); c++)
    event_trace_sweep_environment_clause(
        k, seed, c, e.clause_lits.data() + e.clause_off[c],
        e.clause_off[c + 1] - e.clause_off[c]);
  for (uint32_t i = 0; i < e.vars.size(); i++)
    event_trace_sweep_environment_var(k, seed, i, e.vars[i], e.depth[i]);
#endif
  e.solver.reset(static_cast<uint32_t>(e.vars.size()),
                 SubSearchMode::Kitten);
  if (e.vars.size() < 2 || e.nclauses() < 2)
    return 0; // nothing to be equivalent TO
  k.stats.sweep_environments++;
  k.stats.sweep_env_clauses += e.nclauses();

  std::vector<SubLit> &buf = r.sub_scratch;
  for (uint32_t c = 0; c < e.nclauses(); c++) {
    buf.clear();
    for (uint32_t i = e.clause_off[c]; i < e.clause_off[c + 1]; i++) {
      const Lit l = e.clause_lits[i];
      buf.push_back(sub_lit(e.sub_of_var[var_of(l)], negated(l)));
    }
    const uint32_t id = e.solver.add_clause(buf);
    // The environment's own numbering is the sub-solver's, which is what makes
    // `clause_ids[id - 1]` the main-formula clause emission must cite.
    assert(id == c + 1);
    (void)id;
  }

  uint32_t found = 0;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  uint32_t query_ordinal = 0;
#endif
  std::vector<SubLit> &assumptions = r.assumptions;
  assumptions.clear();
  k.stats.sweep_solves++;
#ifdef TACTIX_ENABLE_EVENT_TRACE
  const uint64_t initial_ticks_before = e.solver.kitten_ticks();
#endif
  SubResult res = sweep_solve(e, assumptions, lim.env_ticks);
#ifdef TACTIX_ENABLE_EVENT_TRACE
  event_trace_sweep_query(k, seed, query_ordinal++, "initial", INVALID_LIT,
                          INVALID_LIT, static_cast<int>(res),
                          e.solver.kitten_ticks() - initial_ticks_before,
                          e.solver.conflicts());
  if (res == SubResult::Sat)
    event_trace_sweep_model(k, seed, query_ordinal - 1, e.vars.data(),
                            e.solver.model().data(), e.vars.size());
#endif
  if (res == SubResult::Budget) {
    k.stats.sweep_budget++;
    k.stats.sweep_ticks += e.solver.kitten_ticks();
    k.stats.sweep_ticks_sub += e.solver.kitten_ticks();
    return 0;
  }
  if (res == SubResult::Unsat) {
    // The environment alone refutes F. Turning that into `add-empty` is a
    // separate proof obligation nothing here discharges, so detection touches
    // nothing — the same reason there is no backbone half: those root units are
    // V1 Probe / V2 Backbone's proof delta.
    k.stats.sweep_env_unsat++;
    k.stats.sweep_ticks += e.solver.kitten_ticks();
    k.stats.sweep_ticks_sub += e.solver.kitten_ticks();
    return 0;
  }

  if (lim.full_partition) {
    const uint32_t found = detect_full_partition(
        k,
#ifdef TACTIX_ENABLE_EVENT_TRACE
        seed,
#endif
        lim, r, commit_inline
#ifdef TACTIX_ENABLE_EVENT_TRACE
        , query_ordinal
#endif
    );
    k.stats.sweep_ticks += e.solver.kitten_ticks();
    k.stats.sweep_ticks_sub += e.solver.kitten_ticks();
    // Detection after the initial solve reads only the embedded environment.
    // Commit its ordered units/equivalences once before the next main-formula
    // seed, where Kissat's connected-clause substitutions first become
    // observable.  Substituting after every pair forced Tactix's variable-width
    // arena bridge to collect and rebuild the whole formula hundreds of times.
    if (commit_inline)
      (void)sweep_emit_round(k, r, found);
    return found;
  }

  // Candidate selection: every environment literal agreeing with the seed in
  // the first model. Literals differing in any model cannot be equivalent, so
  // this is the cheapest sound over-approximation, shrunk by every further
  // model (`refine`).
  r.candidates.clear();
  for (uint32_t u = 1; u < e.vars.size(); u++) {
    const Var w = e.vars[u];
    if (root_fixed(k, w) || kernel_var_retired(k, w))
      continue;
    const Lit y =
        make_lit(w, e.solver.model()[u] != e.solver.model()[e.sub_of_var[seed]]);
    const Lit ry = repr_find(r, y);
    if (ry == seed_lit || ry == negate(seed_lit))
      continue; // this round already knows the answer for y
    r.candidates.push_back(y);
  }
  // The loop pops from the back and the seeding walk is in BFS discovery order,
  // so without this reversal every environment spends its queries on its
  // outermost variables, about which it can prove nothing.
  std::reverse(r.candidates.begin(), r.candidates.end());
  k.stats.sweep_cand_seeded += r.candidates.size();

  uint32_t solved = 0;
  while (!r.candidates.empty() &&
         (!lim.max_candidates || solved < lim.max_candidates)) {
    const Lit y = r.candidates.back();
    const Lit ry = repr_find(r, y);
    if (ry == seed_lit || ry == negate(seed_lit)) {
      // an earlier pair in this environment already settled y's class
      r.candidates.pop_back();
      continue;
    }
    if (binary_live(k, negate(seed_lit), y) &&
        binary_live(k, seed_lit, negate(y))) {
      // the formula already states this equivalence: nothing for Sweep to add
      k.stats.sweep_known++;
      repr_unite(k, r, seed_lit, y);
      r.candidates.pop_back();
      continue;
    }
    // The sub-solver keeps only the last solve's derivation, so direction 1's
    // must be copied out before direction 2's solve overwrites it.
    SweepPairProof pair;
    // Direction 1: x ∧ ¬y unsatisfiable ⇔ F ⊨ (¬x ∨ y).
    assumptions.clear();
    assumptions.push_back(sub_lit(e.sub_of_var[seed], false));
    assumptions.push_back(
        sub_lit(e.sub_of_var[var_of(y)], !negated(y)));
    solved++;
    k.stats.sweep_candidates++;
    k.stats.sweep_solves++;
    res = sweep_solve(e, assumptions, lim.env_ticks);
    if (res == SubResult::Budget) {
      k.stats.sweep_budget++;
      break;
    }
    if (res == SubResult::Sat) {
      k.stats.sweep_dir_sat++;
      k.stats.sweep_cand_refined +=
          refine(e, e.solver.model(), seed_lit, r.candidates);
      continue;
    }
    k.stats.sweep_dir_unsat++;
    assert(!e.solver.inconsistent() && "a satisfiable environment cannot then "
                                       "derive the empty clause");
#ifndef NDEBUG
    { // Emission depends on this: a refutation under assumptions must hand back a
      // derivation whose target clause is a sub-clause of the negated
      // assumptions. Checked here, where the assumptions are still in hand.
      SubDerivation d;
      assert(e.solver.derivation(d) && !d.steps.empty());
      for (const SubLit tl : d.steps.back().lits)
        assert(tl == sub_negate(assumptions[0]) ||
               tl == sub_negate(assumptions[1]));
    }
#endif
    sweep_capture_dir(e, pair.fwd);
    // Direction 2: ¬x ∧ y unsatisfiable ⇔ F ⊨ (x ∨ ¬y).
    assumptions.clear();
    assumptions.push_back(sub_lit(e.sub_of_var[seed], true));
    assumptions.push_back(sub_lit(e.sub_of_var[var_of(y)], negated(y)));
    solved++;
    k.stats.sweep_candidates++;
    k.stats.sweep_solves++;
    res = sweep_solve(e, assumptions, lim.env_ticks);
    if (res == SubResult::Budget) {
      k.stats.sweep_budget++;
      break;
    }
    if (res == SubResult::Sat) {
      k.stats.sweep_dir_sat++;
      k.stats.sweep_cand_refined +=
          refine(e, e.solver.model(), seed_lit, r.candidates);
      continue;
    }
    k.stats.sweep_dir_unsat++;
    assert(!e.solver.inconsistent());
    sweep_capture_dir(e, pair.rev);
    // Both directions refuted over env ⊆ F, so F ⊨ x ≡ y.
    r.found.push_back({std::min(seed_lit, y), std::max(seed_lit, y)});
    r.proofs.push_back(std::move(pair));
    k.stats.sweep_equivalences++;
    found++;
    repr_unite(k, r, seed_lit, y);
    r.candidates.pop_back();
  }
  k.stats.sweep_ticks += e.solver.kitten_ticks();
  k.stats.sweep_ticks_sub += e.solver.kitten_ticks();
  if (commit_inline)
    (void)sweep_emit_round(k, r, found);
  return found;
}

uint32_t sweep_detect_seed(Kernel &k, Var seed, const SweepLimits &lim,
                           SweepRound &r) {
  return sweep_process_seed(k, seed, lim, r, false);
}

uint32_t sweep_detect_round(Kernel &k, const SweepLimits &lim, SweepRound &r) {
  if (!sweep_guard(k))
    return 0;
  k.stats.sweep_rounds++;
  sweep_round_begin(k, r);
  sweep_seeds(k, r.seeds);
  const uint64_t start = sweep_semantic_effort(k);
  uint32_t found = 0, seeds = 0;
  for (const Var v : r.seeds) {
    if (lim.max_seeds && seeds >= lim.max_seeds)
      break;
    if (lim.round_ticks &&
        sweep_semantic_effort(k) - start > lim.round_ticks)
      break;
    seeds++;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const uint64_t before = sweep_semantic_effort(k);
    const uint64_t solves_before = k.stats.sweep_solves;
    const uint32_t external = static_cast<uint32_t>(
        kernel_externalize(k, make_lit(v, false)));
#endif
    SweepLimits seed_lim = lim;
    if (lim.round_ticks) {
      const uint64_t spent = sweep_semantic_effort(k) - start;
      const uint64_t remaining =
          spent < lim.round_ticks ? lim.round_ticks - spent : 1;
      if (!seed_lim.env_ticks || remaining < seed_lim.env_ticks)
        seed_lim.env_ticks = remaining;
    }
    found += sweep_detect_seed(k, v, seed_lim, r);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const uint64_t spent = sweep_semantic_effort(k) - before;
    if (spent)
      event_trace_sweep_effort(k, external, before,
                               sweep_semantic_effort(k),
                               k.stats.sweep_solves - solves_before);
#endif
  }
  return found;
}

namespace {
constexpr uint32_t SWEEP_FRONT_BIT = 1u << 31;

// Record the slot of every tail entry appended since the last call (the
// resumable build, or a test assigning `sweep_tail` directly). A variable that
// already has a live entry keeps it: only its first unconsumed copy is the one
// a reschedule removes, exactly as the former linear search found it.
void sweep_index_tail(Kernel &k) {
  for (uint32_t i = k.sweep_tail_indexed; i < k.sweep_tail.size(); i++) {
    const Var external = k.sweep_tail[i];
    if (external == INVALID_VAR)
      continue;
    if (k.sweep_slot.size() <= external)
      k.sweep_slot.resize(static_cast<size_t>(external) + 1, 0);
    if (!k.sweep_slot[external])
      k.sweep_slot[external] = i + 1;
    k.sweep_live++;
  }
  k.sweep_tail_indexed = static_cast<uint32_t>(k.sweep_tail.size());
}

// Pop the next live entry: the reschedule front first (last in, first out),
// then the tail in order. INVALID_VAR when nothing live remains.
Var sweep_take_next(Kernel &k) {
  while (!k.sweep_front.empty()) {
    const uint32_t j = static_cast<uint32_t>(k.sweep_front.size() - 1);
    const Var external = k.sweep_front.back();
    k.sweep_front.pop_back();
    if (external == INVALID_VAR)
      continue;
    if (external < k.sweep_slot.size() &&
        k.sweep_slot[external] == ((j | SWEEP_FRONT_BIT) + 1))
      k.sweep_slot[external] = 0;
    k.sweep_live--;
    return external;
  }
  while (k.sweep_tail_next < k.sweep_tail.size()) {
    const uint32_t i = k.sweep_tail_next++;
    const Var external = k.sweep_tail[i];
    if (external == INVALID_VAR)
      continue;
    if (external < k.sweep_slot.size() && k.sweep_slot[external] == i + 1)
      k.sweep_slot[external] = 0;
    k.sweep_live--;
    return external;
  }
  return INVALID_VAR;
}

// Move one representative to the pop end: tombstone its live entry, if any,
// and push it on the front. The last representative pushed is the next seed.
void sweep_reschedule_one(Kernel &k, Var external) {
  if (k.sweep_slot.size() <= external)
    k.sweep_slot.resize(static_cast<size_t>(external) + 1, 0);
  const uint32_t slot = k.sweep_slot[external];
  if (slot) {
    const uint32_t location = slot - 1;
    if (location & SWEEP_FRONT_BIT)
      k.sweep_front[location & ~SWEEP_FRONT_BIT] = INVALID_VAR;
    else
      k.sweep_tail[location] = INVALID_VAR;
    k.sweep_live--;
  }
  k.sweep_front.push_back(external);
  k.sweep_slot[external] =
      (static_cast<uint32_t>(k.sweep_front.size() - 1) | SWEEP_FRONT_BIT) + 1;
  k.sweep_live++;
}

void sweep_schedule_clear(Kernel &k) {
  k.sweep_tail.clear();
  k.sweep_tail.shrink_to_fit();
  k.sweep_front.clear();
  k.sweep_front.shrink_to_fit();
  std::vector<uint32_t>().swap(k.sweep_slot);
  k.sweep_tail_next = 0;
  k.sweep_tail_indexed = 0;
  k.sweep_live = 0;
}

// Kissat's `schedule_inner` moves each newly substituted equivalence
// representative to the pop end of the live schedule.  Since representatives
// are scheduled in discovery order, the last one becomes the next seed.
static void sweep_reschedule_representatives(
    Kernel &k, SweepRound &round,
    const std::vector<std::pair<Var, Var>> &external_pairs) {
  for (const auto &pair : external_pairs) {
    Var endpoints[2] = {INVALID_VAR, INVALID_VAR};
    bool resolved = true;
    for (uint32_t i = 0; i < 2 && resolved; i++) {
      const Var external = i ? pair.second : pair.first;
      resolved = external < k.external_to_internal.size();
      if (resolved) {
        endpoints[i] = k.external_to_internal[external];
        resolved = endpoints[i] != INVALID_VAR;
      }
    }
    if (!resolved)
      continue;
    // The rescheduled variable is the pair's REPRESENTATIVE, chosen by import
    // order exactly as `substitute_apply_sweep_equivalences` chooses it and as
    // Kissat's `sweep_equivalence_candidates` chooses `lit < other`.  Deriving
    // it instead from "the endpoint that survived retirement" silently declined
    // every pair whose substitution was skipped, where Kissat still schedules.
    const Var representative =
        kernel_import_var_order(k, endpoints[0]) <=
                kernel_import_var_order(k, endpoints[1])
            ? endpoints[0]
            : endpoints[1];
    // `schedule_inner` declines a variable that is no longer ACTIVE, and a
    // root-fixed variable is deactivated the moment it is assigned
    // (kissat flags.c `kissat_mark_fixed_literal`), not only a retired one.
    const Lit representative_lit = make_lit(representative, false);
    if (root_fixed(k, representative) ||
        kernel_var_retired(k, representative) ||
        repr_find(round, representative_lit) != representative_lit)
      continue;
    // Do not deduplicate: Kissat's `schedule_inner` is a move-to-pop-end for
    // every proved equivalence.  If A, B, A are discovered in that order, the
    // final move of A is observable and A—not B—is the next seed.
    sweep_reschedule_one(k, k.internal_to_external[representative]);
    k.stats.sweep_rescheduled++;
  }
}
} // namespace

Var sweep_next_scheduled(const Kernel &k) {
  for (size_t j = k.sweep_front.size(); j-- > 0;)
    if (k.sweep_front[j] != INVALID_VAR)
      return k.sweep_front[j];
  for (size_t i = k.sweep_tail_next; i < k.sweep_tail.size(); i++)
    if (k.sweep_tail[i] != INVALID_VAR)
      return k.sweep_tail[i];
  return INVALID_VAR;
}

static ApplyResult sweep_apply_resumable(Kernel &k, SweepLimits lim,
                                         SweepRound &r) {
  if (!sweep_guard(k))
    return ApplyResult::NoOp;
  k.stats.sweep_rounds++;
  sweep_completed_limits(k, lim);
  sweep_round_begin(k, r);

  sweep_index_tail(k);
  const bool retained = k.sweep_live > 0;
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  const uint64_t schedule_t0 = sweep_time ? now_nanos() : 0;
#endif
  if (!retained) {
    sweep_schedule_clear(k);
    sweep_seeds(k, r.seeds);
    for (const Var v : r.seeds)
      if (sweep_schedulable(k, v, lim))
        k.sweep_tail.push_back(k.internal_to_external[v]);
    k.stats.sweep_scheduled += k.sweep_tail.size();
    sweep_index_tail(k);
  }
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  if (sweep_time)
    r.time_schedule += now_nanos() - schedule_t0;
#endif

  const uint64_t start = sweep_semantic_effort(k);
  uint32_t seeds = 0;
  ApplyResult result = ApplyResult::NoOp;
  while (k.sweep_live > 0) {
    if (lim.max_seeds && seeds >= lim.max_seeds)
      break;
    if (lim.round_ticks &&
        sweep_semantic_effort(k) - start > lim.round_ticks)
      break;
    const Var external = sweep_take_next(k);
    if (external == INVALID_VAR)
      break;
    if (external >= k.external_to_internal.size())
      continue;
    const Var v = k.external_to_internal[external];
    if (v == INVALID_VAR || root_fixed(k, v) || kernel_var_retired(k, v))
      continue;
    if (retained)
      k.stats.sweep_resumed++;
    seeds++;
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const uint64_t before = sweep_semantic_effort(k);
    const uint64_t solves_before = k.stats.sweep_solves;
#endif
    SweepLimits seed_lim = lim;
    if (lim.round_ticks) {
      const uint64_t spent = sweep_semantic_effort(k) - start;
      const uint64_t remaining =
          spent < lim.round_ticks ? lim.round_ticks - spent : 1;
      if (!seed_lim.env_ticks || remaining < seed_lim.env_ticks)
        seed_lim.env_ticks = remaining;
    }
    const uint64_t emissions_before = r.emission_batches;
#ifdef TACTIX_ENABLE_CONFLICT_STOP
    const uint64_t process_t0 = sweep_time ? now_nanos() : 0;
#endif
    (void)sweep_process_seed(k, v, seed_lim, r, true);
#ifdef TACTIX_ENABLE_CONFLICT_STOP
    if (sweep_time)
      r.time_process += now_nanos() - process_t0;
    const uint64_t post_seed_t0 = sweep_time ? now_nanos() : 0;
#endif
#ifdef TACTIX_ENABLE_EVENT_TRACE
    const uint64_t spent = sweep_semantic_effort(k) - before;
    if (spent)
      event_trace_sweep_effort(k, external + 1, before,
                               sweep_semantic_effort(k),
                               k.stats.sweep_solves - solves_before);
#endif
    std::vector<std::pair<Var, Var>> external_pairs;
    external_pairs.reserve(r.found.size());
    for (const SweepEquiv &e : r.found)
      external_pairs.emplace_back(k.internal_to_external[var_of(e.a)],
                                  k.internal_to_external[var_of(e.b)]);
    if (k.inconsistent)
      return ApplyResult::Unsat;
    // Kissat closes the dense root trail after every swept environment
    // (`sweep_variable` calls `kissat_dense_propagate` after releasing its
    // local sweeper).  Those consequences are part of the live formula seen
    // by the next environment: fixed variables leave its BFS and falsified
    // literals are stripped from rewritten clauses.  Deferring this drain to
    // the end of the whole Sweep pass changes both structural caps and the
    // candidate partition, rather than merely batching equivalent work.
    if (inprocess_reach_root(k))
      return ApplyResult::Unsat;
    if (r.emission_batches != emissions_before) {
      result = ApplyResult::Ok;
      sweep_reschedule_representatives(k, r, external_pairs);
    }
    r.found.clear();
    r.proofs.clear();
    r.units.clear();
    r.units_emitted = 0;
    r.equivalences_emitted = 0;
#ifdef TACTIX_ENABLE_CONFLICT_STOP
    if (sweep_time)
      r.time_post_seed += now_nanos() - post_seed_t0;
#endif
  }
  if (k.sweep_live == 0) {
    sweep_schedule_clear(k);
    k.sweep_passes_completed++;
    k.stats.sweep_completed++;
  }
  // Each connected rewrite above is incremental.  Cross the variable-width
  // arena bridge once, after every reference-ordered environment in this call
  // has observed its predecessor's consequences.  Charging here records the
  // real host cost without letting that representation detail postpone the
  // next semantic Sweep transaction.
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  const uint64_t collect_t0 = sweep_time ? now_nanos() : 0;
#endif
  k.stats.sweep_ticks_repr += r.substitution.collect(k);
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  if (sweep_time)
    r.time_collect += now_nanos() - collect_t0;
#endif
  // The per-environment drains above normally leave no work.  Keep the final
  // closure for the unproductive/early-exit tail and as a lifecycle guard.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  return result;
}

SweepLimits sweep_limits_from_env(SweepLimits lim) {
  const auto u32 = [](const char *name, uint32_t &slot) {
    if (const char *s = std::getenv(name))
      slot = static_cast<uint32_t>(std::strtoul(s, nullptr, 10));
  };
  const auto u64 = [](const char *name, uint64_t &slot) {
    if (const char *s = std::getenv(name))
      slot = std::strtoull(s, nullptr, 10);
  };
  u32("TACTIX_SWEEP_DEPTH", lim.max_depth);
  u32("TACTIX_SWEEP_VARS", lim.max_env_vars);
  u32("TACTIX_SWEEP_CLAUSES", lim.max_env_clauses);
  u32("TACTIX_SWEEP_CANDIDATES", lim.max_candidates);
  u32("TACTIX_SWEEP_FLIP_ROUNDS", lim.flip_rounds);
  u32("TACTIX_SWEEP_SEEDS", lim.max_seeds);
  u64("TACTIX_SWEEP_TICKS", lim.env_ticks);
  u64("TACTIX_SWEEP_ROUND_TICKS", lim.round_ticks);
  if (const char *s = std::getenv("TACTIX_SWEEP_STRIP"))
    lim.strip_false = std::strtoul(s, nullptr, 10) != 0;
  if (const char *s = std::getenv("TACTIX_SWEEP_CLOSURE"))
    lim.measure_closure = std::strtoul(s, nullptr, 10) != 0;
  if (const char *s = std::getenv("TACTIX_SWEEP_PARTITION"))
    lim.full_partition = std::strtoul(s, nullptr, 10) != 0;
  if (const char *s = std::getenv("TACTIX_SWEEP_RESUME"))
    lim.resume = std::strtoul(s, nullptr, 10) != 0;
  return lim;
}

// ---- emission: the RUP fast path (`docs/calculus.md` V5, strategy (a)) -----
// Entailment is not a proof line, so the main formula is asked the same
// question in the one form a checker accepts, via Congruence's replay
// primitive. A non-RUP equivalence is declined (`sweep_not_rup`) and the
// general path below covers it; both directions must replay or the pair is
// skipped whole. The replay runs under DRAT too, where no chain is needed: it
// is the RUP test itself, and an unchecked line breaches N2.
static bool sweep_emit_equiv(Kernel &k, Lit a, Lit b,
                             std::vector<uint64_t> &chain_fwd,
                             std::vector<uint64_t> &chain_rev) {
  if (a == b || a == negate(b))
    return false; // degenerate: nothing to state, and not emittable
  const Lit fwd[2] = {negate(a), b};
  const Lit rev[2] = {a, negate(b)};
  if (!congruence_replay_rup(k, fwd, 2, chain_fwd, RupReplay::Decide))
    return false;
  if (!congruence_replay_rup(k, rev, 2, chain_rev, RupReplay::Decide))
    return false;
  const bool lrat = k.proof && k.proof->lrat();
  auto add = [&](const Lit *l, const std::vector<uint64_t> &ch) {
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, l, 2, lrat ? ch.data() : nullptr,
                       lrat ? static_cast<uint32_t>(ch.size()) : 0);
    kernel_add_binary_clause(k, l[0], l[1], id);
  };
  add(fwd, chain_fwd);
  add(rev, chain_rev);
  k.stats.sweep_binaries += 2;
  return true;
}

// Emission, the general path (`docs/calculus.md` V5, strategy (b)): replay the
// sub-solver's refutation into the main proof, each lemma its own line in
// derivation order, antecedents translated to main-formula proof ids. The final
// lemma is the target binary, by which point the checker has seen every
// antecedent it names. Anything not mappable exactly declines rather than
// emits. `dry` validates without emitting or consuming an id, so a pair is
// checked whole before either half is written.
enum class SweepGeneralDecline : uint8_t {
  None,
  CaptureMissing,
  StripShape,
  SubVariable,
  EmptyStep,
  TautologicalStep,
  AntecedentMissing,
  FixedTarget,
  FixedUnitId,
  FinalShape,
  FinalTarget,
};

enum class SweepPairDecline : uint8_t {
  None,
  ProofIndexMissing,
  ForwardCaptureMissing,
  ReverseCaptureMissing,
  TargetMismatch,
};

struct SweepPairSelection {
  SweepPairDecline decline = SweepPairDecline::None;
  const SweepDirProof *forward = nullptr;
  const SweepDirProof *reverse = nullptr;
  const SweepPairProof *captured = nullptr;
  bool unit_consequences = false;
  bool eligible() const { return decline == SweepPairDecline::None; }
};

static bool sweep_proof_is_pair(const SweepDirProof &proof, Lit a, Lit b) {
  return (proof.t0 == a && proof.t1 == b) ||
         (proof.t0 == b && proof.t1 == a);
}

static SweepPairSelection sweep_select_pair(const SweepRound &round,
                                            size_t index, Lit a, Lit b) {
  SweepPairSelection result;
  if (index >= round.proofs.size()) {
    result.decline = SweepPairDecline::ProofIndexMissing;
    return result;
  }
  result.captured = &round.proofs[index];
  const SweepPairProof &proof = *result.captured;
  if (!proof.fwd.ok) {
    result.decline = SweepPairDecline::ForwardCaptureMissing;
    return result;
  }
  if (!proof.rev.ok) {
    result.decline = SweepPairDecline::ReverseCaptureMissing;
    return result;
  }
  const Lit f0 = negate(a), f1 = b, r0 = a, r1 = negate(b);
  if (sweep_proof_is_pair(proof.fwd, f0, f1) &&
      sweep_proof_is_pair(proof.rev, r0, r1)) {
    result.forward = &proof.fwd;
    result.reverse = &proof.rev;
  } else if (sweep_proof_is_pair(proof.rev, f0, f1) &&
             sweep_proof_is_pair(proof.fwd, r0, r1)) {
    result.forward = &proof.rev;
    result.reverse = &proof.fwd;
  } else {
    // A refuted direction can simplify all the way to a unit when the local
    // environment fixes that endpoint. If the two captured units fix the two
    // endpoints consistently, they are stronger than the requested
    // equivalence and are the real semantic consequences to commit.
    const auto endpoint_value = [&](Lit unit, Lit endpoint) {
      return unit == endpoint ? 1 : unit == negate(endpoint) ? -1 : 0;
    };
    if (proof.fwd.t1 == INVALID_LIT && proof.rev.t1 == INVALID_LIT) {
      const int fwd_a = endpoint_value(proof.fwd.t0, a);
      const int fwd_b = endpoint_value(proof.fwd.t0, b);
      const int rev_a = endpoint_value(proof.rev.t0, a);
      const int rev_b = endpoint_value(proof.rev.t0, b);
      const int value_a = fwd_a ? fwd_a : rev_a;
      const int value_b = fwd_b ? fwd_b : rev_b;
      if (value_a && value_b && value_a == value_b &&
          (fwd_a != 0) != (rev_a != 0)) {
        result.forward = &proof.fwd;
        result.reverse = &proof.rev;
        result.unit_consequences = true;
        return result;
      }
    }
    result.decline = SweepPairDecline::TargetMismatch;
  }
  return result;
}

#ifdef TACTIX_ENABLE_RULE_JOURNAL
static const char *sweep_pair_decline_name(SweepPairDecline decline) {
  switch (decline) {
  case SweepPairDecline::None:
    return "none";
  case SweepPairDecline::ProofIndexMissing:
    return "proof-index-missing";
  case SweepPairDecline::ForwardCaptureMissing:
    return "forward-capture-missing";
  case SweepPairDecline::ReverseCaptureMissing:
    return "reverse-capture-missing";
  case SweepPairDecline::TargetMismatch:
    return "captured-target-mismatch";
  }
  return "unknown";
}
#endif

struct SweepGeneralStepPlan {
  uint32_t sub_id = 0;
  std::vector<Lit> lits;
  std::vector<uint64_t> strip_ids;
  std::vector<uint32_t> antecedents;
};

struct SweepGeneralPlan {
  SweepGeneralDecline decline = SweepGeneralDecline::None;
  uint32_t step = 0;
  uint32_t fact = 0;
  Lit t0 = INVALID_LIT, t1 = INVALID_LIT;
  uint64_t formula_epoch = 0;
  std::vector<uint64_t> source_ids;
  std::vector<SweepGeneralStepPlan> steps;
  bool eligible() const { return decline == SweepGeneralDecline::None; }
};

#ifdef TACTIX_ENABLE_RULE_JOURNAL
static const char *sweep_general_decline_name(SweepGeneralDecline decline) {
  switch (decline) {
  case SweepGeneralDecline::None:
    return "none";
  case SweepGeneralDecline::CaptureMissing:
    return "capture-missing";
  case SweepGeneralDecline::StripShape:
    return "strip-shape";
  case SweepGeneralDecline::SubVariable:
    return "sub-variable-out-of-range";
  case SweepGeneralDecline::EmptyStep:
    return "empty-step";
  case SweepGeneralDecline::TautologicalStep:
    return "tautological-step";
  case SweepGeneralDecline::AntecedentMissing:
    return "antecedent-missing";
  case SweepGeneralDecline::FixedTarget:
    return "fixed-target-mismatch";
  case SweepGeneralDecline::FixedUnitId:
    return "fixed-unit-id-missing";
  case SweepGeneralDecline::FinalShape:
    return "final-shape";
  case SweepGeneralDecline::FinalTarget:
    return "final-target-mismatch";
  }
  return "unknown";
}
#endif

// The single eligibility evaluator for captured Sweep proofs. Its structured
// plan controls execution and is also serialized by the rule journal; emission
// below does not reconstruct or repeat the guards.
static SweepGeneralPlan sweep_plan_general(const Kernel &k,
                                           const SweepDirProof &p,
                                           const Lit t0, const Lit t1,
                                           const Lit fixed = INVALID_LIT) {
  SweepGeneralPlan plan;
  plan.t0 = t0;
  plan.t1 = t1;
  plan.formula_epoch = p.formula_epoch;
  plan.source_ids = p.orig_ids;
  const auto decline = [&](SweepGeneralDecline reason, uint32_t step = 0,
                           uint32_t fact = 0) {
    plan.decline = reason;
    plan.step = step;
    plan.fact = fact;
    plan.steps.clear();
    return plan;
  };
  if (!p.ok || p.steps.empty())
    return decline(SweepGeneralDecline::CaptureMissing, 0,
                   static_cast<uint32_t>(p.steps.size()));
  if (p.strip_off.size() != p.orig_ids.size() + 1)
    return decline(SweepGeneralDecline::StripShape, 0,
                   static_cast<uint32_t>(p.strip_off.size()));
  const bool lrat = k.proof && k.proof->lrat();
  uint32_t max_id = static_cast<uint32_t>(p.orig_ids.size());
  for (const SubStep &step : p.steps)
    max_id = std::max(max_id, step.id);
  std::vector<uint8_t> named(static_cast<size_t>(max_id) + 1, 0);
  for (size_t i = 0; i < p.orig_ids.size(); i++)
    named[i + 1] = p.orig_ids[i] != 0;

  for (size_t si = 0; si < p.steps.size(); si++) {
    const SubStep &step = p.steps[si];
    SweepGeneralStepPlan translated;
    translated.sub_id = step.id;
    for (const SubLit sub_lit : step.lits) {
      if (sub_var_of(sub_lit) >= p.vars.size())
        return decline(SweepGeneralDecline::SubVariable,
                       static_cast<uint32_t>(si), sub_var_of(sub_lit));
      translated.lits.push_back(sweep_kernel_lit(p.vars, sub_lit));
    }
    if (translated.lits.empty())
      return decline(SweepGeneralDecline::EmptyStep,
                     static_cast<uint32_t>(si));
    if (Proof::tautological(translated.lits.data(),
                           static_cast<uint32_t>(translated.lits.size())))
      return decline(SweepGeneralDecline::TautologicalStep,
                     static_cast<uint32_t>(si));
    for (const uint32_t antecedent : step.chain) {
      if (antecedent == 0 || antecedent > p.orig_ids.size())
        continue;
      for (uint32_t s = p.strip_off[antecedent - 1];
           s < p.strip_off[antecedent]; s++) {
        const uint64_t unit_id = p.strip_ids[s];
        if (std::find(translated.strip_ids.begin(),
                      translated.strip_ids.end(), unit_id) ==
            translated.strip_ids.end())
          translated.strip_ids.push_back(unit_id);
      }
    }
    for (const uint32_t antecedent : step.chain) {
      if (antecedent >= named.size() || !named[antecedent])
        return decline(SweepGeneralDecline::AntecedentMissing,
                       static_cast<uint32_t>(si), antecedent);
      translated.antecedents.push_back(antecedent);
    }
    if (si + 1 == p.steps.size()) {
      const bool unit = t1 == INVALID_LIT;
      if (unit) {
        std::vector<Lit> remaining;
        std::vector<uint64_t> fixed_ids;
        bool satisfied = false, saw_fixed = fixed == INVALID_LIT;
        for (const Lit lit : translated.lits) {
          const int8_t value = k.value(lit);
          if (value > 0) {
            satisfied = true;
            break;
          }
          if (!value) {
            remaining.push_back(lit);
            continue;
          }
          if (fixed != INVALID_LIT && lit == negate(fixed))
            saw_fixed = true;
          const uint64_t unit_id = k.unit_ids[var_of(lit)];
          if (lrat && unit_id == 0)
            return decline(SweepGeneralDecline::FixedUnitId,
                           static_cast<uint32_t>(si), var_of(lit));
          if (lrat &&
              std::find(fixed_ids.begin(), fixed_ids.end(), unit_id) ==
                  fixed_ids.end())
            fixed_ids.push_back(unit_id);
        }
        if (satisfied || !saw_fixed)
          return decline(SweepGeneralDecline::FixedTarget,
                         static_cast<uint32_t>(si));
        if (remaining.size() != 1)
          return decline(SweepGeneralDecline::FinalShape,
                         static_cast<uint32_t>(si),
                         static_cast<uint32_t>(remaining.size()));
        if (remaining[0] != t0)
          return decline(SweepGeneralDecline::FinalTarget,
                         static_cast<uint32_t>(si));
        translated.strip_ids.insert(translated.strip_ids.begin(),
                                    fixed_ids.begin(), fixed_ids.end());
        translated.lits.assign(1, t0);
      } else {
        if (fixed != INVALID_LIT || translated.lits.size() != 2)
          return decline(SweepGeneralDecline::FinalShape,
                         static_cast<uint32_t>(si),
                         static_cast<uint32_t>(translated.lits.size()));
        const bool same =
            (translated.lits[0] == t0 && translated.lits[1] == t1) ||
            (translated.lits[0] == t1 && translated.lits[1] == t0);
        if (!same)
          return decline(SweepGeneralDecline::FinalTarget,
                         static_cast<uint32_t>(si));
      }
    }
    if (step.id < named.size())
      named[step.id] = 1;
    plan.steps.push_back(std::move(translated));
  }
  return plan;
}

static void sweep_apply_general_plan(Kernel &k,
                                     const SweepGeneralPlan &plan) {
  assert(plan.eligible() && !plan.steps.empty());
  const bool lrat = k.proof && k.proof->lrat();
  uint32_t max_id = static_cast<uint32_t>(plan.source_ids.size());
  for (const SweepGeneralStepPlan &step : plan.steps)
    max_id = std::max(max_id, step.sub_id);
  std::vector<uint64_t> id_of(static_cast<size_t>(max_id) + 1, 0);
  for (size_t i = 0; i < plan.source_ids.size(); i++)
    id_of[i + 1] = plan.source_ids[i];
  std::vector<uint64_t> chain;
  uint64_t last_id = 0;
  for (const SweepGeneralStepPlan &step : plan.steps) {
    chain = step.strip_ids;
    if (lrat)
      k.stats.sweep_strip_cited += step.strip_ids.size();
    for (const uint32_t antecedent : step.antecedents) {
      assert(antecedent < id_of.size() && id_of[antecedent]);
      chain.push_back(id_of[antecedent]);
    }
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, step.lits.data(),
                       static_cast<uint32_t>(step.lits.size()),
                       lrat ? chain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(chain.size()) : 0);
    if (step.sub_id < id_of.size())
      id_of[step.sub_id] = id;
    last_id = id;
  }
  if (plan.t1 == INVALID_LIT) {
    k.unit_ids[var_of(plan.t0)] = last_id;
    kernel_assign_unit(k, plan.t0);
    k.stats.sweep_units++;
  } else {
    kernel_add_binary_clause(k, plan.t0, plan.t1, last_id);
    k.stats.sweep_binaries++;
  }
}

#ifdef TACTIX_ENABLE_RULE_JOURNAL
static std::string sweep_journal_candidate(const Kernel &k,
                                           const SweepGeneralPlan &plan) {
  std::string result = std::to_string(kernel_externalize(k, plan.t0));
  if (plan.t1 != INVALID_LIT)
    result += "," + std::to_string(kernel_externalize(k, plan.t1));
  return result;
}

static bool begin_journal_sweep_general(const Kernel &k,
                                        const SweepGeneralPlan &plan,
                                        uint64_t &pre_digest) {
  if (!rule_journal_wants(k, "Sweep", sweep_journal_candidate(k, plan)))
    return false;
  pre_digest = rule_journal_digest(k);
  return true;
}

static void journal_sweep_general(const Kernel &k,
                                  const SweepGeneralPlan &plan,
                                  uint64_t pre_digest, const char *outcome,
                                  const char *decline,
                                  const char *deltas) {
  RuleJournalRecord record;
  record.rule = "Sweep";
  record.event = "consequence-commit";
  record.candidate = sweep_journal_candidate(k, plan);
  record.trigger = "embedded-refutation";
  record.context = "path=general-core;proof=" +
                   std::to_string(k.proof != nullptr) +
                   ";level=" + std::to_string(k.level);
  record.formula_epoch = plan.formula_epoch;
  record.root_epoch = k.touch_fixed_head;
  record.pre_digest = pre_digest;
  record.guards =
      "capture=" + std::to_string(plan.decline !=
                                  SweepGeneralDecline::CaptureMissing) +
      ";strip_shape=" +
      std::to_string(plan.decline != SweepGeneralDecline::StripShape) +
      ";step=" + std::to_string(plan.step) +
      ";fact=" + std::to_string(plan.fact) +
      ";steps=" + std::to_string(plan.steps.size());
  record.antecedents = "source_clauses=" +
                       std::to_string(plan.source_ids.size());
  record.outcome = outcome;
  record.decline = decline;
  record.deltas = deltas;
  record.witnesses = "kitten-core";
  record.effort = k.stats.sweep_ticks_sub;
  record.budget = 0;
  record.post_digest = rule_journal_digest(k);
  rule_journal_emit(k, record);
}

static std::string sweep_proof_target(const Kernel &k,
                                      const SweepDirProof &proof) {
  if (!proof.ok)
    return "missing";
  std::string result = std::to_string(kernel_externalize(k, proof.t0));
  if (proof.t1 != INVALID_LIT)
    result += "," + std::to_string(kernel_externalize(k, proof.t1));
  return result;
}

static void journal_sweep_pair_selection(const Kernel &k, Lit a, Lit b,
                                         const SweepPairSelection &selection) {
  RuleJournalRecord record;
  record.rule = "Sweep";
  record.event = "consequence-select";
  record.candidate = std::to_string(kernel_externalize(k, a)) + "," +
                     std::to_string(kernel_externalize(k, b));
  if (!rule_journal_wants(k, "Sweep", record.candidate))
    return;
  record.trigger = "proved-equivalence";
  record.context = "proof_index=present";
  record.formula_epoch = selection.captured
                             ? selection.captured->fwd.formula_epoch
                             : 0;
  record.root_epoch = k.touch_fixed_head;
  record.pre_digest = rule_journal_digest(k);
  record.guards =
      "proof_index=" + std::to_string(selection.captured != nullptr) +
      ";fwd=" +
      (selection.captured ? sweep_proof_target(k, selection.captured->fwd)
                          : "missing") +
      ";rev=" +
      (selection.captured ? sweep_proof_target(k, selection.captured->rev)
                          : "missing") +
      ";unit_consequences=" +
      std::to_string(selection.unit_consequences);
  record.antecedents = "kitten-captures";
  record.outcome = selection.eligible() ? "selected" : "declined";
  record.decline = sweep_pair_decline_name(selection.decline);
  record.deltas = "-";
  record.witnesses = "captured-targets";
  record.effort = k.stats.sweep_ticks_sub;
  record.post_digest = rule_journal_digest(k);
  rule_journal_emit(k, record);
}
#endif

static bool sweep_emit_unit(Kernel &k, Lit unit) {
  if (k.value(unit) > 0)
    return false;
  if (k.value(unit) < 0)
    return false; // the caller's consistent environment cannot prove this
  std::vector<uint64_t> chain;
  // Admission, exactly as in `sweep_emit_equiv`: Kitten's refutation is the
  // semantic certificate, and this asks the separate, format-independent
  // question of whether a checker can replay the line.
  if (!congruence_replay_rup(k, &unit, 1, chain, RupReplay::Decide))
    return false;
  const uint64_t id = ++k.next_clause_id;
  if (k.proof) {
    const bool lrat = k.proof->lrat();
    k.proof->add_rup(lrat ? id : 0, &unit, 1,
                     lrat ? chain.data() : nullptr,
                     lrat ? static_cast<uint32_t>(chain.size()) : 0);
  }
  k.unit_ids[var_of(unit)] = id;
  kernel_assign_unit(k, unit);
  k.stats.sweep_units++;
  return true;
}

// A successful Kitten refutation is the semantic certificate used by Kissat's
// Sweep implementation.  When no external proof is requested, commit that
// consequence directly: replaying it through the main propagator is redundant
// and can decline valid consequences merely because its bounded traversal does
// not reconstruct the embedded refutation.  Proof-producing runs still take
// one of the checked translation paths below.
static bool sweep_commit_unit_without_proof(Kernel &k, Lit unit) {
  if (k.proof || k.value(unit) != 0)
    return false;
  const uint64_t id = ++k.next_clause_id;
  k.unit_ids[var_of(unit)] = id;
  kernel_assign_unit(k, unit);
  k.stats.sweep_units++;
  return true;
}

static bool sweep_commit_equiv_without_proof(Kernel &k, Lit a, Lit b) {
  if (k.proof || a == b || a == negate(b) || k.value(a) != 0 ||
      k.value(b) != 0)
    return false;
  const Lit fwd[2] = {negate(a), b};
  const Lit rev[2] = {a, negate(b)};
  kernel_add_binary_clause(k, fwd[0], fwd[1], ++k.next_clause_id);
  kernel_add_binary_clause(k, rev[0], rev[1], ++k.next_clause_id);
  k.stats.sweep_binaries += 2;
  return true;
}

namespace {
ApplyResult sweep_emit_pending_units(Kernel &k, SweepRound &round) {
  bool any = false;
  const bool force_general =
      std::getenv("TACTIX_SWEEP_FORCE_GENERAL") != nullptr;
  while (round.units_emitted < round.units.size()) {
    const SweepUnitProof &unit = round.units[round.units_emitted++];
    if (k.value(unit.unit) != 0)
      continue;
    bool emitted =
        !force_general && sweep_commit_unit_without_proof(k, unit.unit);
    if (!emitted) {
      const SweepGeneralPlan plan =
          sweep_plan_general(k, unit.proof, unit.unit, INVALID_LIT);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      uint64_t pre_digest = 0;
      const bool journal_plan =
          begin_journal_sweep_general(k, plan, pre_digest);
#endif
      if (plan.eligible()) {
        sweep_apply_general_plan(k, plan);
        emitted = true;
      }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (journal_plan)
        journal_sweep_general(
            k, plan, pre_digest, emitted ? "applied" : "declined",
            emitted ? "none" : sweep_general_decline_name(plan.decline),
            emitted ? "unit" : "-");
#endif
    }
    if (!emitted)
      emitted = sweep_emit_unit(k, unit.unit);
    if (emitted) {
      any = true;
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_unit(k, unit.unit);
#endif
    }
  }
  return any ? ApplyResult::Ok : ApplyResult::NoOp;
}

// Emission, shared by the canonical round and the per-seed instance so the two
// cannot drift in what they are willing to write.
ApplyResult sweep_emit_round(Kernel &k, SweepRound &round, uint32_t found) {
  (void)found;
  if (round.units_emitted >= round.units.size() &&
      round.equivalences_emitted >= round.found.size())
    return ApplyResult::NoOp;
  // A refuted environment means F itself is UNSAT; detection records it and the
  // caller re-derives it through the normal root path rather than here, so this
  // apply stays a pure clause-addition (proof delta: two `derive`s per pair).
  if (k.inconsistent)
    return ApplyResult::Unsat;
  // TACTIX_SWEEP_FORCE_GENERAL: make the RUP fast path decline so every proved
  // equivalence routes through the general path. A coverage knob, not a dosing
  // one — both paths justify the same two binaries. The general path otherwise
  // fires too rarely on fuzz-scale formulas for a green leg to mean anything
  // (`benchmarks/reports/2026-08-05-sweep-se-polarity-fix.md`).
  const bool force_general = std::getenv("TACTIX_SWEEP_FORCE_GENERAL") != nullptr;
  std::vector<uint64_t> chain_fwd, chain_rev;
  std::vector<std::pair<Lit, Lit>> emitted_equivalences;
  bool any = sweep_emit_pending_units(k, round) == ApplyResult::Ok;
  while (round.equivalences_emitted < round.found.size()) {
    const size_t idx = round.equivalences_emitted++;
    const SweepEquiv &e = round.found[idx];
    if (k.level != 0 || k.conflict_valid)
      break; // replay needs a quiescent root; stop rather than assert
    // Units are committed before equivalences, as in Kissat.  If one member
    // of a proved class became fixed in that first batch, the class merge is
    // a unit on the other member, not a reason to discard the consequence.
    // The proof-producing path keeps using its translated cores below; the
    // direct path mirrors Kissat's trusted Kitten-core commitment.
    const int8_t av = k.value(e.a), bv = k.value(e.b);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    event_trace_sweep_commit(k, e.a, e.b, av, bv, "entry");
#endif
    if (!k.proof && (av || bv)) {
      bool unit_emitted = false;
      if (!av && bv)
        unit_emitted = sweep_commit_unit_without_proof(
            k, bv > 0 ? e.a : negate(e.a));
      else if (av && !bv)
        unit_emitted = sweep_commit_unit_without_proof(
            k, av > 0 ? e.b : negate(e.b));
      if (unit_emitted) {
        any = true;
#ifdef TACTIX_ENABLE_EVENT_TRACE
        const Lit unit = !av ? (bv > 0 ? e.a : negate(e.a))
                             : (av > 0 ? e.b : negate(e.b));
        event_trace_sweep_unit(k, unit);
#endif
      }
      if ((!av || !bv) || av == bv) {
        any = true;
        emitted_equivalences.emplace_back(e.a, e.b);
#ifdef TACTIX_ENABLE_EVENT_TRACE
        // Kissat reports the proved equivalence before root-fixed commitment.
        // Keep the semantic event stream complete when this early branch
        // retains the same consequence for the substitution batch.
        event_trace_sweep_equivalence(k, e.a, e.b);
#endif
      }
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_commit(k, e.a, e.b, av, bv,
                               unit_emitted ? "fixed-no-proof-unit"
                                            : "fixed-no-proof-no-unit");
#endif
      continue;
    }
    // Detection excludes fixed variables, but a unit proved earlier in this
    // environment can fix one endpoint before this pair is emitted. Collapse
    // the captured implication in that direction directly to the partner unit,
    // matching Kissat's unit batch without installing an intermediate binary.
    // Kissat commits the captured Kitten core without propagating the main
    // formula.  Prefer the same path; the main-formula RUP replay is retained
    // only as a proof-safe fallback for a bounded or untranslatable capture.
    if (e.a == e.b || e.a == negate(e.b)) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_commit(k, e.a, e.b, av, bv, "degenerate");
#endif
      continue;
    }
    if (k.proof && (av || bv)) {
      if (av && bv) {
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_sweep_commit(k, e.a, e.b, av, bv, "fixed-proof-both");
#endif
        continue;
      }
      const Lit fixed = av ? (av > 0 ? e.a : negate(e.a))
                           : (bv > 0 ? e.b : negate(e.b));
      const Lit unit = av ? (av > 0 ? e.b : negate(e.b))
                          : (bv > 0 ? e.a : negate(e.a));
      const Lit need0 = negate(fixed), need1 = unit;
      const SweepDirProof *proof = nullptr;
      Lit proof_fixed = INVALID_LIT;
      if (idx < round.proofs.size()) {
        const SweepPairProof &pp = round.proofs[idx];
        // Kitten can simplify the implication to a unit when the environment
        // itself already fixes the partner. That unit core is stronger than
        // the binary implication and must be selected before looking for the
        // unsimplified target.
        if (pp.fwd.ok && pp.fwd.t0 == unit && pp.fwd.t1 == INVALID_LIT)
          proof = &pp.fwd;
        else if (pp.rev.ok && pp.rev.t0 == unit &&
                 pp.rev.t1 == INVALID_LIT)
          proof = &pp.rev;
        else if (sweep_proof_is_pair(pp.fwd, need0, need1)) {
          proof = &pp.fwd;
          proof_fixed = fixed;
        } else if (sweep_proof_is_pair(pp.rev, need0, need1)) {
          proof = &pp.rev;
          proof_fixed = fixed;
        }
      }
      SweepGeneralPlan plan;
      if (proof)
        plan =
            sweep_plan_general(k, *proof, unit, INVALID_LIT, proof_fixed);
      else {
        plan.decline = SweepGeneralDecline::CaptureMissing;
        plan.t0 = unit;
        plan.t1 = INVALID_LIT;
      }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      uint64_t pre_digest = 0;
      const bool journal_plan =
          begin_journal_sweep_general(k, plan, pre_digest);
#endif
      bool emitted = plan.eligible();
      if (emitted)
        sweep_apply_general_plan(k, plan);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      if (journal_plan)
        journal_sweep_general(
            k, plan, pre_digest, emitted ? "applied" : "declined",
            emitted ? "none" : sweep_general_decline_name(plan.decline),
            emitted ? "unit-from-fixed-equivalence" : "-");
#endif
      if (!emitted)
        emitted = sweep_emit_unit(k, unit);
      if (emitted) {
        any = true;
#ifdef TACTIX_ENABLE_EVENT_TRACE
        event_trace_sweep_unit(k, unit);
#endif
      }
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_commit(k, e.a, e.b, av, bv,
                               emitted ? "fixed-proof-unit"
                                       : "fixed-proof-declined");
#endif
      continue;
    }
    if (kernel_binary_live(k, negate(e.a), e.b) &&
        kernel_binary_live(k, e.a, negate(e.b))) {
      k.stats.sweep_known++;
      any = true;
      emitted_equivalences.emplace_back(e.a, e.b);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_equivalence(k, e.a, e.b);
      event_trace_sweep_commit(k, e.a, e.b, av, bv, "known");
#endif
      continue;
    }
    // The proof paths below may publish the pair immediately.  Prime only
    // these four connected streams first so a fresh binary is appended after
    // their pre-existing mixed occurrences, matching Kissat without an eager
    // all-literals ledger build.
    round.substitution.prime_equivalence(k, e.a, e.b);
    const SweepPairSelection selection = sweep_select_pair(round, idx, e.a, e.b);
    bool emitted =
        !force_general && sweep_commit_equiv_without_proof(k, e.a, e.b);
#ifdef TACTIX_ENABLE_EVENT_TRACE
    if (emitted)
      event_trace_sweep_commit(k, e.a, e.b, av, bv, "direct");
#endif
#ifdef TACTIX_ENABLE_RULE_JOURNAL
    if (!emitted) {
      journal_sweep_pair_selection(k, e.a, e.b, selection);
    }
#endif
    if (!emitted && selection.unit_consequences) {
      const SweepGeneralPlan first = sweep_plan_general(
          k, *selection.forward, selection.forward->t0, INVALID_LIT);
      const SweepGeneralPlan second = sweep_plan_general(
          k, *selection.reverse, selection.reverse->t0, INVALID_LIT);
      if (first.eligible() && second.eligible() && k.value(first.t0) >= 0 &&
          k.value(second.t0) >= 0) {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        uint64_t pre_digest = 0;
        bool journal_plan =
            begin_journal_sweep_general(k, first, pre_digest);
#endif
        const bool apply_first = k.value(first.t0) == 0;
        if (apply_first) {
          sweep_apply_general_plan(k, first);
#ifdef TACTIX_ENABLE_EVENT_TRACE
          event_trace_sweep_unit(k, first.t0);
#endif
        }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        if (journal_plan)
          journal_sweep_general(k, first, pre_digest,
                                apply_first ? "applied" : "no-op", "none",
                                "unit");
        journal_plan = begin_journal_sweep_general(k, second, pre_digest);
#endif
        const bool apply_second = k.value(second.t0) == 0;
        if (apply_second) {
          sweep_apply_general_plan(k, second);
#ifdef TACTIX_ENABLE_EVENT_TRACE
          event_trace_sweep_unit(k, second.t0);
#endif
        }
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        if (journal_plan)
          journal_sweep_general(k, second, pre_digest,
                                apply_second ? "applied" : "no-op", "none",
                                "unit");
#endif
        k.stats.sweep_general++;
        any = true;
        continue;
      }
    }
    if (!emitted && selection.eligible() && !selection.unit_consequences) {
      const SweepGeneralPlan fwd =
          sweep_plan_general(k, *selection.forward, selection.forward->t0,
                             selection.forward->t1);
      const SweepGeneralPlan rev =
          sweep_plan_general(k, *selection.reverse, selection.reverse->t0,
                             selection.reverse->t1);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
      uint64_t fwd_pre_digest = 0;
      const bool journal_fwd =
          begin_journal_sweep_general(k, fwd, fwd_pre_digest);
#endif
      if (fwd.eligible() && rev.eligible()) {
        sweep_apply_general_plan(k, fwd);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        if (journal_fwd)
          journal_sweep_general(k, fwd, fwd_pre_digest, "applied", "none",
                                "binary");
        uint64_t rev_pre_digest = 0;
        const bool journal_rev =
            begin_journal_sweep_general(k, rev, rev_pre_digest);
#endif
        sweep_apply_general_plan(k, rev);
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        if (journal_rev)
          journal_sweep_general(k, rev, rev_pre_digest, "applied", "none",
                                "binary");
#endif
        k.stats.sweep_general++;
        emitted = true;
      } else {
#ifdef TACTIX_ENABLE_RULE_JOURNAL
        const char *fwd_decline =
            fwd.eligible() ? "paired-direction-declined"
                           : sweep_general_decline_name(fwd.decline);
        if (journal_fwd)
          journal_sweep_general(k, fwd, fwd_pre_digest, "declined",
                                fwd_decline, "-");
        uint64_t rev_pre_digest = 0;
        const bool journal_rev =
            begin_journal_sweep_general(k, rev, rev_pre_digest);
        const char *rev_decline =
            rev.eligible() ? "paired-direction-declined"
                           : sweep_general_decline_name(rev.decline);
        if (journal_rev)
          journal_sweep_general(k, rev, rev_pre_digest, "declined",
                                rev_decline, "-");
#endif
      }
    }
    // Replaying against the live kernel is a fallback, not the primary proof
    // path. Sweep deliberately batches newly proved root units without
    // propagation; a probing replay can drain those units and thereby change
    // later consequence order even though it restores its speculative levels.
    // The captured Kitten derivation has no such main-formula side effect.
    if (!emitted && !force_general)
      emitted = sweep_emit_equiv(k, e.a, e.b, chain_fwd, chain_rev);
    if (!emitted && !force_general)
      k.stats.sweep_not_rup++;
    if (emitted) {
      any = true;
      emitted_equivalences.emplace_back(e.a, e.b);
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_equivalence(k, e.a, e.b);
      event_trace_sweep_commit(k, e.a, e.b, av, bv, "emitted");
#endif
    } else {
#ifdef TACTIX_ENABLE_EVENT_TRACE
      event_trace_sweep_commit(k, e.a, e.b, av, bv, "declined");
#endif
    }
  }
  if (!emitted_equivalences.empty()) {
    // Applying one Sweep environment's consequences crosses Tactix's
    // variable-width representation bridge once. Sweep owns that scheduled
    // cost; Substitute remains unaware of who requested the rewrite.
    const ApplyResult substituted =
        substitute_apply_sweep_equivalences(k, emitted_equivalences,
                                            round.substitution);
    if (substituted == ApplyResult::Unsat)
      return substituted;
    if (!k.dense && round.substitution.changed) {
      // Without the dense session ledger, the next environment owns raw arena
      // Refs and needs the generation bump immediately.  A canonical dense
      // call instead keeps every replacement visible through
      // `dense_occurrences` and crosses the variable-width arena bridge once,
      // after the reference-ordered consequence sequence.  This is the same
      // semantic transaction boundary as Kissat's in-place rewrites without
      // charging one whole-formula collection per productive environment.
      k.stats.sweep_ticks_repr += round.substitution.collect(k);
      round.substitution.begin(k);
    }
    any |= substituted == ApplyResult::Ok;
  }
  if (any)
    round.emission_batches++;
  return any ? ApplyResult::Ok : ApplyResult::NoOp;
}
} // namespace

void sweep_order_retirement_suffix(Kernel &k, size_t retired_begin,
                                   size_t substituted_begin) {
  // Kissat's inline Sweep rewrite only fills `sweeper->reprs`.  The single
  // Substitute call after Sweep later deactivates their union through
  // `for (all_variables (idx))` (probe.c, substitute.c
  // `remove_representative_equivalences`).  Tactix must retire inline because
  // each variable-width rewrite needs a reconstruction witness immediately,
  // but its per-environment batches must not leak into the eventual heuristic
  // removal order.  Kissat's compact internal indices are Tactix's permanent
  // first-import order.
  const auto imported_before = [&](Var a, Var b) {
    return kernel_import_var_order(k, a) < kernel_import_var_order(k, b);
  };
  assert(retired_begin <= k.retired_events.size());
  assert(substituted_begin <= k.substituted_events.size());
  std::sort(k.retired_events.begin() + retired_begin, k.retired_events.end(),
            imported_before);
  std::sort(k.substituted_events.begin() + substituted_begin,
            k.substituted_events.end(), imported_before);
}

ApplyResult sweep_apply_with_limits(Kernel &k, const SweepLimits &base) {
  // TACTIX_NO_SWEEP: diagnostic kill switch, so a fuzz leg can run with V5
  // inert. It stays because Sweep shares the RUP-replay primitive with
  // Congruence, and a crash inside that primitive is otherwise unattributable
  // between the two callers.
  if (std::getenv("TACTIX_NO_SWEEP"))
    return ApplyResult::NoOp;
  if (!sweep_guard(k))
    return ApplyResult::NoOp;
  // The guard stays a pure precondition — demanding a root fixpoint would make
  // it depend on scheduling state and cost N1 — so the apply completes the root
  // closure itself. Required, not tidiness: a RUP replay off a non-fixpoint
  // root forces level-0 literals that never pass through `propagate_service`,
  // the only place `unit_ids[v]` is recorded, and the next LRAT chain citing
  // one trips `kernel_lrat_chain`.
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (k.level != 0 || k.conflict_valid)
    return ApplyResult::NoOp;
  SweepRound round;
  SweepLimits lim = sweep_limits_from_env(base);
  const size_t retired_begin = k.retired_events.size();
  const size_t substituted_begin = k.substituted_events.size();
  // Honour the effort ledger. `round_ticks` defaults to 0 = unbounded,
  // which is right for the diagnostic probe and catastrophic for a scheduled
  // rule; `Kernel::work_budget` is what the policy hands a budget-aware rule.
  if (k.work_budget)
    lim.round_ticks = k.work_budget;
  ApplyResult result;
  if (lim.resume) {
    result = sweep_apply_resumable(k, lim, round);
  } else {
    const uint32_t found = sweep_detect_round(k, lim, round);
    result = sweep_emit_round(k, round, found);
    if (result != ApplyResult::Unsat)
      k.stats.sweep_ticks_repr += round.substitution.collect(k);
  }
  if (result != ApplyResult::Unsat)
    sweep_order_retirement_suffix(k, retired_begin, substituted_begin);
#ifdef TACTIX_ENABLE_CONFLICT_STOP
  if (sweep_time)
    std::fprintf(stderr,
                 "[sweep-time] ms: kitten %.0f dense %.0f occ %.0f ledger "
                 "%.0f repr %.0f schedule %.0f process %.0f post-seed %.0f "
                 "collect %.0f\n",
                 round.time_kitten / 1e6, round.time_dense / 1e6,
                 round.time_occ / 1e6, round.time_ledger / 1e6,
                 round.time_repr / 1e6, round.time_schedule / 1e6,
                 round.time_process / 1e6, round.time_post_seed / 1e6,
                 round.time_collect / 1e6);
#endif
  return result;
}

ApplyResult sweep_apply(Kernel &k) {
  return sweep_apply_with_limits(k, SweepLimits{});
}



// One seed, the per-instance surface. Same guard, same root closure, same
// emission as the canonical round; only the detection scope differs.
ApplyResult sweep_try(Kernel &k, Var seed) {
  if (std::getenv("TACTIX_NO_SWEEP") || !sweep_guard(k))
    return ApplyResult::NoOp;
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (k.level != 0 || k.conflict_valid || seed >= k.nvars)
    return ApplyResult::NoOp;
  SweepRound round;
  SweepLimits lim = sweep_limits_from_env();
  if (k.work_budget)
    lim.round_ticks = k.work_budget;
  sweep_round_begin(k, round);
  (void)sweep_process_seed(k, seed, lim, round, true);
  if (k.inconsistent)
    return ApplyResult::Unsat;
  k.stats.sweep_ticks_repr += round.substitution.collect(k);
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  return round.emission_batches ? ApplyResult::Ok : ApplyResult::NoOp;
}

// Ranks by the canonical seed order and never filters: a variable this rule
// judges unpromising is still offered, because the surface is the policy's
// action space and an omission tells a learner a legal move does not exist.
static uint32_t sweep_instances(Kernel &k, uint64_t *out, uint32_t cap) {
  std::vector<Var> seeds;
  sweep_seeds(k, seeds);
  const uint32_t n = std::min<uint32_t>(cap, static_cast<uint32_t>(seeds.size()));
  for (uint32_t i = 0; i < n; i++)
    out[i] = static_cast<uint64_t>(seeds[i]);
  return n;
}

static ApplyResult sweep_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return sweep_apply(k);
  if (inst >= k.nvars)
    return ApplyResult::NoOp;
  return sweep_try(k, static_cast<Var>(inst));
}

TACTIX_REGISTER_RULE(Sweep, RuleTier::Inprocess, sweep_guard, sweep_apply,
                     sweep_instances, sweep_apply_instance);

} // namespace tactix

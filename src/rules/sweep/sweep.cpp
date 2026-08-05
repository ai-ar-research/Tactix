// V5 Sweep · S-c: candidate selection + environment construction.
// See sweep.hpp for the contract, the soundness argument (env ⊆ F) and the
// deviation ledger; docs/design/v5-sweep-design.md §6.3 for the increment.
#include "rules/sweep/sweep.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>

namespace tactix {

namespace {

// Dedup keys for the environment's clause set. A binary is keyed by its literal
// PAIR (kernel_binary_key), so the two scans that reach it — from either
// endpoint — encode it once; a large clause by its arena ref; a unit by its
// variable. The three spaces are kept disjoint by the tag bits, and
// kernel_binary_key's payload is two 31-bit-ish literal codes, so it never
// reaches them.
constexpr uint64_t KEY_LARGE = 1ull << 63;
constexpr uint64_t KEY_UNIT = 1ull << 62;

// At a quiescent root (the guard) an assigned variable is a FIXED one.
bool root_fixed(const Kernel &k, Var v) {
  return k.values[make_lit(v, false)] != 0;
}

// Is (a ∨ b) live in the watch lists? Deliberately NOT `kernel_binary_id != 0`:
// `binary_ids` is size-monotone (a deleted pair keeps its key, see
// Kernel::binary_ids), so the id map would report a binary V4 has already
// deleted and S-c would score a genuinely new equivalence as "already known" —
// distorting the one number this increment exists to produce.
bool binary_live(const Kernel &k, Lit a, Lit b) {
  const Lit scan = k.watches.size(a) <= k.watches.size(b) ? a : b;
  const Lit want = scan == a ? b : a;
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[scan].offset;
  const uint32_t sz = k.watches.heads[scan].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == want)
        return true;
      i += 1;
    } else {
      i += 2;
    }
  }
  return false;
}

// --- the round's representative map (kissat sweep.c `sweep_repr`) -----------
// Union-find over LITERALS with the mirror invariant repr[¬l] = ¬repr[l], so a
// class and its negation move together. Round-scoped: it exists to stop one
// class being re-proved from every member, not to record anything durable (V3
// Substitute owns the durable substitution).
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

void repr_unite(SweepRound &r, Lit a, Lit b) {
  Lit ra = repr_find(r, a), rb = repr_find(r, b);
  if (ra == rb)
    return;
  // `rb == ¬ra` means this round has proved BOTH x ≡ y and x ≡ ¬y, which is
  // only possible when F is unsatisfiable (every equivalence is then vacuously
  // entailed). Merging anyway would set repr[ra] = ¬ra AND repr[¬ra] = ra — a
  // two-cycle that hangs `repr_find` forever, which is exactly what the random
  // oracle in tests/test_sweep.cpp found. S-c draws no conclusion from it: the
  // classes stay apart and the environments that produced the claims are still
  // counted. (S-d must treat this case as a REFUTATION, not as two binaries.)
  if (rb == negate(ra))
    return;
  if (rb < ra)
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
  e.keys.clear();
  e.clause_lits.clear();
  e.clause_off.assign(1, 0);
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

// Add one clause of F to the environment. Returns false when a limit declines
// it — declining is always sound (the environment only ever shrinks), which is
// why every cap in this file is a plain early return.
bool env_add(SweepEnv &e, const Lit *lits, uint32_t n, uint64_t key,
             uint64_t id, uint32_t depth, const SweepLimits &lim,
             uint64_t *ticks) {
  if (e.nclauses() >= lim.max_env_clauses)
    return false;
  *ticks += e.keys.size();
  for (const uint64_t x : e.keys)
    if (x == key)
      return false; // already encoded (a binary reached from its other end)
  uint32_t fresh = 0;
  for (uint32_t i = 0; i < n; i++) {
    const Var v = var_of(lits[i]);
    if (e.sub_of_var[v] != SWEEP_NO_SUB_VAR)
      continue;
    bool dup = false;
    for (uint32_t j = 0; j < i && !dup; j++)
      dup = var_of(lits[j]) == v;
    fresh += !dup;
  }
  if (e.vars.size() + fresh > lim.max_env_vars)
    return false; // the clause would not fit WHOLE, and half a clause is not
                  // a clause of F — never truncate
  for (uint32_t i = 0; i < n; i++) {
    e.clause_lits.push_back(lits[i]);
    env_var(e, var_of(lits[i]), depth);
  }
  e.clause_off.push_back(static_cast<uint32_t>(e.clause_lits.size()));
  e.keys.push_back(key);
  e.clause_ids.push_back(id);
  return true;
}

// Add a clause, then the UNIT CLAUSES of any root-false literal it carries (see
// deviation 1): those units are members of F only when the proof actually holds
// them (`unit_ids[v] != 0` — a variable fixed by root propagation has none), so
// the citation stays honest and the sub-solver still learns the literal is
// false whenever we can say why.
void env_add_with_units(const Kernel &k, SweepEnv &e, const Lit *lits,
                        uint32_t n, uint64_t key, uint64_t id, uint32_t depth,
                        const SweepLimits &lim, uint64_t *ticks) {
  if (!env_add(e, lits, n, key, id, depth, lim, ticks))
    return;
  for (uint32_t i = 0; i < n; i++) {
    const Lit l = lits[i];
    if (k.values[l] >= 0)
      continue;
    const Var v = var_of(l);
    e.depth[e.sub_of_var[v]] = UINT32_MAX; // fixed: never a BFS frontier
    if (!k.unit_ids[v])
      continue;
    const Lit unit = negate(l);
    env_add(e, &unit, 1, KEY_UNIT | v, k.unit_ids[v], UINT32_MAX, lim, ticks);
  }
}

} // namespace

bool sweep_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

void sweep_seeds(const Kernel &k, std::vector<Var> &out) {
  out.clear();
  for (Var v = 0; v < k.nvars; v++) {
    // Fixed variables belong to V1 Probe / V2 Backbone (their equivalences are
    // units), retired ones are no longer in F. Everything else is a seed.
    // NOTE — there is deliberately NO "does it occur anywhere?" filter here.
    // The obvious cheap one, "its watch lists are empty", is WRONG: a large
    // clause is watched on two literals only, so a variable occurring in
    // thousands of large clauses can have empty watch lists (and propagation
    // moves watches around, so which variables look occurrence-free changes
    // with search history). That version of this function silently dropped the
    // seed of the only equivalence in tests/test_sweep.cpp's non-RUP fixture,
    // but only AFTER a propagation had moved the watches — i.e. it passed the
    // unit test and would have failed in a solve. An unoccurring variable
    // simply builds an empty environment and returns.
    if (root_fixed(k, v) || kernel_var_retired(k, v))
      continue;
    out.push_back(v);
  }
}

void sweep_round_begin(Kernel &k, SweepRound &r) {
  // occurrence index over irredundant large clauses. `Kernel::dense` holds
  // exactly that during a simplification phase; outside one (the API, a
  // stress policy) we build the same thing with a single arena pass — the
  // OccIndex "optional acceleration" contract, so the detector stays usable
  // at any state (N1).
  if (k.dense && k.dense->large.size() >= 2 * static_cast<size_t>(k.nvars)) {
    r.occ = &k.dense->large;
  } else {
    r.occ_owned.assign(2 * k.nvars, {});
    for (Ref ref = 0; ref < k.arena.size();) {
      const Clause *c = clause_at(k.arena, ref);
      const uint32_t words = clause_words(c->size);
      if (!c->garbage() && !c->redundant())
        for (const Lit l : *c)
          r.occ_owned[l].push_back(ref);
      k.stats.sweep_ticks += words;
      ref += words;
    }
    r.occ = &r.occ_owned;
  }
  r.repr.resize(2 * k.nvars);
  for (Lit l = 0; l < 2 * k.nvars; l++)
    r.repr[l] = l; // O(nlits) once per ROUND, never per environment
  r.found.clear();
}

bool sweep_env_build(Kernel &k, Var seed, const SweepLimits &lim,
                     SweepRound &r) {
  assert(r.occ && "sweep_round_begin must run first");
  SweepEnv &e = r.env;
  env_reset(k, e);
  if (root_fixed(k, seed) || kernel_var_retired(k, seed))
    return false;
  env_var(e, seed, 0);
  uint64_t ticks = 0;
  // Breadth-first over the clause neighbourhood (kissat sweep.c
  // sweep_binary/sweep_reference). `vars` doubles as the BFS queue: entries are
  // appended in non-decreasing depth, and fixed variables carry UINT32_MAX so
  // the frontier never expands THROUGH one.
  for (uint32_t i = 0; i < e.vars.size(); i++) {
    const uint32_t d = e.depth[i];
    if (d > lim.max_depth)
      continue;
    const Var v = e.vars[i];
    for (uint32_t pol = 0; pol < 2; pol++) {
      const Lit l = make_lit(v, pol != 0);
      const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
      const uint32_t sz = k.watches.heads[l].size;
      ticks += sz;
      for (uint32_t w_i = 0; w_i < sz;) {
        const Watch w{ws[w_i]};
        if (!w.binary()) {
          w_i += 2; // large clauses come from the occurrence index below
          continue;
        }
        w_i += 1;
        const Lit o = w.other();
        if (k.values[l] > 0 || k.values[o] > 0)
          continue; // root-satisfied: it constrains nothing
        const Lit pair[2] = {l, o};
        env_add_with_units(k, e, pair, 2, kernel_binary_key(l, o),
                           kernel_binary_id(k, l, o), d + 1, lim, &ticks);
      }
      for (const Ref ref : (*r.occ)[l]) {
        ticks += 1;
        const Clause *c = clause_at(k.arena, ref);
        if (c->garbage() || c->redundant())
          continue; // the index may hold stale refs (OccIndex contract)
        bool satisfied = false;
        for (const Lit m : *c)
          satisfied = satisfied || k.values[m] > 0;
        ticks += c->size;
        if (satisfied)
          continue;
        env_add_with_units(k, e, c->begin(), c->size, KEY_LARGE | ref, c->id(),
                           d + 1, lim, &ticks);
      }
    }
  }
  k.stats.sweep_ticks += ticks;
  return e.nclauses() > 0;
}

namespace {

// The value the sub-solver's model gives a KERNEL literal of the environment.
int8_t model_value(const SweepEnv &e, const std::vector<int8_t> &model, Lit l) {
  const int8_t v = model[e.sub_of_var[var_of(l)]];
  return negated(l) ? static_cast<int8_t>(-v) : v;
}

// Drop every candidate the new model separates from the seed. This is kissat's
// sweep_refine_partition in seed-anchored form (deviation 5): a model is free
// evidence that two literals are NOT equivalent, and the candidate that
// produced it is always among the dropped ones — which is what makes the
// candidate loop below terminate.
void refine(const SweepEnv &e, const std::vector<int8_t> &model, Lit seed_lit,
            std::vector<Lit> &candidates) {
  const int8_t sv = model_value(e, model, seed_lit);
  size_t j = 0;
  for (const Lit y : candidates)
    if (model_value(e, model, y) == sv)
      candidates[j++] = y;
  candidates.resize(j);
}

} // namespace

uint32_t sweep_detect_seed(Kernel &k, Var seed, const SweepLimits &lim,
                           SweepRound &r) {
  assert(sweep_guard(k));
  assert(r.repr.size() == 2 * k.nvars && "sweep_round_begin must run first");
  SweepEnv &e = r.env;
  const Lit seed_lit = make_lit(seed, false);
  if (repr_find(r, seed_lit) != seed_lit)
    return 0; // already merged into a class this round: its sweep is that
              // class representative's (kissat schedule_inner)
  if (!sweep_env_build(k, seed, lim, r))
    return 0;
  if (e.vars.size() < 2 || e.nclauses() < 2)
    return 0; // nothing to be equivalent TO
  k.stats.sweep_environments++;
  k.stats.sweep_env_clauses += e.nclauses();

  e.solver.reset(static_cast<uint32_t>(e.vars.size()));
  std::vector<SubLit> &buf = r.sub_scratch;
  for (uint32_t c = 0; c < e.nclauses(); c++) {
    buf.clear();
    for (uint32_t i = e.clause_off[c]; i < e.clause_off[c + 1]; i++) {
      const Lit l = e.clause_lits[i];
      buf.push_back(sub_lit(e.sub_of_var[var_of(l)], negated(l)));
    }
    const uint32_t id = e.solver.add_clause(buf);
    // The environment's own numbering IS the sub-solver's, which is what makes
    // `clause_ids[id - 1]` the main-formula clause S-e must cite.
    assert(id == c + 1);
    (void)id;
  }

  uint32_t found = 0;
  std::vector<SubLit> &assumptions = r.assumptions;
  assumptions.clear();
  k.stats.sweep_solves++;
  SubResult res = e.solver.solve(assumptions, lim.env_ticks);
  if (res == SubResult::Budget) {
    k.stats.sweep_budget++;
    k.stats.sweep_ticks += e.solver.ticks();
    return 0;
  }
  if (res == SubResult::Unsat) {
    // The environment alone refutes F (it IS a subset of F). Real, and worth a
    // count, but turning it into `add-empty` is S-d's proof obligation — S-c
    // touches nothing.
    k.stats.sweep_env_unsat++;
    k.stats.sweep_ticks += e.solver.ticks();
    return 0;
  }

  // Candidate selection: every environment literal that agrees with the seed in
  // the first model. Two literals that differ in ANY model of the environment
  // cannot be equivalent, so this is the cheapest sound over-approximation and
  // every further model shrinks it (`refine`).
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

  uint32_t solved = 0;
  while (!r.candidates.empty() && solved < lim.max_candidates) {
    const Lit y = r.candidates.back();
    const Lit ry = repr_find(r, y);
    if (ry == seed_lit || ry == negate(seed_lit)) {
      // an earlier pair in THIS environment already settled y's class
      r.candidates.pop_back();
      continue;
    }
    if (binary_live(k, negate(seed_lit), y) &&
        binary_live(k, seed_lit, negate(y))) {
      // the formula already states this equivalence: nothing for Sweep to add
      k.stats.sweep_known++;
      repr_unite(r, seed_lit, y);
      r.candidates.pop_back();
      continue;
    }
    // Direction 1: x ∧ ¬y unsatisfiable ⇔ F ⊨ (¬x ∨ y).
    assumptions.clear();
    assumptions.push_back(sub_lit(e.sub_of_var[seed], false));
    assumptions.push_back(
        sub_lit(e.sub_of_var[var_of(y)], !negated(y)));
    solved++;
    k.stats.sweep_candidates++;
    k.stats.sweep_solves++;
    res = e.solver.solve(assumptions, lim.env_ticks);
    if (res == SubResult::Budget) {
      k.stats.sweep_budget++;
      break;
    }
    if (res == SubResult::Sat) {
      refine(e, e.solver.model(), seed_lit, r.candidates);
      continue;
    }
    assert(!e.solver.inconsistent() && "a satisfiable environment cannot then "
                                       "derive the empty clause");
#ifndef NDEBUG
    { // S-d/S-e depend on this: a refutation under assumptions must hand back a
      // derivation whose target clause is a sub-clause of the negated
      // assumptions. Checked here, where the assumptions are still in hand.
      SubDerivation d;
      assert(e.solver.derivation(d) && !d.steps.empty());
      for (const SubLit tl : d.steps.back().lits)
        assert(tl == sub_negate(assumptions[0]) ||
               tl == sub_negate(assumptions[1]));
    }
#endif
    // Direction 2: ¬x ∧ y unsatisfiable ⇔ F ⊨ (x ∨ ¬y).
    assumptions.clear();
    assumptions.push_back(sub_lit(e.sub_of_var[seed], true));
    assumptions.push_back(sub_lit(e.sub_of_var[var_of(y)], negated(y)));
    solved++;
    k.stats.sweep_candidates++;
    k.stats.sweep_solves++;
    res = e.solver.solve(assumptions, lim.env_ticks);
    if (res == SubResult::Budget) {
      k.stats.sweep_budget++;
      break;
    }
    if (res == SubResult::Sat) {
      refine(e, e.solver.model(), seed_lit, r.candidates);
      continue;
    }
    assert(!e.solver.inconsistent());
    // Both directions refuted over env ⊆ F, so F ⊨ x ≡ y.
    r.found.push_back({std::min(seed_lit, y), std::max(seed_lit, y)});
    k.stats.sweep_equivalences++;
    found++;
    repr_unite(r, seed_lit, y);
    r.candidates.pop_back();
  }
  k.stats.sweep_ticks += e.solver.ticks();
  return found;
}

uint32_t sweep_detect_round(Kernel &k, const SweepLimits &lim, SweepRound &r) {
  if (!sweep_guard(k))
    return 0;
  k.stats.sweep_rounds++;
  sweep_round_begin(k, r);
  sweep_seeds(k, r.seeds);
  const uint64_t start = k.stats.sweep_ticks;
  uint32_t found = 0, seeds = 0;
  for (const Var v : r.seeds) {
    if (lim.max_seeds && seeds >= lim.max_seeds)
      break;
    if (lim.round_ticks && k.stats.sweep_ticks - start > lim.round_ticks)
      break;
    seeds++;
    found += sweep_detect_seed(k, v, lim, r);
  }
  return found;
}

SweepLimits sweep_limits_from_env() {
  SweepLimits lim;
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
  u32("TACTIX_SWEEP_SEEDS", lim.max_seeds);
  u64("TACTIX_SWEEP_TICKS", lim.env_ticks);
  u64("TACTIX_SWEEP_ROUND_TICKS", lim.round_ticks);
  return lim;
}

} // namespace tactix

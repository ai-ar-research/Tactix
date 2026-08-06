// Sweep: candidate selection, environment construction and emission.
// See sweep.hpp for the contract, the soundness argument (env ⊆ F) and the
// deviation ledger; docs/design/v5-sweep-design.md for the design.
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/sweep/sweep.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
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
// Kernel::binary_ids), so the id map would report a binary TransitiveReduce
// has already deleted, and a genuinely new equivalence would be scored
// "already known".
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
  // oracle in tests/test_sweep.cpp found. DETECTION draws no conclusion from
  // it: the classes stay apart and the environments that produced the claims
  // are still counted. EMISSION must treat this case as a REFUTATION, never
  // as two binaries.
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

bool SweepBudget::over(const Kernel &k, uint64_t pending) const {
  // `pending` is the scan charge accumulated but not yet folded into
  // `Stats::sweep_ticks`, so the ceiling binds during a scan and not only after
  // it.
  return limit && k.stats.sweep_ticks + pending - start > limit;
}

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
  // `proofs` is PARALLEL to `found` (sweep_apply indexes it by the same `idx`),
  // so it has to be reset with it. Clearing one and not the other leaves every
  // later pair reading the previous round's refutation -- latent today only
  // because `sweep_apply` builds a fresh SweepRound per apply, and caught
  // downstream only by the `is_pair` target check, which declines rather than
  // reporting. Same family as the S-e polarity defect: two things that must
  // agree, kept in step by hand.
  r.proofs.clear();
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
  for (uint32_t i = 0; i < e.vars.size() && !r.budget.over(k, ticks); i++) {
    const uint32_t d = e.depth[i];
    if (d > lim.max_depth)
      continue;
    const Var v = e.vars[i];
    for (uint32_t pol = 0; pol < 2 && !r.budget.over(k, ticks); pol++) {
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
        // The occurrence walk is the unbounded term: `max_env_vars` stops the
        // environment GROWING long before this list ends, and every clause
        // after that is scanned only to be declined. Poll here, not merely
        // between seeds.
        if (r.budget.over(k, ticks))
          break;
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

// SUB-SOLVER LITERAL -> KERNEL LITERAL. Both translation sites go through this
// one function, because getting it wrong is silent: the two encodings share the
// `2*v | negated` shape, so an inverted parity still type-checks, still names a
// real variable and still produces a well-formed clause -- just the NEGATION of
// the one the sub-solver derived. It is the exact inverse of the encode in
// `sweep_detect_seed` (`sub_lit(sub_of_var[var_of(l)], negated(l))`), and of
// `test_subsolver`'s `dimacs()` (an even sub literal is POSITIVE). Spelling it
// out separately at each site is what let the two agree with each other and
// disagree with the sub-solver, making every general-path lemma unverifiable
// (N2 breach, `benchmarks/reports/2026-08-05-sweep-se-polarity-fix.md`).
static inline Lit sweep_kernel_lit(const std::vector<Var> &vars, SubLit sl) {
  return make_lit(vars[sub_var_of(sl)], (sl & 1u) != 0);
}

// Copy one direction's refutation out of the sub-solver, together with the
// translation tables it will need later. Both are necessary: `steps` names
// sub-solver clause ids, and the environment that gives those ids meaning is
// rebuilt for the next seed. Failure here is not an error — it just means this
// pair has no general-path proof and must fall back to the RUP path or be
// declined.
// The target is READ OFF the derivation rather than predicted from the
// assumptions. Predicting it means re-deriving which literal the seed
// contributed and with which polarity, and getting that subtly wrong silently
// swaps the two directions -- which is exactly what happened first: the check
// then fails for every pair and the general path looks merely "unreachable".
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
  if (tl.size() != 2)
    return; // a unit target is a stronger fact and a different proof delta
  out.t0 = sweep_kernel_lit(e.vars, tl[0]);
  out.t1 = sweep_kernel_lit(e.vars, tl[1]);
  out.steps = std::move(d.steps);
  out.orig_ids = e.clause_ids;
  out.vars = e.vars;
  out.ok = true;
}

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
    // `clause_ids[id - 1]` the main-formula clause emission must cite.
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
    // count, but turning it into `add-empty` would be a separate proof
    // obligation that nothing here discharges — so detection touches nothing.
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
    // Refutation capture (see sweep_capture_dir): the sub-solver keeps only the LAST
    // solve's derivation, so direction 1's must be copied out before direction
    // 2's solve overwrites it.
    SweepPairProof pair;
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
    sweep_capture_dir(e, pair.rev);
    // Both directions refuted over env ⊆ F, so F ⊨ x ≡ y.
    r.found.push_back({std::min(seed_lit, y), std::max(seed_lit, y)});
    r.proofs.push_back(std::move(pair));
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
  // The between-seeds check is unconditional; `budget` carries the SAME ceiling
  // into `sweep_env_build` so one seed cannot outspend the round before the
  // next check is ever reached. `enforce_in_env=0` reverts only the inner poll.
  r.budget.start = start;
  r.budget.limit = lim.enforce_in_env ? lim.round_ticks : 0;
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
  if (const char *s = std::getenv("TACTIX_SWEEP_ENV_BUDGET"))
    lim.enforce_in_env = std::strtoul(s, nullptr, 10) != 0;
  return lim;
}

// ---- emission: the RUP fast path -----------------------------------------
//
// Detection proves `a ≡ b` inside a small ENVIRONMENT, which is a subset of F.
// That is a sound entailment, but an entailment is not a proof line: the
// checker knows nothing of the environment. So we ask the main formula the same
// question in the one form a checker accepts, RUP, using Congruence's replay
// primitive rather than deriving a chain by hand (`congruence_rup_chain` — the
// chain comes out in the checker's own propagation order by construction).
//
// This is DELIBERATELY the narrow path. An equivalence that is RUP over F is
// exactly the easy case sweeping is not for (design §4); the interesting ones
// need the sub-solver's core mapped back to main-formula ids. A non-RUP
// equivalence is DECLINED (counted in `sweep_not_rup`), never emitted.
//
// BOTH directions must replay. `a ≡ b` is two binaries, and emitting one
// without the other would state an implication the sub-solver did not prove on
// its own. If either direction fails, the pair is skipped whole.
//
// The replay runs in DRAT mode too, even though DRAT needs no chain: here it is
// not chain CONSTRUCTION but the RUP TEST itself, and a swept equivalence is not
// RUP in general. Emitting an unchecked line would hand the checker something it
// cannot verify — exactly the failure N2 exists to prevent.
static bool sweep_emit_equiv(Kernel &k, Lit a, Lit b,
                             std::vector<uint64_t> &chain_fwd,
                             std::vector<uint64_t> &chain_rev) {
  if (a == b || a == negate(b))
    return false; // degenerate: nothing to state, and not emittable
  const Lit fwd[2] = {negate(a), b};
  const Lit rev[2] = {a, negate(b)};
  if (!congruence_replay_rup(k, fwd, 2, chain_fwd))
    return false;
  if (!congruence_replay_rup(k, rev, 2, chain_rev))
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

// Emission, the GENERAL path.
//
// The RUP fast path above covers only equivalences that unit propagation can
// refute over F. A swept equivalence is entailed but generally NOT RUP -- that
// gap is the whole reason a sub-solver is involved. Here the sub-solver's own
// refutation is replayed into the main proof: each lemma it derived is emitted
// as its own line, in derivation order, with its antecedents translated from
// sub-solver clause ids to main-formula proof ids. The final lemma IS the target
// binary, and by then every antecedent it names is a line the checker has seen.
//
// The translation rests on `env ⊆ F` holding LITERALLY: every environment
// clause is a clause the kernel holds, carried with its own proof id, so an
// original antecedent maps to a real main-formula id rather than to something
// reconstructed. Anything that cannot be mapped exactly -- an unmapped
// antecedent, a tautological lemma, a target that is not the binary we asked
// for -- DECLINES rather than emitting, because a wrong line here is an
// unverifiable proof, not a missed optimisation.
// `dry` translates and validates without emitting a line or consuming an id, so
// a PAIR can be checked whole before either half is written. Emitting one
// direction and then discovering the other is untranslatable would leave a
// half-stated equivalence in F -- sound, but not what the rule claims.
static bool sweep_emit_general(Kernel &k, const SweepDirProof &p,
                               const Lit t0, const Lit t1, bool dry) {
  if (!p.ok || p.steps.empty())
    return false;
  const bool lrat = k.proof && k.proof->lrat();
  uint32_t max_id = static_cast<uint32_t>(p.orig_ids.size());
  for (const SubStep &st : p.steps)
    max_id = std::max(max_id, st.id);
  std::vector<uint64_t> id_of(static_cast<size_t>(max_id) + 1, 0);
  for (size_t i = 0; i < p.orig_ids.size(); i++)
    id_of[i + 1] = p.orig_ids[i]; // sub ids are 1-based, in add order

  std::vector<Lit> lits;
  std::vector<uint64_t> chain;
  uint64_t last_id = 0;
  for (size_t si = 0; si < p.steps.size(); si++) {
    const SubStep &st = p.steps[si];
    lits.clear();
    for (const SubLit sl : st.lits) {
      if (sub_var_of(sl) >= p.vars.size())
        return false;
      lits.push_back(sweep_kernel_lit(p.vars, sl));
    }
    if (lits.empty())
      return false; // the empty clause means F is UNSAT; not this rule's job
    if (Proof::tautological(lits.data(), static_cast<uint32_t>(lits.size())))
      return false; // uncheckable under LRAT; decline the whole pair // uncheckable under LRAT; decline the whole pair
    chain.clear();
    for (const uint32_t c : st.chain) {
      if (c >= id_of.size() || id_of[c] == 0)
        return false; // an antecedent we cannot name in the main proof // an antecedent we cannot name in the main proof
      chain.push_back(id_of[c]);
    }
    const bool final_step = (si + 1 == p.steps.size());
    if (final_step) {
      // The target must be exactly the binary we are justifying. A shorter
      // target (a unit) would be a STRONGER fact but a different proof delta,
      // and it would have been RUP anyway, so the fast path already had it.
      if (lits.size() != 2)
        return false;
      const bool same = (lits[0] == t0 && lits[1] == t1) ||
                        (lits[0] == t1 && lits[1] == t0);
      if (!same)
        return false;
    }
    if (dry) {
      if (st.id < id_of.size())
        id_of[st.id] = UINT64_MAX; // a placeholder is enough to validate naming
      continue;
    }
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, lits.data(),
                       static_cast<uint32_t>(lits.size()),
                       lrat ? chain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(chain.size()) : 0);
    if (st.id < id_of.size())
      id_of[st.id] = id;
    last_id = id;
  }
  if (dry)
    return true;
  kernel_add_binary_clause(k, t0, t1, last_id);
  k.stats.sweep_binaries++;
  return true;
}

ApplyResult sweep_apply(Kernel &k) {
  // TEMPORARY attribution switch (fuzz 1583562): lets a leg run with V5 inert
  // so a crash can be blamed on Sweep or exonerated.
  // TACTIX_NO_SWEEP: diagnostic kill switch. It is what attributed fuzz
  // 1583562's 43 crashes to this rule (3/3 with, 0/3 without) rather than to the
  // shared RUP-replay primitive, so it stays.
  if (std::getenv("TACTIX_NO_SWEEP"))
    return ApplyResult::NoOp;
  if (!sweep_guard(k))
    return ApplyResult::NoOp;
  // D-LEVEL guard purification, the idiom every other inprocessing rule already
  // uses (`transitive_reduce_apply_instance`, `lucky`). `sweep_guard` is a PURE
  // precondition -- `!inconsistent && level == 0` -- and deliberately does not
  // demand a root FIXPOINT, because demanding one would make the guard depend on
  // scheduling state and cost N1. So the apply completes the root closure
  // itself. Without this, Sweep could run with propagation still pending: the
  // RUP replay then ran off a non-fixpoint root, and literals it forced at level
  // 0 never went through `propagate_service`, which is the only place
  // `unit_ids[v]` is recorded. The next LRAT chain to cite one of them tripped
  // `kernel_lrat_chain`'s "cites a literal with no unit id" -- 43 crashes in the
  // random-policy LRAT leg, none in DRAT (which re-derives its own units and so
  // cannot see the defect).
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (k.level != 0 || k.conflict_valid)
    return ApplyResult::NoOp;
  SweepRound round;
  SweepLimits lim = sweep_limits_from_env();
  // Honour the effort ledger. `round_ticks` defaults to 0 = unbounded,
  // which is right for the diagnostic probe and catastrophic for a scheduled
  // rule; `Kernel::work_budget` is what the policy hands a budget-aware rule.
  if (k.work_budget)
    lim.round_ticks = k.work_budget;
  const uint32_t found = sweep_detect_round(k, lim, round);
  if (!found)
    return ApplyResult::NoOp;
  // A refuted environment means F itself is UNSAT; detection records it and the
  // caller re-derives it through the normal root path rather than here, so this
  // apply stays a pure clause-addition (proof delta: two `derive`s per pair).
  if (k.inconsistent)
    return ApplyResult::Unsat;
  // TACTIX_SWEEP_FORCE_GENERAL: route EVERY proved equivalence through S-e by
  // making the S-d fast path decline. A COVERAGE knob for the sweep stress
  // policy, not a dosing one -- both paths justify the same two binaries, so
  // the formula is identical either way and only the proof lines differ. It
  // exists because S-e fires for the pairs RUP declines, which is ~8 in 4380 on
  // fuzz-scale formulas: without it a green general-path leg is vacuous, which
  // is how the 2026-08-05 N2 breach shipped past 14000 green fuzz cases.
  const bool force_general = std::getenv("TACTIX_SWEEP_FORCE_GENERAL") != nullptr;
  std::vector<uint64_t> chain_fwd, chain_rev;
  bool any = false;
  size_t idx = SIZE_MAX;
  for (const SweepEquiv &e : round.found) {
    idx++;
    if (k.level != 0 || k.conflict_valid)
      break; // replay needs a quiescent root; stop rather than assert
    if (congruence_binary_exists(k, negate(e.a), e.b) &&
        congruence_binary_exists(k, e.a, negate(e.b))) {
      k.stats.sweep_known++;
      continue;
    }
    // ROOT-ASSIGNED LITERALS ARE NOT OURS (found by fuzz 1583559, 49 crashes).
    // Detection runs over the seed set, which excludes fixed variables -- but
    // emission happens after the whole round, and by then an earlier pair can
    // have fixed one of these literals through the binaries it added, or the
    // caller can have propagated. Adding a binary over a root-assigned literal
    // turns it into a root unit that nothing recorded an id for, and the next
    // LRAT chain to cite that literal trips
    // `kernel_lrat_chain`'s "cites a literal with no unit id". A fixed
    // literal's equivalences belong to V1 Probe / V2 Backbone anyway.
    if (k.value(e.a) != 0 || k.value(e.b) != 0)
      continue;
    if (!force_general &&
        sweep_emit_equiv(k, e.a, e.b, chain_fwd, chain_rev)) {
      any = true; // RUP over F, the cheap case
      continue;
    }
    k.stats.sweep_not_rup++;
    // Not RUP, so replay the sub-solver's own refutation. Validate BOTH
    // directions DRY before writing either, so the pair lands whole or not at all.
    if (e.a == e.b || e.a == negate(e.b) || idx >= round.proofs.size())
      continue;
    const SweepPairProof &pp = round.proofs[idx];
    // The two captured targets must BE the equivalence -- one (¬a∨b) and the
    // other (a∨¬b), in either order. Verified rather than assumed, so a capture
    // that drifted out of step with `found` declines instead of emitting a
    // binary the sub-solver did not prove.
    auto is_pair = [&](const SweepDirProof &p, Lit u, Lit v) {
      return (p.t0 == u && p.t1 == v) || (p.t0 == v && p.t1 == u);
    };
    const Lit f0 = negate(e.a), f1 = e.b, r0 = e.a, r1 = negate(e.b);
    if (!((is_pair(pp.fwd, f0, f1) && is_pair(pp.rev, r0, r1)) ||
          (is_pair(pp.fwd, r0, r1) && is_pair(pp.rev, f0, f1))))
      continue;
    if (!sweep_emit_general(k, pp.fwd, pp.fwd.t0, pp.fwd.t1, true) ||
        !sweep_emit_general(k, pp.rev, pp.rev.t0, pp.rev.t1, true))
      continue; // no general proof either: decline, F unchanged
    if (sweep_emit_general(k, pp.fwd, pp.fwd.t0, pp.fwd.t1, false) &&
        sweep_emit_general(k, pp.rev, pp.rev.t0, pp.rev.t1, false)) {
      k.stats.sweep_general++;
      any = true;
    }
  }
  return any ? ApplyResult::Ok : ApplyResult::NoOp;
}

TACTIX_REGISTER_RULE(Sweep, RuleTier::Inprocess, sweep_guard, sweep_apply,
                     nullptr, nullptr);

} // namespace tactix

// Sweep: candidate selection, environment construction and emission. Contract,
// soundness argument and deviation ledger: sweep.hpp.
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/sweep/sweep.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <utility>

namespace tactix {

namespace {

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

void repr_unite(SweepRound &r, Lit a, Lit b) {
  Lit ra = repr_find(r, a), rb = repr_find(r, b);
  if (ra == rb)
    return;
  // `rb == ¬ra` means this round proved both x ≡ y and x ≡ ¬y, possible only
  // when F is unsatisfiable. Merging would make repr a two-cycle that hangs
  // `repr_find` forever, so detection draws no conclusion and the classes stay
  // apart. Emission must treat the case as a refutation, not as two binaries.
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
  e.clause_lits.clear();
  e.clause_off.assign(1, 0);
  e.strip_ids.clear();
  e.strip_off.assign(1, 0);
}

// Sized so the index never passes half full — at most `max_env_clauses` keys
// are inserted — so linear probing stays short and the insert cannot loop.
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
// it — always sound, since the environment only ever shrinks, which is why
// every cap here is a plain early return. `strip` (may be null) are the proof
// ids justifying the literals the caller already removed.
bool env_add(Kernel &k, SweepEnv &e, const Lit *lits, uint32_t n, uint64_t key,
             uint64_t id, uint32_t depth, const SweepLimits &lim,
             uint64_t *ticks, const std::vector<uint64_t> *strip = nullptr) {
  if (e.nclauses() >= lim.max_env_clauses)
    return false;
  *ticks += 1;
  k.stats.sweep_ticks_dedup += 1;
  const size_t slot = keyset_slot(e, key);
  if (e.key_slots[slot] == key)
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
  if (strip)
    e.strip_ids.insert(e.strip_ids.end(), strip->begin(), strip->end());
  e.strip_off.push_back(static_cast<uint32_t>(e.strip_ids.size()));
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
                    const SweepLimits &lim, uint64_t *ticks) {
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
               lim, ticks, &e.strip_scratch))
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

bool SweepBudget::over(const Kernel &k, uint64_t pending) const {
  // `pending` is the scan charge accumulated but not yet folded into
  // `Stats::sweep_ticks`, so the ceiling binds during a scan and not only after
  // it.
  return limit && k.stats.sweep_ticks + pending - start > limit;
}

bool sweep_guard(const Kernel &k) { return !k.inconsistent && k.level == 0; }

// The variable's large-clause + watch degree, both polarities. Without the
// phase occurrence index only the watch lists are readable, which still ranks
// binaries and the two watched literals of every large clause.
static uint64_t seed_degree(const Kernel &k, Var v, bool &both_polarities) {
  const Lit pos = make_lit(v, false), neg = make_lit(v, true);
  uint64_t dp = k.watches.size(pos), dn = k.watches.size(neg);
  if (k.dense && k.dense->large.size() >= 2 * static_cast<size_t>(k.nvars)) {
    dp += k.dense->large[pos].size();
    dn += k.dense->large[neg].size();
  }
  both_polarities = dp && dn;
  return dp + dn;
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
  std::vector<std::pair<uint64_t, Var>> ranked;
  ranked.reserve(out.size());
  for (const Var v : out) {
    bool both = false;
    const uint64_t d = seed_degree(k, v, both);
    ranked.emplace_back(both ? d : UINT64_MAX, v);
  }
  // `std::sort` suffices: the pair's second component breaks every tie.
  std::sort(ranked.begin(), ranked.end());
  for (size_t i = 0; i < ranked.size(); i++)
    out[i] = ranked[i].second;
}

void sweep_round_begin(Kernel &k, SweepRound &r) {
  // Occurrence index over irredundant large clauses. Irredundant only, because
  // `Reduce` may delete a learned clause between detection and emission and a
  // chain citing a deleted id is a rejected proof. Outside a simplification
  // phase a single arena pass builds the same thing, so the detector stays
  // usable at any state (N1).
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
  // `proofs` is parallel to `found` (`sweep_apply` indexes both by the same
  // `idx`), so it must be reset with it: clearing one and not the other leaves
  // every later pair reading the previous round's refutation.
  r.proofs.clear();
}

bool sweep_env_build(Kernel &k, Var seed, const SweepLimits &lim,
                     SweepRound &r) {
  assert(r.occ && "sweep_round_begin must run first");
  SweepEnv &e = r.env;
  env_reset(k, e);
  keyset_reset(e, lim.max_env_clauses);
  if (root_fixed(k, seed) || kernel_var_retired(k, seed))
    return false;
  env_var(e, seed, 0);
  uint64_t ticks = 0;
  // Breadth-first over the clause neighbourhood (kissat sweep.c
  // sweep_binary/sweep_reference). `vars` doubles as the BFS queue, appended in
  // non-decreasing depth; fixed variables carry UINT32_MAX, so the frontier
  // never expands through one.
  for (uint32_t i = 0; i < e.vars.size() && !r.budget.over(k, ticks); i++) {
    const uint32_t d = e.depth[i];
    if (d > lim.max_depth)
      continue;
    const Var v = e.vars[i];
    for (uint32_t pol = 0; pol < 2 && !r.budget.over(k, ticks); pol++) {
      const Lit l = make_lit(v, pol != 0);
      ticks += k.watches.size(l);
      for (const WatchEntry w : k.watches.entries(l)) {
        if (!w.binary())
          continue; // large clauses come from the occurrence index below
        const Lit o = w.other();
        if (k.values[l] > 0 || k.values[o] > 0)
          continue; // root-satisfied: it constrains nothing
        const Lit pair[2] = {l, o};
        env_add_clause(k, e, pair, 2, kernel_binary_key(l, o),
                       kernel_binary_id(k, l, o), d + 1, lim, &ticks);
      }
      for (const Ref ref : (*r.occ)[l]) {
        // The occurrence walk is the unbounded term: `max_env_vars` stops the
        // environment growing long before this list ends. Poll here, not
        // merely between seeds.
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
        env_add_clause(k, e, c->begin(), c->size, KEY_LARGE | ref, c->id(),
                       d + 1, lim, &ticks);
      }
    }
  }
  k.stats.sweep_ticks += ticks;
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
  if (tl.size() != 2)
    return; // a unit target is a stronger fact and a different proof delta
  out.t0 = sweep_kernel_lit(e.vars, tl[0]);
  out.t1 = sweep_kernel_lit(e.vars, tl[1]);
  out.steps = std::move(d.steps);
  out.orig_ids = e.clause_ids;
  out.strip_ids = e.strip_ids;
  out.strip_off = e.strip_off;
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
    // The environment's own numbering is the sub-solver's, which is what makes
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
    k.stats.sweep_ticks_sub += e.solver.ticks();
    return 0;
  }
  if (res == SubResult::Unsat) {
    // The environment alone refutes F. Turning that into `add-empty` is a
    // separate proof obligation nothing here discharges, so detection touches
    // nothing — the same reason there is no backbone half: those root units are
    // V1 Probe / V2 Backbone's proof delta.
    k.stats.sweep_env_unsat++;
    k.stats.sweep_ticks += e.solver.ticks();
    k.stats.sweep_ticks_sub += e.solver.ticks();
    return 0;
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
  while (!r.candidates.empty() && solved < lim.max_candidates) {
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
      repr_unite(r, seed_lit, y);
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
    res = e.solver.solve(assumptions, lim.env_ticks);
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
    res = e.solver.solve(assumptions, lim.env_ticks);
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
    repr_unite(r, seed_lit, y);
    r.candidates.pop_back();
  }
  k.stats.sweep_ticks += e.solver.ticks();
  k.stats.sweep_ticks_sub += e.solver.ticks();
  return found;
}

uint32_t sweep_detect_round(Kernel &k, const SweepLimits &lim, SweepRound &r) {
  if (!sweep_guard(k))
    return 0;
  k.stats.sweep_rounds++;
  sweep_round_begin(k, r);
  sweep_seeds(k, r.seeds);
  const uint64_t start = k.stats.sweep_ticks;
  // The between-seeds check is unconditional; `budget` carries the same ceiling
  // into `sweep_env_build`, so one seed cannot outspend the round before the
  // next check. `enforce_in_env=0` reverts only the inner poll.
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
  if (const char *s = std::getenv("TACTIX_SWEEP_STRIP"))
    lim.strip_false = std::strtoul(s, nullptr, 10) != 0;
  if (const char *s = std::getenv("TACTIX_SWEEP_CLOSURE"))
    lim.measure_closure = std::strtoul(s, nullptr, 10) != 0;
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

// Emission, the general path (`docs/calculus.md` V5, strategy (b)): replay the
// sub-solver's refutation into the main proof, each lemma its own line in
// derivation order, antecedents translated to main-formula proof ids. The final
// lemma is the target binary, by which point the checker has seen every
// antecedent it names. Anything not mappable exactly declines rather than
// emits. `dry` validates without emitting or consuming an id, so a pair is
// checked whole before either half is written.
static bool sweep_emit_general(Kernel &k, const SweepDirProof &p,
                               const Lit t0, const Lit t1, bool dry) {
  if (!p.ok || p.steps.empty())
    return false;
  // The strip ledger is indexed by original clause, one CSR slot each. A
  // capture that does not satisfy that cannot be translated, so decline rather
  // than index past it.
  if (p.strip_off.size() != p.orig_ids.size() + 1)
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
      return false; // uncheckable under LRAT; decline the whole pair
    chain.clear();
    // Strip units first: a unit is unit under every assignment, hence valid at
    // any position in an LRAT hint list, and front-loading them is the only
    // order independent of which antecedent needs which unit.
    for (const uint32_t c : st.chain) {
      if (c == 0 || c > p.orig_ids.size())
        continue; // a derived lemma carries no strip ledger
      for (uint32_t s = p.strip_off[c - 1]; s < p.strip_off[c]; s++) {
        const uint64_t uid = p.strip_ids[s];
        if (std::find(chain.begin(), chain.end(), uid) == chain.end()) {
          chain.push_back(uid);
          if (!dry && lrat)
            k.stats.sweep_strip_cited++;
        }
      }
    }
    for (const uint32_t c : st.chain) {
      if (c >= id_of.size() || id_of[c] == 0)
        return false; // an antecedent with no name in the main proof
      chain.push_back(id_of[c]);
    }
    const bool final_step = (si + 1 == p.steps.size());
    if (final_step) {
      // The target must be exactly the binary being justified. A shorter target
      // (a unit) is a stronger fact but a different proof delta, and it would
      // have been RUP anyway, so the fast path already had it.
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

namespace {
// Emission, shared by the canonical round and the per-seed instance so the two
// cannot drift in what they are willing to write.
ApplyResult sweep_emit_round(Kernel &k, SweepRound &round, uint32_t found) {
  if (!found)
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
  bool any = false;
  size_t idx = SIZE_MAX;
  for (const SweepEquiv &e : round.found) {
    idx++;
    if (k.level != 0 || k.conflict_valid)
      break; // replay needs a quiescent root; stop rather than assert
    if (kernel_binary_live(k, negate(e.a), e.b) &&
        kernel_binary_live(k, e.a, negate(e.b))) {
      k.stats.sweep_known++;
      continue;
    }
    // Never emit over a root-assigned literal. Detection excludes fixed
    // variables, but emission runs after the whole round, by which time an
    // earlier pair's binaries may have fixed one — and such a binary becomes a
    // root unit with no recorded id, tripping the next LRAT chain that cites
    // that literal.
    if (k.value(e.a) != 0 || k.value(e.b) != 0)
      continue;
    if (!force_general &&
        sweep_emit_equiv(k, e.a, e.b, chain_fwd, chain_rev)) {
      any = true; // RUP over F, the cheap case
      continue;
    }
    k.stats.sweep_not_rup++;
    // Not RUP, so replay the sub-solver's own refutation. Both directions are
    // validated dry before either is written, so the pair lands whole or not.
    if (e.a == e.b || e.a == negate(e.b) || idx >= round.proofs.size())
      continue;
    const SweepPairProof &pp = round.proofs[idx];
    // The two captured targets must be the equivalence, in either order.
    // Verified rather than assumed, so a capture that drifted out of step with
    // `found` declines instead of emitting an unproved binary.
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
} // namespace

ApplyResult sweep_apply(Kernel &k) {
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
  SweepLimits lim = sweep_limits_from_env();
  // Honour the effort ledger. `round_ticks` defaults to 0 = unbounded,
  // which is right for the diagnostic probe and catastrophic for a scheduled
  // rule; `Kernel::work_budget` is what the policy hands a budget-aware rule.
  if (k.work_budget)
    lim.round_ticks = k.work_budget;
  const uint32_t found = sweep_detect_round(k, lim, round);
  return sweep_emit_round(k, round, found);
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
  const uint32_t found = sweep_detect_seed(k, seed, lim, round);
  return sweep_emit_round(k, round, found);
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

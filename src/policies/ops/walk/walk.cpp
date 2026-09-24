// Transliterated from kissat/src/walk.c (MIT, Biere et al.) — the ProbSAT
// flipper, its break-value scoring and its CB score table. The walker owns its
// scratch values and CSR occurrence index, leaving search guidance and the
// kernel trail separate. Walk is a root-phase operation because its dense-entry
// and sparse-resume normalization rewrites the formula representation.
#include "policies/ops/walk/walk.hpp"
#include "kernel/clause.hpp"
#include "policies/heuristics.hpp"

#include <cmath>

namespace tactix {

namespace {

// Kissat's seed-0 LCG.  Sharing both the recurrence and the 32-bit projection
// is necessary for the selected unsatisfied clause and flip literal to agree.
inline uint64_t next_random(uint64_t &s) {
  s *= UINT64_C(6364136223846793005);
  s += UINT64_C(1442695040888963407);
  return s;
}
// uniform double in [0,1) from the top 53 bits (kissat_pick_double)
inline double pick_double(uint64_t &s) {
  return static_cast<uint32_t>(next_random(s) >> 32) / 4294967296.0;
}

// The walk table: every clause that is neither satisfied nor emptied by the
// trail, flattened into CSR literal storage plus a CSR literal->clause index.
struct Walker {
  const Kernel *k = nullptr;

  std::vector<Lit> cl_lits;     // flat clause literals (free literals only)
  std::vector<uint32_t> cl_beg; // clauses+1 offsets into cl_lits
  std::vector<uint32_t> occ;    // flat clause indices
  std::vector<uint32_t> occ_beg; // 2*nvars+1 offsets into occ

  std::vector<uint32_t> count; // by clause: # true literals
  std::vector<uint32_t> pos;   // by clause: index in `unsat` (iff count == 0)
  std::vector<uint32_t> unsat; // stack of unsatisfied clause indices

  std::vector<int8_t> values;      // by lit: +1 true, -1 false (never 0 here)
  std::vector<int8_t> best_values; // by var: the assignment at `minimum`
  std::vector<Lit> pending;        // flips applied since best_values was synced
  uint32_t best_pending = 0;       // prefix of `pending` that reaches `minimum`
  bool pending_valid = true;       // false: best_values must be resynced wholesale

  std::vector<double> table; // scale_score: table[breaks]
  double epsilon = 1.0;
  double score_base = 0.5;
  bool increasing_scores = false;

  uint32_t nclauses = 0;
  uint32_t binary_clauses = 0; // root-open, deduplicated (kissat BINARY_CLAUSES)
  uint32_t full_irredundant_clauses = 0; // large, garbage excluded, root-satisfied kept
  uint32_t initial = 0, minimum = 0;
  uint32_t initial_binary_unsat = 0;
  uint64_t flips = 0, steps = 0;
  uint64_t rng = 0;

  uint32_t nlits() const { return 2 * k->nvars; }
};

// --- table construction ----------------------------------------------------

// Collect the walkable clauses. Binaries live only in watch lists, so they are
// enumerated from there with an `other > lit` dedup; large irredundant clauses
// come from one arena scan.
bool build_table(Walker &w, Kernel &k) {
  const uint32_t nlits = 2 * k.nvars;
  double total_size = 0;

  w.cl_beg.push_back(0);
  const auto emit = [&](const Lit *lits, uint32_t n) {
    uint32_t free_lits = 0;
    for (uint32_t i = 0; i < n; i++) {
      const int8_t v = k.value(lits[i]);
      if (v > 0)
        return false; // satisfied by the trail: not the walker's problem
      if (v == 0)
        free_lits++;
    }
    if (!free_lits)
      return false; // falsified by the trail: the search's problem, not the walk's
    for (uint32_t i = 0; i < n; i++)
      if (k.value(lits[i]) == 0)
        w.cl_lits.push_back(lits[i]);
    w.cl_beg.push_back(static_cast<uint32_t>(w.cl_lits.size()));
    total_size += free_lits;
    return true;
  };

  // Kissat enters dense mode after compacting its internal variable universe.
  // Tactix keeps stable internal identities, but the permanent import ranks
  // are precisely that compact-order key.  Enumerate by those ranks so the
  // binary counter order (and therefore the deterministic unsatisfied stack)
  // is representation-independent and matches Kissat.
  std::vector<Var> import_order;
  kernel_vars_in_import_order(k, import_order);
  for (const Var v : import_order) {
    for (uint32_t sign = 0; sign < 2; sign++) {
      const Lit l = make_lit(v, sign != 0);
      for (const WatchEntry wt : k.watches.entries(l)) {
        if (!wt.binary())
          continue;
        const Lit other = wt.other();
        if (kernel_import_lit_order(k, other) <
            kernel_import_lit_order(k, l))
          continue; // dedup: each binary is seen from both watch lists
        const Lit pair[2] = {l, other};
        w.binary_clauses += emit(pair, 2);
      }
    }
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant()) {
      w.full_irredundant_clauses++;
      bool root_satisfied = false;
      for (const Lit lit : *c)
        root_satisfied |= kernel_root_value(k, lit) > 0;
      if (root_satisfied) {
        // Kissat's dense Walk entry keeps the pre-collection irredundant
        // count as the score-table denominator, then `connect_large_counters`
        // retires every irredundant clause satisfied by the original root
        // assignment.  Merely omitting such a clause from the private CSR
        // leaves a semantically dead clause in sparse search and changes both
        // propagation and later substitution.  Retire it at the same point;
        // the resume rebuild below discards its stale watches.
        //
        // A trail reason may not name a deleted clause (kernel.hpp
        // kernel_retire_root_reason); Walk runs at a quiescent root.
        kernel_retire_root_reason(k, c, ref);
        kernel_delete_clause(k, c);
      } else {
        emit(c->lits, c->size);
      }
    }
    ref += clause_actual_words(c);
  }

  w.nclauses = static_cast<uint32_t>(w.cl_beg.size() - 1);
  if (!w.nclauses)
    return false;

  // Counting sort into the CSR occurrence index.
  w.occ_beg.assign(nlits + 1, 0);
  for (const Lit l : w.cl_lits)
    w.occ_beg[l + 1]++;
  for (uint32_t l = 0; l < nlits; l++)
    w.occ_beg[l + 1] += w.occ_beg[l];
  w.occ.resize(w.cl_lits.size());
  {
    std::vector<uint32_t> fill(w.occ_beg.begin(), w.occ_beg.end() - 1);
    for (uint32_t c = 0; c < w.nclauses; c++)
      for (uint32_t i = w.cl_beg[c]; i < w.cl_beg[c + 1]; i++)
        w.occ[fill[w.cl_lits[i]]++] = c;
  }

  // The ProbSAT score table, base 1/cb (kissat init_score_table): cb alternates
  // between the fixed 2.0 and the size fit across walks.  The average divides
  // by kissat's BINIRR_CLAUSES as read at init_walker: dense entry
  // (dense.c:flush_large_watches) has already deleted root-satisfied and
  // duplicate binaries, while root-satisfied large clauses are only retired
  // afterwards in connect_large_counters and so still count.
  const uint32_t denominator = w.binary_clauses + w.full_irredundant_clauses;
  const double avg = total_size / denominator;
  const strategy::WalkParameters parameters =
      strategy::WalkParameters::from_kernel(k);
  const double cb = walk_break_base(parameters, avg, k.stats.walks);
  const double base = 1 / cb;
  w.score_base = base;
  w.increasing_scores = base >= 1;
  if (w.increasing_scores)
    return true;
  // break_value sums at most one per entry in the opposite literal's
  // occurrence list. Scores above the largest such list cannot be queried.
  // Bound the stored prefix by that structural maximum, not by a numerical
  // underflow horizon: a legal fitted cb can be only one ULP above 1.
  uint32_t maximum_breaks = 0;
  for (uint32_t l = 0; l < nlits; ++l)
    maximum_breaks = std::max(maximum_breaks, w.occ_beg[l + 1] - w.occ_beg[l]);
  double next = 1;
  for (w.epsilon = 1; next && w.table.size() <= maximum_breaks;) {
    w.table.push_back(w.epsilon = next);
    const double candidate = w.epsilon * base;
    // For bases in (0.5, 1), the smallest subnormal can round back to itself
    // instead of underflowing to zero. Its constant tail is already represented
    // by epsilon, so stop at that floating-point fixed point.
    if (candidate == w.epsilon)
      break;
    next = candidate;
  }
  return true;
}

// Seed the walker assignment: trail values where assigned, otherwise the
// decision phase the search itself would pick (target when stable, else saved).
void seed_values(Walker &w, const Kernel &k) {
  w.values.resize(2 * k.nvars);
  w.best_values.resize(k.nvars);
  for (Var v = 0; v < k.nvars; v++) {
    const Lit pos_lit = make_lit(v, false);
    int8_t val = k.value(pos_lit);
    if (!val)
      val = negated(heuristics_decide_lit(k, v)) ? -1 : 1;
    w.values[pos_lit] = val;
    w.values[pos_lit ^ 1] = static_cast<int8_t>(-val);
    w.best_values[v] = val;
  }
}

void init_counters(Walker &w) {
  w.count.assign(w.nclauses, 0);
  w.pos.assign(w.nclauses, 0);
  for (uint32_t c = 0; c < w.nclauses; c++) {
    uint32_t t = 0;
    for (uint32_t i = w.cl_beg[c]; i < w.cl_beg[c + 1]; i++)
      t += (w.values[w.cl_lits[i]] > 0);
    w.count[c] = t;
    if (!t) {
      w.pos[c] = static_cast<uint32_t>(w.unsat.size());
      w.unsat.push_back(c);
    }
  }
  w.initial = w.minimum = static_cast<uint32_t>(w.unsat.size());
  for (uint32_t c = 0; c < w.binary_clauses; c++)
    w.initial_binary_unsat += w.count[c] == 0;
}

// --- the flip loop ---------------------------------------------------------

inline void push_unsat(Walker &w, uint32_t c) {
  w.pos[c] = static_cast<uint32_t>(w.unsat.size());
  w.unsat.push_back(c);
}
inline bool pop_unsat(Walker &w, uint32_t c) {
  const uint32_t p = w.pos[c];
  const uint32_t last = w.unsat.back();
  w.unsat[p] = last;
  w.pos[last] = p;
  w.unsat.pop_back();
  return last != c;
}

// How many clauses flipping `lit` to true would falsify: those satisfied by
// exactly one literal, namely ~lit (kissat break_value).
inline uint32_t break_value(Walker &w, Lit lit) {
  const Lit nl = negate(lit);
  uint32_t res = 0;
  const uint32_t end = w.occ_beg[nl + 1];
  for (uint32_t i = w.occ_beg[nl]; i < end; i++)
    res += (w.count[w.occ[i]] == 1);
  w.steps += end - w.occ_beg[nl] + 1;
  return res;
}

inline double scale_score(const Walker &w, uint32_t breaks) {
  return breaks < w.table.size() ? w.table[breaks] : w.epsilon;
}

// roulette-wheel pick over exp(-breaks) scores within one unsatisfied clause
Lit pick_literal(Walker &w, std::vector<double> &scores) {
  // Kissat increments walker->flipped once in local_search_step and once more
  // in pick_literal before taking the modulo.  Thus its first positions are
  // 1, 3, 5, ... rather than 0, 1, 2, ... .  Keep `flips` as the true number
  // of flips for Tactix statistics, but reproduce that selection sequence.
  const uint64_t selection = 2 * w.flips + 1;
  const uint32_t pos = selection % w.unsat.size();
  const uint32_t c = w.unsat[pos];
  const uint32_t beg = w.cl_beg[c], end = w.cl_beg[c + 1];
  scores.clear();
  if (w.increasing_scores) {
    uint32_t maximum = 0;
    for (uint32_t i = beg; i < end; i++) {
      const uint32_t breaks = break_value(w, w.cl_lits[i]);
      maximum = std::max(maximum, breaks);
      scores.push_back(static_cast<double>(breaks));
    }
    // cb <= 1 rewards larger break counts. Divide every weight by the largest
    // weight in this clause before roulette selection, keeping the exact
    // distribution while preventing overflow. At cb == 1 every score is one.
    for (double &score : scores)
      score = std::pow(w.score_base, score - maximum);
  } else {
    for (uint32_t i = beg; i < end; i++)
      scores.push_back(scale_score(w, break_value(w, w.cl_lits[i])));
  }
  double sum = 0;
  for (const double score : scores)
    sum += score;
  const double random = pick_double(w.rng);
  const double threshold = sum * random;
  double acc = 0;
  Lit picked = w.cl_lits[end - 1];
  for (uint32_t i = beg; i < end; i++) {
    acc += scores[i - beg];
    if (threshold < acc) {
      picked = w.cl_lits[i];
      break;
    }
  }
  return picked; // rounding: fall back to the last literal
}

// Keep `best_values` recoverable without an O(nvars) copy per improvement:
// remember the flips since the last sync and replay only the winning prefix
// (kissat's walker trail, save_walker_trail).
void sync_best(Walker &w) {
  for (uint32_t i = 0; i < w.best_pending; i++) {
    const Lit l = w.pending[i];
    w.best_values[var_of(l)] = negated(l) ? -1 : 1;
  }
  w.pending.erase(w.pending.begin(), w.pending.begin() + w.best_pending);
  w.best_pending = 0;
}

void record_improvement(Walker &w) {
  w.minimum = static_cast<uint32_t>(w.unsat.size());
  if (w.pending_valid) {
    w.best_pending = static_cast<uint32_t>(w.pending.size());
    return;
  }
  // The trail was invalidated: resync wholesale, then resume recording.
  for (Var v = 0; v < w.k->nvars; v++)
    w.best_values[v] = w.values[make_lit(v, false)];
  w.pending.clear();
  w.best_pending = 0;
  w.pending_valid = true;
}

void push_flip(Walker &w, Lit lit) {
  if (!w.pending_valid)
    return;
  const size_t cap = w.k->nvars / 4 + 1;
  if (w.pending.size() >= cap) {
    if (w.best_pending)
      sync_best(w); // shift the winning prefix into best_values, keep the rest
    else {
      w.pending.clear(); // nothing worth keeping and the trail is too long
      w.pending_valid = false;
      return;
    }
  }
  w.pending.push_back(lit);
}

void flip(Walker &w, Lit lit) {
  w.values[lit] = 1;
  w.values[negate(lit)] = -1;
  { // clauses containing `lit` gain a true literal
    const uint32_t end = w.occ_beg[lit + 1];
    for (uint32_t i = w.occ_beg[lit]; i < end; i++) {
      const uint32_t c = w.occ[i];
      if (!w.count[c]++ && pop_unsat(w, c))
        w.steps++; // Kissat charges the replacement write in pop_unsat.
    }
    w.steps += end - w.occ_beg[lit] + 1;
  }
  { // clauses containing ~lit lose one
    const Lit nl = negate(lit);
    const uint32_t end = w.occ_beg[nl + 1];
    for (uint32_t i = w.occ_beg[nl]; i < end; i++) {
      const uint32_t c = w.occ[i];
      if (!--w.count[c])
        push_unsat(w, c);
    }
    w.steps += end - w.occ_beg[nl] + 1;
  }
}

void local_search(Walker &w, uint64_t limit) {
  std::vector<double> scores;
  while (!w.unsat.empty() && w.steps < limit) {
    const Lit lit = pick_literal(w, scores);
    w.flips++;
    flip(w, lit);
    push_flip(w, lit);
    if (w.unsat.size() < w.minimum)
      record_improvement(w);
  }
}

} // namespace

double walk_break_base(const strategy::WalkParameters &parameters,
                       double average_size, uint64_t walk_count) {
  assert(parameters.valid());
  constexpr double scale = strategy::WalkParameters::BREAK_BASE_SCALE;
  if (!(walk_count & 1))
    return parameters.even_break_base_milli / scale;
  const auto &curve = parameters.score_curve;
  size_t i = 0;
  const auto x = [scale](const strategy::WalkParameters::ScorePoint point) {
    return point.average_clause_size_milli / scale;
  };
  const auto y = [scale](const strategy::WalkParameters::ScorePoint point) {
    return point.break_base_milli / scale;
  };
  while (i + 2 < curve.size() &&
         (x(curve[i]) > average_size || x(curve[i + 1]) < average_size))
    ++i;
  const double x1 = x(curve[i]), x2 = x(curve[i + 1]);
  const double y1 = y(curve[i]), y2 = y(curve[i + 1]);
  return (y2 - y1) * (average_size - x1) / (x2 - x1) + y1;
}

// Requires a consistent root state and `H` for the phase arrays. A caller in
// search explicitly composes Restart before selecting Walk.
RuleEligibility walk_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("root-level", k.level == 0)
      .fact("heuristics-attached", k.h != nullptr)
      .fact("has-variables", k.nvars > 0);
}

static ApplyResult walk_apply_exact_steps(Kernel &k, uint64_t steps) {
  assert(walk_guard(k));
  k.stats.walks++;
  // Kissat's non-null dense entry removes repeated binary watches before it
  // saves the irredundant pairs consumed by Walk.  Deduplicate at the same
  // boundary so copies neither skew the local-search counters nor survive the
  // sparse-resume rebuild as extra live clauses.
  kernel_deduplicate_binary_watches(k);
  Walker w;
  w.k = &k;
  w.rng = k.walk_random_seed ^ k.stats.walks;
  bool improved = false;
  if (build_table(w, k)) {
    seed_values(w, k);
    init_counters(w);
    local_search(w, steps);
    // Kissat's walker temporarily repurposes every large-watch list as an
    // occurrence index.  Leaving dense mode consequently reconnects all large
    // clauses in arena order and reselects their watched pair.  That
    // representation-only delta is search-visible: omitting it changed the
    // very first post-Walk implication order on the Tier-B witness even when
    // every Walk flip matched.  Reproduce the sparse-resume side effect after
    // the private CSR walker has finished.
    kernel_resume_walk_watches(k);
    k.stats.walk_flips += w.flips;
    k.stats.walk_steps += w.steps;
    k.stats.walk_minimum = w.minimum;
    if (w.minimum < w.initial) {
      sync_best(w); // materialise the winning prefix into best_values
      for (Var v = 0; v < k.nvars; v++)
        if (!k.value(make_lit(v, false))) // fixed literals keep their phase
          k.h->saved[v] = w.best_values[v];
      k.stats.walk_improved++;
      improved = true;
    }
  }
  // The rephase tail every type runs (kissat reset_phases): the fresh saved
  // phases become the target ones. `best` is not consumed here — kissat resets
  // its watermark only on type Best.
  std::copy(k.h->saved.begin(), k.h->saved.end(), k.h->target.begin());
  k.h->target_assigned = 0;
  k.stats.rephased++;
  return improved ? ApplyResult::Ok : ApplyResult::NoOp;
}

ApplyResult walk_apply_steps(Kernel &k, uint64_t steps) {
  return walk_apply_exact_steps(k, steps);
}

ApplyResult walk_apply(Kernel &k) {
  const uint64_t steps = kernel_has_work_budget(k) ? k.work_budget
      : strategy::WalkParameters::STEP_BUDGET_DEFAULT;
  return walk_apply_exact_steps(k, steps);
}

// Instance surface (plan §5.1): the payload is the step budget, so the §5.6
// learner can dose a walk as it doses every other rule. The enumeration offers
// the canonical dose; apply_instance is total for any payload.
static uint32_t walk_instances(Kernel &, uint64_t *out, uint32_t cap) {
  if (!cap)
    return 0;
  out[0] = INSTANCE_CANONICAL;
  return 1;
}

static ApplyResult walk_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return walk_apply(k);
  return walk_apply_steps(k, inst);
}

// Policy operation (plan §4, §5.2): `H`-only, no proof delta.
TACTIX_REGISTER_POLICY_OP(Walk, RuleTier::Macro, walk_guard, walk_apply,
                          walk_instances, walk_apply_instance);

} // namespace tactix

// Transliterated from kissat/src/walk.c (MIT, Biere et al.) — the ProbSAT
// flipper, its break-value scoring and its CB score table. The walker owns its
// scratch values and CSR occurrence index, leaving search guidance and the
// kernel trail separate. Walk is a root-phase operation because its dense-entry
// and sparse-resume normalization rewrites the formula representation.
#include "policies/ops/walk/walk.hpp"

#include "strategy/configuration_checking_model.hpp"
#include "kernel/clause.hpp"
#include "policies/heuristics.hpp"

#include <array>
#include <cmath>
#include <random>

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

// The opt-in full-CNF profile owns a separate preparation meter. It never
// performs the legacy mutating dense-entry normalization on a declined dose.
struct PreparationMeter {
  Kernel &kernel;
  uint64_t limit, used = 0;
  bool stopped = false;
  bool charge(uint64_t amount = 1) {
    if (stopped) return false;
    if (kernel_rule_interrupt(kernel, kernel.stats.walk_seed_interrupted)) {
      stopped = true;
      return false;
    }
    if (amount > limit - used) {
      ++kernel.stats.walk_seed_budget_exhausted;
      ++kernel.stats.walk_preparation_budget_exhausted;
      stopped = true;
      return false;
    }
    used += amount;
    return true;
  }
};

// One mirror per physical binary occurrence and every live arena clause form
// the same current-CNF objective for both assignments and the entire walk.
// Duplicate physical clauses have their current multiplicity in both scores.
bool build_full_table(Walker &w, Kernel &k, PreparationMeter &meter) {
  const uint32_t nlits = 2 * k.nvars;
  if (!meter.charge()) return false;
  w.cl_beg.push_back(0);
  const auto emit = [&](const Lit *literals, uint32_t size) {
    bool root_satisfied = false;
    uint32_t free = 0;
    for (uint32_t i = 0; i < size; ++i) {
      if (!meter.charge()) return false;
      const auto value = k.value(literals[i]);
      root_satisfied |= value > 0;
      free += value == 0;
    }
    if (root_satisfied) return true;
    // A root-falsified clause is not a local-search admission objective.
    if (!free) return false;
    if (!meter.charge(uint64_t{1} + size)) return false;
    for (uint32_t i = 0; i < size; ++i)
      if (!k.value(literals[i])) w.cl_lits.push_back(literals[i]);
    w.cl_beg.push_back(static_cast<uint32_t>(w.cl_lits.size()));
    return true;
  };
  for (Lit literal = 0; literal < nlits; ++literal) {
    if (!meter.charge()) return false;
    for (const auto watch : k.watches.entries(literal)) {
      if (!meter.charge()) return false;
      if (!watch.binary() || literal > watch.other()) continue;
      const Lit pair[] = {literal, watch.other()};
      const auto before = w.cl_beg.size();
      if (!emit(pair, 2)) return false;
      w.binary_clauses += w.cl_beg.size() != before;
    }
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    if (!meter.charge()) return false;
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && !emit(clause->lits, clause->size)) return false;
    ref += words;
  }
  w.nclauses = w.cl_beg.size() - 1;
  if (!w.nclauses) return false;
  // Charge arrays before allocation; subsequent visits have their own charge.
  if (!meter.charge(uint64_t{1} + nlits)) return false;
  w.occ_beg.assign(size_t(nlits) + 1, 0);
  for (Lit literal : w.cl_lits) {
    if (!meter.charge()) return false;
    ++w.occ_beg[literal + 1];
  }
  for (uint32_t literal = 0; literal < nlits; ++literal) {
    if (!meter.charge()) return false;
    w.occ_beg[literal + 1] += w.occ_beg[literal];
  }
  if (!meter.charge(w.cl_lits.size() + nlits)) return false;
  w.occ.resize(w.cl_lits.size());
  std::vector<uint32_t> fill(w.occ_beg.begin(), w.occ_beg.end() - 1);
  for (uint32_t clause = 0; clause < w.nclauses; ++clause) {
    if (!meter.charge()) return false;
    for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i) {
      if (!meter.charge()) return false;
      w.occ[fill[w.cl_lits[i]]++] = clause;
    }
  }
  // Snapshot copying, validation and interpolation each visit the curve.
  if (!meter.charge(uint64_t{3} * k.walk_score_curve.size())) return false;
  const auto parameters = strategy::WalkParameters::from_kernel(k);
  const double average = double(w.cl_lits.size()) / w.nclauses;
  w.score_base = 1 / walk_break_base(parameters, average, k.stats.walks);
  w.increasing_scores = w.score_base >= 1;
  if (w.increasing_scores) return true;
  uint32_t maximum_breaks = 0;
  for (uint32_t literal = 0; literal < nlits; ++literal) {
    if (!meter.charge()) return false;
    maximum_breaks = std::max(maximum_breaks,
        w.occ_beg[literal + 1] - w.occ_beg[literal]);
  }
  double next = 1;
  for (w.epsilon = 1; next && w.table.size() <= maximum_breaks;) {
    if (!meter.charge()) return false;
    w.table.push_back(w.epsilon = next);
    const double candidate = next * w.score_base;
    if (candidate == next) break;
    next = candidate;
  }
  return true;
}

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
    if (k.walk_algorithm == uint32_t(
            strategy::WalkParameters::Algorithm::ConfigurationChecking)) {
      std::vector<Lit> ordered;
      ordered.reserve(free_lits);
      for (uint32_t i = 0; i < n; ++i)
        if (!k.value(lits[i])) ordered.push_back(lits[i]);
      std::stable_sort(ordered.begin(), ordered.end(), [&](Lit a, Lit b) {
        const int external_a = kernel_externalize(k, a);
        const int external_b = kernel_externalize(k, b);
        const uint32_t variable_a = static_cast<uint32_t>(
            external_a < 0 ? -int64_t(external_a) : external_a);
        const uint32_t variable_b = static_cast<uint32_t>(
            external_b < 0 ? -int64_t(external_b) : external_b);
        return variable_a < variable_b ||
               (variable_a == variable_b && external_a < external_b);
      });
      w.cl_lits.insert(w.cl_lits.end(), ordered.begin(), ordered.end());
    } else {
      for (uint32_t i = 0; i < n; i++)
        if (k.value(lits[i]) == 0)
          w.cl_lits.push_back(lits[i]);
    }
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
  std::vector<Ref> large_order;
  for (Ref ref = 0; ref < k.arena.size();) {
    Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant()) large_order.push_back(ref);
    ref += clause_actual_words(c);
  }
  if (k.walk_algorithm == uint32_t(
          strategy::WalkParameters::Algorithm::ConfigurationChecking))
    std::stable_sort(large_order.begin(), large_order.end(), [&](Ref a, Ref b) {
      return clause_at(k.arena, a)->id() < clause_at(k.arena, b)->id();
    });
  for (const Ref ref : large_order) {
    Clause *c = clause_at(k.arena, ref);
    {
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

uint32_t decision_phase_unsatisfied(const Walker &w, const Kernel &k) {
  uint32_t result = 0;
  for (uint32_t clause = 0; clause < w.nclauses; ++clause) {
    bool satisfied = false;
    for (uint32_t i = w.cl_beg[clause];
         !satisfied && i < w.cl_beg[clause + 1]; ++i) {
      const Lit literal = w.cl_lits[i];
      const int8_t value = k.value(literal);
      satisfied = value > 0 ||
                  (!value && heuristics_decide_lit(k, var_of(literal)) == literal);
    }
    result += !satisfied;
  }
  return result;
}

// The same walkable current CNF evaluates ordinary phases and imported advice.
// Producer scores are intentionally ignored. Seed work shares the explicit
// allowance: one item per phase/identity visited and per evaluated literal.
// Nothing in this preparation writes CDCL phase memory.
bool seed_candidate(Walker &w, Kernel &k,
                    const strategy::LocalSearchHint &hint, uint64_t limit,
                    uint32_t &ordinary_unsat,
                    PreparationMeter *preparation = nullptr) {
  const auto charge = [&] {
    if (preparation) return preparation->charge();
    if (kernel_rule_interrupt(k, k.stats.walk_seed_interrupted))
      return false;
    if (w.steps == limit) {
      ++k.stats.walk_seed_budget_exhausted;
      return false;
    }
    ++w.steps;
    return true;
  };
  if (preparation && !preparation->charge(uint64_t{3} * k.nvars)) return false;
  w.values.resize(2 * k.nvars);
  w.best_values.resize(k.nvars);
  for (Var v = 0; v < k.nvars; ++v) {
    if (!charge()) return false;
    const Lit lit = make_lit(v, false);
    int8_t value = k.value(lit);
    if (!value) value = negated(heuristics_decide_lit(k, v)) ? -1 : 1;
    w.values[lit] = value;
    w.values[lit ^ 1] = static_cast<int8_t>(-value);
    w.best_values[v] = value;
  }
  ordinary_unsat = 0;
  for (uint32_t c = 0; c < w.nclauses; ++c) {
    if (preparation && !charge()) return false;
    bool satisfied = false;
    for (uint32_t i = w.cl_beg[c]; i < w.cl_beg[c + 1]; ++i) {
      if (!charge()) return false;
      satisfied |= w.values[w.cl_lits[i]] > 0;
    }
    ordinary_unsat += !satisfied;
  }
  uint64_t imported = 0;
  for (size_t external = 0; external < hint.external_phases.size(); ++external) {
    if (!charge()) return false;
    const int8_t phase = hint.external_phases[external];
    if (!phase || external >= k.external_to_internal.size()) continue;
    const Var v = k.external_to_internal[external];
    if (v == INVALID_VAR || v >= k.nvars || kernel_var_retired(k, v) ||
        k.value(make_lit(v, false))) continue;
    ++imported;
    w.values[make_lit(v, false)] = phase;
    w.values[make_lit(v, true)] = static_cast<int8_t>(-phase);
    w.best_values[v] = phase;
  }
  if (!imported) return false;
  if (preparation && !preparation->charge(uint64_t{3} * w.nclauses)) return false;
  if (preparation) w.unsat.reserve(w.nclauses);
  w.count.assign(w.nclauses, 0);
  w.pos.assign(w.nclauses, 0);
  for (uint32_t c = 0; c < w.nclauses; ++c) {
    if (preparation && !charge()) return false;
    uint32_t satisfied = 0;
    for (uint32_t i = w.cl_beg[c]; i < w.cl_beg[c + 1]; ++i) {
      if (!charge()) return false;
      satisfied += w.values[w.cl_lits[i]] > 0;
    }
    w.count[c] = satisfied;
    if (!satisfied) {
      if (preparation && !charge()) return false;
      w.pos[c] = static_cast<uint32_t>(w.unsat.size());
      w.unsat.push_back(c);
    }
  }
  w.initial = w.minimum = static_cast<uint32_t>(w.unsat.size());
  for (uint32_t c = 0; c < w.binary_clauses; ++c) {
    if (preparation && !charge()) return false;
    w.initial_binary_unsat += w.count[c] == 0;
  }
  ++k.stats.walk_seed_imports;
  k.stats.walk_seed_imported_phases += imported;
  k.stats.walk_seed_ordinary_unsat = ordinary_unsat;
  k.stats.walk_seed_candidate_unsat = w.initial;
  return true;
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

// Configuration-checking local search. Unlike ProbSAT's roulette over one
// violated clause, this keeps a weighted make-minus-break score per variable,
// suppresses immediate flips until a neighbor changes, and raises weights on
// persistently violated clauses. It is a second explicit Walk algorithm: the
// formula and CDCL state remain untouched until the best private assignment is
// admitted as phase guidance.
struct ConfigurationCheckingSearch {
  Walker &w;
  Kernel &k;
  const strategy::WalkParameters parameters;
  std::vector<int64_t> score;
  std::vector<uint32_t> weight;
  std::vector<Var> sole_true;
  std::vector<uint64_t> last_flip;
  std::vector<uint8_t> configuration_changed;
  std::vector<uint32_t> candidate_position;
  std::vector<Var> candidates;
  std::vector<std::vector<Var>> neighbors;
  std::vector<std::vector<std::pair<uint32_t, Lit>>> variable_occurrences;
  std::vector<uint32_t> unsat_appear;
  std::vector<uint32_t> unsat_variable_position;
  std::vector<Var> unsat_variables;
  uint64_t step = 0;
  uint64_t local_flip_step = 0;
  uint64_t average_weight = 1;
  uint64_t weight_remainder = 0;
  bool interrupted = false;
  std::mt19937 random;

  explicit ConfigurationCheckingSearch(Walker &walker, Kernel &kernel)
      : w(walker), k(kernel),
        parameters(strategy::WalkParameters::from_kernel(kernel)),
        random(static_cast<uint32_t>(2 * parameters.random_seed + 1)) {
    // Preserve first-neighbor order by clause order, as the reference's
    // variable occurrence lists do. Sign-grouped occurrence scans change
    // equal-score candidate insertion on symmetric formulas.
    std::vector<std::vector<Var>> raw(k.nvars);
    variable_occurrences.resize(k.nvars);
    for (uint32_t clause = 0; clause < w.nclauses; ++clause)
      for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i) {
        const Var variable = var_of(w.cl_lits[i]);
        variable_occurrences[variable].push_back({clause, w.cl_lits[i]});
        for (uint32_t j = w.cl_beg[clause]; j < w.cl_beg[clause + 1]; ++j) {
          const Var neighbor = var_of(w.cl_lits[j]);
          if (neighbor != variable) raw[variable].push_back(neighbor);
        }
      }
    neighbors.resize(k.nvars);
    std::vector<uint32_t> seen(k.nvars, 0);
    uint32_t epoch = 0;
    for (Var variable = 0; variable < k.nvars; ++variable) {
      ++epoch;
      for (const Var neighbor : raw[variable])
        if (seen[neighbor] != epoch) {
          seen[neighbor] = epoch;
          neighbors[variable].push_back(neighbor);
        }
    }
  }

  uint32_t pick_bounded(uint32_t bound) {
    assert(bound);
    uint32_t value;
    do {
      value = random() >> 1;
    } while (value + bound >= UINT32_C(0x80000000));
    return value % bound;
  }

  void remove_candidate(Var variable) {
    const uint32_t position = candidate_position[variable];
    if (position == UINT32_MAX) return;
    const Var last = candidates.back();
    candidates[position] = last;
    candidate_position[last] = position;
    candidates.pop_back();
    candidate_position[variable] = UINT32_MAX;
  }

  void refresh_candidate(Var variable) {
    const bool eligible = configuration_changed[variable] && score[variable] > 0;
    if (eligible && candidate_position[variable] == UINT32_MAX) {
      candidate_position[variable] = static_cast<uint32_t>(candidates.size());
      candidates.push_back(variable);
    } else if (!eligible) {
      remove_candidate(variable);
    }
  }

  void mark_unsatisfied(uint32_t clause) {
    push_unsat(w, clause);
    for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i) {
      const Var variable = var_of(w.cl_lits[i]);
      if (!unsat_appear[variable]++) {
        unsat_variable_position[variable] =
            static_cast<uint32_t>(unsat_variables.size());
        unsat_variables.push_back(variable);
      }
    }
  }

  void mark_satisfied(uint32_t clause) {
    pop_unsat(w, clause);
    for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i) {
      const Var variable = var_of(w.cl_lits[i]);
      assert(unsat_appear[variable]);
      if (!--unsat_appear[variable]) {
        const uint32_t position = unsat_variable_position[variable];
        const Var last = unsat_variables.back();
        unsat_variables[position] = last;
        unsat_variable_position[last] = position;
        unsat_variables.pop_back();
      }
    }
  }

  void assign_initial(bool randomize) {
    w.values.resize(2 * k.nvars);
    for (Var variable = 0; variable < k.nvars; ++variable) {
      const Lit positive = make_lit(variable, false);
      int8_t value = k.value(positive);
      if (!value) {
        if (randomize ||
            parameters.initialization ==
                strategy::WalkParameters::Initialization::Random)
          value = pick_bounded(2) ? 1 : -1;
        else if (parameters.initialization ==
                 strategy::WalkParameters::Initialization::Negative)
          value = -1;
        else
          value = k.h->best[variable] ? k.h->best[variable] : -1;
      }
      w.values[positive] = value;
      w.values[positive ^ 1] = static_cast<int8_t>(-value);
    }
  }

  void initialize(bool randomize) {
    assign_initial(randomize);
    w.count.assign(w.nclauses, 0);
    w.pos.assign(w.nclauses, 0);
    w.unsat.clear();
    weight.assign(w.nclauses, 1);
    sole_true.assign(w.nclauses, INVALID_VAR);
    score.assign(k.nvars, 0);
    last_flip.assign(k.nvars, 0);
    configuration_changed.assign(k.nvars, 1);
    candidate_position.assign(k.nvars, UINT32_MAX);
    candidates.clear();
    unsat_appear.assign(k.nvars, 0);
    unsat_variable_position.assign(k.nvars, 0);
    unsat_variables.clear();
    average_weight = 1;
    weight_remainder = 0;
    local_flip_step = 0;

    for (uint32_t clause = 0; clause < w.nclauses; ++clause) {
      uint32_t satisfied = 0;
      Var unique = INVALID_VAR;
      for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i) {
        const Lit literal = w.cl_lits[i];
        if (w.values[literal] > 0) {
          ++satisfied;
          unique = var_of(literal);
        }
      }
      w.count[clause] = satisfied;
      if (!satisfied)
        mark_unsatisfied(clause);
      else if (satisfied == 1)
        sole_true[clause] = unique;
    }
    for (uint32_t clause = 0; clause < w.nclauses; ++clause) {
      if (!w.count[clause]) {
        for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i)
          ++score[var_of(w.cl_lits[i])];
      } else if (w.count[clause] == 1) {
        --score[sole_true[clause]];
      }
    }
    for (Var variable = 0; variable < k.nvars; ++variable)
      refresh_candidate(variable);
    step += uint64_t{k.nvars} + w.cl_lits.size();
  }

  Var better(Var first, Var second) const {
    if (first == INVALID_VAR) return second;
    if (score[second] != score[first])
      return score[second] > score[first] ? second : first;
    if (last_flip[second] != last_flip[first])
      return last_flip[second] < last_flip[first] ? second : first;
    return first;
  }

  void recompute_scores_and_candidates() {
    std::fill(score.begin(), score.end(), 0);
    for (uint32_t clause = 0; clause < w.nclauses; ++clause) {
      const int64_t clause_weight = weight[clause];
      if (!w.count[clause]) {
        for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i)
          score[var_of(w.cl_lits[i])] += clause_weight;
      } else if (w.count[clause] == 1) {
        score[sole_true[clause]] -= clause_weight;
      }
    }
    candidates.clear();
    std::fill(candidate_position.begin(), candidate_position.end(), UINT32_MAX);
    for (Var variable = 0; variable < k.nvars; ++variable)
      refresh_candidate(variable);
    step += uint64_t{k.nvars} + w.cl_lits.size();
  }

  void smooth_weights() {
    uint64_t total = 0;
    const float retained =
        float(parameters.smoothing_old_numerator) /
        float(parameters.smoothing_denominator);
    const float replacement = 1.0f - retained;
    const uint32_t injected =
        static_cast<uint32_t>(float(average_weight) * replacement);
    for (uint32_t clause = 0; clause < w.nclauses; ++clause) {
      const uint32_t scaled = static_cast<uint32_t>(
          float(weight[clause]) * retained + float(injected));
      weight[clause] = std::max<uint32_t>(1, scaled);
      total += weight[clause];
    }
    average_weight = total / w.nclauses;
    weight_remainder = total % w.nclauses;
    recompute_scores_and_candidates();
  }

  void raise_unsatisfied_weights() {
    for (const uint32_t clause : w.unsat) {
      ++weight[clause];
      ++weight_remainder;
    }
    for (const Var variable : unsat_variables) {
      score[variable] += unsat_appear[variable];
      refresh_candidate(variable);
    }
    average_weight += weight_remainder / w.nclauses;
    weight_remainder %= w.nclauses;
    step += w.unsat.size();
    if (average_weight > parameters.weight_smoothing_threshold)
      smooth_weights();
  }

  Var pick_variable() {
    for (size_t i = candidates.size(); i-- > 0;)
      if (!configuration_changed[candidates[i]] || score[candidates[i]] <= 0)
        remove_candidate(candidates[i]);
    step += candidates.size();
    Var best = INVALID_VAR;
    for (const Var variable : candidates)
      best = better(best, variable);
    if (best != INVALID_VAR) return best;

    if (parameters.aspiration) {
      for (const Var variable : unsat_variables)
        if (score[variable] > static_cast<int64_t>(average_weight))
          best = better(best, variable);
      if (best != INVALID_VAR) return best;
    }

    raise_unsatisfied_weights();
    const uint32_t random_index =
        pick_bounded(static_cast<uint32_t>(w.unsat.size()));
    const uint32_t clause = w.unsat[random_index];
    for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i)
      best = better(best, var_of(w.cl_lits[i]));
    return best;
  }

  void flip_variable(Var variable) {
    const Lit positive = make_lit(variable, false);
    const Lit newly_true = w.values[positive] > 0 ? positive ^ 1 : positive;
    const Lit newly_false = newly_true ^ 1;
    const int64_t original_score = score[variable];
    w.values[newly_true] = 1;
    w.values[newly_false] = -1;

    // Process this variable's occurrences in formula order. Although the
    // score delta is commutative, the swap-pop order of violated clauses and
    // variables is not; it feeds the later randomized diversification step.
    for (const auto &[clause, literal] : variable_occurrences[variable]) {
      if (w.values[literal] > 0) {
        const uint32_t before = w.count[clause]++;
        if (!before) {
          mark_satisfied(clause);
          sole_true[clause] = variable;
          for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i)
            score[var_of(w.cl_lits[i])] -= weight[clause];
        } else if (before == 1) {
          score[sole_true[clause]] += weight[clause];
          sole_true[clause] = INVALID_VAR;
        }
      } else {
        const uint32_t after = --w.count[clause];
        if (!after) {
          mark_unsatisfied(clause);
          sole_true[clause] = INVALID_VAR;
          for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i)
            score[var_of(w.cl_lits[i])] += weight[clause];
        } else if (after == 1) {
          for (uint32_t i = w.cl_beg[clause]; i < w.cl_beg[clause + 1]; ++i) {
            const Lit true_literal = w.cl_lits[i];
            if (w.values[true_literal] > 0) {
              sole_true[clause] = var_of(true_literal);
              score[sole_true[clause]] -= weight[clause];
              break;
            }
          }
        }
      }
    }
    score[variable] = -original_score;
    last_flip[variable] = local_flip_step++;
    configuration_changed[variable] = 0;
    // Candidate compaction is deliberately one reverse pass before neighbor
    // re-enablement. Removing changed neighbors eagerly produces the same set
    // but a different swap-pop order, which changes equal-score tie breaking.
    for (size_t i = candidates.size(); i-- > 0;)
      if (score[candidates[i]] <= 0)
        remove_candidate(candidates[i]);
    for (const Var neighbor : neighbors[variable]) {
      configuration_changed[neighbor] = 1;
      if (score[neighbor] > 0 &&
          candidate_position[neighbor] == UINT32_MAX) {
        candidate_position[neighbor] =
            static_cast<uint32_t>(candidates.size());
        candidates.push_back(neighbor);
      }
    }
    step += variable_occurrences[variable].size();
    ++w.flips;
  }

  void run(uint64_t limit) {
    uint32_t best = UINT32_MAX;
    std::vector<int8_t> best_values(k.nvars, 0);
    for (uint32_t attempt = 0;
         attempt < parameters.maximum_tries && step < limit; ++attempt) {
      initialize(attempt != 0);
      if (attempt == 0) w.initial = static_cast<uint32_t>(w.unsat.size());
      if (w.unsat.size() < best) {
        best = static_cast<uint32_t>(w.unsat.size());
        for (Var variable = 0; variable < k.nvars; ++variable)
          best_values[variable] = w.values[make_lit(variable, false)];
      }
      const uint64_t attempt_begin = w.flips;
      while (!w.unsat.empty() && step < limit &&
             w.flips - attempt_begin < parameters.flips_per_try) {
        if (kernel_rule_interrupt(k, k.stats.walk_seed_interrupted)) {
          interrupted = true;
          break;
        }
        const Var variable = pick_variable();
        assert(variable != INVALID_VAR);
        flip_variable(variable);
        if (w.unsat.size() < best) {
          best = static_cast<uint32_t>(w.unsat.size());
          for (Var v = 0; v < k.nvars; ++v)
            best_values[v] = w.values[make_lit(v, false)];
          if (!best) break;
        }
      }
      if (interrupted || !best) break;
    }
    w.steps = step;
    w.minimum = best;
    w.best_values = std::move(best_values);
  }
};

void configuration_checking_search(Walker &w, Kernel &k, uint64_t limit) {
  ConfigurationCheckingSearch search(w, k);
  search.run(limit);
}

} // namespace

ConfigurationCheckingModelResult configuration_checking_model(
    Kernel &k, const std::vector<std::vector<Lit>> &clauses,
    const strategy::WalkParameters &parameters) {
  ConfigurationCheckingModelResult result;
  if (clauses.empty() || !parameters.valid() ||
      parameters.algorithm !=
          strategy::WalkParameters::Algorithm::ConfigurationChecking)
    return result;

  Walker walker;
  walker.k = &k;
  walker.cl_beg.reserve(clauses.size() + 1);
  walker.cl_beg.push_back(0);
  for (const auto &clause : clauses) {
    if (clause.empty())
      return result;
    walker.cl_lits.insert(walker.cl_lits.end(), clause.begin(), clause.end());
    walker.cl_beg.push_back(static_cast<uint32_t>(walker.cl_lits.size()));
  }
  walker.nclauses = static_cast<uint32_t>(clauses.size());

  strategy::ScopedWalkParameters installed(k, parameters);
  ConfigurationCheckingSearch search(walker, k);
  search.run(parameters.step_budget);
  result.steps = walker.steps;
  result.flips = walker.flips;
  result.interrupted = search.interrupted;
  result.sat = walker.minimum == 0;
  if (result.sat) {
    result.assignment.resize(k.nvars);
    for (Var variable = 0; variable < k.nvars; ++variable)
      result.assignment[variable] = walker.best_values[variable] > 0;
  }
  return result;
}

uint32_t walk_complementary_ternary_fraction_permille(const Kernel &k) {
  struct Signature {
    std::array<Var, 3> variables{};
    uint8_t signs = 0;
  };
  const auto less = [](const Signature &a, const Signature &b) {
    return a.variables < b.variables ||
           (a.variables == b.variables && a.signs < b.signs);
  };
  std::vector<Signature> signatures;
  signatures.reserve(k.live_irredundant_large);
  uint64_t clauses = k.watches.binary_clauses();
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && !clause->redundant()) {
      ++clauses;
      if (clause->size == 3) {
        std::array<std::pair<Var, bool>, 3> literals;
        for (uint32_t i = 0; i < 3; ++i)
          literals[i] = {var_of(clause->lits[i]), negated(clause->lits[i])};
        std::sort(literals.begin(), literals.end());
        if (literals[0].first != literals[1].first &&
            literals[1].first != literals[2].first) {
          Signature signature;
          for (uint32_t i = 0; i < 3; ++i) {
            signature.variables[i] = literals[i].first;
            signature.signs |= uint8_t(literals[i].second) << i;
          }
          signatures.push_back(signature);
        }
      }
    }
    ref += words;
  }
  if (!clauses || signatures.empty()) return 0;
  std::sort(signatures.begin(), signatures.end(), less);
  uint64_t paired = 0;
  for (auto begin = signatures.begin(); begin != signatures.end();) {
    const auto end = std::upper_bound(begin, signatures.end(), *begin, less);
    Signature complement = *begin;
    complement.signs ^= 7;
    // Count each complementary multiset pair once, and only as many copies as
    // exist on both sides. Existence-only counting would misclassify a formula
    // containing many duplicate clauses and one opposite-polarity copy.
    if (less(*begin, complement)) {
      const auto opposite_begin =
          std::lower_bound(signatures.begin(), signatures.end(), complement,
                           less);
      const auto opposite_end =
          std::upper_bound(opposite_begin, signatures.end(), complement, less);
      paired += 2 * std::min<uint64_t>(end - begin,
                                      opposite_end - opposite_begin);
    }
    begin = end;
  }
  return static_cast<uint32_t>(std::min<uint64_t>(1000, 1000 * paired / clauses));
}

UniformNegativeWalkCensus walk_uniform_negative_census(const Kernel &k) {
  UniformNegativeWalkCensus result;
  // Evaluate only the still-open irredundant formula.  Root-satisfied clauses
  // are irrelevant to every completion of the current root assignment; free
  // variables take their negative phase in this structural candidate.
  const auto count = [&](const Lit *literals, uint32_t size) {
    bool root_satisfied = false;
    bool negative_satisfied = false;
    for (uint32_t i = 0; i < size; ++i) {
      const Lit literal = literals[i];
      const int8_t value = k.value(literal);
      root_satisfied |= value > 0;
      negative_satisfied |= !value && negated(literal);
    }
    if (root_satisfied) return;
    ++result.clauses;
    result.unsatisfied += !negative_satisfied;
  };

  const uint32_t nlits = 2 * k.nvars;
  for (Lit literal = 0; literal < nlits; ++literal)
    for (const auto watch : k.watches.entries(literal)) {
      if (!watch.binary() || literal > watch.other()) continue;
      const Lit pair[] = {literal, watch.other()};
      count(pair, 2);
    }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage() && !clause->redundant())
      count(clause->lits, clause->size);
    ref += words;
  }
  return result;
}

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

static ApplyResult walk_apply_split(Kernel &k, uint64_t search_allowance) {
  const auto *hint = kernel_local_search_hint(k, k.walk_seed_candidate);
  if (!hint) {
    ++k.stats.local_search_hints_stale;
    return ApplyResult::NoOp;
  }
  k.stats.walk_seed_ordinary_unsat = UINT64_MAX;
  k.stats.walk_seed_candidate_unsat = UINT64_MAX;
  PreparationMeter preparation{k, k.walk_preparation_budget};
  Walker w;
  w.k = &k;
  ++k.stats.walks;
  w.rng = k.walk_random_seed ^ k.stats.walks;
  uint32_t ordinary_unsat = 0;
  const bool ready = build_full_table(w, k, preparation) &&
      seed_candidate(w, k, *hint, 0, ordinary_unsat, &preparation);
  k.stats.walk_preparation_steps += preparation.used;
  if (!ready) {
    if (!preparation.stopped) ++k.stats.walk_seed_rejected;
    return ApplyResult::NoOp;
  }
  if (w.initial >= ordinary_unsat) {
    ++k.stats.walk_seed_rejected;
    return ApplyResult::NoOp;
  }
  // Reserve the saved/provenance/target visitation dose before search so
  // preparation exhaustion cannot start a walk that cannot publish its result.
  const auto before_export = preparation.used;
  const bool export_ready = preparation.charge(uint64_t{3} * k.nvars);
  k.stats.walk_preparation_steps += preparation.used - before_export;
  if (!export_ready) return ApplyResult::NoOp;
  ++k.stats.walk_seed_admitted;
  std::vector<double> scores;
  bool interrupted = false, exhausted = false;
  const auto charge_search = [&](uint64_t amount) {
    if (kernel_rule_interrupt(k, k.stats.walk_seed_interrupted)) {
      interrupted = true;
      return false;
    }
    if (amount > search_allowance - w.steps) {
      exhausted = true;
      ++k.stats.walk_search_budget_exhausted;
      ++k.stats.walk_seed_budget_exhausted;
      return false;
    }
    w.steps += amount;
    return true;
  };
  while (!w.unsat.empty() && w.steps < search_allowance) {
    // Charge selection metadata before visiting it. Reserve a complete flip
    // quantum before roulette, occurrence scans or private assignment changes.
    if (!charge_search(1)) break;
    const auto clause = w.unsat[(2 * w.flips + 1) % w.unsat.size()];
    const auto begin = w.cl_beg[clause], end = w.cl_beg[clause + 1];
    uint64_t pick_work = 0, flip_work = 0;
    for (auto i = begin; i < end; ++i) {
      if (!charge_search(1)) break;
      const auto literal = w.cl_lits[i];
      const uint64_t positive = w.occ_beg[literal + 1] - w.occ_beg[literal];
      const auto negative_literal = negate(literal);
      const uint64_t negative = w.occ_beg[negative_literal + 1] -
                                w.occ_beg[negative_literal];
      pick_work += negative + 1; // exact native break_value charge
      // Every positive occurrence can also require pop_unsat's replacement.
      flip_work = std::max(flip_work, 2 * positive + negative + 2);
    }
    if (interrupted || exhausted) break;
    // Score allocation, generation, optional scaling, summation and roulette
    // each visit at most the selected clause's literal count. Best assignment
    // materialization visits nvars, with no flip-log allocation or export scan.
    const uint64_t quantum = pick_work + flip_work +
                            uint64_t{5} * (end - begin) + k.nvars;
    if (!charge_search(quantum)) break;
    const auto charged = w.steps;
    scores.clear();
    scores.reserve(end - begin);
    w.steps = 0; // native helpers report the already-reserved occurrence dose
    const auto literal = pick_literal(w, scores);
    ++w.flips;
    flip(w, literal);
    assert(w.steps <= pick_work + flip_work);
    w.steps = charged;
    if (w.unsat.size() < w.minimum) {
      w.minimum = w.unsat.size();
      for (Var variable = 0; variable < k.nvars; ++variable)
        w.best_values[variable] = w.values[make_lit(variable, false)];
    }
  }
  k.stats.walk_search_steps += w.steps;
  k.stats.walk_steps += w.steps;
  k.stats.walk_flips += w.flips;
  k.stats.walk_minimum = w.minimum;
  if (w.minimum < w.initial) ++k.stats.walk_seed_improved;
  if (interrupted || exhausted) return ApplyResult::NoOp;
  // A completed search still needs a fresh stop check before any phase write.
  if (kernel_rule_interrupt(k, k.stats.walk_seed_interrupted))
    return ApplyResult::NoOp;
  // Strict admission already proves this complete private assignment improves
  // ordinary phases, even when the exact search allowance is zero.
  for (Var variable = 0; variable < k.nvars; ++variable)
    if (!k.value(make_lit(variable, false)) && !kernel_var_retired(k, variable))
      k.h->saved[variable] = w.best_values[variable];
  heuristics_supersede_all_target_guidance(k, TargetGuidanceOverwrite::Walk);
  std::copy(k.h->saved.begin(), k.h->saved.end(), k.h->target.begin());
  k.h->target_assigned = 0;
  ++k.stats.rephased;
  ++k.stats.walk_improved;
  ++k.stats.walk_seed_exported;
  return ApplyResult::Ok;
}

static ApplyResult walk_apply_exact_steps(Kernel &k, uint64_t steps) {
  assert(walk_guard(k));
  if (k.walk_objective == uint32_t(strategy::WalkParameters::Objective::FullCurrentCnf))
    return walk_apply_split(k, steps);
  const auto *hint = kernel_local_search_hint(k, k.walk_seed_candidate);
  if (k.walk_seed_candidate && !hint) {
    ++k.stats.local_search_hints_stale;
    return ApplyResult::NoOp;
  }
  if (hint && !steps) {
    ++k.stats.walk_seed_budget_exhausted;
    return ApplyResult::NoOp;
  }
  if (hint && kernel_rule_interrupt(k, k.stats.walk_seed_interrupted))
    return ApplyResult::NoOp;
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
    uint32_t export_threshold = 0;
    if (hint) {
      const bool ready = seed_candidate(w, k, *hint, steps, export_threshold);
      k.stats.walk_seed_steps += w.steps;
      if (!ready) {
        kernel_resume_walk_watches(k);
        k.stats.walk_steps += w.steps;
        return ApplyResult::NoOp;
      }
    } else if (k.walk_algorithm == uint32_t(
                   strategy::WalkParameters::Algorithm::ProbSat) || !steps) {
      seed_values(w, k);
      init_counters(w);
      export_threshold = w.initial;
    } else {
      export_threshold = 0;
    }
    if (!hint && k.walk_algorithm == uint32_t(
                     strategy::WalkParameters::Algorithm::ConfigurationChecking) &&
        steps) {
      export_threshold = decision_phase_unsatisfied(w, k);
      configuration_checking_search(w, k, steps);
    } else {
      local_search(w, steps);
    }
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
    if (hint && w.minimum < w.initial)
      ++k.stats.walk_seed_improved;
    if (w.minimum < export_threshold) {
      sync_best(w); // materialise the winning prefix into best_values
      for (Var v = 0; v < k.nvars; v++)
        if (!k.value(make_lit(v, false)) &&
            (!hint || !kernel_var_retired(k, v)))
          k.h->saved[v] = w.best_values[v];
      k.stats.walk_improved++;
      if (hint)
        ++k.stats.walk_seed_exported;
      improved = true;
    }
  } else if (hint) {
    // No current walkable objective exists on which to compare the advice.
    // Dense-entry cleanup may have retired clauses; restore sparse watches,
    // but decline the seeded request without touching phase memory.
    kernel_resume_walk_watches(k);
    return ApplyResult::NoOp;
  }
  // The rephase tail every type runs (kissat reset_phases): the fresh saved
  // phases become the target ones. `best` is not consumed here — kissat resets
  // its watermark only on type Best.
  heuristics_supersede_all_target_guidance(k, TargetGuidanceOverwrite::Walk);
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

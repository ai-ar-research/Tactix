// Transliterated from kissat/src/walk.c (MIT, Biere et al.) — the ProbSAT
// flipper, its break-value scoring and its CB score table — with two
// deliberate deviations, both recorded in docs/design/s6-walk-design.md §4:
//
//  (1) the walker owns its state.  kissat swaps `solver->values` for a scratch
//      array and repurposes the watch lists as occurrence lists (dense mode);
//      we build a walker-local CSR occurrence index and a walker-local value
//      array instead.  The kernel is therefore untouched by a walk, which is
//      what lets the op run at ANY decision level (N1) rather than only at a
//      quiescent root.
//  (2) literals assigned on the trail are FIXED for the duration: clauses they
//      satisfy are dropped from the table and the literals they falsify are
//      dropped from their clauses.  At level 0 this is exactly kissat's
//      treatment of root-fixed literals; above it, the walk simply optimises
//      the free part of the formula.
#include "policies/ops/walk/walk.hpp"
#include "kernel/clause.hpp"
#include "policies/heuristics.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tactix {

namespace {

// xorshift64* — deterministic, seeded from the walk count so repeated solves
// replay identically (the determinism trace tests depend on this).
inline uint64_t next_random(uint64_t &s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 2685821657736338717ULL;
}
// uniform double in [0,1) from the top 53 bits (kissat_pick_double)
inline double pick_double(uint64_t &s) {
  return static_cast<double>(next_random(s) >> 11) * (1.0 / 9007199254740992.0);
}

// ProbSAT CB base as a function of average clause size (kissat walk.c cbvals)
const double CBVALS[6][2] = {{0.0, 2.00}, {3.0, 2.50}, {4.0, 2.85},
                             {5.0, 3.70}, {6.0, 5.10}, {7.0, 7.40}};

double fit_cbval(double size) {
  size_t i = 0;
  while (i + 2 < 6 && (CBVALS[i][0] > size || CBVALS[i + 1][0] < size))
    i++;
  const double x2 = CBVALS[i + 1][0], x1 = CBVALS[i][0];
  const double y2 = CBVALS[i + 1][1], y1 = CBVALS[i][1];
  return (y2 - y1) * (size - x1) / (x2 - x1) + y1;
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

  uint32_t nclauses = 0;
  uint32_t initial = 0, minimum = 0;
  uint64_t flips = 0, steps = 0;
  uint64_t rng = 0;

  uint32_t nlits() const { return 2 * k->nvars; }
};

// --- table construction ----------------------------------------------------

// Collect the walkable clauses.  Two passes: the first counts (so every vector
// is sized exactly once — no growth reallocations mid-build), the second fills.
// Binaries live only in watch lists, so they are enumerated from there with an
// `other > lit` dedup; large irredundant clauses come from one arena scan.
bool build_table(Walker &w, const Kernel &k) {
  const uint32_t nlits = 2 * k.nvars;
  double total_size = 0;

  // pass 1 + 2 fused for clause storage (push_back on two vectors sized by a
  // reserve estimate); the occurrence index is then built by counting sort.
  w.cl_beg.push_back(0);
  const auto emit = [&](const Lit *lits, uint32_t n) {
    uint32_t free_lits = 0;
    for (uint32_t i = 0; i < n; i++) {
      const int8_t v = k.value(lits[i]);
      if (v > 0)
        return; // satisfied by the trail: not the walker's problem
      if (v == 0)
        free_lits++;
    }
    if (!free_lits)
      return; // falsified by the trail: the search's problem, not the walk's
    for (uint32_t i = 0; i < n; i++)
      if (k.value(lits[i]) == 0)
        w.cl_lits.push_back(lits[i]);
    w.cl_beg.push_back(static_cast<uint32_t>(w.cl_lits.size()));
    total_size += free_lits;
  };

  for (Lit l = 0; l < nlits; l++) {
    const uint32_t *ws = k.watches.arena.data() + k.watches.heads[l].offset;
    const uint32_t n = k.watches.heads[l].size;
    for (uint32_t i = 0; i < n; i++) {
      const Watch wt = {ws[i]};
      if (!wt.binary()) {
        i++; // large watch: [blocking, ref]
        continue;
      }
      const Lit other = wt.other();
      if (other < l)
        continue; // dedup: each binary is seen from both watch lists
      const Lit pair[2] = {l, other};
      emit(pair, 2);
    }
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    if (!c->garbage() && !c->redundant())
      emit(c->lits, c->size);
    ref += clause_words(c->size);
  }

  w.nclauses = static_cast<uint32_t>(w.cl_beg.size() - 1);
  if (!w.nclauses)
    return false;

  // counting sort into the CSR occurrence index
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

  // the ProbSAT score table, base 1/cb (kissat init_score_table): cb
  // alternates between the fixed 2.0 and the size fit across walks.
  const double avg = total_size / w.nclauses;
  const double cb = (k.stats.walks & 1) ? fit_cbval(avg) : 2.0;
  const double base = 1 / cb;
  double next = 1;
  for (w.epsilon = 1; next; next = w.epsilon * base)
    w.table.push_back(w.epsilon = next);
  return true;
}

// Seed the walker assignment: trail values where assigned, otherwise the
// decision phase the search itself would pick (target when stable, else saved).
void seed_values(Walker &w, const Kernel &k) {
  w.values.resize(2 * k.nvars);
  w.best_values.resize(k.nvars);
  // TACTIX_WALK_PREV (DIAGNOSTIC, cadical-shaped arm): resume from the previous
  // walk's assignment (cadical phases.prev) instead of the decision phases.
  static const bool use_prev = std::getenv("TACTIX_WALK_PREV") != nullptr;
  const bool have_prev = use_prev && k.h->walk_prev.size() == k.nvars;
  for (Var v = 0; v < k.nvars; v++) {
    const Lit pos_lit = make_lit(v, false);
    int8_t val = k.value(pos_lit);
    if (!val && have_prev)
      val = k.h->walk_prev[v];
    if (!val)
      val = negated(heuristics_decide_lit(k, v)) ? -1 : 1;
    w.values[pos_lit] = val;
    w.values[pos_lit ^ 1] = static_cast<int8_t>(-val);
    w.best_values[v] = val;
  }
}

// WARMUP (kissat warmup.c + propbeyond.c; cadical warmup.cpp — on by default in
// both): complete the seed into a propagation-
// consistent assignment before walking — decide every variable in turn at its
// decision phase and propagate the consequences, ignoring conflicts (kissat
// ignoring conflicts. Run over the walker's OWN structures, so no kernel
// propagate-beyond-conflicts primitive is needed and nothing outside `H` moves.
// An earlier measurement called this neutral-to-negative, but it was taken in
// the walk-REPLACES-Best rotation and only on instances where walk is not
// decisive; re-measured on SCPC-1000-18 in the shipped rotation it is worth
// 2.4x (248 s -> 102 s, and the walker reaches minimum 1 instead of 2 in 11
// walks instead of 15). Warms up in variable-index order: the references use
// decision-heuristic order, which would mean popping the score heap / VMTF
// queue — the very `H` state this op must leave alone.
// NOT the default, and this is the point: the same knob is a multiple-x WIN on
// the walk-decisive instance and a large tier-scale LOSS elsewhere. Opt-in via
// TACTIX_WALK_WARMUP; a plan §5.6 dosing arm, not a constant anyone can set
// right.
void warmup(Walker &w) {
  const uint32_t nlits = w.nlits();
  std::vector<int8_t> val(nlits, 0); // tri-state: 0 unassigned
  std::vector<uint32_t> ntrue(w.nclauses, 0);
  std::vector<uint32_t> nfree(w.nclauses);
  for (uint32_t c = 0; c < w.nclauses; c++)
    nfree[c] = w.cl_beg[c + 1] - w.cl_beg[c];
  std::vector<Lit> queue;
  const auto assign = [&](Lit lit) {
    val[lit] = 1;
    val[negate(lit)] = -1;
    queue.push_back(lit);
  };
  const auto propagate = [&]() {
    while (!queue.empty()) {
      const Lit lit = queue.back();
      queue.pop_back();
      for (uint32_t i = w.occ_beg[lit]; i < w.occ_beg[lit + 1]; i++)
        ntrue[w.occ[i]]++, nfree[w.occ[i]]--;
      const Lit nl = negate(lit);
      for (uint32_t i = w.occ_beg[nl]; i < w.occ_beg[nl + 1]; i++) {
        const uint32_t c = w.occ[i];
        if (--nfree[c] || ntrue[c])
          continue; // still open, or already satisfied
        // no free literal left => falsified; propagating BEYOND it is the point
        for (uint32_t j = w.cl_beg[c]; j < w.cl_beg[c + 1]; j++)
          if (!val[w.cl_lits[j]]) {
            assign(w.cl_lits[j]);
            break;
          }
      }
      w.steps += (w.occ_beg[lit + 1] - w.occ_beg[lit]) +
                 (w.occ_beg[nl + 1] - w.occ_beg[nl]) + 1;
    }
  };
  for (Var v = 0; v < w.k->nvars; v++) {
    const Lit pos_lit = make_lit(v, false);
    if (val[pos_lit])
      continue;
    assign(w.values[pos_lit] > 0 ? pos_lit : negate(pos_lit));
    propagate();
  }
  for (Var v = 0; v < w.k->nvars; v++) {
    const Lit pos_lit = make_lit(v, false);
    if (!val[pos_lit])
      continue; // variable in no walkable clause: keep its decision phase
    w.values[pos_lit] = val[pos_lit];
    w.values[pos_lit ^ 1] = val[pos_lit ^ 1];
    w.best_values[v] = val[pos_lit];
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
}

// --- the flip loop ---------------------------------------------------------

inline void push_unsat(Walker &w, uint32_t c) {
  w.pos[c] = static_cast<uint32_t>(w.unsat.size());
  w.unsat.push_back(c);
}
inline void pop_unsat(Walker &w, uint32_t c) {
  const uint32_t p = w.pos[c];
  const uint32_t last = w.unsat.back();
  w.unsat[p] = last;
  w.pos[last] = p;
  w.unsat.pop_back();
}

// how many clauses flipping `lit` to true would falsify: those satisfied by
// exactly one literal, namely ~lit (kissat break_value)
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
  const uint32_t c = w.unsat[w.flips % w.unsat.size()];
  const uint32_t beg = w.cl_beg[c], end = w.cl_beg[c + 1];
  scores.clear();
  double sum = 0;
  for (uint32_t i = beg; i < end; i++) {
    const double s = scale_score(w, break_value(w, w.cl_lits[i]));
    scores.push_back(s);
    sum += s;
  }
  const double threshold = sum * pick_double(w.rng);
  double acc = 0;
  for (uint32_t i = beg; i < end; i++) {
    acc += scores[i - beg];
    if (threshold < acc)
      return w.cl_lits[i];
  }
  return w.cl_lits[end - 1]; // rounding: fall back to the last literal
}

// keep `best_values` recoverable without an O(nvars) copy per improvement:
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
  // the trail was invalidated: resync wholesale, then resume recording
  for (Var v = 0; v < w.k->nvars; v++)
    w.best_values[v] = w.values[make_lit(v, false)];
  w.pending.clear();
  w.best_pending = 0;
  w.pending_valid = true;
}

void push_flip(Walker &w, Lit lit) {
  if (!w.pending_valid)
    return;
  const size_t cap = w.k->nvars / 4 + 1024;
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
      if (!w.count[c]++)
        pop_unsat(w, c);
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

// requires `H` (the walk writes the saved/target phase arrays — a pure
// precondition of a policy operation that needs `H`, §5/H5) and a
// non-refuted kernel.  Any decision level is legal: assigned literals are
// held fixed (file header, deviation 2).
bool walk_guard(const Kernel &k) {
  return !k.inconsistent && k.h != nullptr && k.nvars > 0;
}

ApplyResult walk_apply_steps(Kernel &k, uint64_t steps) {
  assert(walk_guard(k));
  k.stats.walks++;
  Walker w;
  w.k = &k;
  w.rng = 0x9E3779B97F4A7C15ULL ^ (k.stats.walks * 0x2545F4914F6CDD1DULL);
  bool improved = false;
  if (build_table(w, k)) {
    seed_values(w, k);
    static const bool want_warmup = std::getenv("TACTIX_WALK_WARMUP") != nullptr;
    if (want_warmup)
      warmup(w);
    init_counters(w);
    uint64_t limit = steps ? steps : WALK_DEFAULT_STEPS;
    // TACTIX_WALK_MAX_STEPS (DIAGNOSTIC): hard-cap the per-walk dose, on top
    // of the policy's budget — the dose-response axis for the mp1-Nb5T15
    // poisoning (design §6, "measured-dead dosing knobs").
    static const char *const cap_env = std::getenv("TACTIX_WALK_MAX_STEPS");
    if (cap_env) {
      const uint64_t cap = std::strtoull(cap_env, nullptr, 10);
      if (cap && limit > cap)
        limit = cap;
    }
    local_search(w, limit);
    k.stats.walk_flips += w.flips;
    k.stats.walk_steps += w.steps;
    k.stats.walk_minimum = w.minimum;
    if (std::getenv("TACTIX_WALK_TRACE"))
      fprintf(stderr,
              "c walk#%llu clauses=%u initial=%u minimum=%u flips=%llu "
              "steps=%llu budget=%llu conflicts=%llu\n",
              (unsigned long long)k.stats.walks, w.nclauses, w.initial,
              w.minimum, (unsigned long long)w.flips,
              (unsigned long long)w.steps, (unsigned long long)steps,
              (unsigned long long)k.stats.conflicts);
    // TACTIX_WALK_DRY (DIAGNOSTIC ONLY, never a shipped config): run the walk
    // but discard its assignment — the control that separates "the phases the
    // walk writes" from "what else the slot changed" — ablate one thing at a
    // time.  It is what showed kissat's Walk-replaces-Best rotation to be
    // 4-7x worse than the baseline with the walk's own output thrown away, so
    // the regression was the lost Best consumption, not local search; see
    // docs/design/s6-walk-design.md §6.  Kept as the durable control.
    static const bool dry = std::getenv("TACTIX_WALK_DRY") != nullptr;
    if (w.minimum < w.initial && !dry) {
      sync_best(w); // materialise the winning prefix into best_values
      for (Var v = 0; v < k.nvars; v++)
        if (!k.value(make_lit(v, false))) // fixed literals keep their phase
          k.h->saved[v] = w.best_values[v];
      k.stats.walk_improved++;
      improved = true;
      static const bool keep_prev = std::getenv("TACTIX_WALK_PREV") != nullptr;
      if (keep_prev)
        k.h->walk_prev = w.best_values; // cadical phases.prev continuity
    }
  }
  // the rephase tail every type runs (kissat reset_phases): the fresh saved
  // phases become the target ones.  `best` is NOT consumed here — kissat
  // resets its watermark only on type Best.
  // TACTIX_WALK_NO_TARGET (DIAGNOSTIC): skip the target copy, so the walk
  // reaches only FOCUSED-mode decisions (stable decisions read target first).
  // Discriminates which phase channel carries the damage.
  static const bool no_target = std::getenv("TACTIX_WALK_NO_TARGET") != nullptr;
  if (!no_target) {
    std::copy(k.h->saved.begin(), k.h->saved.end(), k.h->target.begin());
    k.h->target_assigned = 0;
  }
  k.stats.rephased++;
  return improved ? ApplyResult::Ok : ApplyResult::NoOp;
}

ApplyResult walk_apply(Kernel &k) {
  return walk_apply_steps(k, k.work_budget);
}

// instance surface (plan §5.1): the payload is the step budget, so the §5.6
// learner can dose a walk the way it doses every other rule.  The enumeration
// offers the canonical dose; apply_instance is total for ANY payload.
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

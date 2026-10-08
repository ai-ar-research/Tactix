#undef NDEBUG
// DefaultPolicy's persistent affine run schedule: input-change trigger, NLOG2N
// conflict interval with Kissat's delta scaling, SET_EFFORT_LIMIT effort, and
// the fruitfulness delay (doubling when fruitless, halving when search used
// the run's output through the SymmetryUse AFFINE class).
#include "policies/default_policy.hpp"
#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace tactix;

namespace tactix {
struct DefaultAffineSearchTestAccess {
  static AffineMatrixInitResult initialize(DefaultPolicy &p, Kernel &k) {
    return p.initialize_affine_search(k);
  }
  static bool due(DefaultPolicy &p, const Kernel &k) { return p.affine_run_due(k); }
  static ApplyResult run(DefaultPolicy &p, Kernel &k, bool startup) {
    return p.run_affine_search(k, startup);
  }
  static uint64_t effort(const DefaultPolicy &p, const Kernel &k) {
    return p.affine_run_effort(k);
  }
  static void finish(DefaultPolicy &p, const Kernel &k, uint64_t published) {
    p.finish_affine_run(k, published);
  }
  static uint64_t limit(const DefaultPolicy &p) { return p.affine_schedule_.next_conflicts; }
  static uint64_t occurrences(const DefaultPolicy &p) { return p.affine_schedule_.occurrences; }
  static uint32_t delay(const DefaultPolicy &p) { return p.affine_schedule_.delay_current; }
  static uint32_t delay_count(const DefaultPolicy &p) { return p.affine_schedule_.delay_count; }
  static uint64_t last_ticks(const DefaultPolicy &p) { return p.affine_schedule_.last_ticks; }
};
} // namespace tactix

using Access = DefaultAffineSearchTestAccess;

static DefaultStrategyConfig config(bool use_tracking) {
  DefaultStrategyConfig c;
  c.affine_search.enabled = true;
  c.affine_search.output_use_fruitfulness = use_tracking;
  return c;
}

// kimits.c kissat_nlogpown(n, 2) * eliminateint, then kissat_scale_delta
// with C = irredundant clauses: (4.5 (log10(C + 10) - 5)^2 + 25) * delta.
static uint64_t expected_delta(uint64_t clauses, uint64_t n) {
  const double lg = std::log10(double(n) + 9.0);
  const uint64_t delta = uint64_t(500.0 * double(n) * lg * lg);
  const double f = std::log10(double(clauses) + 10.0) - 5.0;
  return uint64_t((4.5 * f * f + 25.0) * double(delta));
}

static void effort_limit() {
  Kernel k;
  kernel_reserve_vars(k, 2);
  DefaultStrategyConfig c = config(true);
  DefaultPolicy p(c);
  // Zero conflicts: the preprocessing reference, at the sweep per-mille.
  assert(Access::effort(p, k) ==
         c.inprocess.preprocess_initial_ticks * c.inprocess.sweep_permille / 1000);
  k.stats.conflicts = 1;
  k.stats.search_ticks = 5; // below mineffort: floored
  assert(Access::effort(p, k) ==
         c.inprocess.minimum_effort_ticks * c.inprocess.sweep_permille / 1000);
  k.stats.search_ticks = 3 * c.inprocess.minimum_effort_ticks;
  assert(Access::effort(p, k) == k.stats.search_ticks * c.inprocess.sweep_permille / 1000);
  c.affine_search.effort_permille = 7;
  DefaultPolicy q(c);
  assert(Access::effort(q, k) == k.stats.search_ticks * 7 / 1000);
}

struct Fixture {
  uint32_t variables = 9;
  std::vector<std::vector<int32_t>> clauses;
};

static std::string cnf(const Fixture &f) {
  std::ostringstream out;
  out << "p cnf " << f.variables << ' ' << f.clauses.size() << '\n';
  for (const auto &c : f.clauses) {
    for (int32_t x : c) out << x << ' ';
    out << "0\n";
  }
  return out.str();
}

static void add_input(Kernel &k, Proof &proof, Fixture &f, std::vector<int32_t> clause) {
  std::vector<Lit> body;
  for (int32_t x : clause) body.push_back(kernel_internalize(k, x));
  const uint64_t id = ++k.next_clause_id;
  proof.seed_population(id, body.data(), body.size());
  if (body.size() == 1) {
    kernel_assign_unit(k, body[0]);
    assert(propagate_apply(k) == ApplyResult::Ok);
  } else {
    kernel_add_large_clause(k, body.data(), body.size(), false, 0, id);
  }
  f.clauses.push_back(std::move(clause));
}

// The two XORs over {1,2,3} and {2,3,4} imply x1 = x4, which Boolean
// propagation cannot see; x4 -> x5 lets that consequence force a root fact.
// An independent XOR over {7,8,9} supplies further support root facts.
static void schedule(bool use_tracking, const std::string &stem) {
  Fixture f;
  for (const auto &vars : {std::vector<int32_t>{1, 2, 3}, std::vector<int32_t>{2, 3, 4},
                           std::vector<int32_t>{7, 8, 9}})
    for (const auto &clause : affine_semantic_extension::clausal_form(vars, false))
      f.clauses.push_back(clause);
  f.clauses.push_back({-4, 5});
  Kernel k;
  FILE *input = std::tmpfile();
  const std::string text = cnf(f);
  std::fwrite(text.data(), 1, text.size(), input);
  std::rewind(input);
  assert(!parse_dimacs(input, k));
  std::fclose(input);
  AffineMatrixSearchSession session;
  k.affine_matrix_search_session = &session;
  FILE *records = std::tmpfile();
  k.structural_provenance = records;
  const std::string path = stem.empty() ? "/tmp/tactix-affine-run-schedule.dsr" : stem + ".dsr";
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&k.internal_to_external);
  proof.bind_external_variable_map(&k.external_to_internal);
  k.proof = &proof;
  DefaultPolicy p(config(use_tracking));
  const auto init = Access::initialize(p, k);
  assert(init.status == AffineMatrixInitStatus::Applied && session.enabled() &&
         session.persistent());
  // Startup run: no root fact, nothing published; the first interval is the
  // Kissat eliminateinit analogue NLOG2N(1) = 1.
  assert(Access::run(p, k, true) == ApplyResult::Ok);
  const auto &d = p.affine_search_diagnostics();
  assert(d.runs == 1 && d.reasons_published == 0 && Access::occurrences(p) == 1);
  assert(Access::limit(p) == expected_delta(kernel_irredundant_clauses(k), 1));
  assert(Access::last_ticks(p) == k.stats.search_ticks);
  // Below the conflict limit nothing is due.
  k.stats.conflicts = Access::limit(p) - 1;
  assert(!Access::due(p, k));
  // At the limit, unchanged inputs wait (once per episode) instead of firing.
  k.stats.conflicts = Access::limit(p);
  assert(!Access::due(p, k) && !Access::due(p, k));
  assert(d.unchanged_waits == 1 && Access::occurrences(p) == 1);
  // Decision-trail churn and a root fact off the support do not change inputs.
  kernel_assign_decision(k, kernel_internalize(k, 2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  kernel_backtrack(k, 0);
  add_input(k, proof, f, {6});
  assert(!Access::due(p, k) && d.unchanged_waits == 1);
  // A root fact on the support changes the inputs. The startup run published
  // nothing, so it is judged fruitless: the delay goes 0 -> 1 and this
  // occurrence is delayed, advancing the NLOG2N count to 2.
  add_input(k, proof, f, {1});
  assert(!Access::due(p, k));
  assert(d.fruitless_runs == 1 && Access::delay(p) == 1 && Access::delay_count(p) == 0);
  assert(d.delayed_runs == 1 && Access::occurrences(p) == 2);
  const uint64_t clauses = kernel_irredundant_clauses(k);
  assert(Access::limit(p) == k.stats.conflicts + expected_delta(clauses, 2));
  assert(expected_delta(clauses, 2) > expected_delta(clauses, 1));
  k.stats.conflicts = Access::limit(p);
  assert(Access::due(p, k));
  // The run publishes the GF(2) consequence x4; root BCP then forces x5.
  assert(Access::run(p, k, false) == ApplyResult::Ok);
  assert(d.runs == 2 && d.reasons_published >= 1 && Access::occurrences(p) == 3);
  assert(k.values[kernel_internalize(k, 4)] > 0 && k.values[kernel_internalize(k, 5)] > 0);
  assert(Access::limit(p) == k.stats.conflicts + expected_delta(kernel_irredundant_clauses(k), 3));
  if (use_tracking) {
    assert(d.output_tags >= 1);
    assert(kernel_symuse_affine_uses(k, 1) >= 1); // x5 forced by the x4 unit
    assert(kernel_symuse_affine_uses(k, 0) == 0);
  } else {
    assert(d.output_tags == 0 && k.symuse.affine_runs.empty());
  }
  // Without further input change, the next limit waits; the run's verdict is
  // only taken when another occurrence is decided.
  k.stats.conflicts = Access::limit(p);
  assert(!Access::due(p, k) && d.fruitful_runs == 0);
  // Another support root fact; the fruitful run halves the delay 1 -> 0, so
  // this occurrence fires.
  add_input(k, proof, f, {2});
  assert(Access::due(p, k));
  assert(d.fruitful_runs == 1 && Access::delay(p) == 0);
  // Two fruitless runs in a row: the delay goes 0 -> 1 -> 2 (doubling), and
  // the second verdict delays two occurrences before the next run.
  for (uint32_t expected : {1u, 2u}) {
    Access::finish(p, k, 0);
    add_input(k, proof, f, {expected == 1 ? 7 : 8});
    k.stats.conflicts = Access::limit(p);
    assert(!Access::due(p, k));
    assert(Access::delay(p) == expected && Access::delay_count(p) == expected - 1);
    for (uint32_t skipped = 1; skipped < expected; ++skipped) {
      k.stats.conflicts = Access::limit(p);
      assert(!Access::due(p, k));
    }
    k.stats.conflicts = Access::limit(p);
    assert(Access::due(p, k));
  }
  if (!stem.empty()) {
    std::ofstream(stem + ".cnf") << cnf(f);
  } else {
    std::remove(path.c_str());
  }
  session.disable(k);
  k.proof = nullptr;
  k.structural_provenance = nullptr;
  std::fclose(records);
}

int main(int argc, char **argv) {
  assert(argc == 1 || argc == 2);
  effort_limit();
  schedule(true, argc == 2 ? std::string(argv[1]) + "/schedule" : "");
  schedule(false, "");
  Kernel none;
  DefaultPolicy p(config(true));
  none.stats.conflicts = UINT64_MAX;
  assert(!Access::due(p, none)); // no retained session: never due
}

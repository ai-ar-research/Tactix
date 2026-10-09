#undef NDEBUG
// DefaultPolicy's persistent affine search runs only in natural full-restart
// root windows, due at Kissat's Eliminate conflict limit (NLOG2N occurrences,
// kissat_scale_delta), which at expiry waits until the retained inputs
// changed (a root fact on the admitted support, a retired support variable,
// an owner or premise-lease change, or an unfinished slice), with a delay
// that doubles after a fruitless run and halves after a fruitful one. Run
// effort: a SET_EFFORT_LIMIT share of search ticks since the last run.
#include "policies/default_policy.hpp"
#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
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
  static ApplyResult startup(DefaultPolicy &p, Kernel &k) {
    return p.run_affine_search(k, true);
  }
  static uint64_t limit(const DefaultPolicy &p) { return p.affine_schedule_.next_conflicts; }
  static uint64_t occurrences(const DefaultPolicy &p) { return p.affine_schedule_.occurrences; }
  static uint32_t delay(const DefaultPolicy &p) { return p.affine_schedule_.delay_current; }
  static uint32_t delay_count(const DefaultPolicy &p) { return p.affine_schedule_.delay_count; }
  static uint64_t last_ticks(const DefaultPolicy &p) { return p.affine_schedule_.last_ticks; }
  static uint64_t effort(const DefaultPolicy &p, const Kernel &k) {
    return p.root_service_effort(k, p.affine_schedule_, p.config_.inprocess.sweep_permille);
  }
  static ApplyResult restart(DefaultPolicy &p, Kernel &k) {
    return p.affine_restart_service(k);
  }
};
} // namespace tactix

using Access = DefaultAffineSearchTestAccess;

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
  DefaultStrategyConfig c;
  c.affine_search.enabled = true;
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

static void add_unit(Kernel &k, Proof &proof, Fixture &f, int32_t literal) {
  const Lit lit = kernel_internalize(k, literal);
  const uint64_t id = ++k.next_clause_id;
  proof.seed_population(id, &lit, 1);
  kernel_assign_unit(k, lit);
  assert(propagate_apply(k) == ApplyResult::Ok);
  f.clauses.push_back({literal});
}

// The XORs over {1,2,3} and {2,3,4} imply x1 = x4, which Boolean propagation
// cannot see; x4 -> x5 lets that consequence force a root fact off the
// support. An independent XOR over {7,8,9} completes the support.
int main(int argc, char **argv) {
  assert(argc == 1 || argc == 2);
  const std::string stem = argc == 2 ? std::string(argv[1]) + "/trigger" : "";
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
  const std::string path = stem.empty() ? "/tmp/tactix-affine-restart-trigger.dsr" : stem + ".dsr";
  Proof proof;
  assert(proof.open(path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&k.internal_to_external);
  proof.bind_external_variable_map(&k.external_to_internal);
  k.proof = &proof;
  DefaultStrategyConfig config;
  config.affine_search.enabled = true;
  DefaultPolicy p(config);
  const auto init = Access::initialize(p, k);
  assert(init.status == AffineMatrixInitStatus::Applied && session.enabled() &&
         session.persistent());
  assert(Access::startup(p, k) == ApplyResult::Ok);
  const auto &d = p.affine_search_diagnostics();
  // The startup run publishes nothing and reaches its fixpoint: fruitless,
  // so the delay goes 0 -> 1. Its occurrence sets the NLOG2N(1) limit.
  assert(d.reasons_published == 0 && d.restart_runs == 0 && d.fruitless_runs == 1);
  assert(Access::delay(p) == 1 && Access::delay_count(p) == 1);
  assert(Access::occurrences(p) == 1 && Access::last_ticks(p) == k.stats.search_ticks);
  assert(Access::limit(p) == expected_delta(kernel_irredundant_clauses(k), 1));
  // Below the conflict limit a window does nothing, even after a support fact.
  k.stats.conflicts = Access::limit(p) - 1;
  add_unit(k, proof, f, 6);
  assert(Access::restart(p, k) == ApplyResult::NoOp && d.unchanged_skips == 0);
  // At the limit, unchanged inputs wait instead of firing; decisions and a
  // root fact off the support change nothing.
  k.stats.conflicts = Access::limit(p);
  assert(Access::restart(p, k) == ApplyResult::NoOp && d.unchanged_skips == 1);
  kernel_assign_decision(k, kernel_internalize(k, 2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(Access::restart(p, k) == ApplyResult::NoOp); // above the root
  kernel_backtrack(k, 0);
  assert(Access::restart(p, k) == ApplyResult::NoOp && d.unchanged_skips == 2);
  assert(Access::occurrences(p) == 1 && d.restart_runs == 0);
  // A root fact on the support changes the inputs; the pending delay skips
  // this occurrence, which still advances the NLOG2N count to 2.
  add_unit(k, proof, f, 1);
  assert(Access::restart(p, k) == ApplyResult::NoOp);
  assert(d.delayed_runs == 1 && Access::delay_count(p) == 0 && Access::occurrences(p) == 2);
  const uint64_t clauses = kernel_irredundant_clauses(k);
  assert(Access::limit(p) == k.stats.conflicts + expected_delta(clauses, 2));
  assert(expected_delta(clauses, 2) > expected_delta(clauses, 1));
  // The next limit runs: the GF(2) consequence x4 is published and root BCP
  // forces x5. Fruitful halves the delay 1 -> 0.
  k.stats.conflicts = Access::limit(p);
  assert(Access::restart(p, k) == ApplyResult::Ok);
  assert(d.restart_runs == 1 && d.refreshes == 1 && d.reasons_published >= 1);
  assert(k.values[kernel_internalize(k, 4)] > 0 && k.values[kernel_internalize(k, 5)] > 0);
  assert(d.fruitful_runs == 1 && Access::delay(p) == 0 && Access::occurrences(p) == 3);
  assert(Access::limit(p) ==
         k.stats.conflicts + expected_delta(kernel_irredundant_clauses(k), 3));
  // The run reached its fixpoint, so its own support fact x4 is consumed.
  k.stats.conflicts = Access::limit(p);
  assert(Access::restart(p, k) == ApplyResult::NoOp && d.unchanged_skips == 3);
  // x7 leaves two open variables in its XOR: a fruitless run (delay 0 -> 1).
  add_unit(k, proof, f, 7);
  assert(Access::restart(p, k) == ApplyResult::Ok && d.restart_runs == 2);
  assert(d.fruitless_runs == 2 && Access::delay(p) == 1);
  // x8 changes the support (BCP already forced x9); the delay first skips
  // one occurrence, then the next limit runs.
  add_unit(k, proof, f, 8);
  k.stats.conflicts = Access::limit(p);
  assert(Access::restart(p, k) == ApplyResult::NoOp && d.delayed_runs == 2);
  k.stats.conflicts = Access::limit(p);
  assert(Access::restart(p, k) == ApplyResult::Ok && d.restart_runs == 3);
  assert(k.values[kernel_internalize(k, 9)] != 0);
  if (!stem.empty()) {
    std::ofstream(stem + ".cnf") << cnf(f);
  } else {
    std::remove(path.c_str());
  }
  session.disable(k);
  k.proof = nullptr;
  k.structural_provenance = nullptr;
  std::fclose(records);
  // No retained session: never runs.
  Kernel none;
  none.stats.conflicts = UINT64_MAX;
  DefaultPolicy q(config);
  assert(Access::restart(q, none) == ApplyResult::NoOp);
  effort_limit();
}

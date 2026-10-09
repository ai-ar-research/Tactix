#undef NDEBUG
// DefaultPolicy's persistent affine search runs only in natural full-restart
// root windows, and only after its retained inputs changed: a root fact on
// the admitted support, a retired support variable, an owner or premise-lease
// change, or an unfinished slice. Decisions, restarts and root facts off the
// support never trigger it.
#include "policies/default_policy.hpp"
#include "kernel/kernel.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include <cassert>
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
    const ApplyResult result = p.run_affine_search(k, true);
    affine_matrix_search_consume_inputs(k, *k.affine_matrix_search_session, p.affine_inputs_);
    return result;
  }
  static ApplyResult restart(DefaultPolicy &p, Kernel &k) {
    return p.affine_restart_service(k);
  }
};
} // namespace tactix

using Access = DefaultAffineSearchTestAccess;

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
  assert(d.reasons_published == 0 && d.restart_runs == 0);
  // Unchanged inputs: a restart window does nothing.
  assert(Access::restart(p, k) == ApplyResult::NoOp);
  assert(d.unchanged_skips == 1 && d.restart_runs == 0 && d.refreshes == 0);
  // Decision-trail churn and a root fact off the support change nothing.
  kernel_assign_decision(k, kernel_internalize(k, 2));
  assert(propagate_apply(k) == ApplyResult::Ok);
  assert(Access::restart(p, k) == ApplyResult::NoOp); // above the root
  kernel_backtrack(k, 0);
  add_unit(k, proof, f, 6);
  assert(Access::restart(p, k) == ApplyResult::NoOp);
  assert(d.unchanged_skips == 2 && d.restart_runs == 0);
  // A root fact on the support runs the service, which publishes x4; root
  // BCP then forces x5.
  add_unit(k, proof, f, 1);
  assert(Access::restart(p, k) == ApplyResult::Ok);
  assert(d.restart_runs == 1 && d.refreshes == 1 && d.reasons_published >= 1);
  assert(k.values[kernel_internalize(k, 4)] > 0 && k.values[kernel_internalize(k, 5)] > 0);
  // The run's own support fact x4 wakes exactly one more run, which finds
  // nothing; the window is then idle again.
  assert(Access::restart(p, k) == ApplyResult::Ok && d.restart_runs == 2);
  assert(Access::restart(p, k) == ApplyResult::NoOp && d.restart_runs == 2);
  // A support fact in the independent component wakes it again.
  add_unit(k, proof, f, 7);
  assert(Access::restart(p, k) == ApplyResult::Ok && d.restart_runs == 3);
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
  DefaultPolicy q(config);
  assert(Access::restart(q, none) == ApplyResult::NoOp);
}

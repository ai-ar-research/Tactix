#pragma once
#ifdef NDEBUG
#error "query_test_support.hpp requires assertions"
#endif
#include "session/query_kernel.hpp"
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"
#include "strategy/conflict_analysis_execution.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
namespace query_test {
using namespace tactix;
using namespace tactix::session;
inline void clause(QueryHistory &h, std::initializer_list<int32_t> literals) {
  for (const int32_t l : literals) assert(h.add(l) == InputStatus::Ok);
  assert(h.add(0) == InputStatus::Ok);
}

inline std::unique_ptr<Proof> writer(const std::string &stem, const std::string &mode) {
  if (stem.empty() || mode == "none") return {};
  auto proof = std::make_unique<Proof>();
  if (mode == "auto")
    assert(proof->open_automatic((stem+".proof").c_str(), false));
  else
    assert(proof->open((stem+".proof").c_str(),
        mode == "pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
  return proof;
}

inline void export_query(const QueryHistory &history, const std::string &stem) {
  if (stem.empty()) return;
  FILE *file = std::fopen((stem+".cnf").c_str(), "w");
  assert(file && history.write_query_dimacs(history.active_query()->id, file));
  assert(std::fclose(file) == 0);
}

inline void export_model(const QueryHistory &history, const Kernel &kernel,
                         const std::string &stem) {
  if (stem.empty()) return;
  FILE *file = std::fopen((stem+".model").c_str(), "w");
  assert(file);
  for (uint32_t v = 1; v <= history.active_query()->query_variables; ++v)
    assert(std::fprintf(file, "%d ", kernel.model_values[internalize(v)] > 0 ? int(v) : -int(v)) > 0);
  assert(std::fputs("0\n", file) >= 0 && std::fclose(file) == 0);
}

// Two independent fixed-test schedules drive the same query boundary: direct
// registered-rule implementations, and dynamic registry dispatch. This is not
// an incremental adapter for the production strategies inventoried in the plan.
inline int drive(QueryKernel &session, QueryHistory &history, bool dynamic) {
  (void)history; // model checking now belongs to the shared query service
  const auto t = session.ticket();
  Kernel &k = *session.kernel(t);
  for (unsigned steps = 0; steps != 10000; ++steps) {
    kernel_check_invariants(k);
    const auto next = session.next_assumption(t);
    switch (next.status) {
    case AssumptionStatus::BaseUnsat:
      assert(session.flush_proof(t));
      return 20;
    case AssumptionStatus::QueryUnsat:
      assert(session.publish_assumption_refutation(t));
      assert(!k.inconsistent);
      return 20;
    case AssumptionStatus::Propagate:
      if (dynamic) {
        const auto &r = tactix_rules()[tactix_rule_id("Propagate")];
        assert(rule_guard(r, k));
        r.apply(k);
      } else propagate_service(k);
      break;
    case AssumptionStatus::Analyze:
      assert(!k.pending.valid);
      if (dynamic) {
        const auto &r = tactix_rules()[tactix_rule_id("ConflictAnalysis")];
        assert(rule_guard(r, k));
        r.apply(k);
      } else execution::execute_action(k, execution::ExplicitConflictAnalysisRequest{0});
      break;
    case AssumptionStatus::Decide:
      assert(session.decide_assumption(t, next.index));
      break;
    case AssumptionStatus::Backtrack:
      restart_apply_to(k, 0);
      break;
    case AssumptionStatus::Ready:
      if (!k.unassigned_active) {
        assert(session.check_model(t).status == ModelStatus::Valid);
        return 10;
      }
      for (Var v = 0; v < k.nvars; ++v) {
        if (kernel_var_retired(k, v) || k.values[make_lit(v, false)]) continue;
        const Lit l = make_lit(v, false);
        if (dynamic) {
          const auto &r = tactix_rules()[tactix_rule_id("Decide")];
          assert(rule_guard(r, k));
          assert(r.apply_instance(k, lit_instance(k, l)) == ApplyResult::Ok);
        } else assert(decide_apply(k, l, false) == ApplyResult::Ok);
        break;
      }
      break;
    default:
      assert(false && "unexpected unsupported/stale transition in a fixed CDCL test");
    }
  }
  assert(false && "fixed query failed to terminate");
  return 0;
}


} // namespace query_test

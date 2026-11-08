#undef NDEBUG
#include "query_test_support.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/bce/bce.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "strategy/representation_execution.hpp"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

using namespace tactix;
using namespace tactix::session;
using query_test::clause;

static NativeQueryLimits limits() {
  return {{256, 4096, 1024}, {64, 4096, 1024, 8192, 64, 1024, 8192}, 1000000};
}

static void eliminate(Kernel &k, int lit) {
  const auto &bve = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule_guard(bve, k));
  assert(bve.apply_instance(k, lit_instance(k, kernel_internalize(k, lit))) == ApplyResult::Ok);
  heuristics_consume_retirements(k);
}

static void scenario(const std::string &name, const std::string &prefix,
                     const std::string &mode, bool dynamic) {
  QueryHistory h;
  QueryKernel s(limits());
  DefaultPolicy policy;
  if (name != "unitconflict") {
    if (name == "normalize") {
      clause(h, {1}); clause(h, {-1, 2}); clause(h, {3, 4});
      assert(h.assume(3) == InputStatus::Ok);
    } else {
      clause(h, {1, 2});
      if (name == "bve") { clause(h, {-1, 3}); clause(h, {4, 5}); }
      if (name == "bce") { clause(h, {-2, 3}); clause(h, {4, 5}); }
      if (name == "duplicate") clause(h, {3, 4});
      if (name == "default") assert(h.assume(2) == InputStatus::Ok);
      if (name == "bve") assert(h.assume(3) == InputStatus::Ok);
      if (name == "growth" || name == "default")
        assert(h.assume(1) == InputStatus::Ok);
    }
  }
  Kernel *kernel = nullptr;
  Heuristics *guidance = nullptr;
  uint64_t previous = 0;
  const auto query = [&](int expected, bool base_unsat, uint64_t appended, int created) {
    assert(h.begin_query() == InputStatus::Ok);
    const uint64_t id = h.active_query()->id;
    const std::string file = prefix.empty() ? "" : prefix+"-"+std::to_string(id);
    if (!file.empty()) {
      FILE *cnf = std::fopen((file+".cnf").c_str(), "w");
      assert(cnf && h.write_query_dimacs(id, cnf) && std::fclose(cnf) == 0);
    }
    std::unique_ptr<Proof> writer;
    if (mode != "none") {
      writer = std::make_unique<Proof>();
      assert(writer->open((file+".proof").c_str(),
          mode == "pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
    }
    const auto old = s.ticket();
    const auto scores = guidance ? guidance->scores.score : std::vector<double>{};
    const auto phases = guidance ? guidance->saved : std::vector<int8_t>{};
    const auto admitted = s.advance(h, std::move(writer));
    assert(admitted.status == (kernel ? NativeAdvanceStatus::Ok : NativeAdvanceStatus::Replayed));
    if (kernel) {
      assert(admitted.appended == appended && admitted.created == uint64_t(created));
      assert(s.kernel(s.ticket()) == kernel && kernel->h == guidance && !s.kernel(old));
      assert(std::equal(scores.begin(), scores.end(), guidance->scores.score.begin()));
      for (Var v = 0; v < phases.size(); ++v)
        if (!kernel->values[make_lit(v, false)]) assert(phases[v] == guidance->saved[v]);
    } else {
      kernel = s.kernel(s.ticket());
      guidance = kernel->h;
      if (name == "bve") { eliminate(*kernel, 1); assert(kernel_compact_variables(*kernel)); }
      if (name == "bce") {
        for (int lit : {1, -2, 4}) assert(bce_try(*kernel, internalize(lit)) == ApplyResult::Ok);
      }
      if (name == "normalize") assert(kernel_compact_variables(*kernel, true));
      if (name == "licensed") {
        SubstitutionRedundancyInstance sr;
        sr.clause = {internalize(1)}; sr.witness = {internalize(1)};
        assert(substitution_redundancy_apply_detailed(*kernel, sr).rule_result == ApplyResult::Ok);
      }
    }
    if (name == "growth" && id == 5) {
      eliminate(*kernel, 9);
      assert(kernel_compact_variables(*kernel));
      assert(kernel_internalize(*kernel, 9) == INVALID_LIT);
    }
    int verdict;
    if (name == "default") {
      const auto result = policy.solve_query(s);
      assert(result.status == QuerySolveStatus::Complete);
      verdict = result.result;
    } else verdict = query_test::drive(s, h, dynamic);
    assert(verdict == expected && kernel->inconsistent == base_unsat);
    kernel_check_invariants(*kernel);
    if (expected == 10) {
      assert(s.check_model(s.ticket()).status == ModelStatus::Valid);
      if (!file.empty()) {
        FILE *model = std::fopen((file+".model").c_str(), "w");
        assert(model);
        for (uint32_t v = 1; v <= h.active_query()->query_variables; ++v)
          assert(std::fprintf(model, "%d ", kernel->model_values[internalize(v)] > 0 ? int(v) : -int(v)) > 0);
        assert(std::fprintf(model, "0\n") > 0 && std::fclose(model) == 0);
      }
    }
    assert(s.flush_proof(s.ticket()));
    std::printf("q=%llu result=%d base=%d vars=%u external=%zu rho=%zu appended=%llu created=%llu conflicts=%llu decisions=%llu\n",
        (unsigned long long)id, verdict, base_unsat, kernel->nvars,
        kernel->external_to_internal.size(), kernel->reconstruction.size(),
        (unsigned long long)admitted.appended, (unsigned long long)admitted.created,
        (unsigned long long)kernel->stats.conflicts, (unsigned long long)kernel->stats.decisions);
    previous = id;
    assert(h.finish_query(id, verdict == 10 ? QueryResult::Sat : QueryResult::Unsat) == InputStatus::Ok);
  };
  query(10, false, 0, 0);
  if (name == "bve") {
    clause(h, {-1}); clause(h, {2, 6});
    assert(h.assume(-2) == InputStatus::Ok);
    query(20, false, 2, 1);
    assert(kernel->reconstruction.empty()); // restored +pivot taints the later -pivot witness
    clause(h, {-6});
    assert(h.assume(-3) == InputStatus::Ok);
    query(10, false, 1, 0);
  } else if (name == "bce") {
    clause(h, {-1}); clause(h, {-3});
    query(20, true, 2, 0);
    assert(kernel->reconstruction.size() == 3); // unrelated repair retained
  } else if (name == "normalize") {
    clause(h, {-2, 5, 5}); clause(h, {5, 6}); clause(h, {5, 6});
    clause(h, {7, -7}); clause(h, {1, 8}); clause(h, {9, 9});
    assert(h.assume(-5) == InputStatus::Ok);
    query(20, false, 6, 5);
    assert(kernel_external_value(*kernel, internalize(5)) > 0);
    assert(kernel_external_value(*kernel, internalize(9)) > 0);
    for (int lit : {-6, -7, -8}) assert(h.assume(lit) == InputStatus::Ok);
    query(10, false, 0, 0);
  } else if (name == "growth") {
    clause(h, {-2, 9});
    assert(h.assume(-1) == InputStatus::Ok && h.assume(-9) == InputStatus::Ok);
    query(20, false, 1, 1);
    assert(kernel->nvars == 3 && kernel_internalize(*kernel, 3) == INVALID_LIT);
    clause(h, {3, -9});
    for (int lit : {-1, 9, -3}) assert(h.assume(lit) == InputStatus::Ok);
    query(20, false, 1, 1);
    assert(kernel->nvars == 4 && var_of(kernel_internalize(*kernel, 3)) == 3);
    for (int lit : {-1, 9, 3}) assert(h.assume(lit) == InputStatus::Ok);
    query(10, false, 0, 0);
    query(10, false, 0, 0); // weaken and compact a newly appended source
    clause(h, {-9});
    assert(h.assume(-1) == InputStatus::Ok);
    query(20, false, 1, 0); // restore that source using its retained native ID
  } else if (name == "unitconflict") {
    clause(h, {5}); clause(h, {-5});
    query(20, true, 2, 1);
    assert(kernel->nvars == 1 && kernel_internalize(*kernel, 1) == INVALID_LIT);
  } else if (name == "empty") {
    clause(h, {});
    query(20, true, 1, 0);
  } else if (name == "duplicate") {
    clause(h, {2, 1}); clause(h, {1, 1, 2}); clause(h, {1, 2, 3, 3}); clause(h, {5, -5});
    assert(h.assume(-1) == InputStatus::Ok && h.assume(-2) == InputStatus::Ok);
    query(20, false, 4, 1);
    assert(kernel_binary_copies(*kernel, kernel_internalize(*kernel, 1), kernel_internalize(*kernel, 2)) == 1);
    for (int lit : {1, 2, 3, 4, 5}) assert(h.assume(lit) == InputStatus::Ok);
    query(10, false, 0, 0);
  } else if (name == "licensed") {
    // An old restriction is now independently justified by the new prefix.
    clause(h, {1});
    assert(h.assume(-1) == InputStatus::Ok);
    query(20, false, 1, 0);
    assert(h.assume(1) == InputStatus::Ok);
    query(10, false, 0, 0);
  } else {
    assert(name == "default");
    clause(h, {-1, 3});
    assert(h.assume(1) == InputStatus::Ok && h.assume(-3) == InputStatus::Ok);
    query(20, false, 1, 1);
    clause(h, {4, -3});
    for (int lit : {-1, 3, -4}) assert(h.assume(lit) == InputStatus::Ok);
    query(20, false, 1, 1);
    for (int lit : {-1, 3, 4}) assert(h.assume(lit) == InputStatus::Ok);
    query(10, false, 0, 0);
  }
  assert(previous > 1);
}

static void admission_and_limits() {
  Kernel k;
  ScopedHeuristics h(k);
  auto admit = [&](int literal) {
    return execution::execute_action(k, execution::AdmitExternalVariableRequest{literal});
  };
  assert(admit(0).status == NativeVariableStatus::InvalidLiteral);
  assert(admit(INT32_MIN).status == NativeVariableStatus::InvalidLiteral);
  k.warming = true;
  assert(admit(9).status == NativeVariableStatus::Busy && k.nvars == 0);
  k.warming = false;
  assert(admit(9).status == NativeVariableStatus::Created);
  assert(k.nvars == 1 && k.external_to_internal.size() == 9);
  assert(admit(-3).status == NativeVariableStatus::Created);
  assert(k.nvars == 2 && kernel_externalize(k, make_lit(1, true)) == -3);
  assert(admit(9).status == NativeVariableStatus::Existing);
  kernel_check_invariants(k);
  kernel_new_extension_vars(k, 1);
  const auto extent = k.external_to_internal.size();
  assert(admit(2).status == NativeVariableStatus::UnsupportedNamespace);
  assert(k.external_to_internal.size() == extent && k.nvars == 3);

  for (bool width : {false, true}) {
    auto bounds = limits();
    if (width) bounds.proof.maximum_clause_literals = 2;
    else bounds.proof.maximum_variables = 4;
    QueryHistory history;
    clause(history, {1, 2});
    assert(history.begin_query() == InputStatus::Ok);
    QueryKernel owner(bounds);
    assert(owner.replay(history) == ReplayStatus::Ok);
    Kernel &kernel = *owner.kernel(owner.ticket());
    const auto next = kernel.next_clause_id;
    const auto old_extent = kernel.external_to_internal.size();
    assert(history.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
    if (width) clause(history, {1, 2, 3});
    else assert(history.assume(9) == InputStatus::Ok);
    assert(history.begin_query() == InputStatus::Ok);
    assert(owner.advance(history).status == NativeAdvanceStatus::Exhausted);
    assert(kernel.next_clause_id == next && kernel.external_to_internal.size() == old_extent);
    assert(kernel.reconstruction.empty());
  }
}

int main(int argc, char **argv) {
  if (argc == 5) { scenario(argv[1], argv[2], argv[3], std::string(argv[4]) == "dynamic"); return 0; }
  assert(argc == 1);
  admission_and_limits();
  for (bool dynamic : {false, true})
    for (const char *name : {"bve", "bce", "normalize", "growth", "unitconflict", "empty", "duplicate", "licensed", "default"})
      scenario(name, "", "none", dynamic);
}

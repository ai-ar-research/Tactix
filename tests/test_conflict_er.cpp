#include "unit_test.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/analyze/analyze.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include "rules/factor/fx.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/restart/restart.hpp"
#include "strategy/conflict_analysis_execution.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <cstring>
#include <string>

namespace {

void decide(Kernel &k, int literal, ApplyResult expected = ApplyResult::Ok) {
  const Lit l = kernel_internalize(k, literal);
  assert(!k.value(l));
  kernel_assign_decision(k, l);
  assert(propagate_service(k) == expected);
}

ApplyResult analyze(Kernel &k, bool native, bool er) {
  if (native) {
    execution::NativeConflictAnalysisRequest request;
    request.target_mode = AnalysisTarget::Chrono;
    request.conflict_er = er;
    return execution::execute_action(
        k, request, [](auto apply) { return apply(); },
        [](ApplyResult) { return 0; }).result;
  }
  const auto &rule = tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  return apply_rule_semantic_instance(
      rule, k, er ? ANALYZE_TARGET_CHRONO_ER : ANALYZE_TARGET_CHRONO);
}

void publish_fx(Kernel &k) {
  std::vector<Ref> sources;
  for (const auto [ref, clause] : arena_records(k.arena))
    if (!clause->garbage())
      sources.push_back(ref);
  FxOptions options;
  options.gates = unsigned(FxGate::Xor);
  uint64_t work = UINT64_MAX;
  const auto plan = factor_find_fx(k, kernel_internalize(k, 13), sources,
                                   options, work);
  assert(plan && plan->pairs().size() == 5);
  const auto &rule = tactix_rules()[tactix_rule_id("Factor")];
  assert(apply_rule_structured_preserve_startup(rule, k, "FX before ER", [&] {
    const int output = factor_publish_fx(k, *plan, work);
    return output == 18 ? ApplyResult::Ok : ApplyResult::NoOp;
  }) == ApplyResult::Ok);
}

void exercise(const char *stem, const char *format, bool sat, bool native,
              bool er, bool fx = false) {
  const int variables = fx ? 17 : 12;
  const int extension = variables + 1 + fx;
  const int clauses = (sat ? 7 : 10) + (fx ? 10 : 0);
  std::string cnf = "p cnf " + std::to_string(variables) + " " +
      std::to_string(clauses) + "\n" +
      "1 4 5 7 0\n1 6 -7 0\n"
      "2 4 5 8 0\n2 6 -8 0\n"
      "3 -1 9 0\n3 -2 -9 0\n"
      "10 11 0\n" +
      (sat ? "" : "10 -11 0\n-10 11 0\n-10 -11 0\n");
  if (fx) {
    const char *tails[] = {"15 16", "15 -16", "15 17", "15 -17", "16 17"};
    for (const char *tail : tails)
      cnf += std::string("13 -14 ") + tail + " 0\n-13 14 " + tail + " 0\n";
  }
  Kernel k;
  ScopedHeuristics heuristics(k);
  Proof proof;
  if (stem && std::strcmp(format, "none")) {
    assert(proof.open((std::string(stem) + ".proof").c_str(),
                      !std::strcmp(format, "pbp") ? ProofFormat::PbpStreaming
                                                   : ProofFormat::DsrAscii));
    k.proof = &proof;
    proof.bind_variable_map(&k.internal_to_external);
    proof.bind_external_variable_map(&k.external_to_internal);
  }
  FILE *input = std::tmpfile();
  assert(input);
  std::fputs(cnf.c_str(), input);
  std::rewind(input);
  assert(!parse_dimacs(input, k));
  std::fclose(input);
  if (fx)
    publish_fx(k);

  decide(k, -4);
  decide(k, -5);
  decide(k, -6);
  decide(k, -1, ApplyResult::Conflict);
  assert(analyze(k, native, er) == ApplyResult::Ok);
  assert(k.stats.learned == 1 && !k.stats.conflict_er_definitions);
  assert(propagate_service(k) == ApplyResult::Ok);
  decide(k, -2, ApplyResult::Conflict);
  assert(analyze(k, native, er) == ApplyResult::Ok);
  assert(k.stats.learned == 2 && k.stats.conflict_er_definitions == unsigned(er));
  if (er) {
    const Lit output = kernel_internalize(k, extension);
    assert(k.value(output) < 0 && k.var_level(var_of(output)) == 3);
    assert(kernel_binary_live(k, output, kernel_internalize(k, 1)));
    assert(kernel_binary_live(k, output, kernel_internalize(k, 2)));
  }
  assert(propagate_service(k) == ApplyResult::Ok);
  assert(restart_apply(k) == ApplyResult::Ok);
  assert(kernel_compact_variables(k));
  decide(k, 1);
  decide(k, 2);
  decide(k, -3, ApplyResult::Conflict);
  assert(analyze(k, native, er) == ApplyResult::Ok);
  assert(k.stats.conflict_er_rewrites == unsigned(er));
  assert(k.stats.conflict_er_literals_saved == unsigned(er));
  if (er) {
    const Lit quotient = kernel_internalize(k, extension);
    assert(k.pending.lits.size() == 2 && k.pending.lits[1] == quotient);
    assert(k.assigned[var_of(kernel_internalize(k, 3))].reason == quotient);
    assert(k.var_level(var_of(kernel_internalize(k, 3))) == 2);
  }
  assert(propagate_service(k) == ApplyResult::Ok);
  kernel_check_invariants(k);

  if (sat) {
    for (int v = 4; v <= variables; ++v) {
      const Lit literal = kernel_internalize(k, v);
      if (literal != INVALID_LIT && !k.value(literal))
        decide(k, v);
    }
    assert(!k.unassigned_active);
    kernel_extend_model(k);
  } else {
    decide(k, 10, ApplyResult::Conflict);
    if (analyze(k, native, er) != ApplyResult::Unsat)
      assert(propagate_service(k) == ApplyResult::Unsat);
    assert(k.inconsistent);
  }

  if (stem) {
    const std::string path(stem);
    FILE *out = std::fopen((path + ".cnf").c_str(), "w");
    assert(out);
    std::fputs(cnf.c_str(), out);
    std::fclose(out);
    if (sat) {
      out = std::fopen((path + ".model").c_str(), "w");
      assert(out);
      for (int v = 1; v <= variables; ++v)
        std::fprintf(out, "%d ", k.model_values[internalize(v)] > 0 ? v : -v);
      std::fputs("0\n", out);
      std::fclose(out);
    }
    std::printf("%llu %llu %llu\n",
                static_cast<unsigned long long>(k.stats.conflicts),
                static_cast<unsigned long long>(k.stats.conflict_er_definitions),
                static_cast<unsigned long long>(k.stats.conflict_er_rewrites));
  }
}

void check_retirement() {
  Kernel k;
  kernel_reserve_vars(k, 4);
  ScopedHeuristics heuristics(k);
  auto &er = k.conflict_er.emplace();
  const auto define = [&](Lit a, Lit b) {
    if (a > b)
      std::swap(a, b);
    const Lit z = extended_resolution_define_search_or(k, a, b);
    assert(z != INVALID_LIT);
    strategy::finish_semantic_variable_candidate_action(k, ApplyResult::Ok);
    er.retained.resize(k.nvars);
    er.retained[var_of(z)] = 1;
    er.definitions[a].push_back({b, z});
    return z;
  };
  const Lit a = internalize(1), b = internalize(2);
  const Lit c = internalize(3), d = internalize(4);
  const Lit operand = define(a, b);
  const Lit released = define(c, d);
  const Lit dependent = define(d, operand);
  const Lit recent = define(a, c);
  ++k.stats.reductions;
  conflict_er_release_cold_outputs(k);
  assert(conflict_er_retained(er, var_of(operand)));
  assert(!conflict_er_retained(er, var_of(released)));
  assert(conflict_er_retained(er, var_of(dependent)));
  assert(conflict_er_retained(er, var_of(recent)));
  assert(k.stats.conflict_er_released == 1);
  assert(kernel_binary_live(k, released, negate(c)));
  conflict_er_release_cold_outputs(k);
  assert(k.stats.conflict_er_released == 1);
}

void check_automatic_admission() {
  const auto check = [](const std::vector<int32_t> &formula, bool admitted) {
    Kernel k;
    ingest_dimacs(k, formula);
    DefaultStrategyConfig config;
    config.analysis.conflict_er = true;
    config.analysis.conflict_er_auto = true;
    DefaultPolicy policy(config);
    assert(policy.solve(k, nullptr) == 10);
    assert(k.stats.conflict_er_admissions == unsigned(admitted));
  };
  const std::vector<int32_t> binaries{1, 2, 0, 1, 3, 0, 2, 3, 0, 2, 4, 0};
  for (size_t count : {size_t{3}, size_t{4}}) {
    for (bool nonbinary : {false, true}) {
      std::vector<int32_t> formula(binaries.begin(), binaries.begin() + 3 * count);
      if (nonbinary)
        formula.insert(formula.end(), {-1, -2, -3, 0});
      check(formula, nonbinary && count == 4);
    }
  }

  const std::vector<int32_t> binary_dominated{
      1, 4, 0, 2, 4, 0, 3, 4, 0, 1, 5, 0, 2, 5, 0,
      3, 5, 0, 4, 6, 0, 5, 6, 0, -1, -2, -3, 0};
  for (const int32_t first : {4, 1, -1}) {
    auto formula = binary_dominated;
    formula.insert(formula.end(), {first, 5, 6, 0});
    check(formula, first == 4);
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 1) {
    for (bool sat : {false, true})
      for (bool native : {false, true})
        for (bool er : {false, true})
          exercise(nullptr, "none", sat, native, er);
    check_retirement();
    check_automatic_admission();
    exercise(nullptr, "none", true, true, true, true);
  } else {
    assert(argc == 5 || argc == 6);
    exercise(argv[1], argv[2], !std::strcmp(argv[3], "sat"),
             !std::strcmp(argv[4], "native"), true, argc == 6);
  }
}

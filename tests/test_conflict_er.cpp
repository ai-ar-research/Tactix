#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

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

void check_public_learned_pair() {
  tactix_c_handle *solver = tactix_init();
  const int propagate = tactix_rule_id("Propagate");
  const int analyze = tactix_rule_id("ConflictAnalysis");
  const int restart = tactix_rule_id("Restart");
  for (int literal : {1, 4, 5, 7, 0, 1, 6, -7, 0,
                      2, 4, 5, 8, 0, 2, 6, -8, 0, 10, 11, 0})
    tactix_add(solver, literal);
  for (int literal : {-4, -5, -6, -1, -2}) {
    assert(tactix_decide(solver, literal) == 0);
    const int result = tactix_apply(solver, propagate);
    if (literal == -1 || literal == -2) {
      assert(result == 2);
      assert(tactix_apply_semantic_instance(solver, analyze, TACTIX_ANALYZE_TARGET_CHRONO) == 0);
      assert(tactix_apply(solver, propagate) == 0);
    } else {
      assert(result == 0);
    }
  }
  const int tail[] = {6, 4, 5};
  const int fresh = tactix_fresh_variable_start(solver);
  assert(tactix_factor_learned_pair(solver, 1, 2, tail, 3) == -1);
  assert(tactix_apply(solver, restart) == 0);
  assert(tactix_factor_learned_pair(solver, 1, -2, tail, 3) == -1);
  assert(tactix_fresh_variable_start(solver) == fresh);
  assert(tactix_factor_learned_pair(solver, 1, 2, tail, 3) == fresh);
  assert(tactix_factor_learned_pair(solver, 1, 2, tail, 3) == -1);
  assert(tactix_solve(solver) == 10);
  tactix_release(solver);
}

void exercise_paid_origins(const std::string &stem, const char *format, bool sat, bool automatic) {
  Kernel k;
  ScopedHeuristics heuristics(k);
  Proof proof;
  if (std::strcmp(format, "none")) {
    assert(proof.open((stem + ".proof").c_str(),
                      !std::strcmp(format, "pbp") ? ProofFormat::PbpStreaming
                                                    : ProofFormat::DsrAscii));
    k.proof = &proof;
    proof.bind_variable_map(&k.internal_to_external);
    proof.bind_external_variable_map(&k.external_to_internal);
  }
  constexpr int tail_end = 26;
  constexpr int split = 27;
  constexpr int suppress_tail = 28;
  std::string cnf = "p cnf 28 " + std::to_string(sat ? 52 : 53) + "\n";
  for (int head : {1, 2})
    for (int sign : {1, -1}) {
      cnf += std::to_string(head) + ' ';
      for (int t = 3; t <= tail_end; ++t)
        cnf += std::to_string(t) + ' ';
      cnf += std::to_string(sign * split) + " 0\n";
    }
  for (int t = 3; t <= tail_end; ++t)
    for (int sign : {1, -1})
      cnf += std::to_string(-t) + ' ' + std::to_string(sign * suppress_tail) + " 0\n";
  if (!sat)
    cnf += "-1 -2 0\n";
  FILE *input = std::fopen((stem + ".cnf").c_str(), "w+");
  assert(input);
  assert(std::fputs(cnf.c_str(), input) >= 0);
  std::rewind(input);
  assert(!parse_dimacs(input, k));
  std::fclose(input);
  const auto lit = [&](int value) { return kernel_internalize(k, value); };
  Ref originals[2];
  for (int head : {1, 2}) {
    std::vector<Lit> learned{lit(head)};
    for (int t = 3; t <= tail_end; ++t)
      learned.push_back(lit(t));
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      proof.add_rup(id, learned.data(), learned.size());
    const Ref ref = kernel_add_large_clause(k, learned.data(), learned.size(), true, 2, id);
    originals[head - 1] = ref;
    k.pending.ref = ref;
    k.pending.id = id;
    ++k.stats.learned;
    if (automatic)
      conflict_er_observe(k, learned, true);
  }
  if (automatic) {
    assert(k.conflict_er->learned_pairs.size() == 1);
    assert(!k.stats.conflict_er_definitions);
    conflict_er_factor_learned_pairs(k);
    assert(k.stats.conflict_er_factored_words > 0);
    assert(k.stats.conflict_er_definitions == 1);
  } else {
    std::vector<Lit> tail;
    for (int t = tail_end; t >= 3; --t)
      tail.push_back(lit(t));
    const auto pair = conflict_er_find_learned_pair(k, lit(1), lit(2), tail);
    assert(pair.first.ref == originals[0] && pair.second.ref == originals[1]);
    assert(conflict_er_factor_learned_pair(k, pair) != INVALID_LIT);
  }
  assert(k.stats.conflict_er_factored_pairs == 1);
  const Lit output = lit(suppress_tail + 1);
  assert(kernel_binary_live(k, output, lit(1)) && kernel_binary_live(k, output, lit(2)));
  unsigned annotated = 0;
  for (const auto [ref, clause] : arena_records(k.arena))
    annotated += clause->search_er_support() && !clause->redundant();
  assert(annotated == 1);
  kernel_check_invariants(k);
  for (const Ref ref : originals)
    assert(clause_at(k.arena, ref)->garbage());
  if (automatic) {
    ++k.stats.reductions;
    conflict_er_release_cold_outputs(k);
    assert(conflict_er_retained(*k.conflict_er, var_of(output)));
    ++k.stats.reductions;
    conflict_er_release_cold_outputs(k);
    assert(!conflict_er_retained(*k.conflict_er, var_of(output)));
  }
  DefaultPolicy policy;
  assert(policy.solve(k, nullptr) == (sat ? 10 : 20));
  if (sat) {
    kernel_extend_model(k);
    FILE *model = std::fopen((stem + ".model").c_str(), "w");
    assert(model);
    for (int v = 1; v <= suppress_tail; ++v)
      std::fprintf(model, "%d ", k.model_values[internalize(v)] > 0 ? v : -v);
    std::fputs("0\n", model);
    std::fclose(model);
  }
  std::printf("%llu %llu\n", static_cast<unsigned long long>(k.stats.conflicts),
              static_cast<unsigned long long>(k.stats.propagations));
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 6 && !std::strcmp(argv[1], "factor-learned")) {
    exercise_paid_origins(argv[2], argv[3], !std::strcmp(argv[4], "sat"),
                          !std::strcmp(argv[5], "automatic"));
    return 0;
  }
  if (argc == 1) {
    for (bool sat : {false, true})
      for (bool native : {false, true})
        for (bool er : {false, true})
          exercise(nullptr, "none", sat, native, er);
    check_retirement();
    check_public_learned_pair();
    exercise(nullptr, "none", true, true, true, true);
  } else {
    assert(argc == 5 || argc == 6);
    exercise(argv[1], argv[2], !std::strcmp(argv[3], "sat"),
             !std::strcmp(argv[4], "native"), true, argc == 6);
  }
}

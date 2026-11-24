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
    request.conflict_er = er ? execution::ConflictErMode::Eager
                             : execution::ConflictErMode::Off;
    return execution::execute_action(
        k, request, [](auto apply) { return apply(); },
        [](ApplyResult) { return 0; }).result;
  }
  const auto &rule = tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  return apply_rule_semantic_instance(
      rule, k, er ? ANALYZE_TARGET_CHRONO_ER : ANALYZE_TARGET_CHRONO);
}

void append_fx_inputs(std::string &cnf, int first, int shared_tail) {
  const int next = first + 1;
  const int sides[5][2] = {{next + 1, next + 2}, {next + 1, -next - 2},
                           {shared_tail, next + 3}, {shared_tail, -next - 3},
                           {next + 2, next + 3}};
  for (const auto &[left, right] : sides) {
    const std::string tail = std::to_string(left) + ' ' + std::to_string(right) + " 0\n";
    cnf += std::to_string(first) + ' ' + std::to_string(-next) + ' ' + tail;
    cnf += std::to_string(-first) + ' ' + std::to_string(next) + ' ' + tail;
  }
}

void publish_fx(Kernel &k, int first, int output_literal) {
  std::vector<Ref> sources;
  for (const auto [ref, clause] : arena_records(k.arena))
    if (!clause->garbage())
      sources.push_back(ref);
  FxOptions options;
  options.gates = unsigned(FxGate::Xor);
  uint64_t work = UINT64_MAX;
  const auto plan = factor_find_fx(k, kernel_internalize(k, first), sources,
                                   options, work);
  assert(plan && plan->pairs().size() == 5);
  const auto &rule = tactix_rules()[tactix_rule_id("Factor")];
  assert(apply_rule_structured_preserve_startup(rule, k, "FX before ER", [&] {
    const int output = factor_publish_fx(k, *plan, work);
    return output == output_literal ? ApplyResult::Ok : ApplyResult::NoOp;
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
  if (fx)
    append_fx_inputs(cnf, 13, 15);
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
    publish_fx(k, 13, variables + 1);

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

void check_automatic_analysis_replay() {
  Kernel k;
  ScopedHeuristics heuristics(k);
  FILE *input = std::tmpfile();
  assert(input);
  std::fputs("p cnf 38 6\n", input);
  // Each head has two parents which resolve to the same wide tail.
  for (int head : {1, 2, 3}) {
    const int pivot = 35 + head;
    std::fprintf(input, "%d %d ", head, pivot);
    for (int lit = 4; lit < 20; ++lit)
      std::fprintf(input, "%d ", lit);
    std::fputs("0\n", input);
    std::fprintf(input, "%d %d ", head, -pivot);
    for (int lit = 20; lit <= 35; ++lit)
      std::fprintf(input, "%d ", lit);
    std::fputs("0\n", input);
  }
  std::rewind(input);
  assert(!parse_dimacs(input, k));
  std::fclose(input);

  for (int lit = 4; lit <= 35; ++lit)
    decide(k, -lit);
  execution::NativeConflictAnalysisRequest request;
  request.target_mode = AnalysisTarget::Chrono;
  request.conflict_er = execution::ConflictErMode::Automatic;
  for (int head : {1, 2}) {
    decide(k, -head, ApplyResult::Conflict);
    assert(execution::execute_action(
        k, request, [](auto apply) { return apply(); },
        [](ApplyResult) { return 0; }).result == ApplyResult::Ok);
    assert(propagate_service(k) == ApplyResult::Ok);
  }
  assert(k.conflict_er && k.conflict_er->factor_learned);
  assert((k.conflict_er->learned_pair_heads == std::vector<std::pair<int, int>>{{1, 2}}));

  const uint64_t recorded = execution::chrono_analysis_instance(request.conflict_er);
  assert(recorded == TACTIX_ANALYZE_TARGET_CHRONO_ER_AUTO);
  const auto &rule = tactix_rules()[tactix_rule_id("ConflictAnalysis")];
  decide(k, -3, ApplyResult::Conflict);
  assert(apply_rule_semantic_instance(rule, k, recorded) == ApplyResult::Ok);
  assert(propagate_service(k) == ApplyResult::Ok);
  assert(k.conflict_er->factor_learned && !k.stats.conflict_er_definitions);
  assert((k.conflict_er->learned_pair_heads ==
          std::vector<std::pair<int, int>>{{1, 2}, {2, 3}}));
  kernel_check_invariants(k);
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

void check_subthreshold_batch(int tail_end, size_t expected_offers) {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, tail_end);
  std::vector<Ref> originals;
  for (int omitted = 3; omitted <= 6; ++omitted)
    for (int head : {1, 2}) {
      std::vector<Lit> learned{kernel_internalize(k, head)};
      for (int t = 3; t <= tail_end; ++t)
        if (t != omitted)
          learned.push_back(kernel_internalize(k, t));
      const uint64_t id = ++k.next_clause_id;
      const Ref ref = kernel_add_large_clause(k, learned.data(), learned.size(), true, 2, id);
      originals.push_back(ref);
      k.pending.ref = ref;
      k.pending.id = id;
      ++k.stats.learned;
      conflict_er_observe(k, learned, true);
    }
  assert(k.conflict_er->learned_pair_heads.size() == expected_offers);
  conflict_er_factor_learned_pairs(k);
  assert(!k.stats.conflict_er_definitions);
  assert(!k.stats.conflict_er_factored_pairs);
  for (const Ref ref : originals)
    assert(!clause_at(k.arena, ref)->garbage());
}

void check_shared_root_sources() {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 31);
  std::vector<Ref> originals;
  for (int omitted = 4; omitted <= 8; ++omitted)
    for (int head : {1, 2, 3}) {
      std::vector<Lit> learned{kernel_internalize(k, head)};
      for (int t = 4; t <= 30; ++t)
        if (t != omitted)
          learned.push_back(kernel_internalize(k, t));
      const uint64_t id = ++k.next_clause_id;
      const Ref ref = kernel_add_large_clause(k, learned.data(), learned.size(), true, 2, id);
      originals.push_back(ref);
      k.pending.ref = ref;
      k.pending.id = id;
      ++k.stats.learned;
      conflict_er_observe(k, learned, true);
    }
  const auto nominations = k.conflict_er->learned_pair_heads;
  assert(nominations.size() == 10);
  kernel_assign_unit(k, kernel_internalize(k, 31));
  assert(!kernel_propagation_fixpoint(k));
  assert(!conflict_er_factor_learned_pairs(k, /*consume_seeds=*/false));
  assert(k.conflict_er->learned_pair_heads == nominations);
  assert(!k.stats.conflict_er_definitions);
  assert(propagate_service(k) == ApplyResult::Ok);
  assert(conflict_er_factor_learned_pairs(k, /*consume_seeds=*/false));
  assert(!k.conflict_er->learned_pair_heads.empty());
  assert(conflict_er_factor_learned_pairs(k));
  assert(k.conflict_er->learned_pair_heads.empty());
  assert(k.stats.conflict_er_definitions == 1);
  assert(k.stats.conflict_er_factored_pairs == 5);
  assert(k.nvars == 32);
  for (size_t i = 0; i < originals.size(); ++i)
    assert(clause_at(k.arena, originals[i])->garbage() == (i % 3 != 2));
}

void check_deferred_sources() {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 40);
  std::vector<Ref> originals;
  for (Var unique_tail = 31; unique_tail <= 36; ++unique_tail)
    for (Var head : {1u, 2u}) {
      std::vector<Lit> learned{make_lit(head, false)};
      for (Var v = 3; v < 30; ++v)
        learned.push_back(make_lit(v, false));
      learned.push_back(make_lit(unique_tail, false));
      const uint64_t id = ++k.next_clause_id;
      const Ref ref = kernel_add_large_clause(k, learned.data(), learned.size(), true, 3, id);
      originals.push_back(ref);
      k.pending.ref = ref;
      k.pending.id = id;
      ++k.stats.learned;
      if (originals.size() <= 2)
        conflict_er_observe(k, learned, true);
    }
  auto &er = *k.conflict_er;
  assert(er.learned_pair_heads.size() == 1);
  const auto heads = er.learned_pair_heads.front();
  {
    const DeferredConflictErBatch batch(k);
    for (const Ref ref : {originals[0], originals[1]})
      kernel_delete_clause(k, clause_at(k.arena, ref));
    kernel_collect_garbage(k);
    k.var_status.assign(k.nvars, VarStatus::Active);
    k.var_status[0] = VarStatus::Eliminated;
    heuristics_deactivate_var(k, 0);
    --k.unassigned_active;
    assert(kernel_compact_variables(k));
    assert(batch);
    assert(er.learned_pair_heads.size() == 1 && er.learned_pair_heads.front() == heads);
    assert(kernel_internalize(k, heads.first) == make_lit(0, false));
    assert(kernel_internalize(k, heads.second) == make_lit(1, false));
    conflict_er_factor_learned_pairs(k);
    assert(k.stats.conflict_er_definitions == 1);
    assert(k.stats.conflict_er_factored_pairs == 5);
  }
  assert(er.learned_pair_heads.empty());
  kernel_check_invariants(k);
}

void check_automatic_output_random_selection() {
  Kernel k;
  ScopedHeuristics heuristics(k);
  kernel_reserve_vars(k, 260);
  const Var output = kernel_new_extension_vars(k, 1);
  k.conflict_er.emplace().automatic_outputs_external.push_back(k.internal_to_external[output]);
  k.stats.conflicts = 855;
  DefaultBranching branching;
  branching.reset(0);
  assert(kernel_externalize(k, branching.pick(k)) == 255);
  assert(kernel_externalize(k, branching.pick(k)) == 130);

  for (Var v = 0; v < output; ++v)
    kernel_assign_unit(k, make_lit(v, false));
  heuristics_consume_root_assignments(k, 0);
  assert(k.unassigned_active == 1);
  assert(var_of(branching.pick(k)) == output);
}

void exercise_paid_origins(const std::string &stem, const char *format, bool sat,
                           bool automatic, bool fx) {
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
  constexpr int tail_end = 28;
  constexpr int split = 29;
  constexpr int suppress_tail = 30;
  const int variables = suppress_tail + (fx ? 5 : 0);
  std::vector<std::vector<int>> remainders(automatic ? 5 : 1);
  for (size_t i = 0; i < remainders.size(); ++i)
    for (int t = 3; t <= tail_end; ++t)
      if (t != static_cast<int>(i) + 3)
        remainders[i].push_back(t);
  const size_t clauses = 4 * remainders.size() + 2 * (tail_end - 2) + !sat + (fx ? 10 : 0);
  std::string cnf = "p cnf " + std::to_string(variables) + ' ' + std::to_string(clauses) + "\n";
  for (const auto &remainder : remainders)
    for (int head : {1, 2})
      for (int sign : {1, -1}) {
        cnf += std::to_string(head) + ' ';
        for (int t : remainder)
          cnf += std::to_string(t) + ' ';
        cnf += std::to_string(sign * split) + " 0\n";
      }
  for (int t = 3; t <= tail_end; ++t)
    for (int sign : {1, -1})
      cnf += std::to_string(-t) + ' ' + std::to_string(sign * suppress_tail) + " 0\n";
  if (!sat)
    cnf += "-1 -2 0\n";
  if (fx)
    append_fx_inputs(cnf, suppress_tail + 1, 4);
  FILE *input = std::fopen((stem + ".cnf").c_str(), "w+");
  assert(input);
  assert(std::fputs(cnf.c_str(), input) >= 0);
  std::rewind(input);
  assert(!parse_dimacs(input, k));
  std::fclose(input);
  if (fx)
    publish_fx(k, suppress_tail + 1, variables + 1);
  const auto lit = [&](int value) { return kernel_internalize(k, value); };
  std::vector<Ref> originals;
  std::vector<std::pair<int, int>> heads;
  for (const auto &remainder : remainders)
    for (int head : {1, 2}) {
      std::vector<Lit> learned{lit(head)};
      for (int t : remainder)
        learned.push_back(lit(t));
      const uint64_t id = ++k.next_clause_id;
      if (k.proof)
        proof.add_rup(id, learned.data(), learned.size());
      const Ref ref = kernel_add_large_clause(k, learned.data(), learned.size(), true, 2, id);
      originals.push_back(ref);
      k.pending.ref = ref;
      k.pending.id = id;
      ++k.stats.learned;
      if (automatic && originals.size() <= 2)
        conflict_er_observe(k, learned, true);
      if (automatic && originals.size() == 8) {
        heads = k.conflict_er->learned_pair_heads;
        assert(heads.size() == 1);
        conflict_er_factor_learned_pairs(k);
        assert(!k.stats.conflict_er_definitions);
        assert(!k.stats.conflict_er_factored_pairs);
        for (const Ref source : originals)
          assert(!clause_at(k.arena, source)->garbage());
      }
    }
  if (automatic) {
    for (const Ref ref : originals)
      clause_at(k.arena, ref)->set_glue(k.tier2 + 1);
    k.conflict_er->learned_pair_heads = heads;
    conflict_er_factor_learned_pairs(k);
    assert(!k.stats.conflict_er_definitions);
    assert(!k.stats.conflict_er_factored_pairs);
    for (const Ref ref : originals)
      assert(!clause_at(k.arena, ref)->garbage());

    clause_at(k.arena, originals.front())->set_glue(k.tier2);
    k.conflict_er->learned_pair_heads = std::move(heads);
    conflict_er_factor_learned_pairs(k);
    assert(k.stats.conflict_er_factored_words > 0);
    assert(k.stats.conflict_er_definitions == 1);
  } else {
    std::vector<Lit> tail;
    for (auto it = remainders.front().rbegin(); it != remainders.front().rend(); ++it)
      tail.push_back(lit(*it));
    const auto pair = conflict_er_find_learned_pair(k, lit(1), lit(2), tail);
    assert(pair.first.ref == originals[0] && pair.second.ref == originals[1]);
    assert(conflict_er_factor_learned_pair(k, pair) != INVALID_LIT);
  }
  assert(k.stats.conflict_er_factored_pairs == remainders.size());
  const Lit output = lit(variables + 1 + fx);
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
    for (int v = 1; v <= variables; ++v)
      std::fprintf(model, "%d ", k.model_values[internalize(v)] > 0 ? v : -v);
    std::fputs("0\n", model);
    std::fclose(model);
  }
  std::printf("%llu %llu\n", static_cast<unsigned long long>(k.stats.conflicts),
              static_cast<unsigned long long>(k.stats.propagations));
}

} // namespace

int main(int argc, char **argv) {
  if ((argc == 6 || argc == 7) && !std::strcmp(argv[1], "factor-learned")) {
    exercise_paid_origins(argv[2], argv[3], !std::strcmp(argv[4], "sat"),
                          !std::strcmp(argv[5], "automatic"), argc == 7);
    return 0;
  }
  if (argc == 1) {
    for (bool sat : {false, true})
      for (bool native : {false, true})
        for (bool er : {false, true})
          exercise(nullptr, "none", sat, native, er);
    check_retirement();
    check_automatic_analysis_replay();
    check_public_learned_pair();
    check_subthreshold_batch(7, 0);
    check_subthreshold_batch(30, 4);
    check_shared_root_sources();
    check_deferred_sources();
    check_automatic_output_random_selection();
    exercise(nullptr, "none", true, true, true, true);
  } else {
    assert(argc == 5 || argc == 6);
    exercise(argv[1], argv[2], !std::strcmp(argv[3], "sat"),
             !std::strcmp(argv[4], "native"), true, argc == 6);
  }
}

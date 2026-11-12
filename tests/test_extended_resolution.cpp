#include "unit_test.hpp"
#include "tactix_c_api.hpp"
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/extended_resolution/extended_resolution.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/substitute/substitute.hpp"
#include "strategy/bve_invocation.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <csignal>
#include <climits>

static int define(Kernel &k, ExtensionDefinition definition) {
  int output = 0;
  const auto &rule = tactix_rules()[tactix_rule_id("ExtendedResolution")];
  apply_rule_structured_preserve_startup(rule, k, "fixture", [&] {
    output = extended_resolution_define(k, definition);
    return output ? ApplyResult::Ok : ApplyResult::NoOp;
  });
  return output;
}

static bool value(int literal, unsigned assignment) {
  return bool(assignment & (1u << (std::abs(literal) - 1))) == (literal > 0);
}

TEST(truth_tables) {
  // Complete enumeration, including signed, repeated and complementary inputs.
  for (int gate = 1; gate <= 3; ++gate)
    for (int a : {-2, -1, 1, 2})
      for (int b : {-2, -1, 1, 2})
        for (unsigned assignment = 0; assignment < 4; ++assignment)
          for (int chosen = 0; chosen < 2; ++chosen) {
            Kernel k;
            kernel_reserve_vars(k, 2);
            const bool left = value(a, assignment), right = value(b, assignment);
            const bool expected = gate == 1 ? left && right :
                                  gate == 2 ? left || right : left != right;
            const int output = define(k, {static_cast<ExtensionGate>(gate), a, b});
            assert(output == 3 && kernel_num_output_vars(k) == 2);
            for (unsigned bit = 0; bit < 2; ++bit)
              kernel_assign_unit(k, make_lit(bit, !(assignment & (1u << bit))));
            kernel_assign_unit(k, kernel_internalize(k, chosen ? output : -output));
            const auto result = propagate_apply(k);
            assert((result == ApplyResult::Unsat) == (bool(chosen) != expected));
          }
}

static tactix_c_handle *fixture() {
  auto *s = tactix_init();
  for (int literal : {1, 2, 0, -1, 3, 0, -2, -3, 0}) tactix_add(s, literal);
  assert(tactix_status(s) == 0);
  return s;
}

TEST(public_composition_and_advice) {
  auto *s = fixture();
  auto &k = api_detail::kernel_for_fixed_testing(s);
  const int rule = tactix_rule_id("ExtendedResolution");
  assert(rule == 47 && tactix_action_kind(rule) == TACTIX_ACTION_PROOF_RULE);
  assert(!tactix_rules()[rule].default_inprocess);
  const auto scores = k.h->scores.score;
  const int first = tactix_define_extension(s, TACTIX_EXTENSION_XOR, 1, -2);
  assert(first == 4);
  for (size_t i = 0; i < scores.size(); ++i) assert(k.h->scores.score[i] == scores[i]);
  const int second = tactix_define_extension(s, TACTIX_EXTENSION_AND, first, 3);
  assert(second == 5);
  const auto candidates = strategy::semantic_variable_candidates(k);
  assert(candidates.size() == 1 && candidates[0].introduced_variables == 1);
  assert(candidates[0].producer == strategy::SemanticVariableProducer::ExtendedResolution);

  // Use only the public strategy API to choose the priority dose.
  tactix_online_step step{};
  step.step_id = 1;
  step.rule_id = tactix_rule_id("SemanticVariableAdvice");
  step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.budget_unlimited = 1;
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_SEMANTIC_VARIABLE_ADVICE;
  step.parameters.value.semantic_variable_advice = {1, 7, 1, 7};
  const tactix_online_program program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
  assert(tactix_online_configure(s, &program, 1, 0) == 0);
  tactix_online_result result{};
  const Var v = var_of(kernel_internalize(k, second));
  const double before = k.h->scores.score[v];
  assert(tactix_online_advance(s, &result) == 0);
  assert(k.h->scores.score[v] > before && k.h->queue_last == v);
  assert(tactix_decide(s, -second) == 0);
  assert(tactix_define_extension(s, TACTIX_EXTENSION_OR, 1, 2) == -1);
  assert(tactix_solve(s) == 10);
  assert(tactix_value(s, 1) == 1 || tactix_value(s, 2) == 2);
  assert(tactix_value(s, -1) == -1 || tactix_value(s, 3) == 3);
  assert(tactix_value(s, -2) == -2 || tactix_value(s, -3) == -3);
  tactix_release(s);
}

TEST(decline_and_namespace) {
  auto *s = fixture();
  auto &k = api_detail::kernel_for_fixed_testing(s);
  const auto n = k.nvars;
  const auto extent = k.external_to_internal.size();
  const auto id = k.next_clause_id, epoch = k.strategy_epoch;
  for (const auto bad : {ExtensionDefinition{ExtensionGate::And, 0, 1},
                         ExtensionDefinition{ExtensionGate::Or, INT_MIN, 1},
                         ExtensionDefinition{ExtensionGate::Xor, 77, 1}})
    assert(tactix_define_extension(s, int(bad.gate), bad.left, bad.right) == -1);
  assert(tactix_define_extension(s, 99, 1, 2) == -1);
  assert(k.nvars == n && k.external_to_internal.size() == extent &&
         k.next_clause_id == id && k.strategy_epoch == epoch);
  Var holes;
  assert(kernel_reserve_proof_variables(k, 5, holes));
  assert(tactix_define_extension(s, TACTIX_EXTENSION_AND, 4, 1) == -1);
  assert(tactix_define_extension(s, TACTIX_EXTENSION_AND, 1, 2) == 9);
  assert(kernel_num_output_vars(k) == 3);
  tactix_release(s);
}

TEST(assigned_root_rearmed) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_assign_unit(k, internalize(1));
  kernel_assign_unit(k, internalize(-2));
  assert(propagate_apply(k) != ApplyResult::Unsat);
  assert(kernel_propagation_fixpoint(k));
  const int output = define(k, {ExtensionGate::Xor, 1, 2});
  assert(output == 3 && !kernel_propagation_fixpoint(k));
  assert(!extended_resolution_guard(k));
  assert(propagate_apply(k) != ApplyResult::Unsat);
  assert(k.value(kernel_internalize(k, output)) > 0);
}

// The formula (1 v 2 v 3) for the extension-retention advice cases.
static tactix_c_handle *advice_fixture() {
 auto *s=tactix_init();for(int l:{1,2,3,0})tactix_add(s,l);
 assert(tactix_status(s)==0);return s;
}
TEST(advice_and_selected_substitution) {
 auto *s=advice_fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
 int q=tactix_define_extension(s,TACTIX_EXTENSION_OR,1,1);assert(q==4);
 assert(tactix_variable_retained(s,q)==1);
 Var v=var_of(kernel_internalize(k,q));double before=k.h->scores.score[v];
 assert(!tactix_bump_variable(s,q,3));assert(k.h->scores.score[v]>before&&k.h->queue_last==v);
 assert(!tactix_bump_variable(s,q,1)); // no staged-candidate dependency
 int candidate=0;assert(tactix_decide_candidates(s,&candidate,1)==1);
 assert(std::abs(candidate)==q);assert(!tactix_decide(s,candidate));
 assert(tactix_stat(s,TACTIX_STAT_DECISIONS_EXTENSION)==1);
 assert(!tactix_support(s,TACTIX_SUPPORT_PREPARATION_RESTART,0));
 const auto size=k.variable_retention.size();
 const auto epoch=k.strategy_epoch;
 assert(tactix_set_variable_retention(s,77,1)==-1&&k.variable_retention.size()==size);
 assert(k.strategy_epoch==epoch);
 assert(tactix_bump_variable(s,q,0)==-1);
 // DefaultPolicy's explicit selection keeps q; the mathematical rule remains
 // available without that selection after the startup deferral.
 k.substitute_retention=&k.variable_retention;
 assert(substitute_apply(k)!=ApplyResult::Unsat);assert(!kernel_var_retired(k,v));
 k.substitute_retention=nullptr;
 const auto release_epoch=k.strategy_epoch;
 assert(!tactix_set_variable_retention(s,q,0));assert(tactix_variable_retained(s,q)==0);
 assert(k.strategy_epoch==release_epoch+1);
 assert(substitute_apply(k)!=ApplyResult::Unsat);assert(kernel_var_retired(k,v));
 tactix_release(s);
}
TEST(initial_elimination) {
 auto *s=advice_fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
 int q=tactix_define_extension(s,TACTIX_EXTENSION_AND,1,2);assert(q==4);
 const Var v=var_of(kernel_internalize(k,q));
 k.substitute_retention=&k.variable_retention;
 DefaultPolicy p;p.inprocess().build(k);
 assert(p.inprocess().run_initial_fast_elimination(k,nullptr)==0);
 assert(!kernel_var_retired(k,v));
 k.substitute_retention=nullptr;
 const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
 assert(bve.guard(k));(void)bve.apply_instance(k,lit_instance(k,kernel_internalize(k,q)));
 assert(kernel_var_retired(k,v));
 tactix_release(s);
}
TEST(compact_retention) {
 auto *s=tactix_init();for(int l:{1,0,2,0,3,4,5,0})tactix_add(s,l);
 assert(tactix_status(s)==0);auto &k=api_detail::kernel_for_fixed_testing(s);
 const int q=tactix_define_extension(s,TACTIX_EXTENSION_XOR,3,4);assert(q==6);
 const Var before=var_of(kernel_internalize(k,q));
 assert(kernel_compact_variables(k,true));
 const Var after=var_of(kernel_internalize(k,q));assert(after<before);
 assert(tactix_variable_retained(s,q)==1);
 assert(k.variable_retention.count(k.internal_to_external[after]));
 assert(!tactix_bump_variable(s,q,1));
 assert(tactix_solve(s)==10);assert(tactix_variable_retained(s,q)==0);
 tactix_release(s);
}
TEST(generated_elimination) {
 auto *s=advice_fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
 const int q=tactix_define_extension(s,TACTIX_EXTENSION_AND,1,-1);assert(q==4);
 const Lit literal=kernel_internalize(k,q);const uint64_t instance=lit_instance(k,literal);
 const auto &bve=tactix_rules()[tactix_rule_id("BVE")];
 for(auto scope:{strategy::BveCandidateScope::All,strategy::BveCandidateScope::PureOnly,
                 strategy::BveCandidateScope::PureClosure}) {
  auto parameters=strategy::BveParameters::from_kernel(k);parameters.candidate_scope=scope;
  std::vector<uint64_t> selected;k.substitute_retention=&k.variable_retention;
  strategy::bve_complete_instances(k,bve,parameters,selected);
  assert(std::find(selected.begin(),selected.end(),instance)==selected.end());
  k.substitute_retention=nullptr;
  strategy::bve_complete_instances(k,bve,parameters,selected);
  assert(std::find(selected.begin(),selected.end(),instance)!=selected.end());
 }
 // Retention selects automatic candidates, never bans a mathematical step.
 k.substitute_retention=&k.variable_retention;assert(bve.guard(k));
 (void)bve.apply_instance(k,instance);assert(kernel_var_retired(k,var_of(literal)));
 k.substitute_retention=nullptr;tactix_release(s);
}
TEST(interrupted_startup) {
 auto *s=advice_fixture();auto &k=api_detail::kernel_for_fixed_testing(s);
 const int q=tactix_define_extension(s,TACTIX_EXTENSION_AND,1,2);assert(q==4);
 volatile sig_atomic_t stop=1;DefaultPolicy p;assert(p.solve(k,&stop)==0);
 assert(!k.substitute_retention&&tactix_variable_retained(s,q)==1);
 assert(tactix_solve(s)==10);assert(tactix_variable_retained(s,q)==0);
 tactix_release(s);
}

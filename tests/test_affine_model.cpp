#undef NDEBUG
#include "rules/lucky/affine_model.hpp"
#include "rules/lucky/model.hpp"
#include "rules/bve/bve.hpp"
#include "rules/propagate/propagate.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "strategy/random_online.hpp"
#include <algorithm>
#include <bit>
#include <cassert>
#include <set>
#include <iostream>
using namespace tactix;
using namespace tactix::strategy;

namespace tactix {
struct DefaultAffineModelTestAccess {
  static ApplyResult consume(DefaultPolicy &policy, Kernel &kernel) {
    return policy.consume_affine_model_hint(kernel);
  }
};
} // namespace tactix

static Kernel fixture(bool duplicate=false,bool incomplete=false) {
  Kernel k;kernel_reserve_vars(k,5);
  for(unsigned mask=0;mask<16;++mask)if(std::popcount(mask)%2==0) {
    if(incomplete&&mask==0)continue;
    Lit lits[4];for(unsigned i=0;i<4;++i)lits[i]=make_lit(i,(mask>>i)&1);
    kernel_add_large_clause(k,lits,4,false,0);
    if(duplicate)kernel_add_large_clause(k,lits,4,false,0);
  }
  for(unsigned i=0;i<3;++i)kernel_add_binary_clause(k,make_lit(i,true),make_lit(i+1,true));
  return k;
}
static void check(Kernel &k) {
  std::vector<uint8_t> model(k.nvars);
  for(Var v=0;v<k.nvars;++v)model[v]=k.value(make_lit(v,false))>0;
  assert(lucky_model_valid(k,model));
  assert(kernel_propagation_fixpoint(k));
}
static OnlineStrategyProgram program(AffineModelParameters parameters) {
  OnlineInvocation step;step.id=1;step.rule="Lucky";
  step.parameters=std::move(parameters);
  step.granularity=InvocationGranularity::OneInstance;step.instances={8};
  OnlineStrategyProgram p;p.sequence.push_back(std::move(step));return p;
}
int main() {
  // A validated Lucky model also settles binary-only propagation debt. The
  // public status path requires the full fixpoint, not just propagate_head.
  {
    Kernel k; kernel_reserve_vars(k, 2);
    kernel_add_binary_clause(k, internalize(1), internalize(2));
    k.binary_only_head = 0;
    const std::vector<uint8_t> model{1, 0};
    assert(lucky_model_valid(k, model));
    lucky_publish_model(k, model);
    assert(k.unassigned_active == 0);
    assert(kernel_propagation_fixpoint(k));
    kernel_check_invariants(k);
  }
  // A legal dense-BVE composition may retain an entailed redundant clause on
  // a retired pivot. Private completion must not count an unpublished value
  // as its only satisfying literal. The duplicate below is entailed by F.
  {
    Kernel k; kernel_reserve_vars(k, 3);
    const Lit clause[] = {internalize(1), internalize(2), internalize(3)};
    kernel_add_large_clause(k, clause, 3, false, 0, ++k.next_clause_id);
    kernel_add_large_clause(k, clause, 3, true, 1, ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-2), internalize(3), ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(2), internalize(-3), ++k.next_clause_id);
    assert(propagate_service(k) == ApplyResult::Ok);
    OccIndex caller;
    kernel_dense_connect(k, caller, false);
    const auto &bve = tactix_rules()[tactix_rule_id("BVE")];
    assert(apply_rule_instance(bve, k, lit_instance(k, internalize(1))) == ApplyResult::Ok);
    kernel_dense_disconnect(k);
    assert(kernel_var_retired(k, 0));
    assert(!lucky_model_valid(k, std::vector<uint8_t>{1, 0, 0}));
    assert(lucky_model_valid(k, std::vector<uint8_t>{0, 1, 1}));
    auto &parameters = k.affine_model_parameters;
    parameters.minimum_equations = 1;
    parameters.free_phase = parameters.residual_phase = false;
    parameters.candidate_budget = 1;
    const auto values = k.values;
    assert(affine_model_apply(k) == ApplyResult::NoOp);
    assert(k.values == values && k.trail.empty() && k.level == 0);
    kernel_check_invariants(k);
    // A later candidate satisfying the clause through an active variable
    // remains admissible, even though its private retired value is false.
    parameters.candidate_budget = 8;
    assert(affine_model_apply(k) == ApplyResult::Sat);
    kernel_check_invariants(k);
    assert(k.value(internalize(2)) > 0 && k.value(internalize(3)) > 0);
    kernel_extend_model(k);
    const int x = k.value(internalize(1));
    const int a = k.value(internalize(2)), b = k.value(internalize(3));
    assert(x > 0 || a > 0 || b > 0);
    assert((a < 0 || b > 0) && (a > 0 || b < 0));
  }
  // Count prefiltering must never mistake repeated copies for distinct parity
  // patterns. Exhaust every subset of the eight clauses over three variables,
  // including contradictory complete groups and mixed-parity near misses.
  for (unsigned subset = 0; subset < 256; ++subset) {
    Kernel k; kernel_reserve_vars(k, 3); ScopedHeuristics owner(k);
    unsigned counts[2] = {0, 0};
    for (unsigned pattern = 0; pattern < 8; ++pattern) {
      if (!(subset & (1u << pattern))) continue;
      ++counts[std::popcount(pattern) & 1];
      Lit clause[3];
      for (unsigned i = 0; i < 3; ++i)
        clause[2-i] = make_lit(i, (pattern >> i) & 1);
      for (unsigned copy = 0; copy < 3; ++copy)
        kernel_add_large_clause(k, clause, 3, false, 0);
    }
    const auto result = affine_model_apply(k);
    assert(k.stats.affine_model_equations == unsigned(counts[0] == 4) +
                                             unsigned(counts[1] == 4));
    if (result == ApplyResult::Sat) check(k);
    else assert(k.trail.empty());
    if (counts[0] == 4 && counts[1] == 4) assert(result == ApplyResult::NoOp);
  }
  uint64_t complete_work=0;
  for(unsigned mode=0;mode<5;++mode) {
    Kernel k=fixture(mode==1);ScopedHeuristics owner(k);
    auto parameters=k.affine_model_parameters;
    if(mode>=2){kernel_assign_unit(k,make_lit(0,mode==3));assert(propagate_apply(k)==ApplyResult::Ok);}
    if(mode==4){parameters.projection_mode=AffineProjectionMode::Explicit;parameters.projection={4,-1,3,-2};parameters.scopes={{2,2,2,4}};}
    install_rule_parameters(k,parameters);
    assert(affine_model_apply(k)==ApplyResult::Sat);check(k);
    assert(k.stats.affine_model_candidates>0);
    if(!mode)complete_work=k.stats.affine_model_work;
  }
  // Every charged-work boundary below the successful transaction must retain
  // exactly the original trail and values. Reconfiguration on the SAME Kernel
  // then completes the model, without rebuilding or restarting the solver.
  for(uint64_t budget=0;budget<complete_work;++budget) {
    Kernel k=fixture();ScopedHeuristics owner(k);
    auto parameters=k.affine_model_parameters;parameters.maximum_work=budget;
    install_rule_parameters(k,parameters);const std::vector<Lit> trail=k.trail;
    const auto values=k.values;
    assert(affine_model_apply(k)==ApplyResult::NoOp);
    assert(k.trail==trail&&k.values==values&&k.stats.affine_model_work<=budget);
    parameters.maximum_work=complete_work;install_rule_parameters(k,parameters);
    assert(affine_model_apply(k)==ApplyResult::Sat);check(k);
  }
  {
    Kernel k=fixture(false,true);ScopedHeuristics owner(k);
    assert(affine_model_apply(k)==ApplyResult::NoOp);
    assert(k.stats.affine_model_equations==0&&k.trail.empty());
  }
  {
    Kernel k=fixture();ScopedHeuristics owner(k);
    kernel_reserve_vars(k,7);
    kernel_add_binary_clause(k,make_lit(5,false),make_lit(6,false));
    assert(affine_model_apply(k)==ApplyResult::NoOp);
    const auto &hint=k.local_search_hints[
        static_cast<size_t>(strategy::LocalSearchKind::Affine)];
    assert(hint.available());
    assert(hint.external_phases[k.internal_to_external[5]]==0);
    assert(hint.external_phases[k.internal_to_external[6]]==0);
  }
  // A wide boundary declines exponential syndrome search but still offers one
  // bounded Gaussian completion.  Installing it remains the separate public
  // LocalSearchGuidance action; the model rule itself changes no assignment.
  {
    Kernel k=fixture();ScopedHeuristics owner(k);
    auto parameters=k.affine_model_parameters;
    parameters.maximum_projection=1;
    install_rule_parameters(k,parameters);
    assert(affine_model_apply(k)==ApplyResult::NoOp);
    assert(k.trail.empty()&&k.stats.affine_model_projection>1);
    const auto &hint=k.local_search_hints[
        static_cast<size_t>(strategy::LocalSearchKind::Affine)];
    assert(hint.available());
    unsigned parity=0;
    for(Var v=0;v<4;++v) {
      const int8_t phase=hint.external_phases[k.internal_to_external[v]];
      assert(phase);
      parity^=phase>0;
    }
    assert(parity==1);
    const auto &guidance=tactix_rules()[tactix_rule_id("LocalSearchGuidance")];
    assert(guidance.apply_instance(
        k,static_cast<uint64_t>(strategy::LocalSearchKind::Affine))==
        ApplyResult::Ok);
    for(Var v=0;v<4;++v)
      assert(k.h->target[v]==
             hint.external_phases[k.internal_to_external[v]]);
    assert(k.stats.local_search_hints_applied==1);
  }
  // DefaultPolicy consumes the same exact retained candidate through public
  // split Walk.  Complete-current-CNF admission must beat the ordinary all-
  // positive phases, use no stochastic flip budget, and publish both saved
  // and target phases without changing the formula or assignment trail.
  {
    Kernel k=fixture();ScopedHeuristics owner(k);
    auto parameters=k.affine_model_parameters;
    parameters.maximum_projection=1;
    install_rule_parameters(k,parameters);
    assert(affine_model_apply(k)==ApplyResult::NoOp);
    const auto &hint=k.local_search_hints[
        static_cast<size_t>(strategy::LocalSearchKind::Affine)];
    assert(hint.available());
    std::fill(k.h->saved.begin(),k.h->saved.end(),int8_t{1});
    std::fill(k.h->target.begin(),k.h->target.end(),int8_t{1});
    const auto arena=k.arena;
    const auto trail=k.trail;
    DefaultPolicy policy;
    assert(DefaultAffineModelTestAccess::consume(policy,k)==ApplyResult::Ok);
    assert(k.stats.walk_seed_imports==1&&k.stats.walk_seed_admitted==1&&
           k.stats.walk_seed_exported==1&&!k.stats.walk_seed_rejected);
    assert(k.stats.walk_preparation_steps>0&&
           !k.stats.walk_preparation_budget_exhausted&&
           !k.stats.walk_search_steps&&!k.stats.walk_flips);
    assert(k.arena==arena&&k.trail==trail&&k.h->saved==k.h->target);
    for(Var v=0;v<4;++v)
      assert(k.h->saved[v]==hint.external_phases[k.internal_to_external[v]]);
    kernel_check_invariants(k);
  }
  {
    Kernel k=fixture();ScopedHeuristics owner(k);volatile sig_atomic_t stop=1;k.stop=&stop;
    assert(affine_model_apply(k)==ApplyResult::NoOp&&k.trail.empty());
    assert(k.stats.affine_model_interrupted);stop=0;
    assert(affine_model_apply(k)==ApplyResult::Sat);check(k);
  }
  {
    Kernel k=fixture();ScopedHeuristics owner(k);k.work_budget_bounded=true;k.work_budget=0;
    assert(affine_model_apply(k)==ApplyResult::NoOp&&k.trail.empty());
    k.work_budget_bounded=false;assert(affine_model_apply(k)==ApplyResult::Sat);
  }
  {
    Kernel k;kernel_reserve_vars(k,1);kernel_assign_unit(k,make_lit(0,false));
    assert(!lucky_model_valid(k,std::vector<uint8_t>{0}));
    assert(lucky_model_valid(k,std::vector<uint8_t>{1}));
  }
  AffineModelParameters explicit_parameters;
  explicit_parameters.projection_mode=AffineProjectionMode::Explicit;
  explicit_parameters.projection={4,-1,3,-2};
  explicit_parameters.scopes={{4,4,0,4},{0,0,4,4},{2,2,2,4}};
  explicit_parameters.seed=UINT64_MAX;explicit_parameters.shuffle_first=true;
  auto p=program(explicit_parameters);
  const auto text=serialize_online_strategy_program(p);OnlineStrategyProgram replay;std::string error;
  assert(deserialize_online_strategy_program(text,replay,&error));
  assert(serialize_online_strategy_program(replay)==text);
  {
    Kernel k=fixture();ScopedHeuristics owner(k);OnlineStrategyExecutor executor;
    assert(executor.configure(replay,1));
    assert(executor.advance(k).status==InvocationStatus::Sat);check(k);
    assert(k.affine_model_parameters.projection.empty());
  }
  // Signed coordinates with zero support select ANY requested live assignment,
  // including non-parity variables. The oracle is the original fixture CNF.
  for(unsigned requested=0;requested<32;++requested) {
    Kernel k=fixture();ScopedHeuristics owner(k);
    AffineModelParameters exact;exact.projection_mode=AffineProjectionMode::Explicit;
    for(unsigned v=0;v<5;++v)exact.projection.push_back((requested&(1u<<v))?-int(v+1):int(v+1));
    exact.scopes={{0,0,0,0}};exact.permutations=1;
    std::vector<uint8_t> wanted(5);for(unsigned v=0;v<5;++v)wanted[v]=(requested>>v)&1;
    const bool legal=lucky_model_valid(k,wanted);
    install_rule_parameters(k,exact);
    assert((affine_model_apply(k)==ApplyResult::Sat)==legal);
    if(legal)for(unsigned v=0;v<5;++v)assert((k.value(make_lit(v,false))>0)==bool(wanted[v]));
    else assert(k.trail.empty());
  }
  auto invalid=explicit_parameters;invalid.projection={1,-1};assert(!invalid.valid());
  invalid=explicit_parameters;invalid.projection={0};assert(!invalid.valid());
  invalid=explicit_parameters;invalid.projection={INT32_MIN};assert(!invalid.valid());
  {
    Kernel k=fixture();ScopedHeuristics owner(k);
    auto unavailable=explicit_parameters;unavailable.projection={100};
    install_rule_parameters(k,unavailable);assert(affine_model_apply(k)==ApplyResult::NoOp);
    assert(k.stats.affine_model_unavailable==1&&k.trail.empty());
  }
  {
    DefaultStrategyConfig config;
    config.inprocess.structured_root_stages={{p,BudgetExhaustionPolicy::Stop}};
    assert(config.valid(&error));
  }
  {
    Kernel k=fixture();ScopedHeuristics owner(k);RandomOnlineConfig config;config.rules={"Lucky"};
    config.maximum_sequence_length=1;
    unsigned affine=0;
    for(uint64_t seed=1;seed<=256;++seed) {
      OnlineStrategyProgram random;
      assert(random_online_strategy_program(k,seed,config,random,&error));
      const auto serialized=serialize_online_strategy_program(random);OnlineStrategyProgram restored;
      assert(deserialize_online_strategy_program(serialized,restored,&error));
      assert(serialize_online_strategy_program(restored)==serialized);
      if(std::holds_alternative<AffineModelParameters>(random.sequence[0].parameters)) {
        ++affine;
      }
    }
    assert(affine);
  }
  std::cout<<"Affine reference model, "<<complete_work<<" exact budget boundaries, root negatives, and public scheduling PASS\n";
}

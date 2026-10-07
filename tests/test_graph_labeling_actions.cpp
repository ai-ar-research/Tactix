#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "rules/lucky/graph_labeling_model.hpp"
#include "rules/lucky/model.hpp"
#include "policies/heuristics.hpp"
#include "strategy/lucky_execution.hpp"
#include "strategy/random_online.hpp"
#include "strategy/work_execution.hpp"
#include <cassert>
#include <fstream>
using namespace tactix;
using namespace tactix::strategy;
static void permutation(Kernel &k, bool difficult) {
  constexpr uint32_t n = 4;
  kernel_reserve_vars(k, n * n);
  for (uint32_t column = 0; column < n; ++column) {
    std::vector<Lit> cover;
    for (uint32_t row = 0; row < n; ++row)
      cover.push_back(make_lit(row * n + column, false));
    kernel_add_large_clause(k, cover.data(), cover.size(), false, 0);
  }
  for (uint32_t row = 0; row < n; ++row)
    for (uint32_t a = 0; a < n; ++a)
      for (uint32_t b = a + 1; b < n; ++b)
        kernel_add_binary_clause(k, make_lit(row * n + a, true),
                                    make_lit(row * n + b, true));
  // Redundant same-column graph exclusions. The first fixture's separation
  // one is solved by a permutation; the second deliberately induces an overly
  // strong projected objective, which must remain guidance rather than UNSAT.
  for (uint32_t a = 0; a < n; ++a)
    for (uint32_t b = a + 1; b < n; ++b)
      kernel_add_binary_clause(k, make_lit(a * n + (difficult ? 2 : 0), true),
                                  make_lit(b * n + (difficult ? 2 : 0), true));
  k.graph_labeling_parameters.work.fill(100000);
  k.graph_labeling_parameters.workspace_bytes = 1 << 20;
  k.graph_labeling_parameters.repair_rounds = 3;
  k.graph_labeling_parameters.anneal_iterations = 20;
  k.graph_labeling_parameters.stall_anneal_iterations = 3;
}

static execution::GraphLabelingActionResult act(Kernel &k, bool auth) {
  return execution::execute_action(k, execution::GraphLabelingActionRequest{
      auth ? execution::GraphLabelingOperation::Authenticate
           : execution::GraphLabelingOperation::Continue, k.graph_labeling_parameters});
}
int main(int argc, char **argv) {
  static_assert(LUCKY_GRAPH_LABELING == 21 && LUCKY_NOL_HYPERGRAPH == 25 &&
                LUCKY_GRAPH_AUTHENTICATE == 26 && LUCKY_GRAPH_CONTINUE == 27);
  {
    Kernel k; ScopedHeuristics h(k); permutation(k, false);
    assert(act(k, false).slice.status == GraphLabelingSliceStatus::NoStructure);
    auto auth = act(k, true);
    assert(auth.authentication.status == GraphLabelingAuthenticationStatus::Authenticated);
    assert(auth.authentication.work && k.graph_labeling_session.authenticated());
    const auto recognition = k.stats.graph_labeling_work;
    assert(act(k, true).authentication.work == 0);
    assert(k.stats.graph_labeling_work == recognition);
    auto slice = act(k, false);
    assert(slice.slice.status == GraphLabelingSliceStatus::Pending);
    assert(k.trail.empty() && !k.inconsistent && slice.slice.hint_published);
    const auto fingerprint = k.graph_labeling_session.candidate_fingerprint();
    const auto work = k.stats.graph_labeling_work;
    k.level = 1;
    assert(act(k, true).authentication.status == GraphLabelingAuthenticationStatus::NoStructure);
    assert(act(k, false).slice.status == GraphLabelingSliceStatus::NoStructure);
    assert(k.stats.graph_labeling_work == work);
    k.level = 0;
    volatile sig_atomic_t stop = 1; k.stop = &stop;
    assert(act(k, false).slice.status == GraphLabelingSliceStatus::Interrupted);
    assert(k.graph_labeling_session.candidate_fingerprint() == fingerprint);
    assert(k.trail.empty() && !k.inconsistent);
    k.stop = nullptr;
    Kernel copy = k;
    assert(!copy.graph_labeling_session.authenticated());
    assert(graph_labeling_model_slice(copy, k.graph_labeling_session).status == GraphLabelingSliceStatus::NoStructure);
    assert(graph_labeling_model_authenticate(copy, k.graph_labeling_session).status == GraphLabelingAuthenticationStatus::NoStructure);
    assert(k.graph_labeling_session.candidate_fingerprint() == fingerprint);
    ++k.graph_labeling_parameters.seed;
    assert(act(k, false).slice.status == GraphLabelingSliceStatus::NoStructure);
    assert(act(k, true).authentication.work == recognition);
    assert(!k.graph_labeling_session.candidate_fingerprint());
    k.graph_labeling_session.clear();
    assert(!k.graph_labeling_session.authenticated());
    assert(act(k, true).authentication.work == recognition);
    for (unsigned i = 0; i < 8 && slice.result != ApplyResult::Sat; ++i) slice = act(k, false);
    assert(slice.result == ApplyResult::Sat);
    std::vector<uint8_t> model(k.nvars);
    for (Var v=0; v<k.nvars; ++v) model[v] = k.value(make_lit(v, false)) > 0;
    assert(lucky_model_valid(k, model));
  }
  {
    Kernel native, replay; ScopedHeuristics nh(native), rh(replay);
    permutation(native, false); permutation(replay, false);
    OnlineStrategyProgram program;
    for (uint64_t i=0; i<8; ++i) {
      OnlineInvocation step; step.id=i+1; step.rule="Lucky";
      step.granularity=InvocationGranularity::OneInstance;
      step.instances={i ? LUCKY_GRAPH_CONTINUE : LUCKY_GRAPH_AUTHENTICATE};
      step.parameters=native.graph_labeling_parameters;
      program.sequence.push_back(step);
    }
    const auto wire = serialize_online_strategy_program(program);
    OnlineStrategyProgram decoded; std::string error;
    assert(deserialize_online_strategy_program(wire, decoded, &error));
    assert(serialize_online_strategy_program(decoded) == wire);
    OnlineStrategyExecutor executor;
    assert(executor.configure(decoded, 1, ReconfigureMode::ResetRuntime, &error));
    bool sat=false;
    for (uint64_t i=0; i<8 && !sat; ++i) {
      const auto direct=act(native, !i);
      const auto public_result=executor.advance(replay);
      assert(native.graph_labeling_session.candidate_fingerprint() == replay.graph_labeling_session.candidate_fingerprint());
      assert(native.stats.graph_labeling_work == replay.stats.graph_labeling_work);
      sat=direct.result==ApplyResult::Sat;
      assert(sat == (public_result.status==InvocationStatus::Sat));
    }
    assert(sat && replay.stats.graph_labeling_recognized == 1);
  }
  {
    // Public 26/27 replay carries the same bounded Window continuation as the
    // native action. Repeated slices preserve cursor/random/candidate identity.
    Kernel native, replay;
    ScopedHeuristics nh(native), rh(replay);
    permutation(native, true);
    permutation(replay, true);
    native.graph_labeling_parameters.work[size_t(
        GraphLabelingStage::Construction)] = 64;
    replay.graph_labeling_parameters = native.graph_labeling_parameters;
    OnlineStrategyProgram program;
    for (uint64_t i = 0; i < 512; ++i) {
      OnlineInvocation step;
      step.id = i + 1;
      step.rule = "Lucky";
      step.granularity = InvocationGranularity::OneInstance;
      step.instances = {i ? LUCKY_GRAPH_CONTINUE
                          : LUCKY_GRAPH_AUTHENTICATE};
      step.parameters = native.graph_labeling_parameters;
      if (i)
        step.rule_work_budget = {RuleWorkBudgetMode::Bounded, 64};
      program.sequence.push_back(step);
    }
    const auto wire = serialize_online_strategy_program(program);
    OnlineStrategyProgram decoded;
    std::string error;
    assert(deserialize_online_strategy_program(wire, decoded, &error));
    assert(serialize_online_strategy_program(decoded) == wire);
    OnlineStrategyExecutor executor;
    assert(executor.configure(decoded, 1, ReconfigureMode::ResetRuntime,
                              &error));
    uint64_t window_slices = 0;
    bool reached_window = false;
    for (uint64_t i = 0; i < 512; ++i) {
      execution::GraphLabelingActionResult direct;
      if (!i) {
        direct = act(native, true);
      } else {
        strategy::ScopedRuleWorkBudget budget(
            native, {RuleWorkBudgetMode::Bounded, 64});
        reached_window |=
            native.graph_labeling_session.recommended_continuation_work() != 0;
        window_slices +=
            native.graph_labeling_session.recommended_continuation_work() != 0;
        direct = act(native, false);
      }
      const auto public_result = executor.advance(replay);
      assert(native.graph_labeling_session.candidate_fingerprint() ==
             replay.graph_labeling_session.candidate_fingerprint());
      assert(native.stats.graph_labeling_work == replay.stats.graph_labeling_work);
      assert(direct.result == public_result.rule_result);
      if (reached_window &&
          !native.graph_labeling_session.recommended_continuation_work())
        break;
    }
    assert(reached_window && window_slices > 1);
    assert(!native.graph_labeling_session.recommended_continuation_work());
    assert(!replay.graph_labeling_session.recommended_continuation_work());
    assert(native.trail.empty() && replay.trail.empty());
  }
  {
    auto *solver=tactix_init();
    std::vector<std::vector<int>> clauses;
    const auto add=[&](std::vector<int> c) {
      for (int lit:c) tactix_add(solver, lit);
      tactix_add(solver, 0); clauses.push_back(std::move(c));
    };
    for (int col=0; col<4; ++col) add({col+1,col+5,col+9,col+13});
    for (int row=0; row<4; ++row)
      for (int a=0; a<4; ++a) for (int b=a+1; b<4; ++b)
        add({-(4*row+a+1),-(4*row+b+1)});
    for (int a=0; a<4; ++a) for (int b=a+1; b<4; ++b) add({-(4*a+1),-(4*b+1)});
    uint64_t ids[]={LUCKY_GRAPH_AUTHENTICATE,LUCKY_GRAPH_CONTINUE};
    tactix_online_step steps[8]{};
    for (unsigned i=0; i<8; ++i) {
      auto &s=steps[i]; s.step_id=i+1; s.rule_id=tactix_rule_id("Lucky");
      s.granularity=TACTIX_ONLINE_GRANULARITY_ONE;
      s.instances=&ids[i ? 1 : 0]; s.instance_count=1;
      s.repeat_mode=TACTIX_ONLINE_REPEAT_ONCE; s.repeat_limit=1;
      s.work_unit=TACTIX_ONLINE_WORK_APPLICATIONS; s.budget=1;
      s.parameters.kind=TACTIX_ONLINE_PARAMETERS_GRAPH_LABELING;
      auto &p=s.parameters.value.graph_labeling;
      p.maximum_vertices=16; p.workspace_bytes=1<<20;
      for(auto &dose:p.work) dose=100000;
      p.maximum_dfs_vertices=140; p.maximum_dfs_nodes=50000000;
      p.maximum_dfs_starts=12; p.early_dfs_choices=28;
      p.repair_rounds=3; p.anneal_iterations=20; p.stall_anneal_iterations=3;
    }
    tactix_online_program program{steps,8,TACTIX_ONLINE_REPEAT_ONCE,1};
    assert(tactix_online_configure(solver,&program,0,TACTIX_ONLINE_RECONFIGURE_RESET)==0);
    tactix_online_result result{};
    bool sat=false;
    for(unsigned i=0; i<8 && !sat; ++i) {
      assert(tactix_online_advance(solver,&result)==0);
      assert(result.instance==ids[i ? 1 : 0]);
      sat=result.status==TACTIX_ONLINE_SAT;
    }
    assert(sat);
    for(const auto &clause:clauses) {
      bool satisfied=false;
      for(int lit:clause) satisfied |= tactix_value(solver,lit)==lit;
      assert(satisfied);
    }
    if(argc==3) {
      std::ofstream cnf(argv[1]), model(argv[2]);
      cnf<<"p cnf 16 "<<clauses.size()<<"\n";
      for(const auto &clause:clauses) { for(int lit:clause) cnf<<lit<<' '; cnf<<"0\n"; }
      model<<"s SATISFIABLE\nv ";
      for(int v=1;v<=16;++v) model<<(tactix_value(solver,v)>0 ? v : -v)<<' ';
      model<<"0\n";
    }
    tactix_release(solver);
  }
}

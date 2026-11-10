#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "policies/strategy_settings.hpp"
#include "query_test_support.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <locale>
#include <cassert>
#include <cstring>
#include <iostream>
using namespace tactix;
struct Comma : std::numpunct<char> {
  char do_decimal_point() const override {return ',';}
  char do_thousands_sep() const override {return '.';}
  std::string do_grouping() const override {return "\3";}
};
static void scheduler_refresh() {
  DefaultStrategyConfig config;
  Kernel kernel;
  DefaultInprocessScheduler scheduler(&config.inprocess);
  scheduler.build(kernel);
  const auto entry = [&]() -> DefaultInprocessScheduler::SlotEntry & {
    const auto it = std::find_if(scheduler.entries.begin(), scheduler.entries.end(),
        [](const auto &e) { return e.rule == tactix_rule_id("BVE"); });
    assert(it != scheduler.entries.end());return *it;
  };
  const auto step_id = entry().step_id;
  entry().cursor=17;entry().doses=3;entry().tm.spent=29;
  entry().delay_count=5;entry().binaries_mark=31;
  scheduler.probe_clock.rounds=7;scheduler.probe_clock.anchor=41;
  for (int round=0;round<3;++round) {
    config.inprocess.bve_enabled=round!=1;
    auto step=std::find_if(config.inprocess.program.begin(),config.inprocess.program.end(),
        [=](const auto &s) {return s.id==step_id;});
    assert(step!=config.inprocess.program.end());
    step->effort_permille=7+round;step->max_applications=11+round;
    step->cursor_enabled=round!=1;
    step->candidate_order=round==1?CandidateOrder::Native:CandidateOrder::Heuristic;
    std::reverse(config.inprocess.program.begin(),config.inprocess.program.end());
    scheduler.build(kernel);
    const auto &e=entry();
    assert(e.enabled==(round!=1)&&e.permille==unsigned(7+round));
    assert(e.max_applies==unsigned(11+round)&&e.cursor_enabled==(round!=1));
    assert(e.order==(round==1?CandidateOrder::Native:CandidateOrder::Heuristic));
    assert(e.cursor==17&&e.doses==3&&e.tm.spent==29&&e.delay_count==5&&e.binaries_mark==31);
    assert(scheduler.probe_clock.rounds==7&&scheduler.probe_clock.anchor==41);
  }
  // Reusing an occurrence ID for another rule must not inherit its cursor.
  auto step=std::find_if(config.inprocess.program.begin(),config.inprocess.program.end(),
      [=](const auto &s) {return s.id==step_id;});
  step->rule="Probe";scheduler.build(kernel);
  for(const auto &e:scheduler.entries)
    if(e.step_id==step_id)assert(e.rule==tactix_rule_id("Probe")&&!e.cursor&&!e.doses);
}
static void query_history_definition() {
  using namespace session;
  QueryHistory history;query_test::clause(history,{1,2});query_test::clause(history,{-1,2});
  QueryKernel kernel(NativeQueryLimits{{256,4096,1024},{64,4096,1024,8192,64,1024,8192},1000000});
  DefaultPolicy persistent;StrategySettings options;
  for(int round=0;round<3;++round) {
    if(round==0) {
      assert(!api_detail::set_strategy_option(options,"TACTIX_CHRONO_LEVELS","123"));
      assert(!api_detail::set_strategy_option(options,"TACTIX_RANDEC","0"));
    } else {
      assert(!api_detail::set_strategy_option(options,"TACTIX_CHRONO_LEVELS",nullptr));
      assert(!api_detail::set_strategy_option(options,"TACTIX_RANDEC",nullptr));
    }
    assert(!api_detail::set_strategy_option(options,"TACTIX_NO_BVE",round==1?"1":nullptr));
    assert(!api_detail::set_strategy_option(options,"TACTIX_SLOT_PERMILLE",round==1?"BVE:7":"BVE:9"));
    assert(history.begin_query()==InputStatus::Ok&&kernel.replay(history)==ReplayStatus::Ok);
    auto *view=static_cast<tactix_c_handle *>(api_detail::create_query_execution(kernel,persistent,options,nullptr));assert(view);
    const char *borrowed=tactix_default_definition(view);assert(borrowed);
    const std::string expected=borrowed;
    ScopedStrategySettings settings(&options);
    const auto result=persistent.solve_query(kernel);
    assert(result.result==10&&persistent.strategy_config_snapshot()==expected);
    bool found=false;
    for(const auto &entry:persistent.inprocess().entries)
      if(entry.rule==tactix_rule_id("BVE")) {
        found=true;assert(entry.enabled==(round!=1)&&entry.permille==(round==1?7u:9u));
      }
    assert(found);
    assert(persistent.chrono_levels==123&&!persistent.strategy_config().branching.random_sequences);
    assert(!std::strcmp(borrowed,expected.c_str()));
    api_detail::destroy_query_execution(view);
    assert(kernel.check_model(kernel.ticket()).status==ModelStatus::Valid);
    assert(history.finish_query(history.active_query()->id,QueryResult::Sat)==InputStatus::Ok);
  }
}
int main(int argc,char **argv) {
  const bool query=argc==2&&!std::strcmp(argv[1],"--query");assert(argc==1||query);
  assert(!tactix_default_definition(nullptr));
  scheduler_refresh();
  query_history_definition();
  for(int variant=0;variant<3;++variant) {
    auto *s=tactix_init_configured(TACTIX_INIT_DEFAULT_FRONTEND);assert(s);
    StrategySettings options;
    const auto option=[&](const char *name,const char *value) {
      assert(!tactix_set_default_option(s,name,value));
      assert(!api_detail::set_strategy_option(options,name,value));
    };
    if(variant) {
      option("TACTIX_FORCE_MODE","stable");
      option("TACTIX_BUMP_REASONS_RATE","0.12345678901234566");
      option("TACTIX_SLOT_PERMILLE","BVE:7");
      option("TACTIX_FACTOR_INIT_TICKS","777");
    }
    if(variant==2)option("TACTIX_FORCE_MODE","focused");
    auto &k=api_detail::kernel_for_fixed_testing(s);
    const auto epoch=k.strategy_epoch;
    const std::string actual=tactix_default_definition(s);
    assert(actual==tactix_default_definition(s));
    const auto locale=std::locale();std::locale::global(std::locale(locale,new Comma));
    assert(actual==tactix_default_definition(s));std::locale::global(locale);
    const char *borrowed=tactix_default_definition(s);
    for(const auto &[name,value]:{std::pair{"TACTIX_ORDER_BVE","invalid"},std::pair{"TACTIX_SLOT_PERMILLE","BVE:1001"},std::pair{"TACTIX_CURSOR_VIVIFY","1"},std::pair{"TACTIX_ORDER_FACTOR","native"}}) {
      assert(!tactix_set_default_option(s,name,value));
      assert(!tactix_default_definition(s));
      assert(!std::strcmp(borrowed,actual.c_str()));
      assert(!tactix_set_default_option(s,name,nullptr));
    }
    if(variant)assert(!tactix_set_default_option(s,"TACTIX_SLOT_PERMILLE","BVE:7"));

    assert(!tactix_stat(s,TACTIX_STAT_INPUT_CLOSED)&&!k.nvars&&!k.h&&k.cold_state_deferred);
    assert(k.strategy_epoch==epoch);
    for(int lit:{1,2,0,-1,2,0})assert(!tactix_add_checked(s,lit));
    const auto added=k.strategy_epoch;
    assert(actual==tactix_default_definition(s)&&k.strategy_epoch==added);
    assert(!tactix_stat(s,TACTIX_STAT_INPUT_CLOSED));
    ScopedStrategySettings scope(&options);
    DefaultPolicy native;
    const std::string ordinary=actual;
    if(query)std::cout<<native.configured_strategy_config_snapshot(k,true)<<'\n';
    // The real solve-entry path, not only the new configuration reader.
    assert(tactix_status(s)==0);
    assert(native.solve(k)==10);
    assert(native.strategy_config_snapshot()==ordinary);
    if(!query)std::cout<<ordinary<<'\n';
    tactix_release(s);
  }
}

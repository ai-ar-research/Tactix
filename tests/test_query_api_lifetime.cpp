#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "api/api_internal.hpp"
#include "policies/default_policy.hpp"
#include "query_test_support.hpp"

#include <cassert>
#include <string>
#include <unordered_set>
#include <vector>

using namespace tactix;
using namespace tactix::session;

int main() {
  QueryHistory history;
  query_test::clause(history, {1, 2});
  query_test::clause(history, {-1, 3});
  query_test::clause(history, {-2, 4});
  assert(history.begin_query() == InputStatus::Ok);
  QueryKernel query;
  assert(query.replay(history) == ReplayStatus::Ok);
  volatile sig_atomic_t previous_stop = 0, session_stop = 0, custom_stop = 0;
  uint64_t previous_ticks = 0;
  const std::unordered_set<uint32_t> previous_retention{0};
  DefaultPolicy policy;
  std::vector<std::pair<std::string, std::string>> options;
  {
    Kernel &kernel = *query.kernel(query.ticket());
    auto *view = static_cast<tactix_c_handle *>(
        api_detail::create_query_execution(query, policy, options, &session_stop));
    assert(view && api_detail::query_execution_settled(view));
    assert(!tactix_support(view, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,
                          TACTIX_PHASE_ELIMINATE));
    assert(!tactix_support(view, TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0));
    assert(!tactix_bve_schedule_open(view));
    assert(kernel.bve_score_update_hook && kernel.bve_occurrence_count_image);
    api_detail::destroy_query_execution(view);
    assert(!kernel.bve_score_update_hook && !kernel.bve_score_update_state);
    assert(!kernel.bve_occurrence_count_image && !kernel.dense);
  }
  // Session::unknown discards an abandoned Eliminate epoch before replay.
  query.discard_epoch();
  assert(query.replay(history) == ReplayStatus::Ok);
  Kernel &kernel = *query.kernel(query.ticket());
  kernel.stop = &previous_stop;
  kernel.tick_sink = &previous_ticks;
  kernel.substitute_retention = &previous_retention;
  auto *view = static_cast<tactix_c_handle *>(
      api_detail::create_query_execution(query, policy, options, &session_stop));
  assert(view && api_detail::query_execution_settled(view));
  assert(kernel.stop == &session_stop);
  assert(!tactix_support(view, TACTIX_SUPPORT_ENTER_INPROCESS_PHASE,
                        TACTIX_PHASE_ELIMINATE));
  assert(!tactix_support(view, TACTIX_SUPPORT_OPEN_OCCURRENCE_SESSION, 0));
  assert(!tactix_bve_schedule_open(view));
  assert(!tactix_support(view, TACTIX_SUPPORT_LEAVE_INPROCESS_PHASE, 0));
  assert(tactix_bve_schedule_next(view) == TACTIX_INSTANCE_CANONICAL);
  assert(api_detail::query_execution_settled(view));
  assert(api_detail::strategy_configuration(view).inprocess.bce_parameters.preserve_query);
  assert(!tactix_set_interrupt(view, &custom_stop));
  assert(kernel.stop == &custom_stop);
  assert(!tactix_install_tick_attribution(view, TACTIX_STAT_PROBE_TICKS));
  assert(!tactix_support(view, TACTIX_SUPPORT_STARTUP_RETENTION, 1));
  assert(!api_detail::query_execution_settled(view));
  api_detail::destroy_query_execution(view);
  assert(kernel.substitute_retention == &previous_retention);
  assert(kernel.stop == &previous_stop && kernel.tick_sink == &previous_ticks);
  const auto probe_ticks = kernel.stats.probe_ticks;
  const auto retained_ticks = previous_ticks;
  kernel_charge_ticks(kernel, 7);
  assert(previous_ticks == retained_ticks + 7 && kernel.stats.probe_ticks == probe_ticks);
  assert(query_test::drive(query, history, false) == 10);
  assert(query.check_model(query.ticket()).status == ModelStatus::Valid);
}

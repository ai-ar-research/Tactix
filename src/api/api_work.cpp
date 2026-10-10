// Work settings and tick attribution (tactix.h). A translation unit of its own
// so api.cpp's code generation is unchanged by these entries.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix

#include "api/api_internal.hpp"
#include "kernel/kernel.hpp"
#include "strategy/work_execution.hpp"

using namespace tactix;

int tactix_install_work_settings(tactix_c_handle *s, const tactix_work_settings *in) {
  constexpr uint32_t all = TACTIX_WORK_BUDGET | TACTIX_WORK_OFFSET |
                           TACTIX_WORK_BOUNDED | TACTIX_WORK_MIDROUND;
  if (!s || !in || (in->fields & ~all)) return -1;
  execution::WorkSettingsRequest request;
  if (in->fields & TACTIX_WORK_BUDGET) request.budget = in->budget;
  if (in->fields & TACTIX_WORK_OFFSET) request.offset = in->offset;
  if (in->fields & TACTIX_WORK_BOUNDED) request.bounded = in->bounded != 0;
  if (in->fields & TACTIX_WORK_MIDROUND) request.midround = in->midround != 0;
  execution::execute_action(api_detail::kernel_for_fixed_testing(s), request);
  return 0;
}

int tactix_read_work_settings(tactix_c_handle *s, tactix_work_settings *out) {
  if (!s || !out) return -1;
  const Kernel &k = api_detail::kernel_for_fixed_testing(s);
  *out = {TACTIX_WORK_BUDGET | TACTIX_WORK_OFFSET | TACTIX_WORK_BOUNDED | TACTIX_WORK_MIDROUND,
          k.work_budget, k.work_budget_offset, k.work_budget_bounded, k.midround_budget};
  return 0;
}

int tactix_install_tick_attribution(tactix_c_handle *s, int stat) {
  if (!s) return -1;
  uint64_t Stats::*counter = nullptr;
  switch (stat) {
  case TACTIX_TICK_ATTRIBUTION_NONE: break;
  case TACTIX_STAT_PROBE_TICKS: counter = &Stats::probe_ticks; break;
  case TACTIX_STAT_HBR_TICKS: counter = &Stats::hbr_ticks; break;
  case TACTIX_STAT_BACKBONE_TICKS: counter = &Stats::backbone_ticks; break;
  case TACTIX_STAT_VIVIFY_TICKS: counter = &Stats::vivify_ticks; break;
  case TACTIX_STAT_INSTANTIATE_TICKS: counter = &Stats::instantiate_ticks; break;
  default: return -1;
  }
  Kernel &k = api_detail::kernel_for_fixed_testing(s);
  k.tick_sink = counter ? &(k.stats.*counter) : nullptr;
  return 0;
}

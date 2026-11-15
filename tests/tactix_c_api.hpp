#pragma once
// The public C API for C++ tests. Its handle type `tactix` collides with
// namespace tactix, so C++ code names the handle tactix_c_handle.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "public_online_step.h"

// Select one instance with a budget of one application; keep other fields.
inline void one_application_step(tactix_online_step &step) {
  step.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget = 1;
}

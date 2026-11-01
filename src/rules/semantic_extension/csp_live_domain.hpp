#pragma once

#include "kernel/kernel.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace tactix::csp_live_domain {

// Canonicalize an unassigned, non-retired internal clause as an external CSP
// domain. Exactly-one authentication and work charging remain with the
// calling discovery engine.
inline bool canonicalize(const Kernel &kernel, const Lit *literals,
                         uint32_t size, uint32_t maximum_size,
                         std::vector<int32_t> &output) {
  output.clear();
  if (size < 2 || size > maximum_size) return false;
  output.reserve(size);
  for (uint32_t index = 0; index < size; ++index) {
    const Var variable = var_of(literals[index]);
    if (kernel_var_retired(kernel, variable) || kernel.value(literals[index]))
      return false;
    output.push_back(kernel_externalize(kernel, literals[index]));
  }
  std::sort(output.begin(), output.end(), external_literal_less);
  for (size_t index = 1; index < output.size(); ++index)
    if (external_variable_of(output[index - 1]) ==
        external_variable_of(output[index]))
      return false;
  return true;
}

} // namespace tactix::csp_live_domain

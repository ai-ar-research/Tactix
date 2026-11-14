#pragma once
// A bare kernel with variable storage, before proof or heuristic attachment.
#include "kernel/kernel.hpp"

inline tactix::Kernel kernel_with_vars(uint32_t variables) {
  tactix::Kernel kernel;
  tactix::kernel_reserve_vars(kernel, variables);
  return kernel;
}

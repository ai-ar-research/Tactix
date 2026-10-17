#pragma once

// Internal fixed-test access; this is not part of the public C interface.
#include <cstdio>

namespace tactix {
struct Kernel;
namespace strategy {
struct OnlineStrategyProgram;
}
namespace api_detail {
Kernel &kernel_for_fixed_testing(void *opaque);
// The configured online program as converted from C, or null.
const strategy::OnlineStrategyProgram *online_program_for_fixed_testing(void *opaque);
// Parse a DIMACS stream into a fresh session exactly as the CLI does and close
// its input. Returns null on success, or the parser's error string.
const char *load_dimacs_for_fixed_testing(void *opaque, FILE *in);
}
}

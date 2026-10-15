#pragma once

// Internal fixed-test access; this is not part of the public C interface.
namespace tactix {
struct Kernel;
namespace strategy {
struct OnlineStrategyProgram;
}
namespace api_detail {
Kernel &kernel_for_fixed_testing(void *opaque);
// The configured online program as converted from C, or null.
const strategy::OnlineStrategyProgram *online_program_for_fixed_testing(void *opaque);
}
}

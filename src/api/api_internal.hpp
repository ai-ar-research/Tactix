#pragma once

// Internal fixed-test access; this is not part of the public C interface.
namespace tactix {
struct Kernel;
namespace api_detail {
Kernel &kernel_for_fixed_testing(void *opaque);
}
}

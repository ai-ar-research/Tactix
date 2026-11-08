#undef NDEBUG
// Keep the default replay entry usable without including the opaque writer's
// definition. A default unique_ptr<Proof> argument instantiated its destructor
// in every caller and made the otherwise self-contained header fail to compile.
#include "session/query_kernel.hpp"

#include <cassert>

int main() {
  tactix::session::QueryHistory history;
  tactix::session::QueryKernel kernel;
  assert(kernel.replay(history) == tactix::session::ReplayStatus::NoActiveQuery);
}

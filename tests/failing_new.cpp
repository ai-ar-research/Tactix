// Test-only process allocator: force the optional proof exporter to see the
// same std::bad_alloc class as an address-space limit, without a huge formula.
#include <atomic>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> fail_allocations{false};
}

extern "C" void tactix_test_fail_allocations(bool fail) {
  fail_allocations.store(fail, std::memory_order_relaxed);
}

void *operator new(std::size_t size) {
  if (fail_allocations.load(std::memory_order_relaxed))
    throw std::bad_alloc();
  if (void *memory = std::malloc(size ? size : 1))
    return memory;
  throw std::bad_alloc();
}

void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept {
  std::free(memory);
}

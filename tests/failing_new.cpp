// Test-only process allocator: force the optional proof exporter to see the
// same std::bad_alloc class as an address-space limit, without a huge formula.
#include <atomic>
#include <cstdlib>
#include <new>
#include <cstdint>

namespace {
std::atomic<bool> fail_allocations{false};
std::atomic<size_t> allocation_budget{SIZE_MAX};
}

extern "C" void tactix_test_fail_allocations(bool fail) {
  if (!fail) allocation_budget.store(SIZE_MAX, std::memory_order_relaxed);
  fail_allocations.store(fail, std::memory_order_relaxed);
}

extern "C" void tactix_test_fail_allocation_after(size_t count) {
  allocation_budget.store(count, std::memory_order_relaxed);
}

void *operator new(std::size_t size) {
  if (fail_allocations.load(std::memory_order_relaxed))
    throw std::bad_alloc();
  auto remaining = allocation_budget.load(std::memory_order_relaxed);
  while (remaining != SIZE_MAX) {
    if (!remaining) throw std::bad_alloc();
    if (allocation_budget.compare_exchange_weak(remaining, remaining - 1,
                                                std::memory_order_relaxed)) break;
  }
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

#include "population/population_observer.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"

namespace tactix {

PopulationObserver *kernel_bind_population_observer(
    Kernel &kernel, PopulationObserver *observer) noexcept {
  PopulationObserver *const previous = kernel.population_observer;
  if (previous == observer) {
    kernel_sync_population_observer(kernel);
    return previous;
  }
  if (kernel.proof && kernel.proof->population_observer() == previous)
    kernel.proof->bind_population_observer(nullptr);
  kernel.population_observer = observer;
  if (kernel.proof)
    kernel.proof->bind_population_observer(observer);
  return previous;
}

void kernel_sync_population_observer(Kernel &kernel) noexcept {
  if (kernel.proof &&
      kernel.proof->population_observer() != kernel.population_observer)
    kernel.proof->bind_population_observer(kernel.population_observer);
}

ScopedPopulationObserverBinding::ScopedPopulationObserverBinding(
    Kernel &kernel, PopulationObserver &observer) noexcept
    : kernel_(&kernel),
      previous_(kernel_bind_population_observer(kernel, &observer)) {}

ScopedPopulationObserverBinding::~ScopedPopulationObserverBinding() {
  if (!kernel_)
    return;
  // Proof::~Proof also flushes, but Automatic's route decision is semantic:
  // removing an observer must never select DSR. Explicit routes can settle
  // their pending transaction before the non-owning observer disappears.
  if (kernel_->proof)
    kernel_->proof->settle_population_observer();
  kernel_bind_population_observer(*kernel_, previous_);
}

} // namespace tactix

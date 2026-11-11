#include "population/population_observer.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"

namespace tactix {

uint64_t kernel_prepare_weakening(Kernel &k, WeakeningFamily family,
    PopulationClauseView clause, PopulationClauseView witness,
    uint64_t source_id, bool binary, bool all_binary_copies,
    uint64_t reconstruction_words) noexcept {
  if (!kernel_records_weakening(k)) return 0;
  // Reserve zero for the inactive path. A wrapped token cannot name staging.
  if (k.next_population_transaction == UINT64_MAX) {
    WeakeningPopulationEvent event;
    k.population_observer->observe(event);
    return 0;
  }
  WeakeningPopulationEvent event;
  event.family = family;
  event.transaction_id = ++k.next_population_transaction;
  event.source_id = source_id;
  event.reconstruction_offset = k.reconstruction.size();
  event.reconstruction_words = reconstruction_words ? reconstruction_words : uint64_t(clause.size()) + 1;
  event.binary = binary;
  event.all_binary_copies = all_binary_copies;
  event.clause = clause;
  event.witness = witness;
  k.population_observer->observe(event);
  return event.transaction_id;
}

void kernel_finish_weakening(Kernel &k, uint64_t token, bool committed) noexcept {
  if (!token || !k.population_observer) return;
  WeakeningPopulationEvent event;
  event.kind = committed ? WeakeningEventKind::Commit : WeakeningEventKind::Cancel;
  event.transaction_id = token;
  k.population_observer->observe(event);
}

void kernel_record_reconstruction(Kernel &k, WeakeningFamily family,
                                   uint64_t begin) noexcept {
  if (!kernel_records_weakening(k)) return;
  WeakeningPopulationEvent event;
  event.kind = WeakeningEventKind::Reconstruction;
  event.family = family;
  if (k.next_population_transaction != UINT64_MAX)
    event.transaction_id = ++k.next_population_transaction;
  event.reconstruction_offset = begin;
  event.reconstruction_words = k.reconstruction.size() - begin;
  k.population_observer->observe(event);
}

PopulationObserver *kernel_bind_population_observer(
    Kernel &kernel, PopulationObserver *observer) noexcept {
  PopulationObserver *const previous = kernel.population_observer;
  if (previous == observer) {
    kernel_sync_population_observer(kernel);
    return previous;
  }
  if (kernel.population_binding_epoch != UINT64_MAX)
    ++kernel.population_binding_epoch;
  if (kernel.proof && kernel.proof->population_observer() == previous)
    kernel.proof->bind_population_observer(nullptr);
  for (auto *at = previous; at; at = at->forwarded_observer())
    if (!population_observer_contains(observer, at)) at->observe_binding(kernel, false);
  kernel.population_observer = observer;
  for (auto *at = observer; at; at = at->forwarded_observer())
    if (!population_observer_contains(previous, at)) at->observe_binding(kernel, true);
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
  // Proof::~Proof also flushes. Settle any pending explicit-route transaction
  // before the non-owning observer disappears.
  if (kernel_->proof)
    kernel_->proof->settle_population_observer();
  kernel_bind_population_observer(*kernel_, previous_);
}

} // namespace tactix

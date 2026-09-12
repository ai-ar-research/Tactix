#undef NDEBUG
#include "kernel/occ.hpp"
#include <cstdio>
#include <vector>
using namespace tactix;

static uint64_t digest = UINT64_C(1469598103934665603);
static unsigned boundaries = 0;
static void mix(uint64_t value) { digest = (digest ^ value) * UINT64_C(1099511628211); }
static void inspect(const FlatLists<Lit> &store) {
  ++boundaries;
  mix(store.lits()); mix(store.arena_capacity_bytes()); mix(store.heads_capacity_bytes());
  mix(store.relocations); mix(store.defrags);
  const auto *heads = static_cast<const FlatLists<Lit>::Head *>(store.heads_data());
  for (Lit lit = 0; lit < store.lits(); ++lit) {
    mix(heads[lit].offset); mix(heads[lit].size); mix(heads[lit].capacity);
    for (const auto entry : store[lit]) mix(entry);
  }
}

int main() {
  FlatLists<Lit> store;
  store.resize_lits(17); inspect(store);
  for (unsigned i = 0; i < 300000; ++i) {
    store.push((i * 11) % 17, i % 997);
    if (i % 4096 == 0) inspect(store);
  }
  inspect(store);
  for (Lit lit = 0; lit < 17; ++lit) store.shrink(lit,lit % 3);
  inspect(store);
  store.defrag_if_needed(); assert(store.defrags); inspect(store);
  for (unsigned round = 0; round < 32; ++round) {
    store.begin_exact_layout(17);
    for (Lit lit = 0; lit < 17; ++lit) store.count(lit,(lit+round)%11);
    store.finish_exact_layout();
    for (Lit lit = 0; lit < 17; ++lit)
      for (unsigned i = 0; i < (lit+round)%11; ++i) store.push(lit,i%3);
    inspect(store);
    auto copied = store; inspect(copied);
    FlatLists<Lit> assigned;
    assigned = copied; inspect(assigned);
    auto moved = std::move(assigned); inspect(moved);
    for (Lit lit = 0; lit < 17; ++lit)
      if (store.size(lit)) store.erase_at(lit,store.size(lit)/2);
    inspect(store);
    for (Lit lit = 0; lit < 17; ++lit) store.clear(lit);
    inspect(store);
  }
  store.release(); inspect(store);
  store.begin_exact_layout(3); store.count(2,2); store.finish_exact_layout();
  store.push(2,1); store.push(2,1); inspect(store);
  std::printf("flat-payload boundaries=%u state=%016llx\n",boundaries,(unsigned long long)digest);
}

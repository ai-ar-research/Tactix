// Compare exact physical occurrence lifetimes using real parent/new objects.
#include "forward-fixture.hpp"
#include "rules/bve/bve.hpp"

#ifndef EXPECT_INCOMING
#define EXPECT_INCOMING 0
#endif
int main() {
  for (bool incoming : {true, false}) {
    Fixture f;
    f.seed();
    if (incoming) f.add({3, 8, 9});
    else { f.add({-3, 4, 5}); f.add({-4}); f.add({5, 8, 9}); }
    f.enter(true);
    OccIndex occ;
    kernel_dense_connect(f.k, occ, true);
    f.k.midround_budget = true;
    f.k.work_budget = 1;
    assert(strengthen_apply(f.k) == ApplyResult::Ok);
    f.k.midround_budget = false;
    f.k.work_budget = 0;
    assert(strengthen_reconnect_and_flush(f.k, f.entry) == ApplyResult::Ok);
    const uint32_t expected = incoming ? EXPECT_INCOMING : 1;
    for (int lit : {8, 9})
      assert(bve_schedule_occurrence_count(f.k, internalize(lit)) == expected);
    std::printf("incoming=%u physical8=%u physical9=%u\n", incoming,
        bve_schedule_occurrence_count(f.k, internalize(8)),
        bve_schedule_occurrence_count(f.k, internalize(9)));
    kernel_check_invariants(f.k);
    kernel_dense_disconnect(f.k);
    assert(f.finish(true) == 10);
  }
}

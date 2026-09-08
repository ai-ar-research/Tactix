// Fixed branch coverage linked to immutable5798, not a new solver build.
#include "forward-fixture.hpp"

static void stop_after_unit(Kernel &k, void *state, Var, bool) {
  if (k.value(internalize(3)) > 0)
    *static_cast<volatile sig_atomic_t *>(state) = 1;
}

static void interrupted_after_unit(bool dense) {
  Fixture f;
  f.seed(); f.add({4,5,6}); f.add({4,5});
  f.enter(dense);
  volatile sig_atomic_t stop = 0;
  KernelStopPublisher pub(f.k, &stop);
  f.k.touch_filter = true;
  f.k.bve_score_update_hook = stop_after_unit;
  f.k.bve_score_update_state = const_cast<sig_atomic_t *>(&stop);
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(stop && f.k.value(internalize(3)) > 0);
  assert(f.k.stats.strengthen_checked == 1 && f.k.stats.strengthened == 1);
  assert(f.k.stats.strengthen_subsumed == 0 && f.k.stats.strengthen_interrupted == 1);
  assert(!f.k.strengthen_complete && f.k.strengthen_touch_mark == 0);
  kernel_check_invariants(f.k);
  stop = 0;
  f.k.bve_score_update_hook = nullptr;
  f.k.bve_score_update_state = nullptr;
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.stats.strengthen_subsumed == 1 && f.k.strengthen_complete);
  assert(f.finish(dense) == 10);
}

static void satisfied_connected_partner(bool dense) {
  Fixture f;
  f.add({4,5,6}); // connects on6, then becomes satisfied through4
  f.seed(); f.add({-3,4}); f.add({6,7,8});
  f.enter(dense);
  assert(strengthen_apply(f.k) == ApplyResult::Ok);
  assert(f.k.value(internalize(3)) > 0 && f.k.value(internalize(4)) > 0);
  assert(f.k.stats.strengthen_checked == 3 && f.k.stats.strengthened == 1);
  unsigned live = 0;
  for (Ref ref = 0; ref < f.k.arena.size();) {
    const Clause *clause = clause_at(f.k.arena,ref);
    if (!clause->garbage()) { ++live; assert(clause->id() != 1); }
    ref += clause_actual_words(clause);
  }
  assert(live == 1 && f.k.strengthen_complete);
  kernel_check_invariants(f.k);
  assert(f.finish(dense) == 10);
}

static void conflict_case(bool dense, unsigned kind, ProofFormat format,
                          const std::string &stem) {
  Proof proof;
  assert(proof.open((stem + ".proof").c_str(), format));
  Fixture f(&proof);
  if (kind == 1) f.add({4,5,6}); // connected before the SSR unit
  f.seed();
  if (kind == 0) {
    // No complementary binary pair at entry: conflict occurs at propagation7.
    f.add({-3,6}); f.add({-3,7}); f.add({-6,8}); f.add({-7,-8});
  } else if (kind == 1) {
    f.add({-3,-5}); f.add({-3,-6}); f.add({-4});
  } else {
    f.add({-3,4,5}); // not connected yet: later candidate root recheck refutes
    f.add({-3,-4}); f.add({-3,-5});
  }
  f.enter(dense);
  const uint64_t conflicts = f.k.stats.conflicts;
  assert(strengthen_apply(f.k) == ApplyResult::Unsat);
  assert(f.k.inconsistent && !f.k.conflict_valid);
  assert(f.k.stats.strengthened == 1 && f.k.value(internalize(3)) > 0);
  assert(f.k.stats.conflicts == conflicts); // root refutation,not search conflict
  assert(f.k.stats.strengthen_checked == (kind == 0 ? 1 : 2));
  if (kind == 0) {
    assert(f.k.propagate_head + 1 == f.k.trail.size());
    assert(f.k.trail[f.k.propagate_head-1] == internalize(7));
  } else assert(f.k.propagate_head == f.k.trail.size());
  proof.flush();
  std::ofstream cnf(stem + ".cnf");
  cnf << "p cnf 16 " << f.inputs.size() << '\n';
  for (const auto &clause : f.inputs) {
    for (int lit : clause) cnf << lit << ' ';
    cnf << "0\n";
  }
}

int main(int argc, char **argv) {
  assert(argc == 2);
  for (bool dense : {false,true}) {
    interrupted_after_unit(dense);
    satisfied_connected_partner(dense);
    for (unsigned kind = 0; kind != 3; ++kind)
      for (ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat})
        conflict_case(dense,kind,format,std::string(argv[1]) + "/edge-" +
            std::to_string(dense) + "-" + std::to_string(kind) +
            (format == ProofFormat::Lrat ? "-lrat" : "-drat"));
  }
  puts("PASS:4 interrupt/satisfied-partner states;12 direct root-conflict proofs");
}

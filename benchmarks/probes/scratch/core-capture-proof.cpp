// Fixed integration test of the isolated 5317 capture/save/native-proof path.
#undef NDEBUG
#include "rules/sweep/sweep.cpp"
#include <cassert>
#include <cstdio>
#include <string>

int main(int argc, char **argv) {
  using namespace tactix;
  assert(argc == 2);
  const std::string base = argv[1];
  FILE *cnf = fopen((base + ".cnf").c_str(), "w");
  assert(cnf);
  fputs("p cnf 4 5\n-1 2 3 4 0\n-2 0\n-3 0\n-4 0\n1 0\n", cnf);
  fclose(cnf);
  for (const bool lrat : {false, true}) {
    Proof proof;
    assert(proof.open((base + (lrat ? ".lrat" : ".drat")).c_str(),
                      lrat ? ProofFormat::Lrat : ProofFormat::DratAscii));
    Kernel k;
    kernel_reserve_vars(k, 4);
    const Lit original[] = {internalize(-1), internalize(2),
                            internalize(3), internalize(4)};
    kernel_add_large_clause(k, original, 4, false, 4, 1);
    k.next_clause_id = 5; // input id 5 belongs to the checker-only closing unit
    k.proof = &proof;
    SweepEnv e;
    e.vars = {0, 1, 2, 3};
    e.clause_ids = {1, 2, 3, 4};
    e.strip_off = {0, 0, 0, 0, 0};
    e.solver.reset(4, SubSearchMode::Kitten);
    e.solver.add_clause({sub_lit(0, true), sub_lit(1, false),
                         sub_lit(2, false), sub_lit(3, false)});
    for (unsigned v = 1; v < 4; ++v)
      e.solver.add_clause({sub_lit(v, true)});
    assert(e.solver.solve({sub_lit(0, false)}, 0, SubTickMode::Kitten) == SubResult::Unsat);
    SweepDirProof before;
    sweep_capture_dir(k, e, before);
    assert(std::none_of(before.core_clauses.begin(), before.core_clauses.end(),
                       [](const auto &cc) { return !cc.learned && cc.id == 1; }));
    for (int v = 2; v <= 4; ++v) {
      const Lit lit = internalize(-v);
      k.unit_ids[var_of(lit)] = static_cast<uint64_t>(v);
      kernel_assign_unit(k, lit);
    }
    SweepDirProof saved;
    sweep_capture_dir(k, e, saved);
    assert(std::any_of(saved.core_clauses.begin(), saved.core_clauses.end(),
                      [](const auto &cc) { return !cc.learned && cc.id == 1; }));
    SweepRound round;
    const auto head = k.propagate_head;
    sweep_capture_core_units(k, saved, round, true);
    assert(k.value(internalize(-1)) > 0 && k.unit_ids[0] == 6);
    assert(k.propagate_head == head && !kernel_propagation_fixpoint(k));
    assert(round.emission_batches == 1 && k.stats.sweep_units == 1);
    const uint64_t chain[] = {6, 5};
    proof.add_empty(++k.next_clause_id, chain, 2);
    proof.flush();
  }
  puts("save-time filtering and original-core unit capture passed without root replay");
}

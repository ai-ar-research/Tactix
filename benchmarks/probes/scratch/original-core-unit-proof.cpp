// Fixed proof-interface probe, not a solver or scheduling repair.
// Compile against the immutable 5312 observer source/object tree; excluding
// its sweep.cpp.o lets this translation unit exercise the internal planner.
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
  // Clause 5 is checker-only, closing the emitted prefix without making the
  // host kernel inconsistent before the original-core unit is constructed.
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
    for (int v = 2; v <= 4; ++v) {
      const Lit lit = internalize(-v);
      k.unit_ids[var_of(lit)] = static_cast<uint64_t>(v);
      kernel_assign_unit(k, lit);
    }
    k.next_clause_id = 5;
    k.proof = &proof;
    SweepDirProof p;
    p.ok = true;
    p.orig_ids = {1};
    p.strip_off = {0, 0};
    p.vars = {0, 1, 2, 3};
    p.steps.push_back({2, {sub_lit(0, true), sub_lit(1, false),
                           sub_lit(2, false), sub_lit(3, false)}, {1}});
    const Lit unit = internalize(-1);
    const auto plan = sweep_plan_general(k, p, unit, INVALID_LIT);
    assert(plan.eligible() && plan.steps.size() == 1);
    if (lrat)
      assert((plan.steps[0].strip_ids == std::vector<uint64_t>{2, 3, 4}));
    sweep_apply_general_plan(k, plan);
    assert(k.value(unit) > 0 && k.unit_ids[var_of(unit)] == 6);
    const uint64_t chain[] = {6, 5};
    proof.add_empty(++k.next_clause_id, chain, 2);
    proof.flush();
  }
  puts("original-core unit: existing general planner emits root hints then original id");
}

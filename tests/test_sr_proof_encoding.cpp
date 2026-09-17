#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace tactix;

// Each requested clause has two open images and is not RUP: the contradictory
// core on 4/5 is UP-consistent and independent of the requested variables.
// The optional output mode lets the complete certificates be checked externally.
static void fixture(Proof *proof, FILE *cnf, bool negative, bool compact) {
  Kernel k;
  kernel_reserve_vars(k, 9);
  const int core[][2] = {{4, 5}, {4, -5}, {-4, 5}, {-4, -5}, {4, 5}};
  if (cnf)
    std::fputs("p cnf 9 5\n", cnf);
  for (const auto &clause : core) {
    const Lit literals[] = {kernel_internalize(k, clause[0]),
                           kernel_internalize(k, clause[1])};
    const uint64_t id = ++k.next_clause_id;
    kernel_add_binary_clause(k, literals[0], literals[1], id);
    if (proof)
      proof->seed_population(id, literals, 2);
    if (cnf)
      std::fprintf(cnf, "%d %d 0\n", clause[0], clause[1]);
  }
  k.proof = proof;
  if (proof) {
    proof->bind_variable_map(&k.internal_to_external);
    proof->arm_population_shadow();
  }

  const auto apply = [&](int first, int second) {
    const int phase = negative ? -1 : 1;
    SubstitutionRedundancyInstance instance;
    instance.clause = {kernel_internalize(k, phase * first),
                       kernel_internalize(k, phase * second)};
    instance.substitution = {
        {kernel_internalize(k, first), kernel_internalize(k, phase * 4)},
        {kernel_internalize(k, second), kernel_internalize(k, phase * 5)}};
    const uint32_t variables = k.nvars;
    const size_t external = k.external_to_internal.size();
    const uint64_t id = k.next_clause_id;
    const auto result = substitution_redundancy_apply_detailed(k, instance);
    assert(result.status == SubstitutionRedundancyApplicationStatus::Applied);
    assert(k.nvars == variables && k.external_to_internal.size() == external);
    assert(k.next_clause_id == id + 1);
    assert(substitution_redundancy_apply_detailed(k, instance).status ==
           SubstitutionRedundancyApplicationStatus::NoChange);
    kernel_check_invariants(k);
  };

  apply(2, 3);
  if (compact) {
    // Retire an unused variable so internal and external identities differ.
    k.var_status.assign(k.nvars, VarStatus::Active);
    k.var_status[0] = VarStatus::Eliminated;
    --k.unassigned_active;
    assert(kernel_compact_variables(k));
    assert(k.nvars == 8 && k.internal_to_external[0] != 0);
  }
  apply(6, 7); // Reuse the now-absent proof-only external variable 10.

  // A later real extension may take that identity: none of its old temporary
  // clauses may remain in the checker. Add the definition x <-> literal 4.
  const Var extension = kernel_new_extension_vars(k, 1);
  assert(k.internal_to_external[extension] == 9);
  const Lit x = make_lit(extension, false);
  const Lit a = kernel_internalize(k, 4);
  const Lit definitions[][2] = {{negate(x), a}, {x, negate(a)}};
  for (const auto &clause : definitions) {
    const uint64_t id = ++k.next_clause_id;
    if (proof)
      proof->add_rat(id, clause, 2, nullptr, 0);
    kernel_add_binary_clause(k, clause[0], clause[1], id);
  }
  apply(8, 9); // The next temporary must now use external variable 11.
  assert(k.stats.sr_applied == 3);
  if (proof) {
    // Original binary pairs make (4) RUP, then make the empty clause RUP.
    proof->add_rup(++k.next_clause_id, &a, 1, nullptr, 0);
    proof->add_empty(++k.next_clause_id, nullptr, 0);
    assert(!proof->unbacked_deletions());
    proof->flush();
  }
  k.proof = nullptr;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    for (bool negative : {false, true})
      for (bool compact : {false, true})
        fixture(nullptr, nullptr, negative, compact);
  } else {
    assert(argc == 6);
    FILE *cnf = std::fopen(argv[1], "w");
    assert(cnf);
    Proof proof;
    assert(proof.open(argv[2], !std::strcmp(argv[3], "binary")
                                   ? ProofFormat::DsrBinary
                                   : ProofFormat::DsrAscii));
    fixture(&proof, cnf, std::atoi(argv[4]) != 0, std::atoi(argv[5]) != 0);
    std::fclose(cnf);
  }
  std::puts("general SR encoding: legal applications, compaction and extensions pass");
}

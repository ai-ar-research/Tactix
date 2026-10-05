#undef NDEBUG
#include "proof/proof.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace tactix;

namespace tactix {
SubstitutionRedundancyApplicationResult
substitution_redundancy_apply_orbitopal_prefix(
    Kernel &, const SubstitutionRedundancyInstance &,
    const SubstitutionRedundancyParameters &, volatile sig_atomic_t *);
}

namespace {

void add(Kernel &kernel, const std::vector<int32_t> &clause,
         bool redundant = false) {
  std::vector<Lit> internal;
  for (int32_t literal : clause)
    internal.push_back(kernel_internalize(kernel, literal));
  const uint64_t id = ++kernel.next_clause_id;
  if (internal.size() == 2)
    kernel_add_binary_clause(kernel, internal[0], internal[1], id);
  else
    kernel_add_large_clause(kernel, internal.data(), internal.size(),
                            redundant, redundant ? internal.size() : 0, id);
  if (kernel.proof)
    kernel.proof->seed_population(id, internal.data(), internal.size());
}

SubstitutionRedundancyInstance pair(Kernel &kernel, int first, int second) {
  SubstitutionRedundancyInstance result;
  result.clause = {kernel_internalize(kernel, -first),
                   kernel_internalize(kernel, -second)};
  result.unique_literal_amo_certificate = true;
  return result;
}

void complete(Kernel &kernel) {
  SubstitutionRedundancyParameters parameters;
  for (const auto &[first, second] :
       std::vector<std::pair<int, int>>{{1, 2}, {1, 3}, {2, 3}}) {
    const auto result = substitution_redundancy_apply_orbitopal_prefix(
        kernel, pair(kernel, first, second), parameters, nullptr);
    assert(result.status == SubstitutionRedundancyApplicationStatus::Applied &&
           result.validation_ticks == 0);
  }
}

bool completed_model(unsigned mask) {
  if (!(mask & 7)) return false;
  return (mask & (mask - 1)) == 0;
}

void exhaustive_private_repair() {
  for (unsigned original = 0; original < 8; ++original) {
    const bool source_model = original != 0;
    const unsigned repaired_assignment = original & (~original + 1);
    const bool repaired = completed_model(repaired_assignment);
    assert(!source_model || repaired);
  }
  for (unsigned model = 0; model < 8; ++model)
    if (completed_model(model)) assert(model != 0);
}

void nol_scale_ulc_census() {
  constexpr uint32_t domain_count = 363;
  Kernel kernel;
  kernel_reserve_vars(kernel, 3 * domain_count);
  for (uint32_t domain = 0; domain < domain_count; ++domain) {
    const Var first = 3 * domain;
    const Lit alo[] = {make_lit(first, false), make_lit(first + 1, false),
                       make_lit(first + 2, false)};
    kernel_add_large_clause(kernel, alo, 3, false, 0,
                            ++kernel.next_clause_id);
  }
  SubstitutionRedundancyParameters parameters;
  parameters.producer = SubstitutionRedundancyProducer::Orbitopal;
  parameters.complete_unique_literal_amos = true;
  parameters.maximum_orbitopal_matrices = 0;
  uint64_t instances = 0;
  const auto result = substitution_redundancy_enumerate_orbitopal(
      kernel, parameters,
      [&](const SubstitutionRedundancyInstance &instance) {
        assert(instance.unique_literal_amo_certificate &&
               instance.clause.size() == 2 && instance.witness.empty() &&
               instance.substitution.empty());
        ++instances;
        return true;
      });
  assert(result.status == OrbitopalEnumerationStatus::Complete &&
         result.instances == 3 * domain_count &&
         instances == 3 * domain_count);
}

void unsupported_and_ambiguous_are_atomic() {
  Kernel binary_source;
  kernel_reserve_vars(binary_source, 2);
  add(binary_source, {1, 2});
  const uint64_t binary_id = binary_source.next_clause_id;
  const auto not_an_alo = substitution_redundancy_apply_orbitopal_prefix(
      binary_source, pair(binary_source, 1, 2),
      SubstitutionRedundancyParameters{}, nullptr);
  assert(not_an_alo.status ==
             SubstitutionRedundancyApplicationStatus::Inapplicable &&
         binary_source.next_clause_id == binary_id &&
         !kernel_binary_live(binary_source,
                             kernel_internalize(binary_source, -1),
                             kernel_internalize(binary_source, -2)));

  Kernel kernel;
  kernel_reserve_vars(kernel, 5);
  add(kernel, {1, 2, 3});
  add(kernel, {1, 4, 5}, true);
  add(kernel, {2, -4, -5}, true);
  const uint64_t id = kernel.next_clause_id;
  const size_t arena = kernel.arena.size();
  const auto ambiguous = substitution_redundancy_apply_orbitopal_prefix(
      kernel, pair(kernel, 1, 2), SubstitutionRedundancyParameters{}, nullptr);
  assert(ambiguous.status ==
             SubstitutionRedundancyApplicationStatus::Inapplicable &&
         kernel.next_clause_id == id && kernel.arena.size() == arena);

  Kernel hinted;
  kernel_reserve_vars(hinted, 3);
  add(hinted, {1, 2, 3});
  hinted.unit_ids[0] = ++hinted.next_clause_id;
  kernel_assign_unit(hinted, kernel_internalize(hinted, 1));
  hinted.unit_ids[1] = ++hinted.next_clause_id;
  kernel_assign_unit(hinted, kernel_internalize(hinted, 2));
  const uint64_t hinted_id = hinted.next_clause_id;
  const auto two_hints = substitution_redundancy_apply_orbitopal_prefix(
      hinted, pair(hinted, 1, 2), SubstitutionRedundancyParameters{}, nullptr);
  assert(two_hints.status ==
             SubstitutionRedundancyApplicationStatus::Inapplicable &&
         hinted.next_clause_id == hinted_id &&
         !kernel_binary_live(hinted, kernel_internalize(hinted, -1),
                             kernel_internalize(hinted, -2)));

  Kernel lrat;
  kernel_reserve_vars(lrat, 3);
  add(lrat, {1, 2, 3});
  Proof proof;
  assert(proof.open("/tmp/tactix-ulc-rat-declined.lrat", ProofFormat::Lrat));
  proof.bind_variable_map(&lrat.internal_to_external);
  lrat.proof = &proof;
  const uint64_t before = lrat.next_clause_id;
  const auto unsupported = substitution_redundancy_apply_orbitopal_prefix(
      lrat, pair(lrat, 1, 2), SubstitutionRedundancyParameters{}, nullptr);
  assert(unsupported.status ==
             SubstitutionRedundancyApplicationStatus::UnsupportedProofFormat &&
         lrat.next_clause_id == before &&
         !kernel_binary_live(lrat, kernel_internalize(lrat, -1),
                             kernel_internalize(lrat, -2)));
  lrat.proof = nullptr;
}

void certificate(const char *cnf_path, const char *proof_path,
                 ProofFormat format) {
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  std::fputs("p cnf 5 5\n1 2 3 0\n4 5 0\n4 -5 0\n-4 5 0\n-4 -5 0\n",
             cnf);
  assert(!std::fclose(cnf));

  Kernel kernel;
  kernel_reserve_vars(kernel, 5);
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  add(kernel, {1, 2, 3});
  add(kernel, {4, 5});
  add(kernel, {4, -5});
  add(kernel, {-4, 5});
  add(kernel, {-4, -5});
  complete(kernel);
  const int32_t four = 4;
  assert(proof.add_external_rup(++kernel.next_clause_id, &four, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
}

} // namespace

int main(int argc, char **argv) {
  exhaustive_private_repair();
  nol_scale_ulc_census();
  unsupported_and_ambiguous_are_atomic();
  if (argc == 4) {
    certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    certificate(argv[1], argv[3], ProofFormat::DratAscii);
  } else {
    assert(argc == 1);
    Kernel kernel;
    kernel_reserve_vars(kernel, 3);
    add(kernel, {1, 2, 3});
    complete(kernel);
  }
  std::puts("ULC exact-one completion: authenticated RAT and atomic declines pass");
}

#undef NDEBUG
#include "proof/proof.hpp"
#include "rules/graph_interval_equivalence/graph_interval_equivalence.hpp"
#include "rules/registry.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace tactix;

namespace {

void add(Kernel &kernel, std::initializer_list<int32_t> external) {
  std::vector<Lit> literals;
  for (int32_t literal : external) literals.push_back(internalize(literal));
  const uint64_t id = ++kernel.next_clause_id;
  if (literals.size() == 2)
    kernel_add_binary_clause(kernel, literals[0], literals[1], id);
  else
    kernel_add_large_clause(kernel, literals.data(), literals.size(), false, 0,
                            id);
}

void fixture(Kernel &kernel, bool complete) {
  kernel_reserve_vars(kernel, 11);
  add(kernel, {1, 4, 7});
  add(kernel, {2, 5, 8});
  add(kernel, {3, 6, 9});
  add(kernel, {-1, -4});
  add(kernel, {1, 2, 10});
  add(kernel, {-1, -10});
  add(kernel, {-2, -10});
  add(kernel, {1, 2, 11});
  add(kernel, {-1, -11});
  if (complete) add(kernel, {-2, -11});
}

std::string contents(FILE *file) {
  std::fflush(file);
  std::rewind(file);
  std::string result;
  char buffer[4096];
  while (const size_t size = std::fread(buffer, 1, sizeof buffer, file))
    result.append(buffer, size);
  assert(!std::ferror(file));
  return result;
}

void positive(const char *proof_path) {
  Kernel kernel;
  fixture(kernel, true);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  Proof proof;
  assert(proof.open(proof_path, ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;

  const RuleVTable &rule =
      tactix_rules().at(tactix_rule_id("GraphIntervalEquivalence"));
  assert(rule.guard(kernel));
  uint64_t semantic[4]{};
  assert(rule.semantic_instances(kernel, semantic, 4) == 1);
  const size_t trail_before = kernel.trail.size();
  assert(rule.apply_semantic_instance(kernel, semantic[0]) == ApplyResult::Ok);
  assert(kernel_binary_live(kernel, internalize(-10), internalize(11)));
  assert(kernel_binary_live(kernel, internalize(10), internalize(-11)));
  assert(kernel.trail.size() == trail_before); // Propagate is a separate action.
  assert(kernel.stats.graph_interval_equivalence_published == 2);
  assert(kernel.stats.graph_interval_equivalence_proof_literal_occurrences == 4);
  assert(kernel.structural_action_sequence == 1);

  const uint64_t proof_id = kernel.next_clause_id;
  assert(rule.apply_instance(kernel, 117) == ApplyResult::NoOp);
  assert(kernel.next_clause_id == proof_id);
  assert(kernel.structural_action_sequence == 1);
  assert(kernel.stats.graph_interval_equivalence_duplicates == 2);

  const std::string provenance = contents(records);
  assert(provenance.find("\"kind\":\"authenticated-duplicate-interval\"") !=
         std::string::npos);
  assert(provenance.find("\"row\":0,\"first\":0,\"last\":1") !=
         std::string::npos);
  assert(provenance.find("\"existing_variables_only\":true") !=
         std::string::npos);
  assert(provenance.find("\"proof_additions\":2") != std::string::npos);
  proof.flush();
  kernel.proof = nullptr;
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

void negative_near_miss() {
  Kernel kernel;
  fixture(kernel, false);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  const RuleVTable &rule =
      tactix_rules().at(tactix_rule_id("GraphIntervalEquivalence"));
  const uint64_t id = kernel.next_clause_id;
  assert(rule.apply(kernel) == ApplyResult::NoOp);
  assert(kernel.next_clause_id == id);
  assert(kernel.structural_action_sequence == 0);
  assert(!kernel_binary_live(kernel, internalize(-10), internalize(11)));
  kernel.structural_provenance = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  const char *proof = argc > 1 ? argv[1] : "/tmp/graph-interval-equivalence.dsr";
  positive(proof);
  negative_near_miss();
  std::puts("test_graph_interval_equivalence: all passed");
}

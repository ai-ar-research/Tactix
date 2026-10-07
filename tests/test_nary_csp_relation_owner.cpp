#undef NDEBUG
#include "rules/semantic_extension/nary_csp_relation_owner.hpp"

#include "frontend/parse.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace tactix;

namespace {

using Clauses = std::vector<std::vector<int32_t>>;
using Domains = std::vector<std::vector<int32_t>>;

Domains domains(int32_t first) {
  return {{first, first + 1, first + 2},
          {first + 3, first + 4, first + 5},
          {first + 6, first + 7, first + 8}};
}

Clauses relation_clauses(int32_t first, bool omit_amo = false,
                         bool duplicate_alo = false) {
  Clauses result;
  for (const auto &domain : domains(first)) {
    result.push_back(domain);
    if (duplicate_alo && domain.front() == first) result.push_back(domain);
    for (size_t left = 0; left < domain.size(); ++left)
      for (size_t right = left + 1; right < domain.size(); ++right)
        if (!(omit_amo && domain.front() == first && left == 0 && right == 1))
          result.push_back({-domain[left], -domain[right]});
  }
  result.push_back({-first, -(first + 3), -(first + 6)});
  result.push_back({-(first + 1), -(first + 4), -(first + 7)});
  result.push_back({-(first + 2), -(first + 5), -(first + 8)});
  // Same arity and selected variables, but not one complemented value from
  // each domain. It must not become a premise.
  result.push_back({first, -(first + 3), -(first + 6)});
  return result;
}

std::string dimacs(const Clauses &clauses, uint32_t variables) {
  std::ostringstream output;
  output << "p cnf " << variables << ' ' << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  return output.str();
}

Kernel parse(const Clauses &clauses, uint32_t variables) {
  const std::string text = dimacs(clauses, variables);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(!kernel.inconsistent && kernel_propagation_fixpoint(kernel));
  return kernel;
}

NaryCspRelationCaptureRequest request(int32_t first = 1) {
  NaryCspRelationCaptureRequest result;
  result.domains = domains(first);
  result.maximum_source_clauses = 100;
  result.maximum_source_literals = 1000;
  result.maximum_recognition_work = 100000;
  result.maximum_domains = 8;
  result.maximum_domain_values = 16;
  result.maximum_forbidden_tuples = 100;
  result.maximum_external_extent = 100;
  return result;
}

struct KernelImage {
  uint32_t nvars = 0;
  uint64_t next_clause_id = 0;
  size_t arena = 0;
  uint64_t arena_generation = 0;
  uint64_t touch_epoch = 0;
  uint64_t root_assignments = 0;
  uint64_t namespace_epoch = 0;
  uint64_t structural_sequence = 0;
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  std::vector<Lit> trail;
  std::vector<size_t> watches;

  bool operator==(const KernelImage &) const = default;
};

KernelImage image(const Kernel &kernel) {
  KernelImage result;
  result.nvars = kernel.nvars;
  result.next_clause_id = kernel.next_clause_id;
  result.arena = kernel.arena.size();
  result.arena_generation = kernel.arena_generation;
  result.touch_epoch = kernel.touch_epoch;
  result.root_assignments = kernel.stats.root_assignments;
  result.namespace_epoch = kernel.affine_semantic_namespace_epoch;
  result.structural_sequence = kernel.structural_action_sequence;
  result.internal_to_external = kernel.internal_to_external;
  result.external_to_internal = kernel.external_to_internal;
  result.trail = kernel.trail;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

NaryCspRelationOwnerSnapshot sentinel_snapshot() {
  NaryCspRelationOwnerSnapshot result;
  result.external_extent = 77;
  result.source_literals = 3;
  result.capture_work = 99;
  result.source.push_back({1, {41, -42, 43}, 123});
  result.relation.domains.push_back({{41, 42}, 1, {0, 1, 1, 0}});
  result.relation.forbidden.push_back({{0}, 1});
  return result;
}

void exact_capture_is_observational() {
  Kernel kernel = parse(relation_clauses(1), 9);
  const KernelImage before = image(kernel);
  const std::string proof_path =
      "/tmp/tactix-relation-owner-" + std::to_string(getpid()) + ".dsr";
  Proof proof;
  assert(proof.open(proof_path.c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  proof.flush();
  const auto proof_before = std::filesystem::file_size(proof_path);

  NaryCspRelationOwnerSnapshot owner = sentinel_snapshot();
  const auto captured = nary_csp_relation_capture_owner(kernel, request(), owner);
  assert(captured.status == NaryCspRelationCaptureStatus::Applied);
  assert(captured.source_clauses == 15 && captured.source_literals == 36);
  assert(captured.recognition_work && !captured.recognition_limit_exhausted);
  assert(!captured.session_work && !captured.owner_retained);
  assert(owner.source.size() == 15 && owner.source_literals == 36);
  assert(owner.relation.domains.size() == 3);
  assert(owner.relation.forbidden.size() == 3);
  for (size_t index = 0; index < owner.source.size(); ++index)
    assert(owner.source[index].occurrence == index + 1);
  for (const auto &domain : owner.relation.domains) {
    assert(domain.at_least_one && domain.at_most_one.size() == 9);
    for (size_t row = 0; row < 3; ++row)
      for (size_t column = 0; column < 3; ++column)
        assert(bool(domain.at_most_one[row * 3 + column]) == (row != column));
  }
  assert(owner.relation.forbidden[0].value_indices ==
         std::vector<uint32_t>({0, 0, 0}));
  assert(owner.relation.forbidden[1].value_indices ==
         std::vector<uint32_t>({1, 1, 1}));
  assert(owner.relation.forbidden[2].value_indices ==
         std::vector<uint32_t>({2, 2, 2}));
  proof.flush();
  assert(std::filesystem::file_size(proof_path) == proof_before);
  assert(image(kernel) == before);
  kernel.proof = nullptr;
  std::filesystem::remove(proof_path);
}

void exact_authentication_and_capture_bounds() {
  for (const auto &[clauses, expected] :
       std::vector<std::pair<Clauses, NaryCspRelationCaptureStatus>>{
           {relation_clauses(1, true),
            NaryCspRelationCaptureStatus::NoStructure},
           {relation_clauses(1, false, true),
            NaryCspRelationCaptureStatus::Invalid}}) {
    Kernel kernel = parse(clauses, 9);
    NaryCspRelationOwnerSnapshot output = sentinel_snapshot();
    const auto before = output;
    assert(nary_csp_relation_capture_owner(kernel, request(), output).status ==
           expected);
    assert(output == before);
  }

  Kernel kernel = parse(relation_clauses(1), 9);
  const KernelImage kernel_before = image(kernel);
  for (int kind = 0; kind < 5; ++kind) {
    auto bounded = request();
    if (kind == 0) bounded.maximum_recognition_work = 1;
    if (kind == 1) bounded.maximum_source_clauses = 14;
    if (kind == 2) bounded.maximum_source_literals = 35;
    if (kind == 3) bounded.maximum_forbidden_tuples = 2;
    if (kind == 4)
      bounded.maximum_external_extent =
          static_cast<uint32_t>(kernel.external_to_internal.size() - 1);
    NaryCspRelationOwnerSnapshot output = sentinel_snapshot();
    const auto before = output;
    const auto result =
        nary_csp_relation_capture_owner(kernel, bounded, output);
    assert(result.status == NaryCspRelationCaptureStatus::ResourceExhausted);
    assert(output == before && image(kernel) == kernel_before);
    if (kind == 0) assert(result.recognition_limit_exhausted);
  }

  auto malformed = request();
  std::swap(malformed.domains[0][0], malformed.domains[0][1]);
  NaryCspRelationOwnerSnapshot output = sentinel_snapshot();
  const auto before = output;
  assert(nary_csp_relation_capture_owner(kernel, malformed, output).status ==
         NaryCspRelationCaptureStatus::Invalid);
  assert(output == before);

  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  assert(nary_csp_relation_capture_owner(kernel, request(), output).status ==
         NaryCspRelationCaptureStatus::Interrupted);
  assert(output == before);
  kernel.stop = nullptr;
}

void transactional_session_and_staleness() {
  Clauses clauses = relation_clauses(1);
  const Clauses second = relation_clauses(10);
  clauses.insert(clauses.end(), second.begin(), second.end());
  Kernel kernel = parse(clauses, 18);
  const NaryCspRelationSessionLimits generous{4, 100000};

  NaryCspRelationOwnerSession empty;
  NaryCspRelationOwnerSession staged;
  const KernelImage before = image(kernel);
  const auto first = nary_csp_relation_prepare_owner(
      kernel, empty, request(1), generous, staged);
  assert(first.status == NaryCspRelationCaptureStatus::Applied);
  assert(first.owner_retained && first.retained_owners == 1);
  assert(first.session_work == staged.session_work());
  assert(staged.belongs_to(kernel) && staged.version() == 1);
  assert(staged.snapshot(domains(1)) && !staged.snapshot(domains(10)));
  assert(image(kernel) == before);

  NaryCspRelationOwnerSession session = std::move(staged);
  NaryCspRelationOwnerSession untouched;
  Kernel other = parse(relation_clauses(30), 38);
  assert(nary_csp_relation_prepare_owner(other, empty, request(30), generous,
                                         untouched)
             .status == NaryCspRelationCaptureStatus::Applied);
  const uint64_t untouched_version = untouched.version();
  const uint64_t untouched_work = untouched.session_work();
  const Kernel *untouched_owner =
      untouched.belongs_to(other) ? &other : nullptr;

  const auto duplicate = nary_csp_relation_prepare_owner(
      kernel, session, request(1), generous, untouched);
  assert(duplicate.status == NaryCspRelationCaptureStatus::Duplicate);
  assert(!duplicate.owner_retained && duplicate.retained_owners == 1);
  assert(untouched.version() == untouched_version &&
         untouched.session_work() == untouched_work && untouched_owner &&
         untouched.belongs_to(*untouched_owner));

  NaryCspRelationOwnerSession second_stage;
  const auto added = nary_csp_relation_prepare_owner(
      kernel, session, request(10), generous, second_stage);
  assert(added.status == NaryCspRelationCaptureStatus::Applied);
  assert(added.retained_owners == 2 && second_stage.version() == 2);
  assert(second_stage.snapshot(domains(1)) && second_stage.snapshot(domains(10)));

  const NaryCspRelationSessionLimits owner_bound{1, 100000};
  assert(nary_csp_relation_prepare_owner(kernel, session, request(10),
                                         owner_bound, untouched)
             .status == NaryCspRelationCaptureStatus::ResourceExhausted);
  const NaryCspRelationSessionLimits work_bound{
      4, added.session_work - 1};
  assert(nary_csp_relation_prepare_owner(kernel, session, request(10),
                                         work_bound, untouched)
             .status == NaryCspRelationCaptureStatus::ResourceExhausted);
  assert(untouched.version() == untouched_version &&
         untouched.session_work() == untouched_work &&
         untouched.belongs_to(other));

  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  assert(nary_csp_relation_prepare_owner(kernel, session, request(10), generous,
                                         untouched)
             .status == NaryCspRelationCaptureStatus::Interrupted);
  kernel.stop = nullptr;
  assert(untouched.belongs_to(other) && untouched.version() == untouched_version);

  assert(kernel_delete_binary_all_copies(
      kernel, kernel_internalize(kernel, -1), kernel_internalize(kernel, -2)));
  assert(nary_csp_relation_prepare_owner(kernel, session, request(1), generous,
                                         untouched)
             .status == NaryCspRelationCaptureStatus::Stale);
  assert(untouched.belongs_to(other) && untouched.version() == untouched_version);
}

} // namespace

int main() {
  exact_capture_is_observational();
  exact_authentication_and_capture_bounds();
  transactional_session_and_staleness();
  std::puts("test_nary_csp_relation_owner: passed");
}

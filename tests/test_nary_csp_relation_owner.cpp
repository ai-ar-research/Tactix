#undef NDEBUG
#include "dimacs_text.hpp"
#include "kernel_image.hpp"
#include "rules/semantic_extension/nary_csp_relation_owner.hpp"

#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <filesystem>
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

Kernel parse(const Clauses &clauses, uint32_t variables) {
  return parse_dimacs_text(dimacs_text(variables, clauses), ParsedRoot::Consistent);
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

// Owner capture must not move the scheduling and namespace clocks either.
struct OwnerImage {
  Image base;
  uint64_t touch_epoch, root_assignments, namespace_epoch, structural_sequence;
  std::vector<Lit> trail;
  bool operator==(const OwnerImage &) const = default;
};

OwnerImage owner_image(const Kernel &kernel) {
  return {image(kernel), kernel.touch_epoch, kernel.stats.root_assignments,
          kernel.affine_semantic_namespace_epoch,
          kernel.structural_action_sequence, kernel.trail};
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
  const OwnerImage before = owner_image(kernel);
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
  assert(captured.status == NaryCspRelationCaptureStatus::Applied &&
         captured.source_clauses == 15 && captured.source_literals == 36 &&
         captured.recognition_work && !captured.recognition_limit_exhausted &&
         !captured.session_work && !captured.owner_retained);
  assert(owner.source.size() == 15 && owner.source_literals == 36 &&
         owner.relation.domains.size() == 3 &&
         owner.relation.forbidden.size() == 3);
  for (size_t index = 0; index < owner.source.size(); ++index)
    assert(owner.source[index].occurrence == index + 1);
  for (const auto &domain : owner.relation.domains) {
    assert(domain.at_least_one && domain.at_most_one.size() == 9);
    for (size_t row = 0; row < 3; ++row)
      for (size_t column = 0; column < 3; ++column)
        assert(bool(domain.at_most_one[row * 3 + column]) == (row != column));
  }
  // The decoy {1,-4,-7} is not a forbidden tuple.
  for (uint32_t i = 0; i < 3; ++i)
    assert(owner.relation.forbidden[i].value_indices ==
           std::vector<uint32_t>({i, i, i}));
  proof.flush();
  assert(std::filesystem::file_size(proof_path) == proof_before);
  assert(owner_image(kernel) == before);
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
  const OwnerImage kernel_before = owner_image(kernel);
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
    assert(output == before && owner_image(kernel) == kernel_before);
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

// Every decline leaves the caller's output session untouched.
void transactional_session_and_staleness() {
  Clauses clauses = relation_clauses(1);
  const Clauses second = relation_clauses(10);
  clauses.insert(clauses.end(), second.begin(), second.end());
  Kernel kernel = parse(clauses, 18);
  const NaryCspRelationSessionLimits generous{4, 100000};

  NaryCspRelationOwnerSession empty;
  NaryCspRelationOwnerSession session;
  const OwnerImage before = owner_image(kernel);
  const auto first = nary_csp_relation_prepare_owner(
      kernel, empty, request(1), generous, session);
  assert(first.status == NaryCspRelationCaptureStatus::Applied &&
         first.owner_retained && first.retained_owners == 1 &&
         first.session_work == session.session_work());
  assert(session.belongs_to(kernel) && session.snapshot(domains(1)) &&
         !session.snapshot(domains(10)) && owner_image(kernel) == before);

  // `untouched` belongs to another kernel; no decline may overwrite it.
  Kernel other = parse(relation_clauses(30), 38);
  NaryCspRelationOwnerSession untouched;
  assert(nary_csp_relation_prepare_owner(other, empty, request(30), generous,
                                         untouched)
             .status == NaryCspRelationCaptureStatus::Applied);
  const uint64_t untouched_work = untouched.session_work();
  const auto declines = [&](const NaryCspRelationSessionLimits &limits,
                            int32_t first_value,
                            NaryCspRelationCaptureStatus expected) {
    const auto result = nary_csp_relation_prepare_owner(
        kernel, session, request(first_value), limits, untouched);
    assert(result.status == expected && !result.owner_retained);
    assert(untouched.belongs_to(other) && untouched.retained_owners() == 1 &&
           untouched.session_work() == untouched_work);
    return result;
  };
  assert(declines(generous, 1, NaryCspRelationCaptureStatus::Duplicate)
             .retained_owners == 1);

  NaryCspRelationOwnerSession second_stage;
  const auto added = nary_csp_relation_prepare_owner(
      kernel, session, request(10), generous, second_stage);
  assert(added.status == NaryCspRelationCaptureStatus::Applied &&
         added.retained_owners == 2 && second_stage.snapshot(domains(1)) &&
         second_stage.snapshot(domains(10)));

  declines({1, 100000}, 10, NaryCspRelationCaptureStatus::ResourceExhausted);
  declines({4, added.session_work - 1}, 10,
           NaryCspRelationCaptureStatus::ResourceExhausted);
  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  declines(generous, 10, NaryCspRelationCaptureStatus::Interrupted);
  kernel.stop = nullptr;
  assert(kernel_delete_binary_all_copies(
      kernel, kernel_internalize(kernel, -1), kernel_internalize(kernel, -2)));
  declines(generous, 1, NaryCspRelationCaptureStatus::Stale);
}

} // namespace

int main() {
  exact_capture_is_observational();
  exact_authentication_and_capture_bounds();
  transactional_session_and_staleness();
  std::puts("test_nary_csp_relation_owner: passed");
}

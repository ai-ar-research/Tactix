#undef NDEBUG
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"

#include "frontend/parse.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

namespace {

std::string dimacs() {
  const std::vector<std::vector<int32_t>> formula{
      {1, 2, 3}, {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6}, {-4, -5}, {-4, -6}, {-5, -6},
      {7, 8, 9}, {-7, -8}, {-7, -9}, {-8, -9},
      {-1, -4, -7}, {-2, -5, -8}, {-3, -6, -9}};
  std::ostringstream output;
  output << "p cnf 9 " << formula.size() << '\n';
  for (const auto &clause : formula) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  return output.str();
}

Kernel parse() {
  const std::string text = dimacs();
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

NaryCspPrefixActionLimits limits() {
  return {100, 1000, 100000, 8, 16, 100, 7, 16, 1000,
          32,  200,  100000, 1, 100, 32, 100000};
}

NaryCspPrefixActionSelection selection(uint32_t value = 0) {
  return {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}},
          {2, {{0, value}, {1, value}}}};
}

void repeated_apply_retains_one_owner_and_durable_keys() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;

  const auto first =
      nary_csp_prefix_action_apply(kernel, session, limits(), selection(0));
  assert(first.status == NaryCspPrefixActionStatus::Applied);
  assert(first.result == ApplyResult::Ok);
  assert(first.proposed_action_id == 1 && first.committed_action_id == 1 &&
         kernel.structural_action_sequence == 1);
  assert(first.committed_keys == 1 && first.retained_owners == 1 &&
         first.owner_retained);
  assert(session.belongs_to(kernel) && session.committed_keys() == 1 &&
         session.retained_owners() == 1);
  assert(first.semantic_candidate_ready &&
         first.semantic_variables.size() == 4);
  assert(first.semantic_variables.back().role ==
         strategy::SemanticVariableRole::IntroducedTheory);
  assert(first.projection.extension_variable == 10 &&
         first.projection.definition_inputs == std::vector<int32_t>({1, 4}) &&
         first.projection.exact_and_definition &&
         first.projection.project_from_external_model &&
         !first.projection.reconstruction_witness_required);

  const long journal_after_first = std::ftell(records);
  const auto repeated =
      nary_csp_prefix_action_apply(kernel, session, limits(), selection(0));
  assert(repeated.status == NaryCspPrefixActionStatus::AlreadyPublished);
  assert(repeated.live.status == NaryCspPrefixLiveStatus::Duplicate);
  assert(repeated.result == ApplyResult::NoOp);
  assert(kernel.structural_action_sequence == 1 &&
         session.committed_keys() == 1 &&
         std::ftell(records) == journal_after_first);

  const auto second =
      nary_csp_prefix_action_apply(kernel, session, limits(), selection(1));
  assert(second.status == NaryCspPrefixActionStatus::Applied);
  assert(second.committed_action_id == 2 &&
         kernel.structural_action_sequence == 2);
  assert(second.committed_keys == 2 && second.retained_owners == 1);
  assert(second.projection.extension_variable == 11 &&
         second.projection.definition_inputs == std::vector<int32_t>({2, 5}));
  assert(session.keys()[0] < session.keys()[1]);

  auto no_consequence = selection(0);
  no_consequence.prefix.prefix[1].value = 1;
  const auto declined = nary_csp_prefix_action_apply(
      kernel, session, limits(), no_consequence);
  assert(declined.status == NaryCspPrefixActionStatus::NoConsequence);
  assert(kernel.structural_action_sequence == 2 &&
         session.committed_keys() == 2 && session.retained_owners() == 1);
  std::fclose(records);
}

void guard_validation_resource_and_staleness() {
  {
    Kernel kernel = parse();
    NaryCspPrefixActionSession session;
    assert(!nary_csp_prefix_action_guard(kernel));
    const auto result =
        nary_csp_prefix_action_apply(kernel, session, limits(), selection());
    assert(result.status == NaryCspPrefixActionStatus::Inapplicable);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    kernel.structural_provenance = records;
    NaryCspPrefixActionSession session;
    auto invalid = selection();
    invalid.prefix.prefix.pop_back();
    const auto result =
        nary_csp_prefix_action_apply(kernel, session, limits(), invalid);
    assert(result.status == NaryCspPrefixActionStatus::Invalid);
    assert(!session.retained() && !kernel.structural_action_sequence);

    auto bounded = limits();
    bounded.maximum_session_work = 1;
    const auto exhausted = nary_csp_prefix_action_apply(
        kernel, session, bounded, selection());
    assert(exhausted.status ==
           NaryCspPrefixActionStatus::ResourceExhausted);
    assert(!session.retained() && !kernel.structural_action_sequence);
    std::fclose(records);
  }
  {
    Kernel first = parse();
    FILE *first_records = std::tmpfile();
    assert(first_records);
    first.structural_provenance = first_records;
    NaryCspPrefixActionSession session;
    assert(nary_csp_prefix_action_apply(first, session, limits(), selection())
               .status == NaryCspPrefixActionStatus::Applied);

    Kernel second = parse();
    FILE *second_records = std::tmpfile();
    assert(second_records);
    second.structural_provenance = second_records;
    const auto stale = nary_csp_prefix_action_apply(
        second, session, limits(), selection(1));
    assert(stale.status == NaryCspPrefixActionStatus::Stale);
    assert(!second.structural_action_sequence);

    assert(kernel_delete_binary_all_copies(
        first, kernel_internalize(first, -1),
        kernel_internalize(first, -2)));
    const auto changed = nary_csp_prefix_action_apply(
        first, session, limits(), selection(2));
    assert(changed.status == NaryCspPrefixActionStatus::Stale);
    assert(first.structural_action_sequence == 1 &&
           session.committed_keys() == 1);
    std::fclose(second_records);
    std::fclose(first_records);
  }
}

} // namespace

int main() {
  repeated_apply_retains_one_owner_and_durable_keys();
  guard_validation_resource_and_staleness();
  std::puts("test_nary_csp_prefix_action: passed");
}

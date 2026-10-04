#undef NDEBUG
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"

#include "frontend/parse.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
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

Kernel parse_formula(const std::vector<std::vector<int32_t>> &formula) {
  uint32_t variables = 0;
  std::ostringstream output;
  for (const auto &clause : formula)
    for (int32_t literal : clause)
      variables = std::max(
          variables, static_cast<uint32_t>(literal < 0 ? -int64_t(literal)
                                                        : literal));
  output << "p cnf " << variables << ' ' << formula.size() << '\n';
  for (const auto &clause : formula) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  const std::string text = output.str();
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

NaryCspPrefixActionSelection oriented_selection(uint32_t residual,
                                                uint32_t value) {
  NaryCspPrefixActionSelection result;
  result.domains = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
  result.prefix.residual_domain = residual;
  for (uint32_t domain = 0; domain < 3; ++domain)
    if (domain != residual)
      result.prefix.prefix.push_back({domain, value});
  return result;
}

NaryCspPrefixActionKey action_key(
    const NaryCspPrefixActionSelection &selected) {
  return {selected.domains, selected.prefix};
}

std::vector<NaryCspPrefixActionSelection> expected_offers() {
  std::vector<NaryCspPrefixActionSelection> result;
  for (uint32_t residual = 0; residual < 3; ++residual)
    for (uint32_t value = 0; value < 3; ++value)
      result.push_back(oriented_selection(residual, value));
  return result;
}

NaryCspPrefixActionOfferWindow offer_window() {
  NaryCspPrefixActionOfferWindow result;
  result.relation_arity = 3;
  result.maximum_domain_candidates = 32;
  result.maximum_relation_candidates = 32;
  result.maximum_prefix_candidates = 100;
  result.maximum_offers = 100;
  result.maximum_enumeration_work = 10000000;
  assert(result.valid());
  return result;
}

struct Image {
  uint32_t nvars = 0;
  uint64_t next_clause_id = 0;
  uint64_t action_sequence = 0;
  size_t arena_words = 0;
  size_t external_extent = 0;
  size_t trail = 0;
  uint64_t arena_generation = 0;
  bool candidate_pending = false;
  std::vector<size_t> watches;

  bool operator==(const Image &) const = default;
};

Image image(const Kernel &kernel) {
  Image result;
  result.nvars = kernel.nvars;
  result.next_clause_id = kernel.next_clause_id;
  result.action_sequence = kernel.structural_action_sequence;
  result.arena_words = kernel.arena.size();
  result.external_extent = kernel.external_to_internal.size();
  result.trail = kernel.trail.size();
  result.arena_generation = kernel.arena_generation;
  result.candidate_pending = kernel.semantic_variable_candidate_pending;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
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

    auto noncanonical = selection();
    std::swap(noncanonical.domains[0], noncanonical.domains[1]);
    const auto unordered = nary_csp_prefix_action_apply(
        kernel, session, limits(), noncanonical);
    assert(unordered.status == NaryCspPrefixActionStatus::Invalid);
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

void bounded_offerer_is_complete_paged_pure_and_stale_safe() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  const Image before = image(kernel);
  const long journal = position(records);
  const auto expected = expected_offers();

  const auto complete =
      nary_csp_prefix_action_offer(kernel, session, limits(), offer_window());
  assert(complete.status == NaryCspPrefixActionOfferStatus::Ready &&
         complete.complete && !complete.have_resume_after &&
         complete.offers == expected &&
         complete.effort.domain_candidates == 3 &&
         complete.effort.exact_domains == 3 &&
         complete.effort.relation_candidates == 1 &&
         complete.effort.forbidden_tuples == 3 &&
         complete.effort.prefix_candidates == 9 &&
         complete.effort.session_preflight_work &&
         complete.effort.preflight_work &&
         complete.effort.planned_additions &&
         complete.effort.enumeration_work && image(kernel) == before &&
         position(records) == journal && !session.retained());
  const auto repeated =
      nary_csp_prefix_action_offer(kernel, session, limits(), offer_window());
  assert(repeated.status == complete.status &&
         repeated.complete == complete.complete &&
         repeated.offers == complete.offers && image(kernel) == before &&
         position(records) == journal && !session.retained());

  std::vector<NaryCspPrefixActionSelection> paged;
  auto page_window = offer_window();
  page_window.maximum_offers = 2;
  for (uint32_t page = 0; page < 10; ++page) {
    const auto offered =
        nary_csp_prefix_action_offer(kernel, session, limits(), page_window);
    assert(offered.status == NaryCspPrefixActionOfferStatus::Ready);
    paged.insert(paged.end(), offered.offers.begin(), offered.offers.end());
    if (offered.complete) {
      assert(!offered.have_resume_after);
      break;
    }
    assert(offered.have_resume_after && !offered.offers.empty() &&
           offered.resume_after == action_key(offered.offers.back()));
    page_window.have_after = true;
    page_window.after = offered.resume_after;
  }
  assert(paged == expected && image(kernel) == before &&
         position(records) == journal && !session.retained());

  assert(nary_csp_prefix_action_apply(kernel, session, limits(), expected[0])
             .status == NaryCspPrefixActionStatus::Applied);
  const Image after_apply = image(kernel);
  const long after_apply_journal = position(records);
  const auto after_commit =
      nary_csp_prefix_action_offer(kernel, session, limits(), offer_window());
  assert(after_commit.status == NaryCspPrefixActionOfferStatus::Ready &&
         after_commit.complete && after_commit.offers.size() == 8 &&
         after_commit.effort.retained_owners_revalidated == 1 &&
         after_commit.effort.committed_prefixes_skipped == 1 &&
         std::find(after_commit.offers.begin(), after_commit.offers.end(),
                   expected[0]) == after_commit.offers.end() &&
         image(kernel) == after_apply &&
         position(records) == after_apply_journal &&
         session.committed_keys() == 1 && session.retained_owners() == 1);

  assert(kernel_delete_binary_all_copies(
      kernel, kernel_internalize(kernel, -1),
      kernel_internalize(kernel, -2)));
  const Image after_delete = image(kernel);
  const long after_delete_journal = position(records);
  const auto stale =
      nary_csp_prefix_action_offer(kernel, session, limits(), offer_window());
  assert(stale.status == NaryCspPrefixActionOfferStatus::Stale &&
         !stale.complete && stale.offers.empty() &&
         image(kernel) == after_delete &&
         position(records) == after_delete_journal &&
         session.committed_keys() == 1 && session.retained_owners() == 1);

  Kernel other = parse();
  FILE *other_records = std::tmpfile();
  assert(other_records);
  other.structural_provenance = other_records;
  const Image other_before = image(other);
  const auto wrong_owner =
      nary_csp_prefix_action_offer(other, session, limits(), offer_window());
  assert(wrong_owner.status == NaryCspPrefixActionOfferStatus::Stale &&
         wrong_owner.offers.empty() && image(other) == other_before &&
         !position(other_records));
  std::fclose(other_records);
  std::fclose(records);
}

void signed_domains_are_discovered_canonically() {
  const std::vector<std::vector<int32_t>> formula{
      {-1, 2, 3}, {1, -2}, {1, -3}, {-2, -3},
      {4, -5, 6}, {-4, 5}, {-4, -6}, {5, -6},
      {7, 8, -9}, {-7, -8}, {-7, 9}, {-8, 9},
      {1, -4, -7}};
  Kernel kernel = parse_formula(formula);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  const Image before = image(kernel);
  const auto offered =
      nary_csp_prefix_action_offer(kernel, session, limits(), offer_window());
  const std::vector<std::vector<int32_t>> domains{
      {-1, 2, 3}, {4, -5, 6}, {7, 8, -9}};
  assert(offered.status == NaryCspPrefixActionOfferStatus::Ready &&
         offered.complete && offered.offers.size() == 3 &&
         offered.effort.domain_candidates == 3 &&
         offered.effort.forbidden_tuples == 1);
  for (uint32_t residual = 0; residual < 3; ++residual) {
    NaryCspPrefixActionSelection expected;
    expected.domains = domains;
    expected.prefix.residual_domain = residual;
    for (uint32_t domain = 0; domain < 3; ++domain)
      if (domain != residual)
        expected.prefix.prefix.push_back({domain, 0});
    assert(offered.offers[residual] == expected);
  }
  assert(image(kernel) == before && !position(records) &&
         !session.retained());
  std::fclose(records);
}

void offerer_declines_guards_bounds_and_interrupt_atomically() {
  {
    Kernel kernel = parse();
    NaryCspPrefixActionSession session;
    const Image before = image(kernel);
    const auto inapplicable =
        nary_csp_prefix_action_offer(kernel, session, limits(), offer_window());
    assert(inapplicable.status ==
               NaryCspPrefixActionOfferStatus::Inapplicable &&
           inapplicable.offers.empty() && image(kernel) == before);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    kernel.structural_provenance = records;
    NaryCspPrefixActionSession session;
    const Image before = image(kernel);
    const long journal = position(records);

    auto invalid_window = offer_window();
    invalid_window.have_after = true;
    invalid_window.after = action_key(oriented_selection(0, 0));
    std::swap(invalid_window.after.domains[0],
              invalid_window.after.domains[1]);
    const auto invalid = nary_csp_prefix_action_offer(
        kernel, session, limits(), invalid_window);
    assert(invalid.status == NaryCspPrefixActionOfferStatus::Invalid &&
           invalid.offers.empty());

    auto tiny_work = offer_window();
    tiny_work.maximum_enumeration_work = 1;
    const auto work_exhausted = nary_csp_prefix_action_offer(
        kernel, session, limits(), tiny_work);
    assert(work_exhausted.status ==
               NaryCspPrefixActionOfferStatus::ResourceExhausted &&
           work_exhausted.offers.empty() &&
           work_exhausted.effort.enumeration_work == 1);

    auto tiny_domains = offer_window();
    tiny_domains.maximum_domain_candidates = 2;
    const auto domains_exhausted = nary_csp_prefix_action_offer(
        kernel, session, limits(), tiny_domains);
    assert(domains_exhausted.status ==
               NaryCspPrefixActionOfferStatus::ResourceExhausted &&
           domains_exhausted.offers.empty());

    auto tiny_prefixes = offer_window();
    tiny_prefixes.maximum_prefix_candidates = 8;
    const auto prefixes_exhausted = nary_csp_prefix_action_offer(
        kernel, session, limits(), tiny_prefixes);
    assert(prefixes_exhausted.status ==
               NaryCspPrefixActionOfferStatus::ResourceExhausted &&
           prefixes_exhausted.offers.empty() &&
           prefixes_exhausted.effort.prefix_candidates == 8);

    auto source_bound = limits();
    source_bound.maximum_source_clauses = 1;
    const auto source_exhausted = nary_csp_prefix_action_offer(
        kernel, session, source_bound, offer_window());
    assert(source_exhausted.status ==
               NaryCspPrefixActionOfferStatus::ResourceExhausted &&
           source_exhausted.offers.empty() &&
           source_exhausted.effort.resource_rejected_relations == 1);

    auto session_bound = limits();
    session_bound.maximum_session_work = 1;
    const auto session_exhausted = nary_csp_prefix_action_offer(
        kernel, session, session_bound, offer_window());
    assert(session_exhausted.status ==
               NaryCspPrefixActionOfferStatus::ResourceExhausted &&
           session_exhausted.offers.empty() &&
           session_exhausted.effort.resource_rejected_prefixes == 9);

    volatile sig_atomic_t stop = 1;
    KernelStopPublisher publisher(kernel, &stop);
    const auto interrupted =
        nary_csp_prefix_action_offer(kernel, session, limits(), offer_window());
    assert(interrupted.status ==
               NaryCspPrefixActionOfferStatus::Interrupted &&
           interrupted.offers.empty());
    assert(image(kernel) == before && position(records) == journal &&
           !session.retained());
    std::fclose(records);
  }
}

} // namespace

int main() {
  repeated_apply_retains_one_owner_and_durable_keys();
  guard_validation_resource_and_staleness();
  bounded_offerer_is_complete_paged_pure_and_stale_safe();
  signed_domains_are_discovered_canonically();
  offerer_declines_guards_bounds_and_interrupt_atomically();
  std::puts("test_nary_csp_prefix_action: passed");
}

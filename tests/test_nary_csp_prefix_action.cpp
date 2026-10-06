#undef NDEBUG
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"

#include "frontend/parse.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

namespace {

std::vector<std::vector<int32_t>> base_formula() {
  return {
      {1, 2, 3}, {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6}, {-4, -5}, {-4, -6}, {-5, -6},
      {7, 8, 9}, {-7, -8}, {-7, -9}, {-8, -9},
      {-1, -4, -7}, {-2, -5, -8}, {-3, -6, -9}};
}

std::string dimacs() {
  const auto formula = base_formula();
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

void propagated_root_preserves_exact_source_contract() {
  {
    auto formula = base_formula();
    formula.push_back({10});
    Kernel kernel = parse_formula(formula);
    assert(kernel_propagation_fixpoint(kernel) && kernel.trail.size() == 1 &&
           kernel.value(kernel_internalize(kernel, 10)) > 0);
    FILE *records = std::tmpfile();
    assert(records);
    kernel.structural_provenance = records;
    NaryCspPrefixActionSession session;

    assert(nary_csp_prefix_action_guard(kernel));
    const Image before = image(kernel);
    const auto offered = nary_csp_prefix_action_offer(
        kernel, session, limits(), offer_window());
    assert(offered.status == NaryCspPrefixActionOfferStatus::Ready &&
           offered.complete && offered.offers == expected_offers() &&
           image(kernel) == before && !session.retained());

    const auto first = nary_csp_prefix_action_apply(
        kernel, session, limits(), selection(0));
    assert(first.status == NaryCspPrefixActionStatus::Applied &&
           first.projection.extension_variable == 11 &&
           kernel.trail.size() == 1 &&
           kernel.value(kernel_internalize(kernel, 10)) > 0 &&
           kernel_propagation_fixpoint(kernel) &&
           session.committed_keys() == 1 && session.retained_owners() == 1);
    kernel_check_invariants(kernel);

    // An unrelated root assignment preserves the retained exact owner.
    const auto second = nary_csp_prefix_action_apply(
        kernel, session, limits(), selection(1));
    assert(second.status == NaryCspPrefixActionStatus::Applied &&
           second.projection.extension_variable == 12 &&
           session.committed_keys() == 2 && session.retained_owners() == 1);

    // Once a source-domain variable becomes root fixed, exact recapture can no
    // longer retain that owner and must report it stale.
    kernel_assign_unit(kernel, kernel_internalize(kernel, 1));
    assert(!kernel_propagate(kernel) && kernel_propagation_fixpoint(kernel));
    const Image fixed = image(kernel);
    const long journal = position(records);
    const auto stale = nary_csp_prefix_action_offer(
        kernel, session, limits(), offer_window());
    assert(stale.status == NaryCspPrefixActionOfferStatus::Stale &&
           stale.offers.empty() && image(kernel) == fixed &&
           position(records) == journal && session.committed_keys() == 2 &&
           session.retained_owners() == 1);
    std::fclose(records);
  }

  {
    // A domain touched by root propagation is not eligible for discovery, and
    // an explicit payload over it declines without committing any state.
    auto formula = base_formula();
    formula.push_back({1});
    Kernel kernel = parse_formula(formula);
    assert(kernel_propagation_fixpoint(kernel) && !kernel.trail.empty());
    FILE *records = std::tmpfile();
    assert(records);
    kernel.structural_provenance = records;
    NaryCspPrefixActionSession session;
    const Image before = image(kernel);
    const long journal = position(records);
    const auto offered = nary_csp_prefix_action_offer(
        kernel, session, limits(), offer_window());
    assert(offered.status == NaryCspPrefixActionOfferStatus::NoStructure &&
           offered.offers.empty() && image(kernel) == before &&
           position(records) == journal);
    const auto applied = nary_csp_prefix_action_apply(
        kernel, session, limits(), selection());
    assert(applied.status == NaryCspPrefixActionStatus::NoStructure &&
           applied.result == ApplyResult::NoOp && image(kernel) == before &&
           position(records) == journal && !session.retained() &&
           !kernel.structural_action_sequence);
    std::fclose(records);
  }

  {
    // The unit makes the extra literal false, but X17 must not shorten the
    // four-literal clause into a tuple source.
    auto formula = base_formula();
    const std::vector<int32_t> exact{-1, -4, -7};
    const auto found = std::find(formula.begin(), formula.end(), exact);
    assert(found != formula.end());
    formula.erase(found);
    formula.push_back({-1, -4, -7, -10});
    formula.push_back({10});
    Kernel kernel = parse_formula(formula);
    assert(kernel_propagation_fixpoint(kernel) && !kernel.trail.empty());
    FILE *records = std::tmpfile();
    assert(records);
    kernel.structural_provenance = records;
    NaryCspPrefixActionSession session;
    const auto offered = nary_csp_prefix_action_offer(
        kernel, session, limits(), offer_window());
    assert(offered.status == NaryCspPrefixActionOfferStatus::Ready &&
           offered.complete && offered.offers.size() == 6 &&
           std::find(offered.offers.begin(), offered.offers.end(),
                     selection(0)) == offered.offers.end());
    const Image before = image(kernel);
    const long journal = position(records);
    const auto residual = nary_csp_prefix_action_apply(
        kernel, session, limits(), selection(0));
    assert(residual.status == NaryCspPrefixActionStatus::NoConsequence &&
           residual.result == ApplyResult::NoOp && image(kernel) == before &&
           position(records) == journal && !session.retained());
    std::fclose(records);
  }
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
         // Every syntactically valid candidate clause is counted before the
         // exact-AMO filter; this fixture has 15 such clauses and three exact
         // domains.
         complete.effort.domain_candidates == 15 &&
         complete.effort.exact_domains == 3 &&
         complete.effort.indexed_relation_keys == 1 &&
         complete.effort.indexed_tuple_occurrences == 3 &&
         complete.effort.indexed_prefix_candidates == 9 &&
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

  auto exact_relation_bound = offer_window();
  exact_relation_bound.maximum_relation_candidates = 1;
  const auto exact_boundary = nary_csp_prefix_action_offer(
      kernel, session, limits(), exact_relation_bound);
  assert(exact_boundary.status == NaryCspPrefixActionOfferStatus::Ready &&
         exact_boundary.complete && !exact_boundary.have_resume_after &&
         exact_boundary.offers == expected &&
         exact_boundary.effort.indexed_relation_keys == 1 &&
         exact_boundary.effort.indexed_relation_lookahead_keys == 0 &&
         exact_boundary.effort.relation_candidates == 1);

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
         offered.effort.domain_candidates == 13 &&
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

void relation_index_pages_clause_backed_relations_only() {
  const std::vector<std::vector<int32_t>> formula{
      {1, 2, 3},    {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6},    {-4, -5}, {-4, -6}, {-5, -6},
      {7, 8, 9},    {-7, -8}, {-7, -9}, {-8, -9},
      {10, 11, 12}, {-10, -11}, {-10, -12}, {-11, -12},
      {-1, -4, -7}, {-4, -7, -10}};
  Kernel kernel = parse_formula(formula);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  auto bounded = limits();
  bounded.maximum_domains = 4;

  const std::vector<std::vector<std::vector<int32_t>>> relation_domains{
      {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}},
      {{4, 5, 6}, {7, 8, 9}, {10, 11, 12}}};
  std::vector<NaryCspPrefixActionSelection> expected;
  for (const auto &domains : relation_domains)
    for (uint32_t residual = 0; residual < 3; ++residual) {
      NaryCspPrefixActionSelection selected;
      selected.domains = domains;
      selected.prefix.residual_domain = residual;
      for (uint32_t domain = 0; domain < 3; ++domain)
        if (domain != residual)
          selected.prefix.prefix.push_back({domain, 0});
      expected.push_back(std::move(selected));
    }

  auto window = offer_window();
  window.maximum_relation_candidates = 1;
  std::vector<NaryCspPrefixActionSelection> paged;
  for (unsigned page = 0; page < 4; ++page) {
    const auto offered =
        nary_csp_prefix_action_offer(kernel, session, bounded, window);
    assert(offered.status == NaryCspPrefixActionOfferStatus::Ready);
    assert(offered.effort.relation_candidates <= 1 &&
           offered.effort.indexed_relation_keys == 1 &&
           offered.effort.indexed_relation_lookahead_keys <= 1);
    paged.insert(paged.end(), offered.offers.begin(), offered.offers.end());
    if (offered.complete) break;
    assert(offered.have_resume_after && !offered.offers.empty());
    window.have_after = true;
    window.after = offered.resume_after;
  }
  assert(paged == expected && !session.retained() && !position(records));

  Kernel committed = parse_formula(formula);
  FILE *committed_records = std::tmpfile();
  assert(committed_records);
  committed.structural_provenance = committed_records;
  NaryCspPrefixActionSession committed_session;
  for (size_t index = 0; index < 3; ++index)
    assert(nary_csp_prefix_action_apply(committed, committed_session, bounded,
                                        expected[index])
               .status == NaryCspPrefixActionStatus::Applied);
  auto committed_window = offer_window();
  committed_window.maximum_relation_candidates = 1;
  const auto empty_page = nary_csp_prefix_action_offer(
      committed, committed_session, bounded, committed_window);
  assert(empty_page.status == NaryCspPrefixActionOfferStatus::Ready &&
         !empty_page.complete && empty_page.offers.empty() &&
         empty_page.have_resume_after &&
         empty_page.effort.committed_prefixes_skipped == 3 &&
         empty_page.effort.relation_candidates == 1);
  committed_window.have_after = true;
  committed_window.after = empty_page.resume_after;
  const auto after_committed = nary_csp_prefix_action_offer(
      committed, committed_session, bounded, committed_window);
  assert(after_committed.status == NaryCspPrefixActionOfferStatus::Ready &&
         after_committed.complete && after_committed.offers.size() == 3 &&
         after_committed.effort.relation_candidates == 1);
  std::fclose(committed_records);

  // A ternary clause with two values from one domain is not a relation key.
  auto malformed = formula;
  malformed.push_back({-1, -2, -4});
  Kernel atomic = parse_formula(malformed);
  FILE *atomic_records = std::tmpfile();
  assert(atomic_records);
  atomic.structural_provenance = atomic_records;
  NaryCspPrefixActionSession atomic_session;
  auto all = offer_window();
  all.maximum_relation_candidates = 8;
  const auto ignored = nary_csp_prefix_action_offer(
      atomic, atomic_session, bounded, all);
  assert(ignored.status == NaryCspPrefixActionOfferStatus::Ready &&
         ignored.complete && ignored.offers == expected &&
         !atomic_session.retained() && !position(atomic_records));
  std::fclose(atomic_records);
  std::fclose(records);
}

void indexed_discovery_matches_combination_reference() {
  uint32_t random = 0x5eed1234u;
  const auto next = [&] {
    random = random * 1664525u + 1013904223u;
    return random;
  };
  for (unsigned fixture = 0; fixture < 12; ++fixture) {
    constexpr uint32_t domain_count = 5;
    std::vector<std::vector<int32_t>> formula;
    std::vector<std::vector<int32_t>> domains;
    for (uint32_t domain = 0; domain < domain_count; ++domain) {
      const int32_t first = 3 * domain + 1;
      domains.push_back({first, first + 1, first + 2});
      formula.push_back(domains.back());
      formula.push_back({-first, -(first + 1)});
      formula.push_back({-first, -(first + 2)});
      formula.push_back({-(first + 1), -(first + 2)});
    }

    // This is the old combination discovery result computed directly from
    // the generated domain triples and tuple coordinates.  The fixture order
    // is intentionally unrelated to canonical key order.
    std::set<NaryCspPrefixActionKey> reference;
    for (uint32_t first = 0; first < domain_count; ++first)
      for (uint32_t second = first + 1; second < domain_count; ++second)
        for (uint32_t third = second + 1; third < domain_count; ++third) {
          if ((next() & 3u) == 0) continue;
          const std::vector<uint32_t> relation{first, second, third};
          std::set<std::vector<uint32_t>> tuples;
          const uint32_t count = 1 + next() % 3;
          while (tuples.size() < count)
            tuples.insert({next() % 3, next() % 3, next() % 3});
          for (const auto &tuple : tuples) {
            formula.push_back(
                {-domains[first][tuple[0]], -domains[second][tuple[1]],
                 -domains[third][tuple[2]]});
            for (uint32_t residual = 0; residual < 3; ++residual) {
              NaryCspPrefixActionKey key;
              key.domains = {domains[first], domains[second], domains[third]};
              key.prefix.residual_domain = residual;
              for (uint32_t index = 0; index < 3; ++index)
                if (index != residual)
                  key.prefix.prefix.push_back({index, tuple[index]});
              reference.insert(std::move(key));
            }
          }
        }
    assert(!reference.empty());

    Kernel kernel = parse_formula(formula);
    FILE *records = std::tmpfile();
    assert(records);
    kernel.structural_provenance = records;
    NaryCspPrefixActionSession session;
    auto bounded = limits();
    bounded.maximum_domains = domain_count;
    auto window = offer_window();
    window.maximum_relation_candidates = 16;
    window.maximum_prefix_candidates = 100;
    window.maximum_offers = 100;
    const auto indexed =
        nary_csp_prefix_action_offer(kernel, session, bounded, window);
    std::vector<NaryCspPrefixActionSelection> expected;
    for (const auto &key : reference)
      expected.push_back({key.domains, key.prefix});
    assert(indexed.status == NaryCspPrefixActionOfferStatus::Ready &&
           indexed.complete && indexed.offers == expected &&
           indexed.effort.indexed_relation_lookahead_keys == 0 &&
           !session.retained() && !position(records));
    std::fclose(records);
  }
}

void rejected_relation_advances_to_later_index_page() {
  const std::vector<std::vector<int32_t>> formula{
      {1, 2, 3},    {-1, -2}, {-1, -3}, {-2, -3},
      {4, 5, 6},    {-4, -5}, {-4, -6}, {-5, -6},
      {7, 8, 9},    {-7, -8}, {-7, -9}, {-8, -9},
      {10, 11, 12}, {-10, -11}, {-10, -12}, {-11, -12},
      {-1, -4, -7}, {-2, -5, -8}, {-3, -6, -9}, {-4, -7, -10}};
  Kernel kernel = parse_formula(formula);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  auto bounded = limits();
  bounded.maximum_domains = 4;
  bounded.maximum_source_clauses = 13;
  auto window = offer_window();
  window.maximum_relation_candidates = 1;

  const auto rejected =
      nary_csp_prefix_action_offer(kernel, session, bounded, window);
  assert(rejected.status == NaryCspPrefixActionOfferStatus::Ready &&
         !rejected.complete && rejected.offers.empty() &&
         rejected.have_resume_after &&
         rejected.effort.relation_candidates == 1 &&
         rejected.effort.resource_rejected_relations == 1);
  window.have_after = true;
  window.after = rejected.resume_after;
  const auto later =
      nary_csp_prefix_action_offer(kernel, session, bounded, window);
  assert(later.status == NaryCspPrefixActionOfferStatus::Ready &&
         later.complete && later.offers.size() == 3 &&
         later.effort.relation_candidates == 1 &&
         !later.effort.resource_rejected_relations && !session.retained() &&
         !position(records));
  for (const auto &offer : later.offers)
    assert(offer.domains ==
           std::vector<std::vector<int32_t>>(
               {{4, 5, 6}, {7, 8, 9}, {10, 11, 12}}));
  std::fclose(records);
}

void nol_scale_index_census_starts_a_durable_page() {
  constexpr uint32_t domains = 363;
  constexpr uint32_t relations = 2486;
  std::vector<std::vector<int32_t>> formula;
  formula.reserve(domains * 4 + relations * 3);
  for (uint32_t domain = 0; domain < domains; ++domain) {
    const int32_t first = 3 * domain + 1;
    formula.push_back({first, first + 1, first + 2});
    formula.push_back({-first, -(first + 1)});
    formula.push_back({-first, -(first + 2)});
    formula.push_back({-(first + 1), -(first + 2)});
  }
  uint32_t emitted = 0;
  for (uint32_t first = 0; first < domains && emitted < relations; ++first)
    for (uint32_t second = first + 1;
         second < domains && emitted < relations; ++second)
      for (uint32_t third = second + 1;
           third < domains && emitted < relations; ++third) {
        for (uint32_t value = 0; value < 3; ++value)
          formula.push_back(
              {-static_cast<int32_t>(3 * first + value + 1),
               -static_cast<int32_t>(3 * second + value + 1),
               -static_cast<int32_t>(3 * third + value + 1)});
        ++emitted;
      }
  assert(emitted == relations);

  Kernel kernel = parse_formula(formula);
  FILE *records = std::tmpfile();
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  auto bounded = limits();
  bounded.maximum_recognition_work = 10000000;
  bounded.maximum_external_extent = 2000;
  bounded.maximum_session_keys = 30000;
  bounded.maximum_session_work = 100000000;
  auto window = offer_window();
  window.maximum_domain_candidates = 400;
  window.maximum_relation_candidates = 3000;
  window.maximum_prefix_candidates = 30000;
  window.maximum_offers = 1;
  window.maximum_enumeration_work = 1000000000;
  const auto page =
      nary_csp_prefix_action_offer(kernel, session, bounded, window);
  assert(page.status == NaryCspPrefixActionOfferStatus::Ready &&
         !page.complete && page.have_resume_after &&
         page.offers.size() == 1 && page.effort.exact_domains == domains &&
         page.effort.indexed_relation_keys == relations &&
         page.effort.indexed_tuple_occurrences == uint64_t{relations} * 3 &&
         page.effort.indexed_prefix_candidates == uint64_t{relations} * 9 &&
         !session.retained() && !position(records));
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
  propagated_root_preserves_exact_source_contract();
  guard_validation_resource_and_staleness();
  bounded_offerer_is_complete_paged_pure_and_stale_safe();
  signed_domains_are_discovered_canonically();
  relation_index_pages_clause_backed_relations_only();
  indexed_discovery_matches_combination_reference();
  rejected_relation_advances_to_later_index_page();
  nol_scale_index_census_starts_a_durable_page();
  offerer_declines_guards_bounds_and_interrupt_atomically();
  std::puts("test_nary_csp_prefix_action: passed");
}

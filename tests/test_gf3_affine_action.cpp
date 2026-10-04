#undef NDEBUG
#include "rules/semantic_extension/gf3_affine_action.hpp"

#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

namespace {

std::vector<std::vector<int32_t>> formula(bool complete = true) {
  std::vector<std::vector<int32_t>> result;
  std::vector<std::vector<int32_t>> values;
  for (int32_t domain = 0; domain < 7; ++domain) {
    const int32_t first = 3 * domain + 1;
    values.push_back({first, first + 1, first + 2});
    result.push_back(values.back());
    for (uint8_t left = 0; left < 3; ++left)
      for (uint8_t right = left + 1; right < 3; ++right)
        result.push_back({-values.back()[left], -values.back()[right]});
  }
  const auto equation = [&](const std::vector<uint32_t> &domains) {
    for (uint64_t tuple = 0; tuple < 27; ++tuple) {
      uint64_t rest = tuple;
      uint8_t sum = 0;
      std::vector<int32_t> clause;
      for (uint32_t domain : domains) {
        const uint8_t value = static_cast<uint8_t>(rest % 3);
        rest /= 3;
        sum = static_cast<uint8_t>((sum + value) % 3);
        clause.push_back(-values[domain][value]);
      }
      if (sum) result.push_back(std::move(clause));
    }
  };
  equation({0, 1, 2});
  equation({2, 3, 4});
  equation({4, 5, 6});
  if (!complete) result.pop_back();
  return result;
}

std::string dimacs(bool complete = true, bool checker_suffix = false) {
  const auto clauses = formula(complete);
  std::ostringstream output;
  output << "p cnf " << (checker_suffix ? 31 : 21) << ' '
         << clauses.size() + (checker_suffix ? 4 : 0) << '\n';
  for (const auto &clause : clauses) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  if (checker_suffix)
    output << "30 31 0\n-30 31 0\n30 -31 0\n-30 -31 0\n";
  return output.str();
}

Kernel parse(bool complete = true) {
  const std::string text = dimacs(complete);
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  return kernel;
}

Gf3AffineActionLimits limits() {
  Gf3AffineActionLimits result;
  result.recognition = {2000, 20000, 100, 100, 6, 1000};
  result.elimination = {8, 10000, 1000, 10000};
  result.maximum_recognition_work = 1000000;
  result.maximum_planning_work = 1000000;
  result.maximum_duplicate_scan_work = 1000000;
  result.maximum_revalidation_work = 1000000;
  result.maximum_proof_work = 100000;
  result.maximum_proof_additions = 1000;
  result.maximum_proof_literal_occurrences = 10000;
  result.maximum_arena_words = 10000;
  result.maximum_journal_bytes = 1000000;
  result.maximum_external_extent = 100;
  result.maximum_session_keys = 100;
  result.maximum_session_work = 10000;
  assert(result.valid());
  return result;
}

Gf3AffineActionOfferWindow offer_window() {
  return {100, 100, 1000000, false, {}};
}

Gf3AffineLiveRequest live_request(const Gf3AffineActionLimits &action,
                                  const Gf3AffineOwnerSnapshot *owner,
                                  FILE *records) {
  Gf3AffineLiveRequest result;
  result.recognition = action.recognition;
  result.elimination = action.elimination;
  result.retained_owner = owner;
  result.first_equation = 1;
  result.second_equation = 2;
  result.maximum_recognition_work = action.maximum_recognition_work;
  result.maximum_planning_work = action.maximum_planning_work;
  result.maximum_duplicate_scan_work = action.maximum_duplicate_scan_work;
  result.maximum_revalidation_work = action.maximum_revalidation_work;
  result.maximum_proof_work = action.maximum_proof_work;
  result.maximum_proof_additions = action.maximum_proof_additions;
  result.maximum_proof_literal_occurrences =
      action.maximum_proof_literal_occurrences;
  result.maximum_arena_words = action.maximum_arena_words;
  result.maximum_journal_bytes = action.maximum_journal_bytes;
  result.maximum_external_extent = action.maximum_external_extent;
  result.action_id = 1000;
  result.semantic_id = 1000;
  result.records = records;
  return result;
}

void attach(Kernel &kernel, FILE *records) {
  kernel.structural_provenance = records;
  kernel.structural_action_sequence = 80;
}

struct Image {
  uint64_t next_clause_id = 0;
  uint64_t action_sequence = 0;
  size_t arena_words = 0;
  size_t trail = 0;
  uint64_t arena_generation = 0;
  bool candidate_pending = false;
  std::vector<size_t> watches;
};

Image image(const Kernel &kernel) {
  Image result;
  result.next_clause_id = kernel.next_clause_id;
  result.action_sequence = kernel.structural_action_sequence;
  result.arena_words = kernel.arena.size();
  result.trail = kernel.trail.size();
  result.arena_generation = kernel.arena_generation;
  result.candidate_pending = kernel.semantic_variable_candidate_pending;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

bool operator==(const Image &first, const Image &second) {
  return first.next_clause_id == second.next_clause_id &&
         first.action_sequence == second.action_sequence &&
         first.arena_words == second.arena_words &&
         first.trail == second.trail &&
         first.arena_generation == second.arena_generation &&
         first.candidate_pending == second.candidate_pending &&
         first.watches == second.watches;
}

long position(FILE *file) {
  const long result = std::ftell(file);
  assert(result >= 0);
  return result;
}

void delete_published(Kernel &kernel, const std::vector<uint64_t> &ids) {
  uint64_t deleted = 0;
  for (Ref reference = 0; reference < kernel.arena.size();) {
    Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (!clause->garbage() &&
        std::find(ids.begin(), ids.end(), clause->id()) != ids.end()) {
      kernel_delete_clause(kernel, clause);
      ++deleted;
    }
  }
  assert(deleted == ids.size());
}

void success_replay_advice_and_repeated_pairs() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;

  const auto first =
      gf3_affine_action_apply(kernel, session, limits(), 0, 1);
  assert(first.status == Gf3AffineActionStatus::Applied &&
         first.result == ApplyResult::Ok);
  assert(first.proposed_action_id == 81 && first.committed_action_id == 81 &&
         first.semantic_id == 81);
  assert((first.key == Gf3AffineActionKey{0, 1}));
  assert(first.owner_retained && session.retained() &&
         session.belongs_to(kernel) && session.committed_keys() == 1);
  assert(session.snapshot() &&
         session.snapshot()->sources.size() == formula().size() &&
         session.snapshot()->theory.equations.size() == 3);
  assert(first.effort.owner_capture_work && first.effort.recognition_work &&
         first.effort.emitted_additions == 54 &&
         first.live.consequence.domains ==
             std::vector<uint32_t>({0, 1, 3, 4}));
  assert(first.semantic_candidate_ready &&
         first.semantic_candidates.size() == 12 &&
         !kernel.semantic_variable_candidate_pending);
  for (const auto &variable : first.semantic_candidates)
    assert(variable.role ==
               strategy::SemanticVariableRole::ExistingTheory &&
           variable.priority == 1);

  // Publication and propagation remain separate controller actions.
  for (int32_t selected : {1, 4, 10}) {
    kernel_assign_decision(kernel, kernel_internalize(kernel, selected));
    assert(propagate_service(kernel) == ApplyResult::Ok);
  }
  assert(kernel.value(kernel_internalize(kernel, 13)) > 0);
  while (kernel.level) kernel_backtrack(kernel, kernel.level - 1);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));

  // A retained owner can outlive online parameter reconfiguration.  Its
  // source image remains valid, so lowering a capacity below that image is a
  // retryable resource decline rather than an invalid-session diagnosis.
  auto smaller_owner_limit = limits();
  assert(session.snapshot()->sources.size() > 1);
  smaller_owner_limit.recognition.maximum_source_clauses =
      session.snapshot()->sources.size() - 1;
  const Image before_owner_limit = image(kernel);
  const long owner_limit_journal = position(records);
  // Even when the same lower cap is present, malformed retained accounting is
  // still an invalid identity rather than a capacity decline.
  auto *mutable_snapshot =
      const_cast<Gf3AffineOwnerSnapshot *>(session.snapshot());
  ++mutable_snapshot->source_literals;
  const auto corrupt = gf3_affine_action_apply(
      kernel, session, smaller_owner_limit, 1, 2);
  --mutable_snapshot->source_literals;
  assert(corrupt.status == Gf3AffineActionStatus::Invalid &&
         corrupt.live.status == Gf3AffineLiveStatus::Invalid &&
         corrupt.result == ApplyResult::NoOp &&
         corrupt.committed_keys == 1 && session.committed_keys() == 1 &&
         image(kernel) == before_owner_limit &&
         position(records) == owner_limit_journal);
  const auto owner_exhausted = gf3_affine_action_apply(
      kernel, session, smaller_owner_limit, 1, 2);
  assert(owner_exhausted.status ==
             Gf3AffineActionStatus::ResourceExhausted &&
         owner_exhausted.live.status ==
             Gf3AffineLiveStatus::ResourceExhausted &&
         owner_exhausted.result == ApplyResult::NoOp &&
         owner_exhausted.committed_keys == 1 && session.committed_keys() == 1 &&
         image(kernel) == before_owner_limit &&
         position(records) == owner_limit_journal);

  // A different caller-selected pair reuses and revalidates the exact owner.
  const auto second =
      gf3_affine_action_apply(kernel, session, limits(), 1, 2);
  assert(second.status == Gf3AffineActionStatus::Applied &&
         second.committed_action_id == 82 && second.owner_retained &&
         second.effort.owner_capture_work == 0 &&
         second.live.consequence.domains ==
             std::vector<uint32_t>({2, 3, 5, 6}));
  assert((session.committed_keys() == 2 &&
          session.keys()[0] == Gf3AffineActionKey{0, 1} &&
          session.keys()[1] == Gf3AffineActionKey{1, 2}));

  // Retained identity, rather than incidental clause liveness, suppresses the
  // replay after every clause published by this pair has been deleted.
  delete_published(kernel, second.live.published_clause_ids);
  assert(kernel.trail.empty() && kernel_propagation_fixpoint(kernel));
  auto smaller_key_limit = limits();
  smaller_key_limit.maximum_session_keys = 1;
  const Image before_key_limit = image(kernel);
  const long key_limit_journal = position(records);
  const auto key_exhausted = gf3_affine_action_apply(
      kernel, session, smaller_key_limit, 2, 1);
  assert(key_exhausted.status == Gf3AffineActionStatus::ResourceExhausted &&
         key_exhausted.live.status == Gf3AffineLiveStatus::ResourceExhausted &&
         key_exhausted.result == ApplyResult::NoOp &&
         key_exhausted.committed_keys == 2 && session.committed_keys() == 2 &&
         image(kernel) == before_key_limit &&
         position(records) == key_limit_journal);
  // Reverse parent order is the same durable semantic selection.  It consumes
  // neither an action id nor provenance bytes and never enters the live layer.
  const Image before = image(kernel);
  const long journal = position(records);
  const auto repeated =
      gf3_affine_action_apply(kernel, session, limits(), 2, 1);
  assert((repeated.status == Gf3AffineActionStatus::AlreadyPublished &&
          repeated.result == ApplyResult::NoOp &&
          repeated.key == Gf3AffineActionKey{1, 2}));
  assert(repeated.proposed_action_id == 83 && repeated.semantic_id == 83 &&
         !repeated.committed_action_id && repeated.committed_keys == 2 &&
         image(kernel) == before && position(records) == journal);
  std::fclose(records);
}

void declines_are_atomic_and_sessions_are_kernel_local() {
  {
    Kernel kernel = parse(false);
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    Gf3AffineActionSession session;
    const Image before = image(kernel);
    const auto declined =
        gf3_affine_action_apply(kernel, session, limits(), 0, 1);
    assert(declined.status == Gf3AffineActionStatus::NoStructure &&
           declined.result == ApplyResult::NoOp && !session.retained() &&
           image(kernel) == before && !position(records));
    std::fclose(records);
  }
  {
    Kernel first_kernel = parse();
    FILE *first_records = std::tmpfile();
    assert(first_records);
    attach(first_kernel, first_records);
    Gf3AffineActionSession session;
    assert(gf3_affine_action_apply(first_kernel, session, limits(), 0, 1)
               .status == Gf3AffineActionStatus::Applied);

    Kernel second_kernel = parse();
    FILE *second_records = std::tmpfile();
    assert(second_records);
    attach(second_kernel, second_records);
    const Image before = image(second_kernel);
    const auto stale =
        gf3_affine_action_apply(second_kernel, session, limits(), 1, 2);
    assert(stale.status == Gf3AffineActionStatus::Stale &&
           stale.result == ApplyResult::NoOp && image(second_kernel) == before &&
           session.belongs_to(first_kernel) && session.committed_keys() == 1 &&
           !position(second_records));
    std::fclose(second_records);
    std::fclose(first_records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    Gf3AffineActionSession session;
    const Image before = image(kernel);
    const auto invalid =
        gf3_affine_action_apply(kernel, session, limits(), 1, 1);
    assert(invalid.status == Gf3AffineActionStatus::Invalid &&
           image(kernel) == before && !session.retained() &&
           !position(records));
    auto tiny = limits();
    tiny.maximum_session_work = 1;
    const auto exhausted =
        gf3_affine_action_apply(kernel, session, tiny, 0, 1);
    assert(exhausted.status == Gf3AffineActionStatus::ResourceExhausted &&
           image(kernel) == before && !session.retained() &&
           !position(records));
    std::fclose(records);
  }
}

void retained_owner_is_revalidated_before_a_novel_pair() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;
  assert(gf3_affine_action_apply(kernel, session, limits(), 0, 1).status ==
         Gf3AffineActionStatus::Applied);
  const auto source = std::find_if(
      session.snapshot()->sources.begin(), session.snapshot()->sources.end(),
      [](const Gf3AffineLiveSource &item) {
        return item.kind == Gf3AffineSourceKind::Large;
      });
  assert(source != session.snapshot()->sources.end());
  Clause *retained = clause_at(kernel.arena, source->ref);
  assert(!retained->garbage() && retained->id() == source->native_proof_id);
  kernel_delete_clause(kernel, retained);
  const Image before = image(kernel);
  const long journal = position(records);
  const auto stale =
      gf3_affine_action_apply(kernel, session, limits(), 1, 2);
  assert(stale.status == Gf3AffineActionStatus::Stale &&
         stale.result == ApplyResult::NoOp && session.committed_keys() == 1 &&
         image(kernel) == before && position(records) == journal);
  std::fclose(records);
}

void offerer_is_exact_paged_deterministic_and_read_only() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;
  const Image before = image(kernel);
  const long journal = position(records);

  const auto first =
      gf3_affine_action_offer(kernel, session, limits(), offer_window());
  const auto repeated =
      gf3_affine_action_offer(kernel, session, limits(), offer_window());
  assert(first.status == Gf3AffineActionOfferStatus::Ready &&
         first.complete && !first.have_resume_after &&
         !first.owner_retained &&
         first.offers ==
             std::vector<Gf3AffineActionKey>({{0, 1}, {1, 2}}));
  assert(repeated.status == first.status &&
         repeated.complete == first.complete &&
         repeated.offers == first.offers &&
         repeated.effort.source_clauses == first.effort.source_clauses &&
         repeated.effort.source_literals == first.effort.source_literals &&
         repeated.effort.source_work == first.effort.source_work &&
         repeated.effort.recognition_work == first.effort.recognition_work &&
         repeated.effort.enumeration_work == first.effort.enumeration_work);
  assert(first.effort.source_clauses == formula().size() &&
         first.effort.source_literals && first.effort.equations == 3 &&
         first.effort.source_work && first.effort.recognition_work &&
         first.effort.enumeration_work && first.effort.duplicate_scan_work &&
         first.effort.pair_candidates == 3 &&
         first.effort.structurally_ineligible_pairs == 1 &&
         !first.effort.committed_pairs_skipped);
  assert(image(kernel) == before && position(records) == journal &&
         !session.retained() && !session.committed_keys());

  auto one_offer = offer_window();
  one_offer.maximum_offers = 1;
  const auto page_one =
      gf3_affine_action_offer(kernel, session, limits(), one_offer);
  assert((page_one.status == Gf3AffineActionOfferStatus::Ready &&
         !page_one.complete && page_one.have_resume_after &&
         page_one.offers == std::vector<Gf3AffineActionKey>({{0, 1}}) &&
         page_one.resume_after == Gf3AffineActionKey{0, 1}));
  one_offer.have_after = true;
  one_offer.after = page_one.resume_after;
  const auto page_two =
      gf3_affine_action_offer(kernel, session, limits(), one_offer);
  assert(page_two.status == Gf3AffineActionOfferStatus::Ready &&
         page_two.complete && !page_two.have_resume_after &&
         page_two.offers == std::vector<Gf3AffineActionKey>({{1, 2}}));
  auto late_exhaustion = offer_window();
  late_exhaustion.maximum_enumeration_work =
      page_one.effort.enumeration_work + 1;
  const auto preserved =
      gf3_affine_action_offer(kernel, session, limits(), late_exhaustion);
  assert((preserved.status == Gf3AffineActionOfferStatus::Ready &&
          !preserved.complete && preserved.have_resume_after &&
          preserved.resume_after == Gf3AffineActionKey{0, 1} &&
          preserved.offers ==
              std::vector<Gf3AffineActionKey>({{0, 1}})));
  one_offer.after = {1, 2};
  const auto after_final =
      gf3_affine_action_offer(kernel, session, limits(), one_offer);
  assert(after_final.status == Gf3AffineActionOfferStatus::Ready &&
         after_final.complete && !after_final.have_resume_after &&
         after_final.offers.empty());

  auto one_candidate = offer_window();
  one_candidate.maximum_pair_candidates = 1;
  const auto bounded =
      gf3_affine_action_offer(kernel, session, limits(), one_candidate);
  assert((bounded.status == Gf3AffineActionOfferStatus::Ready &&
         !bounded.complete && bounded.have_resume_after &&
         bounded.resume_after == Gf3AffineActionKey{0, 1} &&
         bounded.effort.pair_candidates == 1 &&
         bounded.offers == std::vector<Gf3AffineActionKey>({{0, 1}})));
  assert(image(kernel) == before && position(records) == journal &&
         !session.retained());
  std::fclose(records);
}

void offerer_reuses_owner_and_suppresses_committed_pairs() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;
  assert(gf3_affine_action_apply(kernel, session, limits(), 0, 1).status ==
         Gf3AffineActionStatus::Applied);
  const size_t keys = session.committed_keys();
  const Image before = image(kernel);
  const long journal = position(records);
  const auto offered =
      gf3_affine_action_offer(kernel, session, limits(), offer_window());
  assert(offered.status == Gf3AffineActionOfferStatus::Ready &&
         offered.complete && offered.owner_retained &&
         offered.offers == std::vector<Gf3AffineActionKey>({{1, 2}}) &&
         offered.effort.committed_pairs_skipped == 1 &&
         offered.effort.recognition_work &&
         offered.effort.revalidation_work);
  assert(image(kernel) == before && position(records) == journal &&
         session.committed_keys() == keys && session.belongs_to(kernel));

  auto full_session = limits();
  full_session.maximum_session_keys = session.committed_keys();
  const auto capacity = gf3_affine_action_offer(
      kernel, session, full_session, offer_window());
  assert(capacity.status == Gf3AffineActionOfferStatus::ResourceExhausted &&
         capacity.offers.empty() && !capacity.complete);

  auto one_candidate = offer_window();
  one_candidate.maximum_pair_candidates = 1;
  const auto committed_page =
      gf3_affine_action_offer(kernel, session, limits(), one_candidate);
  assert((committed_page.status == Gf3AffineActionOfferStatus::Ready &&
          !committed_page.complete && committed_page.have_resume_after &&
          committed_page.offers.empty() &&
          committed_page.resume_after == Gf3AffineActionKey{0, 1} &&
          committed_page.effort.committed_pairs_skipped == 1));
  one_candidate.have_after = true;
  one_candidate.after = committed_page.resume_after;
  const auto structural_page =
      gf3_affine_action_offer(kernel, session, limits(), one_candidate);
  assert((structural_page.status == Gf3AffineActionOfferStatus::Ready &&
          !structural_page.complete && structural_page.have_resume_after &&
          structural_page.offers.empty() &&
          structural_page.resume_after == Gf3AffineActionKey{0, 2} &&
          structural_page.effort.structurally_ineligible_pairs == 1));
  one_candidate.after = structural_page.resume_after;
  const auto productive_page =
      gf3_affine_action_offer(kernel, session, limits(), one_candidate);
  assert(productive_page.status == Gf3AffineActionOfferStatus::Ready &&
         productive_page.complete && !productive_page.have_resume_after &&
         productive_page.offers ==
             std::vector<Gf3AffineActionKey>({{1, 2}}));
  assert(image(kernel) == before && position(records) == journal &&
         session.committed_keys() == keys);

  const auto source = std::find_if(
      session.snapshot()->sources.begin(), session.snapshot()->sources.end(),
      [](const Gf3AffineLiveSource &item) {
        return item.kind == Gf3AffineSourceKind::Large;
      });
  assert(source != session.snapshot()->sources.end());
  kernel_delete_clause(kernel, clause_at(kernel.arena, source->ref));
  const Image stale_before = image(kernel);
  const long stale_journal = position(records);
  const auto stale =
      gf3_affine_action_offer(kernel, session, limits(), offer_window());
  assert(stale.status == Gf3AffineActionOfferStatus::Stale &&
         stale.offers.empty() && !stale.complete &&
         stale.effort.revalidation_work);
  assert(image(kernel) == stale_before && position(records) == stale_journal &&
         session.committed_keys() == keys);
  std::fclose(records);

  Kernel other = parse();
  FILE *other_records = std::tmpfile();
  assert(other_records);
  attach(other, other_records);
  const Image other_before = image(other);
  const auto wrong_owner =
      gf3_affine_action_offer(other, session, limits(), offer_window());
  assert(wrong_owner.status == Gf3AffineActionOfferStatus::Stale &&
         wrong_owner.offers.empty() && image(other) == other_before &&
         !position(other_records));
  std::fclose(other_records);
}

void offerer_declines_negative_bounds_and_interrupt_atomically() {
  {
    Kernel kernel = parse(false);
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    Gf3AffineActionSession session;
    const Image before = image(kernel);
    const auto no_structure =
        gf3_affine_action_offer(kernel, session, limits(), offer_window());
    assert(no_structure.status == Gf3AffineActionOfferStatus::NoStructure &&
           no_structure.offers.empty() && image(kernel) == before &&
           !position(records) && !session.retained());
    std::fclose(records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    Gf3AffineActionSession session;
    const Image before = image(kernel);
    auto recognition_bound = limits();
    recognition_bound.maximum_recognition_work = 1;
    const auto recognition_exhausted = gf3_affine_action_offer(
        kernel, session, recognition_bound, offer_window());
    assert(recognition_exhausted.status ==
               Gf3AffineActionOfferStatus::ResourceExhausted &&
           recognition_exhausted.offers.empty());
    auto enumeration_bound = offer_window();
    enumeration_bound.maximum_enumeration_work = 1;
    const auto enumeration_exhausted = gf3_affine_action_offer(
        kernel, session, limits(), enumeration_bound);
    assert(enumeration_exhausted.status ==
               Gf3AffineActionOfferStatus::ResourceExhausted &&
           enumeration_exhausted.offers.empty() &&
           enumeration_exhausted.effort.enumeration_work == 1);
    auto pair_bound = limits();
    pair_bound.elimination.maximum_result_arity = 3;
    const auto locally_rejected = gf3_affine_action_offer(
        kernel, session, pair_bound, offer_window());
    assert(locally_rejected.status ==
               Gf3AffineActionOfferStatus::ResourceExhausted &&
           !locally_rejected.complete && locally_rejected.offers.empty() &&
           locally_rejected.effort.pair_candidates == 3 &&
           locally_rejected.effort.resource_rejected_pairs == 2 &&
           locally_rejected.effort.structurally_ineligible_pairs == 1);
    auto rejected_page = offer_window();
    rejected_page.maximum_pair_candidates = 1;
    const auto resumable_rejection = gf3_affine_action_offer(
        kernel, session, pair_bound, rejected_page);
    assert((resumable_rejection.status == Gf3AffineActionOfferStatus::Ready &&
            !resumable_rejection.complete &&
            resumable_rejection.have_resume_after &&
            resumable_rejection.offers.empty() &&
           resumable_rejection.resume_after == Gf3AffineActionKey{0, 1} &&
            resumable_rejection.effort.resource_rejected_pairs == 1));
    auto journal_bound = limits();
    journal_bound.maximum_journal_bytes = 1;
    const auto journal_rejected = gf3_affine_action_offer(
        kernel, session, journal_bound, offer_window());
    assert(journal_rejected.status ==
               Gf3AffineActionOfferStatus::ResourceExhausted &&
           journal_rejected.offers.empty() &&
           journal_rejected.effort.resource_rejected_pairs == 2);
    const auto journal_apply =
        gf3_affine_action_apply(kernel, session, journal_bound, 0, 1);
    assert(journal_apply.status == Gf3AffineActionStatus::ResourceExhausted &&
           journal_apply.result == ApplyResult::NoOp);
    for (uint32_t cap = 0; cap < 7; ++cap) {
      auto bounded = limits();
      if (cap == 0) bounded.maximum_recognition_work = 1;
      if (cap == 1) bounded.maximum_planning_work = 1;
      if (cap == 2) bounded.maximum_duplicate_scan_work = 1;
      if (cap == 3) bounded.maximum_revalidation_work = 1;
      if (cap == 4) bounded.maximum_proof_work = 1;
      if (cap == 5) bounded.maximum_proof_additions = 1;
      if (cap == 6) bounded.maximum_proof_literal_occurrences = 1;
      const auto rejected = gf3_affine_action_offer(
          kernel, session, bounded, offer_window());
      assert(rejected.status ==
                 Gf3AffineActionOfferStatus::ResourceExhausted &&
             rejected.offers.empty());
      const auto apply =
          gf3_affine_action_apply(kernel, session, bounded, 0, 1);
      assert(apply.status == Gf3AffineActionStatus::ResourceExhausted &&
             apply.result == ApplyResult::NoOp);
    }
    auto arena_bound = limits();
    arena_bound.maximum_arena_words = 1;
    const auto arena_rejected = gf3_affine_action_offer(
        kernel, session, arena_bound, offer_window());
    assert(arena_rejected.status ==
               Gf3AffineActionOfferStatus::ResourceExhausted &&
           arena_rejected.offers.empty());
    const auto arena_apply =
        gf3_affine_action_apply(kernel, session, arena_bound, 0, 1);
    assert(arena_apply.status == Gf3AffineActionStatus::ResourceExhausted &&
           arena_apply.result == ApplyResult::NoOp);
    auto extent_bound = limits();
    extent_bound.maximum_external_extent =
        static_cast<uint32_t>(kernel.external_to_internal.size() - 1);
    const auto extent_rejected = gf3_affine_action_offer(
        kernel, session, extent_bound, offer_window());
    assert(extent_rejected.status ==
               Gf3AffineActionOfferStatus::ResourceExhausted &&
           extent_rejected.offers.empty());
    auto invalid_window = offer_window();
    invalid_window.have_after = true;
    invalid_window.after = {1, 1};
    const auto invalid = gf3_affine_action_offer(
        kernel, session, limits(), invalid_window);
    assert(invalid.status == Gf3AffineActionOfferStatus::Invalid &&
           invalid.offers.empty());
    assert(image(kernel) == before && !position(records) &&
           !session.retained());

    volatile sig_atomic_t stop = 1;
    KernelStopPublisher publisher(kernel, &stop);
    const auto interrupted =
        gf3_affine_action_offer(kernel, session, limits(), offer_window());
    assert(interrupted.status == Gf3AffineActionOfferStatus::Interrupted &&
           interrupted.offers.empty() && image(kernel) == before &&
           !position(records) && !session.retained());
    std::fclose(records);
  }
}

void offerer_suppresses_an_uncommitted_live_duplicate() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;
  assert(gf3_affine_action_apply(kernel, session, limits(), 0, 1).status ==
         Gf3AffineActionStatus::Applied);
  assert(session.committed_keys() == 1);

  // Publish the other productive consequence directly through the same live
  // adapter.  This deliberately leaves its public action key uncommitted and
  // isolates live-clause duplicate suppression from durable-key suppression.
  const Gf3AffineLiveResult published = gf3_affine_live_apply(
      kernel, live_request(limits(), session.snapshot(), records));
  assert(published.status == Gf3AffineLiveStatus::Applied &&
         published.result == ApplyResult::Ok &&
         session.committed_keys() == 1);
  const Image before = image(kernel);
  const long journal = position(records);
  const auto offered =
      gf3_affine_action_offer(kernel, session, limits(), offer_window());
  assert(offered.status == Gf3AffineActionOfferStatus::Ready &&
         offered.complete && offered.offers.empty() &&
         offered.effort.committed_pairs_skipped == 1 &&
         offered.effort.structurally_ineligible_pairs == 1 &&
         offered.effort.duplicate_pairs_skipped == 1 &&
         offered.effort.duplicate_scan_work);
  assert(image(kernel) == before && position(records) == journal &&
         session.committed_keys() == 1);
  std::fclose(records);
}

void proof_modes_and_guard() {
  for (const auto &[format, path] :
       {std::pair{ProofFormat::Lrat, "/tmp/gf3-action-decline.lrat"},
        std::pair{ProofFormat::Pbp, "/tmp/gf3-action-decline.pbp"}}) {
    Kernel kernel = parse();
    Proof proof;
    std::remove(path);
    if (format == ProofFormat::Pbp) {
      assert(proof.open(path, ProofFormat::Automatic));
      proof.finish_input_transaction();
      assert(proof.begin_pb());
    } else {
      assert(proof.open(path, format));
    }
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    FILE *records = std::tmpfile();
    assert(records);
    attach(kernel, records);
    Gf3AffineActionSession session;
    const Image before = image(kernel);
    const auto result =
        gf3_affine_action_apply(kernel, session, limits(), 0, 1);
    assert(result.status == Gf3AffineActionStatus::Inapplicable &&
           result.result == ApplyResult::NoOp && image(kernel) == before &&
           !session.retained() && !position(records));
    kernel.proof = nullptr;
    std::fclose(records);
  }
}

void write_certificate(const char *cnf_path, const char *proof_path,
                       ProofFormat format) {
  const std::string checker = dimacs(true, true);
  FILE *cnf = std::fopen(cnf_path, "w");
  assert(cnf);
  assert(std::fwrite(checker.data(), 1, checker.size(), cnf) == checker.size());
  std::fclose(cnf);

  Kernel kernel = parse();
  kernel.next_clause_id = formula().size() + 4;
  Proof proof;
  assert(proof.open(proof_path, format));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *records = std::tmpfile();
  assert(records);
  attach(kernel, records);
  Gf3AffineActionSession session;
  const auto result =
      gf3_affine_action_apply(kernel, session, limits(), 0, 1);
  assert(result.status == Gf3AffineActionStatus::Applied &&
         result.effort.emitted_additions == 54);
  const int32_t unit = 30;
  assert(proof.add_external_rup(++kernel.next_clause_id, &unit, 1));
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  success_replay_advice_and_repeated_pairs();
  declines_are_atomic_and_sessions_are_kernel_local();
  retained_owner_is_revalidated_before_a_novel_pair();
  offerer_is_exact_paged_deterministic_and_read_only();
  offerer_reuses_owner_and_suppresses_committed_pairs();
  offerer_declines_negative_bounds_and_interrupt_atomically();
  offerer_suppresses_an_uncommitted_live_duplicate();
  proof_modes_and_guard();
  if (argc == 4) {
    write_certificate(argv[1], argv[2], ProofFormat::DsrAscii);
    write_certificate(argv[1], argv[3], ProofFormat::DratAscii);
  }
  std::puts("test_gf3_affine_action: passed");
}

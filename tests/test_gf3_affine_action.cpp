#include "unit_test.hpp"
#include "gf3_formula_support.hpp"
#include "kernel_image.hpp"
#include "test_io.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"

namespace {

// Seven domains in a chain of three equations.
Gf3Clauses formula() {
  return gf3_formula(7, {{0, 1, 2}, {2, 3, 4}, {4, 5, 6}});
}

Kernel parse() { return gf3_parsed_kernel(formula()); }

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

// A parsed kernel with a fresh journal and session; action ids start at 81.
struct Bound {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  Gf3AffineActionSession session;
  Bound() {
    assert(records);
    kernel.structural_provenance = records;
    kernel.structural_action_sequence = 80;
  }
  ~Bound() { std::fclose(records); }
};

// The kernel image plus the action-layer state a decline must not touch.
struct ActionImage {
  Image kernel;
  uint64_t action_sequence = 0;
  bool candidate_pending = false;
  bool operator==(const ActionImage &) const = default;
};

ActionImage action_image(const Kernel &kernel) {
  return {image(kernel), kernel.structural_action_sequence,
          kernel.semantic_variable_candidate_pending};
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

TEST(success_replay_advice_and_repeated_pairs) {
  Bound b;
  Kernel &kernel = b.kernel;
  FILE *records = b.records;
  Gf3AffineActionSession &session = b.session;

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

  // A retained owner can outlive online parameter reconfiguration.  Its
  // source image remains valid, so lowering a capacity below that image is a
  // retryable resource decline rather than an invalid-session diagnosis.
  auto smaller_owner_limit = limits();
  assert(session.snapshot()->sources.size() > 1);
  smaller_owner_limit.recognition.maximum_source_clauses =
      session.snapshot()->sources.size() - 1;
  const ActionImage before_owner_limit = action_image(kernel);
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
         action_image(kernel) == before_owner_limit &&
         position(records) == owner_limit_journal);
  const auto owner_exhausted = gf3_affine_action_apply(
      kernel, session, smaller_owner_limit, 1, 2);
  assert(owner_exhausted.status ==
             Gf3AffineActionStatus::ResourceExhausted &&
         owner_exhausted.live.status ==
             Gf3AffineLiveStatus::ResourceExhausted &&
         owner_exhausted.result == ApplyResult::NoOp &&
         owner_exhausted.committed_keys == 1 && session.committed_keys() == 1 &&
         action_image(kernel) == before_owner_limit &&
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
  const ActionImage before_key_limit = action_image(kernel);
  const long key_limit_journal = position(records);
  const auto key_exhausted = gf3_affine_action_apply(
      kernel, session, smaller_key_limit, 2, 1);
  assert(key_exhausted.status == Gf3AffineActionStatus::ResourceExhausted &&
         key_exhausted.live.status == Gf3AffineLiveStatus::ResourceExhausted &&
         key_exhausted.result == ApplyResult::NoOp &&
         key_exhausted.committed_keys == 2 && session.committed_keys() == 2 &&
         action_image(kernel) == before_key_limit &&
         position(records) == key_limit_journal);
  // Reverse parent order is the same durable semantic selection.  It consumes
  // neither an action id nor provenance bytes and never enters the live layer.
  const ActionImage before = action_image(kernel);
  const long journal = position(records);
  const auto repeated =
      gf3_affine_action_apply(kernel, session, limits(), 2, 1);
  assert((repeated.status == Gf3AffineActionStatus::AlreadyPublished &&
          repeated.result == ApplyResult::NoOp &&
          repeated.key == Gf3AffineActionKey{1, 2}));
  assert(repeated.proposed_action_id == 83 && repeated.semantic_id == 83 &&
         !repeated.committed_action_id && repeated.committed_keys == 2 &&
         action_image(kernel) == before && position(records) == journal);
}

TEST(declines_are_atomic_and_sessions_are_kernel_local) {
  {
    Bound first, second;
    assert(gf3_affine_action_apply(first.kernel, first.session, limits(), 0, 1)
               .status == Gf3AffineActionStatus::Applied);
    const ActionImage before = action_image(second.kernel);
    const auto stale = gf3_affine_action_apply(second.kernel, first.session,
                                               limits(), 1, 2);
    assert(stale.status == Gf3AffineActionStatus::Stale &&
           stale.result == ApplyResult::NoOp &&
           action_image(second.kernel) == before &&
           first.session.belongs_to(first.kernel) &&
           first.session.committed_keys() == 1 && !position(second.records));
  }
  Bound b;
  const ActionImage before = action_image(b.kernel);
  const auto unchanged = [&] {
    assert(action_image(b.kernel) == before && !b.session.retained() &&
           !position(b.records));
  };
  assert(gf3_affine_action_apply(b.kernel, b.session, limits(), 1, 1).status ==
         Gf3AffineActionStatus::Invalid);
  unchanged();
  auto tiny = limits();
  tiny.maximum_session_work = 1;
  assert(gf3_affine_action_apply(b.kernel, b.session, tiny, 0, 1).status ==
         Gf3AffineActionStatus::ResourceExhausted);
  unchanged();
  // The action guard declines streaming PBP before the live layer runs.
  Proof proof;
  bind_pbp_streaming(proof, b.kernel, "/tmp/gf3-action-decline.pbp");
  const auto result = gf3_affine_action_apply(b.kernel, b.session, limits(), 0, 1);
  assert(result.status == Gf3AffineActionStatus::Inapplicable &&
         result.result == ApplyResult::NoOp);
  unchanged();
  b.kernel.proof = nullptr;
}

} // namespace

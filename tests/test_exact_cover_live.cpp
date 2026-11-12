#undef NDEBUG
#include "exact_cover_test_support.hpp"
#include "rules/semantic_extension/exact_cover_live.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <string>
#include <vector>

using namespace tactix;

static Kernel parse(bool extra = false) { return exact_cover_kernel(extra); }

static ExactCoverLiveRequest request(FILE *records,
                                     ExactCoverLiveSession &session) {
  ExactCoverLiveRequest result;
  result.arity = 3;
  result.maximum_items = 100;
  result.maximum_source_clauses = 100;
  result.maximum_source_literals = 1000;
  result.maximum_recognition_work = 100000;
  result.maximum_premise_references = 1000;
  result.maximum_definitions = 10;
  result.maximum_additions = 100;
  result.maximum_literal_occurrences = 1000;
  result.maximum_journal_bytes = 100000;
  result.maximum_extent_growth = 10;
  result.maximum_external_extent = 1000;
  result.action_id = 77;
  result.records = records;
  result.session = &session;
  return result;
}

static void positive_and_repeat() {
  Kernel kernel = parse();
  ExactCoverLiveSession session;
  FILE *records = std::tmpfile();
  assert(records);
  const auto result = exact_cover_live_apply(kernel, request(records, session));
  assert(result.status == ExactCoverLiveStatus::Applied);
  assert(result.result == ApplyResult::Ok);
  assert(result.source_clauses == 15 && result.source_literals == 36);
  assert(result.definitions == 3 && result.additions == 12);
  assert(result.literal_occurrences == 30 && result.extent_growth == 3);
  assert(result.revalidation_work > 0);
  assert(result.journal_bytes > 0);
  assert(result.introduced_theory_variables ==
         std::vector<uint32_t>({10, 11, 12}));
  assert(kernel.nvars == 12 && kernel_num_output_vars(kernel) == 9);
  assert(session.committed_keys() == 3);
  assert(session.keys() == std::vector<std::vector<int32_t>>(
                               {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}));
  assert(std::ftell(records) > 0);
  const std::string journal = stream_contents(records);
  // Proofless kernels name binaries by literals, while large source
  // clauses retain their native identities. Both cases remain explicit.
  assert(journal.find("\"native_proof_id\":0") != std::string::npos);
  assert(journal.find("\"native_proof_id\":10") != std::string::npos);

  FILE *repeat = std::tmpfile();
  assert(repeat);
  const auto before_map = kernel.external_to_internal;
  const uint64_t before_id = kernel.next_clause_id;
  auto repeated = request(repeat, session);
  repeated.action_id = 78;
  const auto repeated_result = exact_cover_live_apply(kernel, repeated);
  assert(repeated_result.result == ApplyResult::NoOp);
  assert(repeated_result.status == ExactCoverLiveStatus::NoStructure);
  assert(kernel.external_to_internal == before_map &&
         kernel.next_clause_id == before_id && std::ftell(repeat) == 0);
  std::fclose(repeat);
  std::fclose(records);
}

static void final_revalidation_obeys_recognition_limit() {
  uint64_t complete_work = 0;
  {
    Kernel kernel = parse();
    ExactCoverLiveSession session;
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = exact_cover_live_apply(kernel, request(records, session));
    assert(result.status == ExactCoverLiveStatus::Applied);
    assert(result.revalidation_work > 0);
    complete_work = result.recognition_work;
    std::fclose(records);
  }
  assert(complete_work > 1);

  Kernel kernel = parse();
  ExactCoverLiveSession session;
  FILE *records = std::tmpfile();
  assert(records);
  auto limited = request(records, session);
  limited.maximum_recognition_work = complete_work - 1;
  const ExactCoverUnchanged unchanged{kernel, records};
  const auto result = exact_cover_live_apply(kernel, limited);
  assert(result.status == ExactCoverLiveStatus::ResourceExhausted);
  assert(result.result == ApplyResult::NoOp);
  assert(result.revalidation_work > 0);
  assert(session.committed_keys() == 0);
  unchanged.check();
  std::fclose(records);
}

// `change` makes the request decline with `expected`; nothing is published.
template <class Change>
static ExactCoverLiveResult declines(bool extra, ExactCoverLiveStatus expected,
                                     Change change) {
  Kernel kernel = parse(extra);
  ExactCoverLiveSession session;
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records, session);
  change(kernel, candidate);
  const ExactCoverUnchanged unchanged{kernel, records};
  const auto result = exact_cover_live_apply(kernel, candidate);
  assert(result.result == ApplyResult::NoOp && result.status == expected);
  unchanged.check();
  assert(session.committed_keys() == 0);
  std::fclose(records);
  return result;
}

static void clean_declines() {
  using Request = ExactCoverLiveRequest;
  declines(true, ExactCoverLiveStatus::NoStructure, [](Kernel &, Request &) {});
  // Each bound one below what the fixture needs.
  const std::vector<void (*)(Request &)> bounds = {
      [](Request &r) { r.maximum_recognition_work = 1; },
      [](Request &r) { r.maximum_source_clauses = 14; },
      [](Request &r) { r.maximum_source_literals = 35; },
      [](Request &r) { r.maximum_items = 8; },
      [](Request &r) { r.maximum_premise_references = 26; },
      [](Request &r) { r.maximum_definitions = 2; },
      [](Request &r) { r.maximum_additions = 11; },
      [](Request &r) { r.maximum_literal_occurrences = 29; },
      [](Request &r) { r.maximum_extent_growth = 2; },
      [](Request &r) { r.maximum_external_extent = 11; },
      [](Request &r) { r.maximum_journal_bytes = 1; },
  };
  for (size_t i = 0; i < bounds.size(); ++i) {
    const auto result = declines(false, ExactCoverLiveStatus::ResourceExhausted,
                                 [&](Kernel &, Request &r) { bounds[i](r); });
    if (i == 0) assert(result.recognition_work > 0);
  }
  volatile sig_atomic_t stop = 1;
  declines(false, ExactCoverLiveStatus::Interrupted,
           [&](Kernel &kernel, Request &) { kernel.stop = &stop; });
}

int main() {
  positive_and_repeat();
  final_revalidation_obeys_recognition_limit();
  clean_declines();
  std::puts("test_exact_cover_live: passed");
}

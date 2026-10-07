#undef NDEBUG
#include "rules/semantic_extension/nary_csp_prefix_live.hpp"

#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "policies/random_controller_policy.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/nary_csp_prefix_action.hpp"
#include "strategy/semantic_variable_advice.hpp"

#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

namespace {

std::vector<std::vector<int32_t>> clauses(bool complete_row = false,
                                          bool omit_amo = false) {
  std::vector<std::vector<int32_t>> result;
  for (const std::vector<int32_t> &domain :
       {std::vector<int32_t>{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}) {
    result.push_back(domain);
    for (size_t i = 0; i < domain.size(); ++i)
      for (size_t j = i + 1; j < domain.size(); ++j)
        if (!(omit_amo && domain.front() == 1 && i == 0 && j == 1))
          result.push_back({-domain[i], -domain[j]});
  }
  result.push_back({-1, -4, -7});
  result.push_back({-2, -5, -8});
  result.push_back({-3, -6, -9});
  if (complete_row) {
    result.push_back({-1, -4, -8});
    result.push_back({-1, -4, -9});
  }
  return result;
}

std::string dimacs(bool complete_row = false, bool omit_amo = false) {
  const auto formula = clauses(complete_row, omit_amo);
  std::ostringstream output;
  output << "p cnf 9 " << formula.size() << '\n';
  for (const auto &clause : formula) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  return output.str();
}

Kernel parse(bool complete_row = false, bool omit_amo = false) {
  const std::string text = dimacs(complete_row, omit_amo);
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

NaryCspPrefixLiveRequest request(FILE *records) {
  NaryCspPrefixLiveRequest result;
  result.domains = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
  result.selection = {2, {{0, 0}, {1, 0}}};
  result.maximum_source_clauses = 100;
  result.maximum_source_literals = 1000;
  result.maximum_recognition_work = 100000;
  result.maximum_domains = 8;
  result.maximum_domain_values = 16;
  result.maximum_forbidden_tuples = 100;
  result.maximum_prefix_arity = 7;
  result.maximum_definition_clause_literals = 16;
  result.maximum_proof_work = 1000;
  result.maximum_proof_additions = 32;
  result.maximum_proof_literal_occurrences = 200;
  result.maximum_journal_bytes = 100000;
  result.maximum_extent_growth = 1;
  result.maximum_external_extent = 100;
  result.action_id = 81;
  result.semantic_id = 101;
  result.records = records;
  return result;
}

struct Image {
  uint32_t nvars = 0;
  uint64_t next_id = 0;
  size_t arena = 0;
  size_t extent = 0;
  size_t trail = 0;
  uint64_t generation = 0;
  std::vector<Var> internal_to_external;
  std::vector<Var> external_to_internal;
  std::vector<size_t> watches;

  bool operator==(const Image &) const = default;
};

Image image(const Kernel &kernel) {
  Image result;
  result.nvars = kernel.nvars;
  result.next_id = kernel.next_clause_id;
  result.arena = kernel.arena.size();
  result.extent = kernel.external_to_internal.size();
  result.trail = kernel.trail.size();
  result.generation = kernel.arena_generation;
  result.internal_to_external = kernel.internal_to_external;
  result.external_to_internal = kernel.external_to_internal;
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal)
    result.watches.push_back(kernel.watches.size(literal));
  return result;
}

NaryCspPrefixLiveResult decline_unchanged(
    Kernel &kernel, const NaryCspPrefixLiveRequest &candidate) {
  const Image before = image(kernel);
  const long journal = std::ftell(candidate.records);
  assert(journal >= 0);
  const auto result = nary_csp_prefix_live_apply(kernel, candidate);
  assert(result.result == ApplyResult::NoOp);
  assert(result.status != NaryCspPrefixLiveStatus::Applied);
  assert(!result.emitted_additions && !result.first_published_clause_id &&
         !result.last_published_clause_id &&
         result.published_consequences.empty() &&
         result.semantic_variables.empty());
  assert(image(kernel) == before);
  assert(std::ftell(candidate.records) == journal);
  return result;
}

bool large_clause_live(const Kernel &kernel,
                       std::vector<int32_t> wanted) {
  std::sort(wanted.begin(), wanted.end());
  for (Ref reference = 0; reference < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, reference);
    reference += clause_actual_words(clause);
    if (clause->garbage() || clause->size != wanted.size()) continue;
    std::vector<int32_t> got;
    for (uint32_t i = 0; i < clause->size; ++i)
      got.push_back(kernel_externalize(kernel, clause->lits[i]));
    std::sort(got.begin(), got.end());
    if (got == wanted) return true;
  }
  return false;
}

void capture_preflight_and_publish() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records);
  NaryCspPrefixOwnerSnapshot owner;
  const auto captured =
      nary_csp_prefix_live_capture_owner(kernel, candidate, owner);
  assert(captured.status == NaryCspPrefixLiveStatus::Applied);
  assert(captured.source_clauses == 15 && owner.source.size() == 15);
  assert(owner.relation.domains.size() == 3);
  assert(owner.relation.forbidden.size() == 3);
  assert(owner.relation.forbidden[0].value_indices ==
         std::vector<uint32_t>({0, 0, 0}));

  candidate.retained_owner = &owner;
  candidate.domains.clear();
  const Image before = image(kernel);
  const long before_journal = std::ftell(records);
  const auto preflight = nary_csp_prefix_live_preflight(kernel, candidate);
  assert(preflight.status == NaryCspPrefixLiveStatus::Applied);
  assert(preflight.planned_additions == 4 && preflight.extent_growth == 1);
  assert(image(kernel) == before && std::ftell(records) == before_journal);

  const auto applied = nary_csp_prefix_live_apply(kernel, candidate);
  assert(applied.status == NaryCspPrefixLiveStatus::Applied);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.emitted_additions == 4 && applied.extent_growth == 1);
  assert(applied.first_published_clause_id + 3 ==
         applied.last_published_clause_id);
  assert(kernel.nvars == 10);
  assert(applied.published_consequences ==
         std::vector<std::vector<int32_t>>({{-10, -7}}));
  assert(applied.semantic_variables.size() == 4);
  assert(applied.semantic_variables.back().literal == 10);
  assert(applied.semantic_variables.back().role ==
         NaryCspSemanticRole::PrefixConjunction);
  assert(applied.semantic_variables.back().definition_inputs ==
         std::vector<int32_t>({1, 4}));
  assert(kernel_binary_live(kernel, kernel_internalize(kernel, -10),
                            kernel_internalize(kernel, 1)));
  assert(kernel_binary_live(kernel, kernel_internalize(kernel, -10),
                            kernel_internalize(kernel, 4)));
  assert(kernel_binary_live(kernel, kernel_internalize(kernel, -10),
                            kernel_internalize(kernel, -7)));
  assert(large_clause_live(kernel, {10, -1, -4}));
  assert(!applied.propagation_pending);
  assert(std::ftell(records) > 0);
  std::rewind(records);
  std::string journal(8192, '\0');
  const size_t bytes = std::fread(journal.data(), 1, journal.size(), records);
  journal.resize(bytes);
  assert(journal.find("nary-csp-prefix-begin") != std::string::npos);
  assert(journal.find("nary-csp-prefix-selection") != std::string::npos);
  assert(journal.find("matching_forbidden_premises") != std::string::npos);
  assert(journal.find("omit-exact-and-extension") != std::string::npos);

  // The retained source remains valid after the first conservative extension;
  // a second prefix gets the next fresh helper instead of staling on namespace
  // growth or treating the first helper clauses as source tuples.
  FILE *second_records = std::tmpfile();
  assert(second_records);
  candidate.records = second_records;
  ++candidate.action_id;
  ++candidate.semantic_id;
  candidate.selection = {2, {{0, 1}, {1, 1}}};
  const auto second = nary_csp_prefix_live_apply(kernel, candidate);
  assert(second.status == NaryCspPrefixLiveStatus::Applied);
  assert(second.semantic_variables.back().literal == 11);
  assert(second.published_consequences ==
         std::vector<std::vector<int32_t>>({{-11, -8}}));
  assert(kernel.nvars == 11);
  std::fclose(second_records);
  std::fclose(records);
}

void complete_row_publishes_unit_and_pending_propagation() {
  Kernel kernel = parse(true);
  FILE *records = std::tmpfile();
  assert(records);
  const auto applied = nary_csp_prefix_live_apply(kernel, request(records));
  assert(applied.status == NaryCspPrefixLiveStatus::Applied);
  assert(applied.emitted_additions == 7);
  assert(applied.published_consequences.back() ==
         std::vector<int32_t>({-10}));
  assert(kernel.value(kernel_internalize(kernel, -10)) > 0);
  assert(applied.propagation_pending);
  std::fclose(records);
}

void clean_declines_and_stale_owner() {
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    auto candidate = request(records);
    candidate.selection = {2, {{0, 0}, {1, 1}}};
    const auto result = decline_unchanged(kernel, candidate);
    assert(result.status == NaryCspPrefixLiveStatus::NoConsequence);
    std::fclose(records);
  }
  {
    Kernel kernel = parse(false, true);
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = decline_unchanged(kernel, request(records));
    assert(result.status == NaryCspPrefixLiveStatus::NoStructure);
    std::fclose(records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    auto candidate = request(records);
    NaryCspPrefixOwnerSnapshot owner;
    assert(nary_csp_prefix_live_capture_owner(kernel, candidate, owner).status ==
           NaryCspPrefixLiveStatus::Applied);
    const Lit extra[] = {kernel_internalize(kernel, -1),
                         kernel_internalize(kernel, -4),
                         kernel_internalize(kernel, -8)};
    kernel_add_large_clause(kernel, extra, 3, false, 0,
                            ++kernel.next_clause_id);
    candidate.retained_owner = &owner;
    candidate.domains.clear();
    const auto result = decline_unchanged(kernel, candidate);
    assert(result.status == NaryCspPrefixLiveStatus::Stale);
    std::fclose(records);
  }
  {
    Kernel kernel = parse();
    FILE *records = std::tmpfile();
    assert(records);
    auto candidate = request(records);
    candidate.maximum_proof_additions = 3;
    const auto result = decline_unchanged(kernel, candidate);
    assert(result.status == NaryCspPrefixLiveStatus::ResourceExhausted);
    std::fclose(records);
  }
}

void interruption_preserves_capture_output() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records);
  volatile sig_atomic_t stop = 1;
  kernel.stop = &stop;
  const auto result = decline_unchanged(kernel, candidate);
  assert(result.status == NaryCspPrefixLiveStatus::Interrupted);
  NaryCspPrefixOwnerSnapshot output;
  output.external_extent = 99;
  const auto captured =
      nary_csp_prefix_live_capture_owner(kernel, candidate, output);
  assert(captured.status == NaryCspPrefixLiveStatus::Interrupted);
  assert(output.external_extent == 99 && output.source.empty());
  std::fclose(records);
}

void expected_image_obeys_capture_budget_and_stop() {
  Kernel kernel = parse();
  FILE *records = std::tmpfile();
  assert(records);
  auto candidate = request(records);
  // With enough work this assigned owner declines as NoStructure. Small
  // allowances must instead stop during expected-image construction, before
  // checking the assigned first value or allocating the complete AMO image.
  kernel_assign_unit(kernel, kernel_internalize(kernel, 1));
  assert(!kernel_propagate(kernel));
  const Image before = image(kernel);
  NaryCspPrefixOwnerSnapshot output;
  output.external_extent = 99;
  const auto sentinel = output;
  for (uint64_t limit : {uint64_t{4}, uint64_t{10}}) {
    candidate.maximum_recognition_work = limit;
    const auto captured =
        nary_csp_prefix_live_capture_owner(kernel, candidate, output);
    assert(captured.status == NaryCspPrefixLiveStatus::ResourceExhausted &&
           captured.recognition_limit_exhausted &&
           captured.recognition_work <= limit &&
           captured.recognition_work > 0 && output == sentinel &&
           image(kernel) == before && !std::ftell(records));
    const auto applied = decline_unchanged(kernel, candidate);
    assert(applied.status == NaryCspPrefixLiveStatus::ResourceExhausted &&
           applied.recognition_limit_exhausted &&
           applied.recognition_work <= limit);
  }
  auto large = candidate;
  large.domains.assign(3, {});
  for (uint32_t domain = 0; domain != 3; ++domain)
    for (uint32_t value = 0; value != 128; ++value)
      large.domains[domain].push_back(1 + domain * 128 + value);
  large.maximum_domain_values = 128;
  large.maximum_external_extent = 1000;
  large.maximum_recognition_work = 260;
  const auto large_exhausted =
      nary_csp_prefix_live_capture_owner(kernel, large, output);
  assert(large_exhausted.status == NaryCspPrefixLiveStatus::ResourceExhausted &&
         large_exhausted.recognition_limit_exhausted &&
         large_exhausted.recognition_work <= large.maximum_recognition_work &&
         output == sentinel && image(kernel) == before && !std::ftell(records));
  candidate.maximum_recognition_work = 100000;
  // Fixed seed: the first charge succeeds and the second interrupts while
  // constructing the expected image; no generated inputs or seed search.
  kernel.stop_fuzz = 2;
  kernel.stop_fuzz_state = 0;
  const auto interrupted =
      nary_csp_prefix_live_capture_owner(kernel, candidate, output);
  assert(interrupted.status == NaryCspPrefixLiveStatus::Interrupted &&
         !interrupted.recognition_limit_exhausted &&
         interrupted.recognition_work == 1 &&
         kernel.stats.stop_fuzz_fires == 1 && output == sentinel &&
         image(kernel) == before);
  kernel.stop_fuzz = 0;
  assert(nary_csp_prefix_live_capture_owner(kernel, candidate, output).status ==
         NaryCspPrefixLiveStatus::NoStructure);
  for (bool empty : {false, true}) {
    auto malformed = candidate;
    if (empty) malformed.domains.clear();
    else malformed.domains.front().front() = 0;
    const auto invalid =
        nary_csp_prefix_live_capture_owner(kernel, malformed, output);
    assert(invalid.status == NaryCspPrefixLiveStatus::Invalid &&
           !invalid.recognition_limit_exhausted && output == sentinel &&
           image(kernel) == before && !std::ftell(records));
  }
  // A healthy owner can retry the same exact request with a fresh allowance.
  Kernel fresh = parse();
  candidate.maximum_recognition_work = 10;
  assert(nary_csp_prefix_live_capture_owner(fresh, candidate, output).status ==
         NaryCspPrefixLiveStatus::ResourceExhausted);
  assert(output == sentinel);
  candidate.maximum_recognition_work = 100000;
  const auto ready =
      nary_csp_prefix_live_capture_owner(fresh, candidate, output);
  assert(ready.status == NaryCspPrefixLiveStatus::Applied &&
         output.source.size() == clauses().size() &&
         output.capture_work == ready.recognition_work &&
         ready.recognition_work <= candidate.maximum_recognition_work &&
         !std::ftell(records));
  std::fclose(records);
}

void unsupported_proofs_decline_before_mutation() {
  for (const auto &[format, path] :
       {std::pair{ProofFormat::Lrat, "/tmp/nary-csp-prefix-decline.lrat"},
        std::pair{ProofFormat::Pbp, "/tmp/nary-csp-prefix-decline.pbp"}}) {
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
    const auto result = decline_unchanged(kernel, request(records));
    assert(result.status == NaryCspPrefixLiveStatus::UnsupportedProof);
    kernel.proof = nullptr;
    std::fclose(records);
  }
}

void proof_modes_emit() {
  for (const auto &[format, path] :
       {std::pair{ProofFormat::DsrAscii, "/tmp/nary-csp-prefix.dsr"},
        std::pair{ProofFormat::DratAscii, "/tmp/nary-csp-prefix.drat"}}) {
    Kernel kernel = parse();
    Proof proof;
    std::remove(path);
    assert(proof.open(path, format));
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
    kernel.proof = &proof;
    FILE *records = std::tmpfile();
    assert(records);
    const auto result = nary_csp_prefix_live_apply(kernel, request(records));
    assert(result.status == NaryCspPrefixLiveStatus::Applied);
    proof.flush();
    kernel.proof = nullptr;
    FILE *input = std::fopen(path, "rb");
    assert(input);
    assert(!std::fseek(input, 0, SEEK_END));
    assert(std::ftell(input) > 0);
    std::fclose(input);
    std::fclose(records);
  }
}

std::string root_fixed_unsat_dimacs() {
  std::vector<std::vector<int32_t>> formula;
  for (const std::vector<int32_t> &domain :
       {std::vector<int32_t>{1, 2}, {3, 4}, {5, 6}}) {
    formula.push_back(domain);
    formula.push_back({-domain[0], -domain[1]});
  }
  for (uint32_t first = 0; first < 2; ++first)
    for (uint32_t second = 0; second < 2; ++second)
      for (uint32_t third = 0; third < 2; ++third)
        formula.push_back({-int32_t(first + 1),
                           -int32_t(second + 3),
                           -int32_t(third + 5)});
  formula.push_back({7});
  std::ostringstream output;
  output << "p cnf 7 " << formula.size() << '\n';
  for (const auto &clause : formula) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  return output.str();
}

void emit_root_fixed_unsat_proof(const std::string &prefix) {
  const std::string cnf = root_fixed_unsat_dimacs();
  {
    std::ofstream output(prefix + ".cnf");
    assert(output && (output << cnf));
  }
  Kernel kernel;
  Proof proof;
  assert(proof.open((prefix + ".dsr").c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(cnf.data(), 1, cnf.size(), input) == cnf.size());
  std::rewind(input);
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(kernel_propagation_fixpoint(kernel) && kernel.trail.size() == 1 &&
         kernel.value(kernel_internalize(kernel, 7)) > 0);

  FILE *records = std::fopen((prefix + ".records.jsonl").c_str(), "w+");
  assert(records);
  kernel.structural_provenance = records;
  NaryCspPrefixActionSession session;
  kernel.nary_csp_prefix_action_session = &session;
  OnlineInvocation invocation;
  invocation.id = 28;
  invocation.rule = "NaryCspPrefix";
  invocation.parameters = NaryCspPrefixParameters{
      100, 1000, 100000, 8, 16, 100, 7, 16,
      1000, 32, 200, 100000, 1, 100, 32, 100000};
  invocation.granularity = InvocationGranularity::OneInstance;
  invocation.nary_csp_prefix_instances = {
      {{{1, 2}, {3, 4}, {5, 6}}, {2, {{0, 0}, {1, 0}}}}};
  invocation.repeat = {RepeatMode::Once, 1};
  invocation.post_application = PostApplication::Propagate;
  invocation.budget = WorkLimit::bounded(1);
  RestartController controller(kernel);
  ActionRequest action;
  action.invocation = invocation;
  action.sequence = controller.observe().sequence + 1;
  action.expected = state_version(kernel);
  const auto applied = controller.apply_rule(action, ActionAllowance{});
  assert(applied.invocation.status == InvocationStatus::Completed &&
         applied.invocation.rule_result == ApplyResult::Ok &&
         applied.invocation.made_rule_progress &&
         kernel.stats.nary_csp_prefix_emitted_additions == 6 &&
         kernel_propagation_fixpoint(kernel) &&
         kernel.value(kernel_internalize(kernel, -8)) > 0);
  const auto observation = controller.observe();
  const auto candidate = std::find_if(
      observation.semantic_variable_candidates.begin(),
      observation.semantic_variable_candidates.end(), [](const auto &value) {
        return value.producer == SemanticVariableProducer::NaryCsp;
      });
  assert(candidate != observation.semantic_variable_candidates.end() &&
         candidate->action_id == 1 && candidate->existing_variables == 4 &&
         candidate->introduced_variables == 0);
  const auto *published = semantic_variable_candidate(kernel, candidate->id);
  assert(published && published->variables.size() == 4 &&
         std::none_of(published->variables.begin(), published->variables.end(),
                      [](const auto &variable) {
                        return variable.external == 8;
                      }));
  assert(default_solve(kernel) == 20);
  proof.flush();
  assert(std::string(proof.verification_route()) == "dsr-trestle" &&
         !proof.unbacked_deletions());
  kernel.proof = nullptr;
  kernel.structural_provenance = nullptr;
  kernel.nary_csp_prefix_action_session = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    emit_root_fixed_unsat_proof(argv[1]);
    std::puts("test_nary_csp_prefix_live: root-fixed DSR emitted");
    return 0;
  }
  assert(argc == 1);
  capture_preflight_and_publish();
  complete_row_publishes_unit_and_pending_propagation();
  clean_declines_and_stale_owner();
  interruption_preserves_capture_output();
  expected_image_obeys_capture_budget_and_stop();
  unsupported_proofs_decline_before_mutation();
  proof_modes_emit();
  std::puts("test_nary_csp_prefix_live: passed");
}

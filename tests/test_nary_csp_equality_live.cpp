#undef NDEBUG
#include "rules/semantic_extension/nary_csp_equality_live.hpp"

#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

namespace {

std::vector<std::vector<int32_t>> clauses(bool unsat_core = false) {
  std::vector<std::vector<int32_t>> result;
  for (const std::vector<int32_t> &domain :
       {std::vector<int32_t>{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}) {
    result.push_back(domain);
    for (size_t i = 0; i < domain.size(); ++i)
      for (size_t j = i + 1; j < domain.size(); ++j)
        result.push_back({-domain[i], -domain[j]});
  }
  result.push_back({-1, -4, -7});
  result.push_back({-2, -5, -8});
  result.push_back({-3, -6, -9});
  if (unsat_core) {
    result.push_back({10, 11});
    result.push_back({10, -11});
    result.push_back({-10, 11});
    result.push_back({-10, -11});
  }
  return result;
}

std::string dimacs(bool unsat_core = false) {
  const auto formula = clauses(unsat_core);
  std::ostringstream output;
  output << "p cnf " << (unsat_core ? 11 : 9) << ' ' << formula.size()
         << '\n';
  for (const auto &clause : formula) {
    for (int32_t literal : clause) output << literal << ' ';
    output << "0\n";
  }
  return output.str();
}

Kernel parse(bool unsat_core = false) {
  const std::string text = dimacs(unsat_core);
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

NaryCspPrefixLiveRequest capture_request() {
  NaryCspPrefixLiveRequest request;
  request.domains = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
  request.selection = {2, {{0, 0}, {1, 0}}};
  request.maximum_source_clauses = 100;
  request.maximum_source_literals = 1000;
  request.maximum_recognition_work = 100000;
  request.maximum_domains = 3;
  request.maximum_domain_values = 16;
  request.maximum_forbidden_tuples = 100;
  request.maximum_prefix_arity = 2;
  request.maximum_definition_clause_literals = 3;
  request.maximum_proof_additions = 100;
  request.maximum_proof_literal_occurrences = 1000;
  request.maximum_external_extent = 100;
  return request;
}

NaryCspEqualityLimits limits() {
  return {100, 1000, 200000, 16, 100, 10000, 100, 1000,
          100000, 1, 100, 100};
}

NaryCspEqualityRequest request(const NaryCspPrefixOwnerSnapshot &owner,
                               FILE *records) {
  return {&owner, {}, limits(), 71, 81, records};
}

bool large_clause_live(const Kernel &kernel, std::vector<int32_t> wanted) {
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

void publish_duplicate_and_records() {
  Kernel kernel = parse();
  NaryCspPrefixOwnerSnapshot owner;
  assert(nary_csp_prefix_live_capture_owner(kernel, capture_request(), owner)
             .status == NaryCspPrefixLiveStatus::Applied);
  FILE *records = std::tmpfile();
  assert(records);
  NaryCspEqualitySession session;
  auto action = request(owner, records);
  const auto applied = nary_csp_equality_live_apply(kernel, session, action);
  assert(applied.status == NaryCspEqualityLiveStatus::Applied);
  assert(applied.result == ApplyResult::Ok);
  assert(applied.planned_additions == 9 && applied.emitted_additions == 9);
  assert(applied.proof_literal_occurrences == 27);
  assert(applied.extent_growth == 1 && kernel.nvars == 10);
  assert(session.belongs_to(kernel) && session.keys().size() == 1);
  assert(applied.projection.extension_variable == 10);
  assert(applied.projection.exact_aligned_equality);
  assert(applied.projection.project_from_external_model);
  assert(!applied.projection.reconstruction_witness_required);
  assert(applied.semantic_variables.size() == 1);
  assert(applied.semantic_variables[0].external == 10);
  assert(applied.semantic_variables[0].role ==
         strategy::SemanticVariableRole::IntroducedTheory);
  assert(applied.semantic_candidate_ready);
  assert(applied.published_consequences ==
         std::vector<std::vector<int32_t>>(
             {{-10, -1, -7}, {-10, -2, -8}, {-10, -3, -9}}));
  assert(large_clause_live(kernel, {10, -1, -4}));
  assert(large_clause_live(kernel, {-10, -1, 4}));
  assert(large_clause_live(kernel, {-10, -1, -7}));
  assert(!applied.propagation_pending);

  const uint32_t before_vars = kernel.nvars;
  const uint64_t before_id = kernel.next_clause_id;
  const long before_records = std::ftell(records);
  action.selection = {1, 0, 2};
  const auto duplicate =
      nary_csp_equality_live_apply(kernel, session, action);
  assert(duplicate.status == NaryCspEqualityLiveStatus::Duplicate);
  assert(duplicate.result == ApplyResult::NoOp);
  assert(kernel.nvars == before_vars && kernel.next_clause_id == before_id);
  assert(std::ftell(records) == before_records);

  std::rewind(records);
  std::string journal(16384, '\0');
  const size_t bytes = std::fread(journal.data(), 1, journal.size(), records);
  journal.resize(bytes);
  assert(journal.find("nary-csp-equality-begin") != std::string::npos);
  assert(journal.find("nary-csp-equality-source") != std::string::npos);
  assert(journal.find("nary-csp-equality-addition") != std::string::npos);
  assert(journal.find("omit-exact-aligned-equality") != std::string::npos);
  std::fclose(records);
}

void clean_declines() {
  Kernel kernel = parse();
  NaryCspPrefixOwnerSnapshot owner;
  assert(nary_csp_prefix_live_capture_owner(kernel, capture_request(), owner)
             .status == NaryCspPrefixLiveStatus::Applied);
  FILE *records = std::tmpfile();
  assert(records);
  NaryCspEqualitySession session;
  auto action = request(owner, records);

  action.selection = {0, 0, 2};
  auto result = nary_csp_equality_live_apply(kernel, session, action);
  assert(result.status == NaryCspEqualityLiveStatus::Invalid);
  assert(!session.retained() && kernel.nvars == 9);

  action = request(owner, records);
  action.limits.maximum_proof_additions = 8;
  result = nary_csp_equality_live_apply(kernel, session, action);
  assert(result.status == NaryCspEqualityLiveStatus::ResourceExhausted);
  assert(!session.retained() && kernel.nvars == 9);

  action = request(owner, records);
  action.limits.maximum_journal_bytes = 10;
  result = nary_csp_equality_live_apply(kernel, session, action);
  assert(result.status == NaryCspEqualityLiveStatus::ResourceExhausted);
  assert(!session.retained() && kernel.nvars == 9);
  std::fclose(records);
}

void changed_owner_declines_without_publication() {
  for (bool assigned : {false, true}) {
    Kernel kernel = parse();
    NaryCspPrefixOwnerSnapshot owner;
    assert(nary_csp_prefix_live_capture_owner(kernel, capture_request(), owner)
               .status == NaryCspPrefixLiveStatus::Applied);
    FILE *records = std::tmpfile();
    assert(records);
    NaryCspEqualitySession session;
    if (assigned) {
      kernel_assign_unit(kernel, kernel_internalize(kernel, 1));
      assert(!kernel_propagate(kernel));
    } else {
      assert(kernel_delete_binary_all_copies(
          kernel, kernel_internalize(kernel, -1),
          kernel_internalize(kernel, -2)));
    }
    assert(kernel_propagation_fixpoint(kernel));
    const std::vector<Lit> trail = kernel.trail;
    const auto arena = kernel.arena.size();
    const auto variables = kernel.nvars;
    auto action = request(owner, records);
    const auto stale = nary_csp_equality_live_apply(kernel, session, action);
    assert(stale.status == NaryCspEqualityLiveStatus::Stale &&
           stale.result == ApplyResult::NoOp && !session.retained() &&
           session.keys().empty() && kernel.nvars == variables &&
           kernel.arena.size() == arena && kernel.trail == trail &&
           !std::ftell(records) && !kernel.structural_action_sequence);
    // Malformed coordinates remain Invalid even when the exact owner is stale.
    action.selection = {0, 0, 2};
    assert(nary_csp_equality_live_apply(kernel, session, action).status ==
           NaryCspEqualityLiveStatus::Invalid);
    auto malformed = owner;
    malformed.relation.domains.front().values.front() = 0;
    action = request(malformed, records);
    assert(nary_csp_equality_live_apply(kernel, session, action).status ==
           NaryCspEqualityLiveStatus::Invalid);
    assert(!session.retained() && !std::ftell(records));
    std::fclose(records);
  }
}

void emit_live_proof(const std::string &stem) {
  const std::string cnf = dimacs(true);
  std::ofstream formula(stem + ".cnf");
  assert(formula);
  formula << cnf;
  formula.close();

  Kernel kernel;
  Proof proof;
  assert(proof.open((stem + ".dsr").c_str(), ProofFormat::DsrAscii));
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  kernel.proof = &proof;
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(cnf.data(), 1, cnf.size(), input) == cnf.size());
  std::rewind(input);
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  assert(!kernel.inconsistent && kernel_propagation_fixpoint(kernel));

  NaryCspPrefixOwnerSnapshot owner;
  assert(nary_csp_prefix_live_capture_owner(kernel, capture_request(), owner)
             .status == NaryCspPrefixLiveStatus::Applied);
  FILE *records = std::fopen((stem + ".records.jsonl").c_str(), "w+");
  assert(records);
  NaryCspEqualitySession session;
  const auto applied =
      nary_csp_equality_live_apply(kernel, session, request(owner, records));
  assert(applied.status == NaryCspEqualityLiveStatus::Applied);
  assert(applied.projection.extension_variable == 12);
  assert(default_solve(kernel) == 20);
  proof.flush();
  // The equality transaction itself is witness-free RAT/RUP.  The subsequent
  // default search may emit SR, which correctly routes the complete stream
  // through the native Trestle checker.
  assert(std::string(proof.verification_route()) == "dsr-trestle");
  kernel.proof = nullptr;
  std::fclose(records);
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 2) {
    emit_live_proof(argv[1]);
    std::puts("test_nary_csp_equality_live: DSR emitted");
    return 0;
  }
  assert(argc == 1);
  publish_duplicate_and_records();
  clean_declines();
  changed_owner_declines_without_publication();
  std::puts("test_nary_csp_equality_live: passed");
}

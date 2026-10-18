#include "rules/semantic_extension/disjunction.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace tactix {

DisjunctionDefinitionResult define_disjunction(
    Kernel &k, const DisjunctionDefinitionRequest &request) {
  DisjunctionDefinitionResult result;
  const uint64_t arity = request.inputs.size();
  const uint64_t extent = k.external_to_internal.size();
  // Nonempty, non-tautological clauses with distinct input variables simplify
  // the defining RAT groups. Empty/duplicate/complementary inputs can be
  // canonicalized by the caller; this is a representation restriction.
  if (!request.records || !request.action_id || !request.semantic_id ||
      !arity || arity > request.maximum_arity ||
      arity + 1 > request.maximum_additions ||
      arity + 1 > UINT32_MAX || extent >= INT32_MAX ||
      extent + 1 > request.maximum_external_extent ||
      k.nvars >= static_cast<uint32_t>(INT32_MAX) ||
      k.next_clause_id > static_cast<uint64_t>(INT64_MAX) - arity - 1 ||
      (arity > 1 && k.arena.size() + arity + 1 + CLAUSE_HEADER_WORDS >= INVALID_REF) ||
      k.inconsistent || k.level || k.conflict_valid || k.pending.valid ||
      k.dense || k.probing.recording || !k.large_watches_connected ||
      !kernel_propagation_fixpoint(k) || (k.proof && k.proof->terminal_pbp()) ||
      kernel_terminating(k))
    return result;

  std::vector<Lit> inputs;
  std::vector<Var> variables;
  inputs.reserve(arity);
  variables.reserve(arity);
  for (int32_t external : request.inputs) {
    if (kernel_terminating(k)) return result;
    ++result.inspected_inputs;
    if (!external || external == INT32_MIN) return result;
    const Lit lit = kernel_internalize(k, external);
    if (lit == INVALID_LIT || kernel_var_retired(k, var_of(lit))) return result;
    inputs.push_back(lit);
    variables.push_back(var_of(lit));
  }
  std::sort(variables.begin(), variables.end());
  if (std::adjacent_find(variables.begin(), variables.end()) != variables.end() ||
      kernel_terminating(k)) return result;

  // All scratch allocation precedes commit. Root-fixed live inputs are legal:
  // the kernel's clause-add primitives rearm previously scanned false watches.
  std::vector<Lit> reverse(arity + 1);
  std::vector<int64_t> hints(arity);
  k.close_startup_certificate();
  const Var helper = kernel_new_extension_vars(k, 1);
  const Lit positive = make_lit(helper, false);
  result.output = kernel_externalize(k, positive);
  result.first_clause_id = k.next_clause_id + 1;
  std::fprintf(request.records,
      "{\"kind\":\"define-disjunction\",\"action\":%llu,\"semantic\":%llu,"
      "\"output\":%d,\"inputs\":[",
      (unsigned long long)request.action_id, (unsigned long long)request.semantic_id,
      result.output);
  for (size_t i = 0; i < arity; ++i)
    std::fprintf(request.records, "%s%d", i ? "," : "", request.inputs[i]);
  std::fprintf(request.records,
      "],\"maximum_arity\":%u,\"maximum_external_extent\":%u,"
      "\"maximum_additions\":%llu,\"projection\":\"omit-fresh-output\","
      "\"lifetime\":\"ordinary-live-clauses\"}\n",
      request.maximum_arity, request.maximum_external_extent,
      (unsigned long long)request.maximum_additions);
  auto publish = [&](const Lit *lits, uint32_t size, const int64_t *chain,
                     uint32_t count) {
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) k.proof->add_rat(id, lits, size, chain, count);
    if (size == 2) kernel_add_binary_clause(k, lits[0], lits[1], id);
    else kernel_add_large_clause(k, lits, size, false, 0, id);
    std::fprintf(request.records,
        "{\"kind\":\"definition-rat\",\"action\":%llu,\"semantic\":%llu,"
        "\"id\":%llu,\"clause\":[",
        (unsigned long long)request.action_id, (unsigned long long)request.semantic_id,
        (unsigned long long)id);
    for (uint32_t i = 0; i < size; ++i)
      std::fprintf(request.records, "%s%d", i ? "," : "", kernel_externalize(k, lits[i]));
    std::fprintf(request.records, "]}\n");
    ++result.additions;
    result.literal_occurrences += size;
    return id;
  };
  for (size_t i = 0; i < arity; ++i) {
    const Lit clause[] = {positive, negate(inputs[i])};
    hints[i] = -static_cast<int64_t>(publish(clause, 2, nullptr, 0));
    reverse[i + 1] = inputs[i];
  }
  reverse[0] = negate(positive);
  // Each resolvent contains input_i and its complement: empty LRAT groups.
  publish(reverse.data(), static_cast<uint32_t>(reverse.size()), hints.data(),
          static_cast<uint32_t>(hints.size()));
  if (std::fflush(request.records) || std::ferror(request.records)) std::abort();
  result.result = ApplyResult::Ok;
  return result;
}
} // namespace tactix

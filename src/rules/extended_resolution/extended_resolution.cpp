#include "rules/extended_resolution/extended_resolution.hpp"

#include "proof/proof.hpp"
#include "rules/structural_action_guard.hpp"
#include "rules/structural_text.hpp"
#include "rules/semantic_extension/action_epilogue.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <vector>

namespace tactix {

RuleEligibility extended_resolution_guard(const Kernel &k) {
  return root_action_guard(k, false)
      .clausal_certificate()
      .semantic_candidate()
      .fact("action-id-available", k.structural_action_sequence != UINT64_MAX)
      .fact("external-id-available", k.external_to_internal.size() < INT32_MAX)
      .fact("not-terminated", !kernel_terminating(k))
      .finish();
}

static bool valid_gate(const Kernel &k, ExtensionGate gate,
                       int32_t left, int32_t right, int32_t otherwise) {
  if (!extended_resolution_guard(k) ||
      (gate != ExtensionGate::And && gate != ExtensionGate::Or &&
       gate != ExtensionGate::Xor && gate != ExtensionGate::Ite))
    return false;
  const int32_t operands[] = {left, right, otherwise};
  for (unsigned i = 0; i < (gate == ExtensionGate::Ite ? 3u : 2u); ++i) {
    const int32_t external = operands[i];
    if (!external || external == INT32_MIN) return false;
    const Lit literal = kernel_internalize(k, external);
    if (literal == INVALID_LIT || kernel_var_retired(k, var_of(literal)))
      return false;
  }
  // At most four ternaries. Reject capacity exhaustion before any mutation.
  return k.next_clause_id <= uint64_t(INT64_MAX) - 4 &&
         k.arena.size() < INVALID_REF &&
         4 * uint64_t(clause_words(3)) < INVALID_REF - k.arena.size();
}

bool extended_resolution_valid(const Kernel &k, const ExtensionDefinition &d) {
  return valid_gate(k, d.gate, d.left, d.right, 0);
}

static int32_t define_gate(Kernel &k, ExtensionGate gate,
                           int32_t left, int32_t right, int32_t otherwise) {
  if (!valid_gate(k, gate, left, right, otherwise)) return 0;
  const Lit a = kernel_internalize(k, left);
  const Lit b = kernel_internalize(k, right);
  const Lit z = make_lit(k.nvars, false);
  const Lit nz = negate(z);
  std::vector<std::vector<Lit>> clauses;
  switch (gate) {
  case ExtensionGate::And:
    clauses = {{nz, a}, {nz, b}, {z, negate(a), negate(b)}};
    break;
  case ExtensionGate::Or:
    clauses = {{z, negate(a)}, {z, negate(b)}, {nz, a, b}};
    break;
  case ExtensionGate::Xor:
    clauses = {{z, a, negate(b)}, {z, negate(a), b},
               {nz, a, b}, {nz, negate(a), negate(b)}};
    break;
  case ExtensionGate::Ite: {
    const Lit c = kernel_internalize(k, otherwise);
    clauses = {{z, negate(a), negate(b)}, {z, a, negate(c)},
               {nz, negate(a), b}, {nz, a, c}};
    break;
  }
  }
  // Preserve the output as RAT pivot. Equal and complementary operands are
  // legal: collapse duplicate literals and omit tautological clauses.
  std::vector<std::vector<Lit>> normalized;
  for (auto &clause : clauses) {
    std::sort(clause.begin() + 1, clause.end());
    clause.erase(std::unique(clause.begin() + 1, clause.end()), clause.end());
    if (clause.size() == 3 && clause[1] == negate(clause[2])) continue;
    if (std::find(normalized.begin(), normalized.end(), clause) == normalized.end())
      normalized.push_back(std::move(clause));
  }

  const int32_t output = static_cast<int32_t>(k.external_to_internal.size() + 1);
  const uint64_t action = k.structural_action_sequence + 1;
  JournalText journal(k.structural_provenance);
  if (!journal.append_if(UINT64_MAX, [](uint64_t) { return true; },
        "{\"kind\":\"extended-resolution\",\"action\":", action,
        ",\"gate\":", static_cast<unsigned>(gate), ",\"left\":", left,
        ",\"right\":", right)) return 0;
  if (gate == ExtensionGate::Ite &&
      !journal.append_if(UINT64_MAX, [](uint64_t) { return true; },
                         ",\"else\":", otherwise)) return 0;
  if (!journal.append_if(UINT64_MAX, [](uint64_t) { return true; },
        ",\"output\":", output,
        ",\"first_clause_id\":", k.next_clause_id + 1,
        ",\"clauses\":", normalized.size(), "}\n")) return 0;

  // From here allocation/I/O failures are fatal, as in other kernel commits.
  // Same-polarity output clauses are vacuous RAT. Each later opposite-polarity
  // output clause has only tautological resolvents with them.
  k.close_startup_certificate();
  const Var fresh = kernel_new_extension_vars(k, 1);
  if (make_lit(fresh, false) != z || kernel_externalize(k, z) != output)
    std::abort();
  for (const auto &clause : normalized) {
    const uint64_t id = ++k.next_clause_id;
    if (k.proof) k.proof->add_rat(id, clause.data(), clause.size());
    if (clause.size() == 2)
      kernel_add_binary_clause(k, clause[0], clause[1], id);
    else
      kernel_add_large_clause(k, clause.data(), clause.size(), false, 0, id);
  }
  k.structural_action_sequence = action;
  if (k.structural_provenance) journal.write(k.structural_provenance);
  stage_theory_variables_or_abort(k,
      strategy::SemanticVariableProducer::ExtendedResolution, action, {}, {output});
  return output;
}

int32_t extended_resolution_define(Kernel &k, const ExtensionDefinition &d) {
  return define_gate(k, d.gate, d.left, d.right, 0);
}

int32_t extended_resolution_define_ite(Kernel &k, int32_t selector,
                                       int32_t then_literal, int32_t else_literal) {
  return define_gate(k, ExtensionGate::Ite, selector, then_literal, else_literal);
}

// Definitions are policy choices supplied through the structured API.
TACTIX_REGISTER_OPT_IN_RULE(ExtendedResolution, RuleTier::Inprocess,
    extended_resolution_guard, [](Kernel &) { return ApplyResult::NoOp; });

} // namespace tactix

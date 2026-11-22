#include "rules/lucky/opportunities.hpp"
#include "rules/lucky/model.hpp"
#include "rules/lucky/parity_selection.hpp"
#include "rules/lucky/startup_pb_writer.hpp"
#include "engine/affine/projection.hpp"

#include <map>
#include <set>

namespace tactix {
namespace {

constexpr uint32_t MAXIMUM_VARIABLES = 1024;
constexpr size_t MAXIMUM_CLAUSES = 8192;
constexpr uint64_t MAXIMUM_WORK = 2'000'000;

struct SupportBlock {
  std::vector<Var> members;
  std::array<uint8_t, 8> state_class{};
  std::array<uint8_t, 2> representative{};
};

// The states of a block have the same effect on the CNF exactly when
// they forbid the same residual assignments in the other variables.
template<class Meter>
bool classify_support_block(const std::vector<std::vector<Lit>> &clauses,
                            const std::vector<size_t> &support,
                            SupportBlock &block, Meter &meter) {
  using Signature = std::vector<std::vector<Lit>>;
  std::array<Signature, 8> signatures;
  for (size_t id : support) {
    const auto &clause = clauses[id];
    if (!meter.charge(1 + clause.size() * block.members.size()))
      return false;
    unsigned state = 0;
    std::vector<Lit> remainder;
    for (Lit literal : clause) {
      const auto start = block.members.begin();
      const auto member = std::find(start, block.members.end(), var_of(literal));
      if (member == block.members.end())
        remainder.push_back(literal);
      else if (negated(literal))
        state |= 1u << (member - start);
    }
    signatures[state].push_back(std::move(remainder));
  }
  uint8_t classes = 0;
  const unsigned states = 1u << block.members.size();
  for (unsigned state = 0; state < states; ++state) {
    Signature &signature = signatures[state];
    if (!meter.charge(1 + 32 * signature.size() *
                          (1 + std::bit_width(signature.size()))))
      return false;
    std::sort(signature.begin(), signature.end());
    signature.erase(std::unique(signature.begin(), signature.end()),
                    signature.end());
    uint8_t id = 0;
    while (id < classes && signature != signatures[block.representative[id]])
      ++id;
    if (id == classes) {
      if (classes == block.representative.size())
        return false;
      block.representative[classes++] = static_cast<uint8_t>(state);
    }
    block.state_class[state] = id;
  }
  return classes == 2;
}

struct SupportQuotient {
  std::vector<SupportBlock> blocks;
  std::vector<parity_premises::CertifiedEquation> equations;
};

template<class Meter>
bool recover_support_quotient(const Kernel &k, const std::vector<std::vector<Lit>> &clauses,
                              Meter &meter, SupportQuotient &quotient_plan) {
  using parity_premises::External;
  if (!meter.charge(k.nvars))
    return false;
  std::vector<std::vector<size_t>> occurrences(k.nvars);
  for (size_t id = 0; id < clauses.size(); ++id) {
    const auto &clause = clauses[id];
    if (clause.empty() || clause.size() > 16 || !meter.charge(clause.size()))
      return false;
    for (Lit literal : clause)
      occurrences[var_of(literal)].push_back(id);
  }
  std::map<std::vector<size_t>, std::vector<Var>> supports;
  for (Var v = 0; v < k.nvars; ++v) {
    if (occurrences[v].empty()) continue;
    if (!meter.charge(1 + occurrences[v].size() * std::bit_width(k.nvars)))
      return false;
    supports[std::move(occurrences[v])].push_back(v);
  }
  auto &blocks = quotient_plan.blocks;
  std::vector<size_t> owner(k.nvars, SIZE_MAX);
  for (auto &[support, members] : supports) {
    if (members.size() < 2 || members.size() > 3) continue;
    SupportBlock block{std::move(members)};
    if (!classify_support_block(clauses, support, block, meter)) continue;
    for (Var member : block.members)
      owner[member] = blocks.size();
    blocks.push_back(std::move(block));
  }
  if (blocks.empty()) return false;
  std::set<std::vector<External>> quotient;
  for (const auto &clause : clauses) {
    if (!meter.charge(1 + 4 * clause.size())) return false;
    std::vector<External> projected;
    for (Lit literal : clause) {
      const Var variable = var_of(literal);
      bool negative = negated(literal);
      if (owner[variable] != SIZE_MAX) {
        const auto &block = blocks[owner[variable]];
        if (variable != block.members.front()) continue;
        unsigned state = 0;
        for (size_t bit = 0; bit < block.members.size(); ++bit)
          if (std::binary_search(clause.begin(), clause.end(),
                                 make_lit(block.members[bit], true)))
            state |= 1u << bit;
        negative = block.state_class[state];
      }
      const auto external = static_cast<External>(variable + 1);
      projected.push_back(negative ? -external : external);
    }
    if (projected.empty() || projected.size() > 4 ||
        projected.size() > k.affine_model_parameters.maximum_arity)
      return false;
    quotient.insert(std::move(projected));
  }
  parity_premises::Collector collector({quotient.size(), quotient.size()});
  std::vector<parity_premises::CertifiedEquation> units;
  uint64_t identity = 0;
  for (const auto &clause : quotient) {
    if (!meter.charge(1 + clause.size())) return false;
    if (clause.size() == 1) {
      parity_premises::CertifiedEquation unit;
      unit.equation.size = 1;
      unit.equation.variables[0] = std::abs(clause.front());
      unit.equation.rhs = clause.front() > 0;
      units.push_back(std::move(unit));
    } else if (!collector.add(++identity, clause.data(), static_cast<uint32_t>(clause.size())))
      return false;
  }
  auto &certified = quotient_plan.equations;
  if (!meter.charge(16 * collector.groups_seen()) || !collector.finish(certified))
    return false;
  uint64_t covered = units.size();
  for (const auto &row : certified)
    covered += row.premises.size();
  if (covered != quotient.size())
    return false;
  certified.insert(certified.end(), units.begin(), units.end());
  return true;
}

class SupportQuotientPrefix : public StartupPbWriter {
  uint64_t limit_;

  void block(const std::vector<int32_t> &scope, size_t depth, unsigned state) {
    for (size_t bit = 0; bit < depth; ++bit)
      term(state & (1u << bit) ? -scope[bit] : scope[bit]);
  }

public:
  SupportQuotientPrefix(const Kernel &k, FILE *out, uint64_t limit)
      : StartupPbWriter(k, out), limit_(limit) { header(); }

  bool define(int32_t output, const SupportBlock &group,
              const std::vector<int32_t> &members) {
    for (unsigned state = 0; state < (1u << members.size()); ++state) {
      if (additions() >= limit_) return false;
      const unsigned value = group.state_class[state];
      text("red ");
      term(value ? output : -output);
      block(members, members.size(), state);
      text(">= 1 : x%d -> %u ;\n", output, value);
      ++id_;
    }
    return true;
  }

  bool derive(const std::vector<int32_t> &clause,
              const std::vector<int32_t> &expanded) {
    // Under a complete assignment of the raw block members, the negation of
    // the quotient clause contradicts a definition or an original clause.
    for (size_t depth = std::max(size_t{1}, expanded.size()); depth-- > 0;)
      for (unsigned state = 0; state < (1u << depth); ++state) {
        if (additions() >= limit_) return false;
        text("rup ");
        for (int32_t literal : clause) term(literal);
        block(expanded, depth, state);
        text(">= 1 ;\n");
        ++id_;
      }
    return true;
  }
};

bool write_support_quotient_proof(Kernel &k, FILE *out, const SupportQuotient &quotient,
                                   const std::vector<size_t> &witness,
                                   uint64_t limit, uint64_t &steps) {
  if (k.external_to_internal.size() >= INT32_MAX) return false;
  uint32_t fresh = static_cast<uint32_t>(k.external_to_internal.size());
  SupportQuotientPrefix prefix(k, out, limit);
  std::vector<const SupportBlock *> owners(k.nvars, nullptr);
  for (const auto &block : quotient.blocks)
    owners[block.members.front()] = &block;
  std::vector<int32_t> names(k.nvars);
  std::vector<PbpParityEquation> rows;
  for (size_t index : witness) {
    const auto &equation = quotient.equations[index].equation;
    if (equation.size < 2) return false;
    PbpParityEquation &row = rows.emplace_back();
    row.rhs = equation.rhs;
    std::vector<int32_t> expanded;
    for (size_t i = 0; i < equation.size; ++i) {
      const auto v = static_cast<Var>(equation.variables[i] - 1);
      if (const SupportBlock *block = owners[v]) {
        std::vector<int32_t> members;
        for (Var member : block->members)
          members.push_back(kernel_externalize(k, make_lit(member, false)));
        if (!names[v]) {
          if (fresh == INT32_MAX) return false;
          names[v] = static_cast<int32_t>(++fresh);
          if (!prefix.define(names[v], *block, members)) return false;
        }
        expanded.insert(expanded.end(), members.begin(), members.end());
      } else {
        names[v] = kernel_externalize(k, make_lit(v, false));
      }
      row.variables.push_back(names[v]);
    }
    if (expanded.size() > 9) return false;
    std::sort(row.variables.begin(), row.variables.end());
    if (!visit_parity_source(row, [&](const auto &clause) {
          return prefix.derive(clause, expanded);
        }))
      return false;
  }
  return write_startup_pb_parity_tail(k, out, rows, prefix.additions(), fresh, limit, steps);
}

template<class Meter>
bool read_input(const Kernel &k, std::vector<std::vector<Lit>> &clauses,
                Meter &meter) {
  const uint64_t expected = k.input_unit_clause_count + k.input_nonunit_clause_count;
  if (k.startup_input.empty() || expected > MAXIMUM_CLAUSES ||
      expected > k.affine_model_parameters.maximum_constraints ||
      !meter.charge(k.startup_input.size()))
    return false;
  clauses.reserve(expected + 1);
  clauses.emplace_back();
  for (const int32_t external : k.startup_input) {
    auto &clause = clauses.back();
    if (external) {
      const Lit literal = kernel_internalize(k, external);
      if (literal == INVALID_LIT || clause.size() == 16) return false;
      clause.push_back(literal);
      continue;
    }
    if (clause.empty() || clauses.size() > expected) return false;
    std::sort(clause.begin(), clause.end());
    if (std::adjacent_find(clause.begin(), clause.end(), [](Lit lhs, Lit rhs) {
          return var_of(lhs) == var_of(rhs);
        }) != clause.end())
      return false;
    clauses.emplace_back();
  }
  if (!clauses.back().empty()) return false;
  clauses.pop_back();
  return clauses.size() == expected;
}

ApplyResult refute(Kernel &k, const SupportQuotient &quotient, uint64_t work) {
  if (k.proof && !k.proof->startup_pb_available()) return ApplyResult::NoOp;
  const auto selection = parity_premises::select(quotient.equations, true,
                                                {work, k.nvars, 1024, 0});
  if (selection.consequences.empty()) return ApplyResult::NoOp;
  const auto &witness = selection.consequences.front().sources;
  const uint64_t limit = k.parity_model_parameters.refutation_maximum_proof_additions;
  uint64_t steps = 0;
  if (!write_support_quotient_proof(k, nullptr, quotient, witness, limit, steps))
    return ApplyResult::NoOp;
  if (k.proof) {
    const auto emit = [&](FILE *out) {
      uint64_t copied = 0;
      return write_support_quotient_proof(k, out, quotient, witness, limit, copied) && copied == steps;
    };
    if (!k.proof->refute_by_counting(emit, k.startup_input))
      return ApplyResult::NoOp;
  }
  ++k.stats.affine_support_quotient_unsat;
  k.close_startup_certificate();
  k.inconsistent = true;
  return ApplyResult::Unsat;
}

} // namespace

ApplyResult startup_affine_quotient_apply(Kernel &k) {
  using namespace strategy::affine;
  const auto &parameters = k.affine_model_parameters;
  if (!startup_affine_quotient_guard(k) ||
      k.nvars > std::min<uint64_t>(MAXIMUM_VARIABLES, parameters.maximum_variables))
    return ApplyResult::NoOp;
  uint64_t budget = std::min(MAXIMUM_WORK, parameters.maximum_work);
  if (kernel_has_work_budget(k)) budget = std::min(budget, k.work_budget);
  Work meter(budget, [&] { return kernel_rule_interrupt(k, k.stats.affine_model_interrupted); });
  std::vector<std::vector<Lit>> clauses;
  SupportQuotient quotient;
  if (!read_input(k, clauses, meter) || !recover_support_quotient(k, clauses, meter, quotient))
    return ApplyResult::NoOp;

  std::vector<Equation> equations;
  uint64_t nonunit = 0;
  for (const auto &premise : quotient.equations) {
    const auto &row = premise.equation;
    auto &equation = equations.emplace_back();
    equation.rhs = row.rhs;
    for (size_t i = 0; i < row.size; ++i)
      equation.variables.push_back(static_cast<size_t>(row.variables[i] - 1));
    nonunit += row.size > 1;
  }
  Projection projection;
  const auto status = project(k.nvars, equations, {}, parameters.linear_bytes, meter, projection);
  if (status == ProjectionStatus::Inconsistent)
    return refute(k, quotient, budget - meter.used());
  if (status != ProjectionStatus::Ready || nonunit < parameters.minimum_equations ||
      parameters.projection_mode == strategy::AffineProjectionMode::Explicit ||
      !lucky_root_eligibility(k))
    return ApplyResult::NoOp;

  std::vector<uint8_t> assignment(k.nvars, uint8_t(parameters.free_phase));
  uint64_t work = 0;
  if (!reconstruct(projection, {}, assignment, meter, work)) return ApplyResult::NoOp;
  for (const auto &block : quotient.blocks) {
    const unsigned state = block.representative[assignment[block.members.front()]];
    for (size_t bit = 0; bit < block.members.size(); ++bit)
      assignment[block.members[bit]] = (state >> bit) & 1;
  }
  if (!lucky_model_valid(k, assignment, [&](uint64_t amount) { return meter.charge(amount); }) ||
      !meter.charge(k.nvars))
    return ApplyResult::NoOp;
  lucky_publish_model(k, assignment);
  ++k.stats.affine_support_quotient_sat;
  return ApplyResult::Sat;
}

} // namespace tactix

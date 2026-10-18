#include "rules/graph_interval_equivalence/graph_interval_equivalence.hpp"

#include "proof/proof.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/lucky/graph_labeling_recognition.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace tactix {
namespace {

constexpr uint64_t DEFAULT_GROUPS = 1;
constexpr uint64_t MAXIMUM_GROUPS = UINT32_MAX / 2;
constexpr uint32_t MAXIMUM_VERTICES = 512;
constexpr uint64_t MAXIMUM_WORK = 100'000'000;
constexpr uint64_t MAXIMUM_WORKSPACE = uint64_t{512} << 20;
constexpr uint64_t MAXIMUM_SOURCE_CLAUSES = 2'000'000;
constexpr uint64_t MAXIMUM_SOURCE_LITERALS = 16'000'000;
// Conservative retained footprint for the ordered source map, the duplicate
// ternary index and later gate/meaning bookkeeping. This intentionally
// overcounts binaries and duplicate clause keys.
constexpr uint64_t SOURCE_CLAUSE_BYTES = 384;
constexpr uint64_t SOURCE_LITERAL_BYTES = 32;

enum class DiscoveryStop : uint8_t { None, Interrupted, Resource };

struct Source {
  uint64_t id = 0;
  bool redundant = false;
};

struct Explanation {
  std::array<uint64_t, 2> parents{};
  uint8_t count = 0;
  bool direct = false;
};

struct Gate {
  Lit output = INVALID_LIT;
  Lit left = INVALID_LIT;
  Lit right = INVALID_LIT;
  uint64_t defining = 0;
  Explanation left_reverse;
  Explanation right_reverse;
};

struct Interval {
  uint32_t row = 0;
  uint32_t first = 0;
  uint32_t last = 0;
};

struct Candidate {
  Interval interval;
  Gate first;
  Gate second;
};

struct Discovery {
  uint32_t vertices = 0;
  uint64_t work = 0;
  uint64_t clauses = 0;
  uint64_t literals = 0;
  uint64_t workspace = 0;
  uint64_t gates = 0;
  uint64_t intervals = 0;
  DiscoveryStop stop = DiscoveryStop::None;
  std::vector<Candidate> candidates;
};

using ClauseKey = std::vector<Lit>;

ClauseKey key(std::initializer_list<Lit> literals) {
  ClauseKey result(literals);
  std::sort(result.begin(), result.end());
  return result;
}

// The gate authentication is deliberately narrower than graph recognition.
// Every accepted z has the live clauses
//   (a v b v z), (-a v -z), (-b v -z),
// where either reverse binary is explicit or has the exact two-clause RUP
// chain used by the retained static audit.  Recursing from indexed primary
// literals therefore proves that z means absence on precisely the recorded
// contiguous interval.  The final equivalence still has an independent RUP
// replay before any mutation.
bool discover(Kernel &kernel, uint64_t maximum_work, Discovery &result) {
  result = {};
  const uint64_t limit = maximum_work;
  auto charge = [&](uint64_t amount) {
    if (kernel_rule_interrupt(
            kernel, kernel.stats.graph_interval_equivalence_interrupted)) {
      result.stop = DiscoveryStop::Interrupted;
      return false;
    }
    if (amount > limit - result.work) {
      result.stop = DiscoveryStop::Resource;
      return false;
    }
    result.work += amount;
    return true;
  };
  const auto retain_source = [&](uint64_t literals) {
    if (result.clauses == MAXIMUM_SOURCE_CLAUSES ||
        literals > MAXIMUM_SOURCE_LITERALS - result.literals)
      return false;
    const uint64_t bytes = SOURCE_CLAUSE_BYTES + SOURCE_LITERAL_BYTES * literals;
    if (bytes > MAXIMUM_WORKSPACE - result.workspace) return false;
    ++result.clauses;
    result.literals += literals;
    result.workspace += bytes;
    return true;
  };

  graph_labeling::Shape shape;
  if (!graph_labeling::recognize(kernel, MAXIMUM_VERTICES,
                                 MAXIMUM_WORKSPACE, charge, shape) ||
      result.stop != DiscoveryStop::None)
    return false;
  result.vertices = shape.vertices;

  std::map<ClauseKey, Source> clauses;
  std::vector<std::pair<ClauseKey, Source>> ternaries;
  for (Ref ref = 0; ref < kernel.arena.size();) {
    const Clause *clause = clause_at(kernel.arena, ref);
    ref += clause_actual_words(clause);
    if (!charge(1)) return false;
    if (clause->garbage()) continue;
    if (!charge(clause->size)) return false;
    if (!retain_source(clause->size)) {
      result.stop = DiscoveryStop::Resource;
      return false;
    }
    ClauseKey literals(clause->begin(), clause->end());
    std::sort(literals.begin(), literals.end());
    const Source source{clause->id(), clause->redundant()};
    clauses.emplace(literals, source);
    if (literals.size() == 3) ternaries.emplace_back(literals, source);
  }
  for (Lit literal = 0; literal < 2 * kernel.nvars; ++literal) {
    for (WatchEntry watch : kernel.watches.entries(literal)) {
      if (!charge(1)) return false;
      if (!watch.binary() || literal >= watch.other()) continue;
      if (!retain_source(2)) {
        result.stop = DiscoveryStop::Resource;
        return false;
      }
      ClauseKey literals{literal, watch.other()};
      std::sort(literals.begin(), literals.end());
      clauses.emplace(literals,
                      Source{kernel_binary_id(kernel, literal, watch.other()),
                             false});
    }
  }

  const auto lookup = [&](const ClauseKey &clause) -> const Source * {
    const auto found = clauses.find(clause);
    return found == clauses.end() ? nullptr : &found->second;
  };
  const auto explain = [&](Lit input, Lit other, Lit output,
                           Explanation &explanation) {
    if (const Source *direct = lookup(key({negate(input), negate(output)}))) {
      explanation.parents[0] = direct->id;
      explanation.count = 1;
      explanation.direct = true;
      return direct->id != 0;
    }
    const Source *last =
        lookup(key({other, negate(input), negate(output)}));
    const Source *first = lookup(key({negate(other), negate(input)}));
    if (!first) first = lookup(key({negate(other), negate(output)}));
    if (!first || !last || !first->id || !last->id) return false;
    explanation.parents = {first->id, last->id};
    explanation.count = 2;
    return true;
  };

  std::map<Lit, Gate> definitions;
  std::set<Lit> ambiguous_outputs;
  for (const auto &[literals, source] : ternaries) {
    if (!charge(4) || !source.id) return false;
    if (var_of(literals[0]) == var_of(literals[1]) ||
        var_of(literals[0]) == var_of(literals[2]) ||
        var_of(literals[1]) == var_of(literals[2]))
      continue;
    Lit output = literals[0];
    for (Lit literal : literals)
      if (kernel_externalize(kernel, literal) >
          kernel_externalize(kernel, output))
        output = literal;
    const int32_t external_output = kernel_externalize(kernel, output);
    if (external_output <= 0) continue;
    bool strictly_largest = true;
    for (Lit literal : literals)
      if (literal != output &&
          std::abs(kernel_externalize(kernel, literal)) >= external_output)
        strictly_largest = false;
    if (!strictly_largest) continue;
    Lit inputs[2];
    unsigned next = 0;
    for (Lit literal : literals)
      if (literal != output) inputs[next++] = literal;
    Gate gate{output, inputs[0], inputs[1], source.id, {}, {}};
    if (!explain(gate.left, gate.right, gate.output, gate.left_reverse) ||
        !explain(gate.right, gate.left, gate.output, gate.right_reverse))
      continue;
    if (ambiguous_outputs.count(output)) continue;
    const auto [position, inserted] = definitions.emplace(output, gate);
    if (!inserted) {
      if (position->second.left == gate.left &&
          position->second.right == gate.right)
        continue; // Repeated copies describe the same unique gate.
      definitions.erase(position);
      ambiguous_outputs.insert(output);
    }
  }
  std::vector<Gate> gates;
  gates.reserve(definitions.size());
  for (auto &[output, gate] : definitions) gates.push_back(std::move(gate));
  std::sort(gates.begin(), gates.end(), [&](const Gate &a, const Gate &b) {
    return kernel_externalize(kernel, a.output) <
           kernel_externalize(kernel, b.output);
  });
  result.gates = gates.size();

  std::map<Lit, Interval> meanings;
  for (uint32_t row = 0; row < shape.vertices; ++row)
    for (uint32_t column = 0; column < shape.vertices; ++column) {
      const Lit literal = shape.primary[uint64_t(row) * shape.vertices + column];
      if (literal == INVALID_LIT) return false;
      meanings.emplace(literal, Interval{row, column, column});
    }

  std::map<std::tuple<uint32_t, uint32_t, uint32_t>, std::vector<Gate>> groups;
  for (const Gate &gate : gates) {
    if (!charge(1)) return false;
    auto left = meanings.find(gate.left);
    if (left == meanings.end()) left = meanings.find(negate(gate.left));
    auto right = meanings.find(gate.right);
    if (right == meanings.end()) right = meanings.find(negate(gate.right));
    if (left == meanings.end() || right == meanings.end()) continue;
    // A derived absence literal occurs negated as the next gate's input. A
    // primary selection literal occurs with its recovered cover-clause sign.
    const bool left_base = std::find(shape.primary.begin(), shape.primary.end(),
                                     gate.left) != shape.primary.end();
    const bool right_base = std::find(shape.primary.begin(), shape.primary.end(),
                                      gate.right) != shape.primary.end();
    if (!left_base && meanings.find(negate(gate.left)) == meanings.end()) continue;
    if (!right_base && meanings.find(negate(gate.right)) == meanings.end()) continue;
    const Interval a = left_base ? meanings.at(gate.left)
                                 : meanings.at(negate(gate.left));
    const Interval b = right_base ? meanings.at(gate.right)
                                  : meanings.at(negate(gate.right));
    if (a.row != b.row || a.last + 1 < b.first || b.last + 1 < a.first)
      continue;
    const Interval combined{a.row, std::min(a.first, b.first),
                            std::max(a.last, b.last)};
    if (!meanings.emplace(gate.output, combined).second) return false;
    groups[{combined.row, combined.first, combined.last}].push_back(gate);
    ++result.intervals;
  }

  for (const auto &[meaning, equivalent] : groups) {
    if (equivalent.size() != 2) continue;
    Candidate candidate;
    candidate.interval = {std::get<0>(meaning), std::get<1>(meaning),
                          std::get<2>(meaning)};
    candidate.first = equivalent[0];
    candidate.second = equivalent[1];
    if (kernel_externalize(kernel, candidate.first.output) >
        kernel_externalize(kernel, candidate.second.output))
      std::swap(candidate.first, candidate.second);
    result.candidates.push_back(std::move(candidate));
  }
  return !result.candidates.empty();
}

struct PlannedClause {
  Lit first = INVALID_LIT;
  Lit second = INVALID_LIT;
  std::vector<uint64_t> chain;
};

ApplyResult apply_selected(Kernel &kernel, uint64_t maximum_groups) {
  if (!graph_interval_equivalence_guard(kernel)) return ApplyResult::NoOp;
  if (!maximum_groups || maximum_groups > MAXIMUM_GROUPS)
    return ApplyResult::NoOp;
  const uint64_t maximum_work = kernel_has_work_budget(kernel)
                                    ? std::min(MAXIMUM_WORK, kernel.work_budget)
                                    : MAXIMUM_WORK;
  const uint64_t maximum_additions = 2 * maximum_groups;
  ++kernel.stats.graph_interval_equivalence_calls;
  Discovery discovery;
  const bool discovered = discover(kernel, maximum_work, discovery);
  kernel.stats.graph_interval_equivalence_work += discovery.work;
  kernel.stats.graph_interval_equivalence_source_clauses += discovery.clauses;
  kernel.stats.graph_interval_equivalence_source_literals += discovery.literals;
  kernel.stats.graph_interval_equivalence_source_workspace += discovery.workspace;
  if (discovery.stop == DiscoveryStop::Resource)
    ++kernel.stats.graph_interval_equivalence_resource_exhausted;
  if (!discovered)
    return ApplyResult::NoOp;
  ++kernel.stats.graph_interval_equivalence_recognized;
  kernel.stats.graph_interval_equivalence_vertices += discovery.vertices;
  kernel.stats.graph_interval_equivalence_gates += discovery.gates;
  kernel.stats.graph_interval_equivalence_intervals += discovery.intervals;
  kernel.stats.graph_interval_equivalence_groups += discovery.candidates.size();

  std::vector<const Candidate *> selected;
  const size_t count = static_cast<size_t>(
      std::min<uint64_t>(maximum_groups, discovery.candidates.size()));
  for (size_t i = 0; i < count; ++i) selected.push_back(&discovery.candidates[i]);
  if (selected.empty()) return ApplyResult::NoOp;

  std::vector<PlannedClause> planned;
  planned.reserve(2 * selected.size());
  const uint64_t ticks_before = kernel.stats.ticks;
  for (const Candidate *candidate : selected) {
    const Lit a = candidate->first.output, b = candidate->second.output;
    for (const std::pair<Lit, Lit> &clause :
         {std::pair<Lit, Lit>{negate(a), b}, {a, negate(b)}}) {
      if (kernel_binary_live(kernel, clause.first, clause.second)) {
        ++kernel.stats.graph_interval_equivalence_duplicates;
        continue;
      }
      PlannedClause plan{clause.first, clause.second, {}};
      const Lit literals[2] = {plan.first, plan.second};
      if (!congruence_replay_rup(kernel, literals, 2, plan.chain,
                                 RupReplay::Decide)) {
        ++kernel.stats.graph_interval_equivalence_rup_rejected;
        return ApplyResult::NoOp;
      }
      planned.push_back(std::move(plan));
      if (planned.size() > maximum_additions) return ApplyResult::NoOp;
    }
  }
  const uint64_t replay_ticks = kernel.stats.ticks - ticks_before;
  kernel.stats.graph_interval_equivalence_rup_ticks += replay_ticks;
  kernel.stats.graph_interval_equivalence_work += replay_ticks;
  if (planned.empty()) return ApplyResult::NoOp;
  if (kernel.next_clause_id > uint64_t(INT64_MAX) - planned.size())
    return ApplyResult::NoOp;

  const uint64_t action = kernel.structural_action_sequence + 1;
  FILE *out = kernel.structural_provenance;
  std::fprintf(out,
      "{\"action\":%llu,\"kind\":\"action-context\",\"version\":1,"
      "\"rule\":\"GraphIntervalEquivalence\",\"strategy_epoch\":%llu,"
      "\"arena_generation\":%llu,\"touch_epoch\":%llu,"
      "\"proof_id_before\":%llu,\"external_extent\":%zu,"
      "\"original_variables\":%u,\"existing_variables_only\":true,"
      "\"propagation\":\"separate-action\",\"maximum_groups\":%llu,"
      "\"maximum_work\":%llu,\"maximum_additions\":%llu,"
      "\"maximum_source_clauses\":%llu,\"maximum_source_literals\":%llu,"
      "\"maximum_source_workspace\":%llu}\n",
      (unsigned long long)action, (unsigned long long)kernel.strategy_epoch,
      (unsigned long long)kernel.arena_generation,
      (unsigned long long)kernel.touch_epoch,
      (unsigned long long)kernel.next_clause_id,
      kernel.external_to_internal.size(), kernel_num_output_vars(kernel),
      (unsigned long long)maximum_groups,
      (unsigned long long)maximum_work,
      (unsigned long long)maximum_additions,
      (unsigned long long)MAXIMUM_SOURCE_CLAUSES,
      (unsigned long long)MAXIMUM_SOURCE_LITERALS,
      (unsigned long long)MAXIMUM_WORKSPACE);
  for (const Candidate *candidate : selected) {
    const auto print_gate = [&](const char *name, const Gate &gate) {
      std::fprintf(out,
          "\"%s\":{\"output\":%d,\"inputs\":[%d,%d],"
          "\"defining\":%llu,\"left_reverse\":[",
          name, kernel_externalize(kernel, gate.output),
          kernel_externalize(kernel, gate.left),
          kernel_externalize(kernel, gate.right),
          (unsigned long long)gate.defining);
      for (uint8_t i = 0; i < gate.left_reverse.count; ++i)
        std::fprintf(out, "%s%llu", i ? "," : "",
                     (unsigned long long)gate.left_reverse.parents[i]);
      std::fputs("],\"right_reverse\":[", out);
      for (uint8_t i = 0; i < gate.right_reverse.count; ++i)
        std::fprintf(out, "%s%llu", i ? "," : "",
                     (unsigned long long)gate.right_reverse.parents[i]);
      std::fputs("]}", out);
    };
    std::fprintf(out,
        "{\"action\":%llu,\"kind\":\"authenticated-duplicate-interval\","
        "\"row\":%u,\"first\":%u,\"last\":%u,",
        (unsigned long long)action, candidate->interval.row,
        candidate->interval.first, candidate->interval.last);
    print_gate("first_gate", candidate->first);
    std::fputc(',', out);
    print_gate("second_gate", candidate->second);
    std::fputs("}\n", out);
  }

  const bool lrat = kernel.proof && kernel.proof->lrat();
  for (const PlannedClause &plan : planned) {
    const uint64_t id = ++kernel.next_clause_id;
    const Lit literals[2] = {plan.first, plan.second};
    if (kernel.proof)
      kernel.proof->add_rup(lrat ? id : 0, literals, 2,
                            lrat ? plan.chain.data() : nullptr,
                            lrat ? static_cast<uint32_t>(plan.chain.size()) : 0);
    kernel_add_binary_clause(kernel, plan.first, plan.second, id);
    std::fprintf(out,
        "{\"action\":%llu,\"kind\":\"publish-interval-equivalence\","
        "\"id\":%llu,\"clause\":[%d,%d],\"rup_hints\":%zu}\n",
        (unsigned long long)action, (unsigned long long)id,
        kernel_externalize(kernel, plan.first),
        kernel_externalize(kernel, plan.second), plan.chain.size());
  }
  kernel.structural_action_sequence = action;
  kernel.stats.graph_interval_equivalence_selected += selected.size();
  kernel.stats.graph_interval_equivalence_published += planned.size();
  kernel.stats.graph_interval_equivalence_proof_literal_occurrences +=
      2 * planned.size();
  std::fprintf(out,
      "{\"action\":%llu,\"kind\":\"action-complete\","
      "\"result\":0,\"discovery_work\":%llu,\"rup_ticks\":%llu,"
      "\"source_clauses\":%llu,\"source_literals\":%llu,"
      "\"source_workspace\":%llu,"
      "\"recognized_gates\":%llu,\"intervals\":%llu,\"groups\":%zu,"
      "\"selected\":%zu,\"proof_additions\":%zu,"
      "\"proof_literal_occurrences\":%zu,\"proof_id_after\":%llu}\n",
      (unsigned long long)action, (unsigned long long)discovery.work,
      (unsigned long long)replay_ticks,
      (unsigned long long)discovery.clauses,
      (unsigned long long)discovery.literals,
      (unsigned long long)discovery.workspace,
      (unsigned long long)discovery.gates,
      (unsigned long long)discovery.intervals, discovery.candidates.size(),
      selected.size(), planned.size(), 2 * planned.size(),
      (unsigned long long)kernel.next_clause_id);
  std::fflush(out);
  if (std::ferror(out)) std::abort();
  return ApplyResult::Ok;
}

uint32_t instances(Kernel &, uint64_t *out, uint32_t cap) {
  if (!cap) return 0;
  out[0] = INSTANCE_CANONICAL;
  return 1;
}

ApplyResult apply_instance(Kernel &kernel, uint64_t instance) {
  if (instance == INSTANCE_CANONICAL)
    return apply_selected(kernel, DEFAULT_GROUPS);
  if (!instance || instance > MAXIMUM_GROUPS) return ApplyResult::NoOp;
  return apply_selected(kernel, instance);
}

} // namespace

RuleEligibility graph_interval_equivalence_guard(const Kernel &kernel) {
  return RuleEligibility{}
      .fact("consistent", !kernel.inconsistent)
      .fact("root", kernel.level == 0)
      .fact("no-conflict", !kernel.conflict_valid && !kernel.pending.valid)
      .fact("propagated", kernel_propagation_fixpoint(kernel))
      .fact("sparse", !kernel.dense && kernel.large_watches_connected)
      .fact("no-probe-transaction", !kernel.probing.recording)
      .fact("provenance-bound", kernel.structural_provenance != nullptr)
      .fact("action-id-available", kernel.structural_action_sequence != UINT64_MAX)
      .fact("clausal-proof", !kernel.proof || !kernel.proof->terminal_pbp());
}

ApplyResult graph_interval_equivalence_apply(Kernel &kernel) {
  if (!graph_interval_equivalence_guard(kernel)) return ApplyResult::NoOp;
  return apply_selected(kernel, DEFAULT_GROUPS);
}

TACTIX_REGISTER_RULE(GraphIntervalEquivalence, RuleTier::Inprocess,
                     graph_interval_equivalence_guard,
                     graph_interval_equivalence_apply, instances,
                     apply_instance);

} // namespace tactix

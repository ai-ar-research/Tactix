// A journal stream and the discard journal take the same search: DefaultPolicy
// solves each formula once per binding, as the CLI sets it up without a proof
// file, and every count it reports agrees. The stream arm must write records.
#include "unit_test.hpp"
#include "dimacs_text.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "strategy/structural_action_sessions.hpp"
#include "strategy/theory_action_parameters.hpp"

#include <random>

using Clauses = std::vector<std::vector<int32_t>>;
using ReasonMode = DefaultStrategyConfig::AffineSearch::ReasonMode;

void add_xor(Clauses &clauses, std::vector<int32_t> variables, bool rhs) {
  std::sort(variables.begin(), variables.end());
  for (auto &clause : affine_semantic_extension::clausal_form(variables, rhs))
    clauses.push_back(clause);
}

std::vector<int32_t> sample(std::mt19937_64 &random, uint32_t variables,
                            size_t count) {
  std::vector<int32_t> result;
  while (result.size() < count) {
    const int32_t v = int32_t(1 + random() % variables);
    if (std::find(result.begin(), result.end(), v) == result.end())
      result.push_back(v);
  }
  return result;
}

// Tseitin on K4 with odd charge: a startup parity refutation.
std::string tseitin() {
  Clauses clauses;
  const int32_t edge[4][4] = {
      {0, 1, 2, 3}, {1, 0, 4, 5}, {2, 4, 0, 6}, {3, 5, 6, 0}};
  for (int v = 0; v < 4; ++v) {
    std::vector<int32_t> incident;
    for (int u = 0; u < 4; ++u)
      if (u != v) incident.push_back(edge[v][u]);
    add_xor(clauses, incident, v == 0);
  }
  return dimacs_text(6, clauses);
}

// Two odd triangles, which TseitinCycleBreaking refutes, beside random 3-SAT.
std::string triangles() {
  Clauses clauses;
  for (int32_t offset : {0, 3}) {
    add_xor(clauses, {offset + 1, offset + 3}, true);
    add_xor(clauses, {offset + 1, offset + 2}, false);
    add_xor(clauses, {offset + 2, offset + 3}, false);
  }
  std::mt19937_64 random(3);
  for (int i = 0; i < 120; ++i) {
    std::vector<int32_t> clause = sample(random, 40, 3);
    for (auto &literal : clause)
      literal = (random() & 1 ? 1 : -1) * (literal + 6);
    clauses.push_back(clause);
  }
  return dimacs_text(46, clauses);
}

// Consistent 4-XORs under random clauses: GF(2) reasons during the search.
std::string parity() {
  std::mt19937_64 random(1);
  std::vector<bool> solution(81);
  for (auto &&value : solution) value = random() & 1;
  Clauses clauses;
  for (int i = 0; i < 30; ++i) {
    const auto variables = sample(random, 80, 4);
    bool rhs = false;
    for (int32_t v : variables) rhs ^= solution[v];
    add_xor(clauses, variables, rhs);
  }
  for (int i = 0; i < 300; ++i) {
    std::vector<int32_t> clause = sample(random, 80, 3);
    for (auto &literal : clause) literal *= random() & 1 ? 1 : -1;
    clauses.push_back(clause);
  }
  return dimacs_text(80, clauses);
}

// R(4,4) > 17 on 18 vertices: the Ramsey startup certificate.
std::string ramsey() {
  int edge[18][18] = {};
  int32_t v = 0;
  for (int a = 0; a < 18; ++a)
    for (int b = a + 1; b < 18; ++b) edge[a][b] = ++v;
  Clauses clauses;
  for (int a = 0; a < 18; ++a)
    for (int b = a + 1; b < 18; ++b)
      for (int c = b + 1; c < 18; ++c)
        for (int d = c + 1; d < 18; ++d)
          for (int32_t sign : {1, -1})
            clauses.push_back({sign * edge[a][b], sign * edge[a][c],
                               sign * edge[a][d], sign * edge[b][c],
                               sign * edge[b][d], sign * edge[c][d]});
  return dimacs_text(v, clauses);
}

struct Outcome {
  int answer;
  uint64_t conflicts, decisions, propagations, clause_ids, actions;
  uint64_t affine_services, affine_published, affine_deferred,
      affine_materialized, tseitin_journal_bytes;
  bool operator==(const Outcome &) const = default;
};

Outcome solve(const std::string &cnf, bool stream, ReasonMode reasons,
              bool cycle_breaking = false) {
  Kernel kernel;
  StructuralActionSessions sessions(kernel);
  FILE *file = stream ? std::tmpfile() : nullptr;
  kernel.structural_provenance =
      stream ? StructuralJournal(file) : StructuralJournal::discard();
  const long discarded = StructuralJournal::discard().tell();
  Proof proof;
  kernel.startup_certificate_open = true;
  assert(proof.open_nowhere());
  kernel.proof = &proof;
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);
  parse_dimacs_text(cnf, kernel);
  uint64_t tseitin_journal_bytes = 0;
  if (cycle_breaking) {
    auto &session = sessions.tseitin_cycle_breaking;
    const auto limits =
        strategy::action_limits(kernel.tseitin_cycle_breaking_parameters);
    const auto offer = tseitin_cycle_breaking_offer(kernel, session, limits);
    assert(offer.status == TseitinCycleBreakingStatus::Applied);
    const auto applied =
        tseitin_cycle_breaking_apply(kernel, session, limits, offer.offers[0]);
    assert(applied.status == TseitinCycleBreakingStatus::Applied);
    tseitin_journal_bytes = applied.effort.journal_bytes;
  }
  DefaultStrategyConfig config;
  config.affine_search.reason_mode = reasons;
  DefaultPolicy policy(config);
  const int answer = policy.solve(kernel);
  assert(answer == 10 || answer == 20);
  const long written = stream ? std::ftell(file)
                              : StructuralJournal::discard().tell() - discarded;
  assert(written > 0);
  const auto &affine = policy.affine_search_diagnostics();
  const Outcome outcome{answer,
                        kernel.stats.conflicts,
                        kernel.stats.decisions,
                        kernel.stats.propagations,
                        kernel.next_clause_id,
                        kernel.structural_action_sequence,
                        affine.search_services,
                        affine.reasons_published,
                        affine.reasons_deferred,
                        affine.reasons_materialized,
                        tseitin_journal_bytes};
  kernel.proof = nullptr;
  kernel.structural_provenance = {};
  if (file) std::fclose(file);
  return outcome;
}

Outcome same(const std::string &cnf, ReasonMode reasons = ReasonMode::LazyMinimal,
             bool cycle_breaking = false) {
  const Outcome streamed = solve(cnf, true, reasons, cycle_breaking);
  assert(solve(cnf, false, reasons, cycle_breaking) == streamed);
  return streamed;
}

int main() {
  assert(same(tseitin()).answer == 20);
  assert(same(ramsey()).answer == 20);
  assert(same(triangles(), ReasonMode::LazyMinimal, true).tseitin_journal_bytes);
  assert(same(parity()).affine_services);
  assert(same(parity(), ReasonMode::Deferred).affine_materialized);
  std::printf("structural journal trajectory: stream and discard agree\n");
}

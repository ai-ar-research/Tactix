#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/parity_refutation.hpp"
#define tactix tactix_handle
#include "tactix.h"
#undef tactix

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdio>
#include <string>
#include <sstream>
#include <vector>

using namespace tactix;

static constexpr uint64_t ACTIVE_LUCKY_INSTANCES[] = {
    LUCKY_ALL_TRUE, LUCKY_ALL_FALSE, LUCKY_FORWARD_FALSE,
    LUCKY_FORWARD_TRUE, LUCKY_BACKWARD_FALSE, LUCKY_BACKWARD_TRUE,
    15, 16, LUCKY_STARTUP_PARITY, LUCKY_STARTUP_COUNT,
    LUCKY_STARTUP_RAMSEY, LUCKY_PARITY_PROOF, LUCKY_EXACT_ONE_PROOF};

static std::string graph_formula(unsigned vertices, bool odd, bool renamed) {
  std::vector<std::vector<int>> incident(vertices), clauses;
  int variables = 0;
  for (unsigned a = 0; a < vertices; ++a)
    for (unsigned b = a + 1; b < vertices; ++b) {
      incident[a].push_back(++variables);
      incident[b].push_back(variables);
    }
  for (unsigned a = 0; a < vertices; ++a) {
    const auto &scope = incident[a];
    for (unsigned mask = 0; mask < (1u << scope.size()); ++mask) {
      if (bool(std::popcount(mask) & 1) == (odd && a == 0))
        continue;
      std::vector<int> clause;
      for (unsigned i = 0; i < scope.size(); ++i) {
        int literal = mask & (1u << i) ? -scope[i] : scope[i];
        if (renamed) {
          const int variable = std::abs(literal);
          const int polarity = ((variable & 1) != (literal < 0)) ? -1 : 1;
          literal = polarity * (variables + 1 - variable);
        }
        clause.push_back(literal);
      }
      std::reverse(clause.begin(), clause.end());
      clauses.push_back(std::move(clause));
    }
  }
  if (renamed)
    std::reverse(clauses.begin(), clauses.end());
  std::string cnf = "p cnf " + std::to_string(variables) + " " +
                    std::to_string(clauses.size()) + "\n";
  for (const auto &clause : clauses) {
    for (const int literal : clause)
      cnf += std::to_string(literal) + " ";
    cnf += "0\n";
  }
  return cnf;
}

static void parse(const std::string &cnf, Kernel &k) {
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(cnf.data(), 1, cnf.size(), input) == cnf.size());
  std::rewind(input);
  assert(!parse_dimacs(input, k));
  std::fclose(input);
}

int main(int argc, char **argv) {
  for (unsigned vertices : {3, 4, 5})
    for (bool renamed : {false, true}) {
      const std::string cnf = graph_formula(vertices, true, renamed);
      const std::string stem = argc > 1 ? std::string(argv[1]) + "/k" +
                              std::to_string(vertices) + (renamed ? "r" : "") : "";
      Kernel k;
      Proof proof;
      if (!stem.empty()) {
        FILE *input = std::fopen((stem + ".cnf").c_str(), "wb");
        assert(input);
        assert(std::fwrite(cnf.data(), 1, cnf.size(), input) == cnf.size());
        std::fclose(input);
        assert(proof.open((stem + ".pbp").c_str(), ProofFormat::Automatic));
        proof.bind_variable_map(&k.internal_to_external);
        k.proof = &proof;
      }
      parse(cnf, k);
      assert(startup_parity_guard(k));
      const auto &lucky = tactix_rules()[tactix_rule_id("Lucky")];
      uint64_t instances[32];
      const unsigned count = lucky.instances(k, instances, 32);
      assert(std::find(instances, instances + count, LUCKY_STARTUP_PARITY) !=
             instances + count);
      assert(apply_rule_instance(lucky, k, LUCKY_STARTUP_PARITY) == ApplyResult::Unsat);
      assert(k.inconsistent && !k.startup_certificate_open);
      assert(!k.proof || k.proof->pb());
      assert(!startup_parity_guard(k));
      Kernel sat;
      parse(graph_formula(vertices, false, renamed), sat);
      assert(startup_parity_apply(sat) == ApplyResult::NoOp);
      assert(!sat.inconsistent);
    }
  for (bool stop : {false, true}) {
    Kernel k;
    parse(graph_formula(4, true, false), k);
    volatile sig_atomic_t interrupted = 1;
    if (stop)
      k.stop = &interrupted;
    else
      k.parity_refutation_maximum_proof_additions = 1;
    const auto before = k.arena.size();
    assert(startup_parity_apply(k) == ApplyResult::NoOp);
    assert(k.startup_certificate_open && !k.inconsistent);
    assert(k.arena.size() == before);
    assert(!k.stats.parity_refutation_unsat);
  }
  // The C convenience API and typed rule dispatch consume one shared boundary.
  for (bool decide : {false, true}) {
    tactix_handle *solver = tactix_init();
    std::istringstream input(graph_formula(4, true, false));
    std::string header;
    std::getline(input, header);
    int literal;
    while (input >> literal)
      tactix_add(solver, literal);
    if (decide)
      assert(tactix_decide(solver, 1) == 0);
    const int lucky = tactix::tactix_rule_id("Lucky");
    // tactix_rule_id here is the internal lookup; public IDs are the same.
    assert(tactix_apply_instance(solver, lucky, LUCKY_STARTUP_PARITY) ==
           (decide ? -1 : 20));
    tactix_release(solver);
  }
  if (argc > 1) {
    const int lucky_id = tactix_rule_id("Lucky");
    tactix_handle *proofless = tactix_init();
    tactix_handle *strict = tactix_init();
    const std::string path = std::string(argv[1]) + "/lucky-strict.dsr";
    assert(tactix_proof(strict, path.c_str(), TACTIX_PROOF_DSR_BINARY) == 0);
    uint64_t no_proof_instances[LUCKY_INSTANCE_COUNT];
    uint64_t strict_instances[LUCKY_INSTANCE_COUNT];
    const int no_proof_count =
        tactix_rule_instances(proofless, lucky_id, no_proof_instances,
                              LUCKY_INSTANCE_COUNT);
    const int strict_count =
        tactix_rule_instances(strict, lucky_id, strict_instances,
                              LUCKY_INSTANCE_COUNT);
    assert(no_proof_count == int(LUCKY_ACTIVE_INSTANCE_COUNT) &&
           strict_count == no_proof_count);
    for (int i = 0; i < strict_count; ++i)
      assert(no_proof_instances[i] == strict_instances[i] &&
             strict_instances[i] == ACTIVE_LUCKY_INSTANCES[i] &&
             !lucky_retired_constructor_instance(strict_instances[i]));
    assert(tactix_apply_instance(strict, lucky_id, LUCKY_STARTUP_PARITY) == 1);
    assert(tactix_apply_instance(strict, lucky_id, LUCKY_STARTUP_COUNT) == 1);
    tactix_release(strict);
    FILE *stream = std::fopen(path.c_str(), "rb");
    assert(stream && std::fseek(stream, 0, SEEK_END) == 0);
    assert(std::ftell(stream) == 0); // both unsupported actions are byte-invisible
    std::fclose(stream);
    tactix_release(proofless);
  }
  Kernel closed;
  parse(graph_formula(4, true, false), closed);
  apply_rule(tactix_rules()[tactix_rule_id("Propagate")], closed);
  assert(!startup_parity_guard(closed));
  // An explicit clausal writer closes the PB boundary, but it must not
  // renumber the public Lucky action space seen by replay/random policies.
  const auto &lucky = tactix_rules()[tactix_rule_id("Lucky")];
  uint64_t available[LUCKY_INSTANCE_COUNT];
  Kernel clausal;
  clausal.close_startup_certificate();
  for (Kernel *state : {&closed, &clausal}) {
    assert(lucky.instances(*state, available, LUCKY_INSTANCE_COUNT) ==
           LUCKY_ACTIVE_INSTANCE_COUNT);
    for (uint64_t index = 0; index < LUCKY_ACTIVE_INSTANCE_COUNT; ++index)
      assert(available[index] == ACTIVE_LUCKY_INSTANCES[index]);
    assert(lucky.instances(*state, available, 8) == 8);
    assert(apply_rule_instance(lucky, *state, LUCKY_STARTUP_PARITY) ==
           ApplyResult::NoOp);
    assert(apply_rule_instance(lucky, *state, LUCKY_STARTUP_COUNT) ==
           ApplyResult::NoOp);
  }
  std::puts("startup parity: arity 2/3/4, signed renaming, SAT controls, boundary OK");
}

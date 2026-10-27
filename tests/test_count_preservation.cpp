#undef NDEBUG
#include "temporary_file.hpp"
#include "frontend/parse.hpp"
#include "rules/lucky/count_preservation.hpp"
#include "rules/lucky/lucky.hpp"
#define tactix tactix_handle
#include "tactix.h"
#undef tactix
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <sstream>
#include <vector>
using namespace tactix;

// Two complete permutation selector layers, with arbitrary signed relabeling.
// Adjacent count-preserving stars share the middle leaf variables.
static std::string formula(unsigned width, bool unsat, bool rename) {
  std::vector<std::vector<int>> clauses;
  int variables = 3 * width;
  for (unsigned layer = 0; layer < 2; ++layer) {
    std::vector<std::vector<int>> selectors(width, std::vector<int>(width));
    for (unsigned a = 0; a < width; ++a)
      for (unsigned b = a; b < width; ++b)
        selectors[a][b] = selectors[b][a] = ++variables;
    for (unsigned a = 0; a < width; ++a) {
      clauses.push_back(selectors[a]);
      for (unsigned b = 0; b < width; ++b) {
        for (unsigned c = 0; c < b; ++c)
          clauses.push_back({-selectors[a][b], -selectors[a][c]});
        int x = layer * width + a + 1, y = (layer + 1) * width + b + 1;
        clauses.push_back({-selectors[a][b], -x, y});
        clauses.push_back({-selectors[a][b], x, -y});
      }
    }
  }
  // Avoid immediate root refutation: exactly one input true; require two
  // output truths for UNSAT. SAT controls keep the same one-true boundary.
  for (unsigned a = 0; a < width; ++a) {
    int in = a + 1, out = 2 * width + a + 1;
    clauses.push_back({a == 0 ? in : -in});
    clauses.push_back({a < (unsat ? 2u : 1u) ? out : -out});
  }
  if (rename) {
    for (auto &clause : clauses) {
      for (int &lit : clause) {
        const int v = std::abs(lit);
        const bool negative = (lit < 0) != bool(v & 1);
        lit = (negative ? -1 : 1) * (variables + 1 - v);
      }
      std::reverse(clause.begin(), clause.end());
    }
    std::reverse(clauses.begin(), clauses.end());
  }
  std::string text = "p cnf " + std::to_string(variables) + " " +
                     std::to_string(clauses.size()) + "\n";
  for (const auto &clause : clauses) {
    for (int lit : clause)
      text += std::to_string(lit) + " ";
    text += "0\n";
  }
  return text;
}
static void parse(const std::string &text, Kernel &k) {
  FILE *f = std::tmpfile();
  assert(f && std::fwrite(text.data(), 1, text.size(), f) == text.size());
  std::rewind(f);
  assert(!parse_dimacs(f, k));
  std::fclose(f);
}
int main(int argc, char **argv) {
  for (unsigned width : {3, 4})
    for (bool renamed : {false, true}) {
      const std::string text = formula(width, true, renamed);
      const std::string stem = argc > 1 ? std::string(argv[1]) + "/count" +
          std::to_string(width) + (renamed ? "r" : "") : "";
      Kernel k;
      Proof proof;
      if (!stem.empty()) {
        FILE *f = std::fopen((stem + ".cnf").c_str(), "wb");
        assert(f && std::fwrite(text.data(), 1, text.size(), f) == text.size());
        std::fclose(f);
        assert(proof.open((stem + ".pbp").c_str(), ProofFormat::PbpStreaming));
        proof.bind_variable_map(&k.internal_to_external);
        k.proof = &proof;
      }
      parse(text, k);
      assert(!k.inconsistent);
      assert(count_preservation_guard(k));
      const auto &lucky = tactix_rules()[tactix_rule_id("Lucky")];
      uint64_t instances[32];
      const unsigned count = lucky.instances(k, instances, 32);
      assert(std::find(instances, instances + count, LUCKY_STARTUP_COUNT) !=
             instances + count);
      assert(apply_rule_instance(lucky, k, LUCKY_STARTUP_COUNT) == ApplyResult::Unsat);
      assert(k.inconsistent && k.stats.counting_unsat == 1);
      assert(k.startup_input.empty() && !k.startup_certificate_open);
      Kernel sat;
      parse(formula(width, false, renamed), sat);
      assert(count_preservation_apply(sat) == ApplyResult::NoOp);
      assert(!sat.inconsistent);
    }
  const auto cnf = formula(3, true, true);
  for (bool stop : {false, true}) {
    Kernel k;
    parse(cnf, k);
    const auto original = k.startup_input;
    const auto arena_size = k.arena.size();
    volatile sig_atomic_t interrupted = 1;
    if (stop)
      k.stop = &interrupted;
    else
      k.work_budget = 1;
    assert(count_preservation_apply(k) == ApplyResult::NoOp);
    assert(!k.inconsistent && k.startup_certificate_open);
    assert(k.startup_input == original && k.arena.size() == arena_size);
    assert(!k.stats.counting_unsat);
  }
  for (ProofFormat format : {ProofFormat::DsrBinary, ProofFormat::DratBinary, ProofFormat::Lrat}) {
    Kernel k;
    TemporaryFile proof_file;
    Proof proof;
    assert(proof.open(proof_file.path.c_str(), format));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    parse(cnf, k);
    assert(count_preservation_apply(k) == ApplyResult::NoOp);
    assert(!k.inconsistent && !proof.pb());
  }
  for (bool ordinary : {false, true}) {
    tactix_handle *solver = tactix_init();
    std::istringstream input(cnf);
    std::string header;
    std::getline(input, header);
    int literal;
    while (input >> literal)
      tactix_add(solver, literal);
    if (ordinary)
      assert(tactix_apply(solver, tactix::tactix_rule_id("Restart")) >= 0);
    const int result = tactix_apply_instance(solver, tactix::tactix_rule_id("Lucky"),
                                            LUCKY_STARTUP_COUNT);
    assert(result == (ordinary ? 1 : 20));
    tactix_release(solver);
  }
  Kernel closed;
  parse(cnf, closed);
  apply_rule(tactix_rules()[tactix_rule_id("Propagate")], closed);
  assert(!count_preservation_guard(closed) && closed.startup_input.empty());
  std::puts("count preservation: signed/permuted two-layer networks and SAT controls OK");
}

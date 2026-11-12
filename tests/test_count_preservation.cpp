#undef NDEBUG
#include "dimacs_text.hpp"
#include "temporary_file.hpp"
#include "api/api_internal.hpp"
#include "frontend/dsr_follow_on.hpp"
#include "frontend/parse.hpp"
#include "rules/lucky/opportunities.hpp"
#include "rules/lucky/lucky.hpp"
#define tactix tactix_handle
#include "tactix.h"
#undef tactix
#include <algorithm>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <sstream>
#include <string>
#include <sys/stat.h>
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
static uint64_t file_size(const std::string &path) {
  struct stat st;
  assert(!stat(path.c_str(), &st));
  return static_cast<uint64_t>(st.st_size);
}

static bool dsr_route(const char *route) {
  return route && std::string(route).rfind("dsr-", 0) == 0;
}

// A counting refutation has no practical clausal spelling. Under a DSR proof the
// rule ends the run as it does with no proof, and nothing reaches the
// destination; a follow-on solve without the rule writes the DSR there. An
// interrupted follow-on leaves the proof pending.
static void dsr_follows_on(const std::string &cnf, const Kernel &unproved) {
  for (const bool stopped : {false, true}) {
    const ProofFormat format = stopped ? ProofFormat::DsrAscii : ProofFormat::DsrBinary;
    TemporaryFile destination;
    Kernel k;
    Proof proof;
    assert(proof.open(destination.path.c_str(), format));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    parse_dimacs_text(cnf, k);
    assert(count_preservation_apply(k) == ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.counting_unsat == 1);
    assert(k.stats.counting_stars == unproved.stats.counting_stars &&
           k.stats.counting_additions == unproved.stats.counting_additions &&
           k.stats.counting_work == unproved.stats.counting_work);
    assert(k.startup_input.empty() && !k.startup_certificate_open);
    assert(proof.startup_pb_written() && proof.dsr_follow_on_pending());
    assert(file_size(destination.path) == 0);
    volatile sig_atomic_t stop = stopped;
    const DsrFollowOn follow_on = run_dsr_follow_on(
        k, proof, DefaultStrategyConfig{}, [](Kernel &) {}, &stop);
    assert(follow_on.result == (stopped ? 0 : 20));
    assert(proof.dsr_follow_on_pending() == stopped);
    assert(stopped || (dsr_route(proof.verification_route()) &&
                       file_size(destination.path) > 0));
    k.proof = nullptr;
  }
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
      parse_dimacs_text(text, k);
      assert(!k.inconsistent);
      assert(count_preservation_guard(k));
      const auto &lucky = tactix_rules()[tactix_rule_id("Lucky")];
      assert(apply_rule_instance(lucky, k, LUCKY_STARTUP_COUNT) == ApplyResult::Unsat);
      assert(k.inconsistent && k.stats.counting_unsat == 1);
      assert(k.startup_input.empty() && !k.startup_certificate_open);
      Kernel sat;
      parse_dimacs_text(formula(width, false, renamed), sat);
      assert(count_preservation_apply(sat) == ApplyResult::NoOp);
      assert(!sat.inconsistent);
    }
  const auto cnf = formula(3, true, true);
  {
    // With no proof the refutation is the same, and its text is never formatted.
    Kernel k;
    Proof proof;
    assert(proof.open_nowhere());
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    parse_dimacs_text(cnf, k);
    assert(count_preservation_apply(k) == ApplyResult::Unsat);
    assert(k.inconsistent && k.stats.counting_unsat == 1 && k.stats.counting_additions > 0);
    k.proof = nullptr;
    dsr_follows_on(cnf, k);
  }
  for (bool stop : {false, true}) {
    Kernel k;
    parse_dimacs_text(cnf, k);
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
  const auto add_input = [&](tactix_handle *solver) {
    std::istringstream input(cnf);
    std::string header;
    std::getline(input, header);
    int literal;
    while (input >> literal)
      tactix_add(solver, literal);
  };
  for (bool ordinary : {false, true}) {
    tactix_handle *solver = tactix_init();
    add_input(solver);
    if (ordinary)
      assert(tactix_apply(solver, tactix::tactix_rule_id("Restart")) >= 0);
    const int result = tactix_apply_instance(solver, tactix::tactix_rule_id("Lucky"),
                                            LUCKY_STARTUP_COUNT);
    assert(result == (ordinary ? 1 : 20));
    tactix_release(solver);
  }
  // A DSR session reports the refutation once its follow-on wrote the proof,
  // after a direct application and in the default solve.
  for (const bool solve : {false, true}) {
    TemporaryFile scratch;
    const std::string path = argc > 1 ? std::string(argv[1]) + "/count3r-" +
                                            (solve ? "solve" : "apply") + ".dsr"
                                      : scratch.path;
    tactix_handle *solver = tactix_init();
    assert(!tactix_proof(solver, path.c_str(), TACTIX_PROOF_DSR_BINARY));
    add_input(solver);
    const int result =
        solve ? tactix_solve(solver)
              : tactix_apply_instance(solver, tactix::tactix_rule_id("Lucky"),
                                      LUCKY_STARTUP_COUNT);
    assert(result == 20);
    assert(api_detail::kernel_for_fixed_testing(solver).stats.counting_unsat == 1);
    assert(dsr_route(tactix_proof_route(solver)));
    tactix_release(solver);
    assert(file_size(path) > 0);
  }
  Kernel closed;
  parse_dimacs_text(cnf, closed);
  apply_rule(tactix_rules()[tactix_rule_id("Propagate")], closed);
  assert(!count_preservation_guard(closed) && closed.startup_input.empty());
  std::puts("count preservation: signed/permuted two-layer networks and SAT controls OK");
}

// Fixed complete-run transcripts compare shared execution with preserved
// native objects. Timings are excluded; requests, observations and work stay.
#undef NDEBUG
#include "policies/random_controller_policy.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

using namespace tactix;

static std::string fixture(Kernel &k, bool unsat) {
  kernel_reserve_vars(k, 3);
  std::vector<std::vector<int>> clauses = {{1, 2}, {-1, 2}, {1, 2, 3}};
  if (unsat) {
    clauses.push_back({1, -2});
    clauses.push_back({-1, -2});
  }
  std::ostringstream cnf;
  cnf << "p cnf 3 " << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    std::vector<Lit> lits;
    for (int lit : clause) {
      lits.push_back(internalize(lit)); cnf << lit << ' ';
    }
    cnf << "0\n";
    const auto id = ++k.next_clause_id;
    if (lits.size() == 2)
      kernel_add_binary_clause(k, lits[0], lits[1], id);
    else
      kernel_add_large_clause(k, lits.data(), lits.size(), false, 0, id);
  }
  return cnf.str();
}

static std::string read(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return {(std::istreambuf_iterator<char>(file)), {}};
}

static uint64_t hash(const std::string &text) {
  uint64_t value = 14695981039346656037ull;
  for (unsigned char byte : text) { value ^= byte; value *= 1099511628211ull; }
  return value;
}

int main(int argc, char **argv) {
  for (uint64_t seed : {1ull, 7ull, 17ull, 29ull, 41ull, 53ull, 67ull, 79ull})
    for (bool unsat : {false, true}) {
      Kernel k;
      const auto cnf = fixture(k, unsat);
      const bool retain = argc > 1;
      const std::string stem = retain
          ? std::string(argv[1]) + "/seed-" + std::to_string(seed) +
                (unsat ? "-unsat" : "-sat")
          : "/tmp/tactix-controller-signature-" + std::to_string(getpid());
      Proof proof;
      assert(proof.open((stem + ".drat").c_str(), ProofFormat::DratAscii));
      proof.bind_variable_map(&k.internal_to_external);
      k.proof = &proof;
      RandomControllerConfig config;
      config.parameters.rules = {"Vivify", "Congruence", "Substitute",
                                 "Strengthen", "Walk", "Rephase"};
      config.default_allowance.native.amount = 256;
      int verdict;
      {
        ControllerTrace trace;
        assert(trace.write((stem + ".trace").c_str(), seed,
                           controller_configuration(config)));
        RandomControllerPolicy policy(seed, config, &trace);
        verdict = policy.solve(k);
        assert(trace.error().empty());
      }
      assert(verdict == (unsat ? 20 : 10));
      kernel_check_invariants(k);
      k.proof = nullptr;
      proof.flush();
      std::istringstream input(read(stem + ".trace"));
      std::string line, deterministic;
      while (std::getline(input, line))
        if (line.rfind("elapsed-nanos ", 0) != 0)
          deterministic += line + '\n';
      if (retain) {
        std::ofstream(stem + ".cnf") << cnf;
        std::ofstream(stem + ".deterministic") << deterministic;
      }
      if (!unsat) {
        kernel_extend_model(k, false);
        assert(k.model_values[internalize(2)] > 0);
        std::ofstream model;
        if (retain) model.open(stem + ".model");
        if (retain) {
          model << "s SATISFIABLE\nv";
          for (Var v = 0; v < 3; ++v)
            model << ' ' << (k.model_values[make_lit(v, false)] > 0
                                 ? int(v + 1) : -int(v + 1));
          model << " 0\n";
        }
      }
      std::cout << "seed=" << seed << " verdict=" << verdict
                << " transcript=" << hash(deterministic)
                << " proof=" << hash(read(stem + ".drat")) << '\n';
      if (!retain) {
        unlink((stem + ".trace").c_str());
        unlink((stem + ".drat").c_str());
      }
    }
}

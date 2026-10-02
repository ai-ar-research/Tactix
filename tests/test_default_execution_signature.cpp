#undef NDEBUG
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

using namespace tactix;

static std::string pigeonhole(Kernel &k, unsigned pigeons, unsigned holes) {
  kernel_reserve_vars(k, pigeons * holes);
  std::ostringstream cnf;
  cnf << "p cnf " << pigeons * holes << ' '
      << pigeons + pigeons * (pigeons - 1) / 2 * holes << '\n';
  auto cell = [=](unsigned p, unsigned h) { return make_lit(p * holes + h, false); };
  for (unsigned p = 0; p < pigeons; ++p) {
    std::vector<Lit> clause;
    for (unsigned h = 0; h < holes; ++h) clause.push_back(cell(p, h));
    kernel_add_large_clause(k, clause.data(), holes, false, 0, ++k.next_clause_id);
    for (Lit lit : clause) cnf << externalize(lit) << ' ';
    cnf << "0\n";
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h) {
        kernel_add_binary_clause(k, negate(cell(p, h)), negate(cell(q, h)),
                                 ++k.next_clause_id);
        cnf << externalize(negate(cell(p, h))) << ' '
            << externalize(negate(cell(q, h))) << " 0\n";
      }
  }
  return cnf.str();
}

int main(int argc, char **argv) {
  uint64_t search_conflicts = 0, search_learned = 0;
  for (unsigned profile : {0u, 1u, 2u})
    for (unsigned pigeons : {4u, 6u}) {
      Kernel k;
      ScopedHeuristics h(k);
      const auto cnf = pigeonhole(k, pigeons, 5);
      const bool retain = argc > 1;
      const std::string stem = retain
          ? std::string(argv[1]) + "/profile-" + std::to_string(profile) +
                "-pigeons-" + std::to_string(pigeons)
          : "/tmp/tactix-default-signature-" + std::to_string(getpid());
      const std::string path = stem + ".drat";
      Proof proof;
      assert(proof.open(path.c_str(), ProofFormat::DratAscii));
      proof.bind_variable_map(&k.internal_to_external);
      k.proof = &proof;
      DefaultStrategyConfig config;
      if (profile) {
        // Native scheduler profiles isolate search with many atomic M3
        // transitions; profile zero still exercises the shipped preprocessing.
        config.inprocess.program.clear();
        config.inprocess.append_unlisted_rules = false;
        config.inprocess.structured_root_stages.clear();
        config.inprocess.structured_root_bounded_backtracking_stages.clear();
        config.inprocess.structured_root_large_domain_stages.clear();
        config.inprocess.initial_fast_elimination = false;
        config.rephase.walk_warmup = false;
        config.rephase.late_lucky = false;
        config.mode.force = profile;
        config.analysis.chronological_levels = profile == 2 ? 0 : 100;
        config.analysis.reuse_ranked_trail_suffix = profile == 1;
        config.restart.reluctant_period = 2;
        config.reduction.initial_interval = 5;
        config.rephase.initial_interval = 10;
        config.reorder.initial_interval = 10;
      }
      DefaultPolicy policy(config);
      const int verdict = policy.solve(k);
      assert(verdict == (pigeons == 6 ? 20 : 10));
      assert(!k.pending.valid && !k.conflict_valid);
      kernel_check_invariants(k);
      if (verdict == 10) {
        kernel_extend_model(k, false);
        for (unsigned p = 0; p < pigeons; ++p) {
          bool assigned = false;
          for (unsigned h = 0; h < 5; ++h) {
            const Lit lit = make_lit(p * 5 + h, false);
            if (k.model_values[lit] <= 0) continue;
            assigned = true;
            for (unsigned q = p + 1; q < pigeons; ++q)
              assert(k.model_values[make_lit(q * 5 + h, false)] < 0);
          }
          assert(assigned);
        }
      }
      proof.flush();
      std::ifstream input(path, std::ios::binary);
      assert(input);
      const std::string bytes{std::istreambuf_iterator<char>(input), {}};
      uint64_t hash = UINT64_C(14695981039346656037);
      for (unsigned char byte : bytes) hash = (hash ^ byte) * UINT64_C(1099511628211);
      if (retain) {
        std::ofstream(stem + ".cnf") << cnf;
        if (verdict == 10) {
          std::ofstream model(stem + ".model");
          model << "v ";
          for (unsigned v = 0; v < pigeons * 5; ++v)
            model << (k.model_values[make_lit(v, false)] > 0 ? int(v + 1) : -int(v + 1)) << ' ';
          model << "0\n";
        }
      } else assert(std::remove(path.c_str()) == 0);
      const auto &s = k.stats;
      std::cout << std::setprecision(17) << "profile=" << profile << " pigeons="
                << pigeons << " verdict=" << verdict << " proof=" << hash
                << ':' << bytes.size() << " work=" << s.decisions << ':'
                << s.conflicts << ':' << s.propagations << ':' << s.ticks << ':'
                << s.search_ticks << ':' << s.learned << ':' << s.learned_units
                << ':' << s.conflict_reused << ':' << s.chronological << ':'
                << s.restarts << ':' << s.restarts_reused << ':' << s.reductions
                << " state=" << k.level << ':' << k.propagate_head << ':'
                << k.next_clause_id;
      for (Lit lit : k.trail) std::cout << ':' << lit;
      std::cout << " phases=";
      for (auto phase : k.h->saved) std::cout << int(phase) << ',';
      std::cout << " scores=";
      for (auto score : k.h->scores.score) std::cout << score << ',';
      std::cout << '\n';
      search_conflicts += profile ? s.conflicts : 0;
      search_learned += profile ? s.learned : 0;
    }
  assert(search_conflicts && search_learned);
}

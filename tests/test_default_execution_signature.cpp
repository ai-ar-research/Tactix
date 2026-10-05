#undef NDEBUG
#include "diagnostics/rule_journal.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "strategy/feedback_trace.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
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
  char temporary[] = "/tmp/tactix-default-signature-XXXXXX";
  if (argc == 1) assert(mkdtemp(temporary));
  const std::string mode = argc == 3 ? argv[2] : "";
  assert(argc == 1 || argc == 2 || (argc == 3 &&
         (mode == "diagnostics" || mode == "feedback" || mode == "diagnostics-feedback")));
  const bool diagnostics = mode == "diagnostics" || mode == "diagnostics-feedback";
  const bool feedback = mode == "feedback" || mode == "diagnostics-feedback";
#if !defined(TACTIX_ENABLE_EVENT_TRACE) || !defined(TACTIX_ENABLE_RULE_JOURNAL)
  assert(!diagnostics);
#endif
  uint64_t search_conflicts = 0, search_learned = 0;
  for (unsigned profile : {0u, 1u, 2u, 3u, 4u, 5u, 6u})
    for (unsigned pigeons : {4u, 6u}) {
      Kernel k;
      ScopedHeuristics h(k);
      const auto cnf = pigeonhole(k, pigeons, 5);
      const bool retain = argc > 1;
      const std::string stem = retain
          ? std::string(argv[1]) + "/profile-" + std::to_string(profile) +
                "-pigeons-" + std::to_string(pigeons)
          : std::string(temporary) + "/case";
      const std::string path = stem + ".drat";
      if (diagnostics) {
        rule_journal_close();
        assert(setenv("TACTIX_RULE_JOURNAL", (stem + ".journal").c_str(), 1) == 0);
        assert(setenv("TACTIX_EVENT_TRACE", (stem + ".events").c_str(), 1) == 0);
      }
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
        config.mode.force = profile <= 2 ? profile : 2;
        config.analysis.chronological_levels = profile == 2 ? 0 : 100;
        config.analysis.reuse_ranked_trail_suffix = profile == 1;
        config.restart.reluctant_period = 2;
        config.reduction.initial_interval = 5;
        config.rephase.initial_interval = 10;
        config.reorder.initial_interval = 10;
        if (profile >= 3) {
          // Exercise support transactions that ordinary short fixtures do not
          // reach: complete warm-up, Walk handoff and automatic mode changes.
          config.rephase.walk_warmup = true;
          config.rephase.structural_walk_seed = false;
          config.rephase.stable_only = false;
          config.rephase.initial_interval = 1;
          config.rephase.interval = 1;
          config.rephase.rotation = {DefaultRephaseSlot::Walk};
          config.rephase.walk_permille = 1;
          if (profile == 4) {
            config.mode.force = 0;
            config.mode.initial_conflicts = 1;
            config.mode.interval = 1;
          }
          if (profile == 5) {
            config.rephase.rotation = {DefaultRephaseSlot::Best,
                DefaultRephaseSlot::Original, DefaultRephaseSlot::Inverted};
          }
          if (profile == 6) {
            // An explicit embedding-supplied advisory hint exercises native
            // guidance consumption without depending on a model producer.
            kernel_offer_local_search_hint(k, strategy::LocalSearchKind::Parity,
                                          std::vector<uint8_t>(k.nvars, 0));
            config.rephase.structural_walk_seed = true;
            config.rephase.structural_seed_rotation = {strategy::LocalSearchKind::Parity};
          }
        }
      }
      DefaultPolicy policy(config);
      strategy::controller::FeedbackTrace receiver;
      if (feedback) {
        assert(receiver.write((stem + ".feedback.jsonl").c_str(),
            "default-profile=" + std::to_string(profile) + ' ' + policy.strategy_config_snapshot()));
        policy.set_feedback(&receiver);
      }
      const int verdict = policy.solve(k);
      if (feedback) assert(receiver.terminal() && receiver.error().empty());
      if (diagnostics) rule_journal_close();
      assert(verdict == (pigeons == 6 ? 20 : 10));
      assert(!k.pending.valid && !k.conflict_valid);
      kernel_check_invariants(k);
      if (profile >= 3 && profile != 5 && verdict == 20) {
        assert(k.stats.warmups && k.stats.warming_decisions);
        assert(k.stats.walks && k.stats.walk_steps);
        if (profile == 4) assert(k.stats.switched);
        if (profile == 6) {
          assert(k.stats.walk_seed_imports);
          assert(!k.stats.local_search_target_install_attempts);
          assert(!k.stats.local_search_target_installs);
        }
      }
      if (profile == 5 && verdict == 20) assert(k.stats.rephased >= 3);
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
                << ':' << s.warmups << ':' << s.warming_decisions << ':'
                << s.warming_propagations << ':' << s.warming_conflicts << ':'
                << s.switched << ':' << s.walks << ':' << s.walk_steps << ':'
                << s.walk_improved << ':' << s.reordered << ':' << s.rephased << ':'
                << s.local_search_hints_offered << ':' << s.local_search_hints_applied
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
  if (argc == 1) assert(rmdir(temporary) == 0);
}

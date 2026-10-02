#undef NDEBUG
#include "diagnostics/rule_journal.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "strategy/feedback_trace.hpp"
#include "strategy/policy_completion.hpp"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

struct InputClause { std::vector<int> literals; bool redundant = false; uint32_t glue = 0; };

static std::vector<InputClause> fixture(unsigned family, bool unsat, unsigned &variables) {
  std::vector<InputClause> clauses;
  variables = 5;
  auto add = [&](std::initializer_list<int> lits) { clauses.push_back({lits}); };
  if (family == 0) { // A binary chain subsumes the large Vivify candidate.
    add({1, 2, 3}); add({1, 4}); add({-4, 2});
  } else if (family == 1) { // Congruent AND gates imply 3 == 4.
    for (int out : {3, 4}) {
      add({-out, 1}); add({-out, 2}); add({out, -1, -2});
    }
  } else if (family == 2) { // Binary-UIP Backbone derives -2 then -1.
    add({-1, 2}); add({-2, 3}); add({-2, -3});
    add({1, 2, 4});
  } else if (family == 3) { // Sweep equivalence requiring embedded decisions.
    for (int z : {-3, 3}) for (int w : {-4, 4}) {
      add({-1, 2, z, w}); add({1, -2, z, w});
    }
  } else if (family == 4) { // A productive native Substitute round/continuation.
    add({-1, 2}); add({-2, 1});
    add({1, -3, 2}); add({-1, 3, -2});
  } else { // Ordinary Eliminate with native BVE instances and session lifetime.
    add({1, 2}); add({-1, 3}); add({-2, 4, 5});
  }
  if (unsat) {
    constexpr unsigned pigeons = 5, holes = 4, offset = 5;
    variables = offset + pigeons * holes;
    auto cell = [](unsigned p, unsigned h) { return int(offset + p * holes + h + 1); };
    for (unsigned p = 0; p < pigeons; ++p) {
      InputClause clause;
      for (unsigned h = 0; h < holes; ++h) clause.literals.push_back(cell(p, h));
      clauses.push_back(std::move(clause));
      for (unsigned q = p + 1; q < pigeons; ++q)
        for (unsigned h = 0; h < holes; ++h)
          add({-cell(p, h), -cell(q, h)});
    }
  }
  return clauses;
}

static std::string load(Kernel &k, const std::vector<InputClause> &clauses, unsigned variables) {
  kernel_reserve_vars(k, variables);
  std::ostringstream cnf;
  cnf << "p cnf " << variables << ' ' << clauses.size() << '\n';
  for (const auto &clause : clauses) {
    std::vector<Lit> lits;
    for (int lit : clause.literals) { lits.push_back(internalize(lit)); cnf << lit << ' '; }
    cnf << "0\n";
    const auto id = ++k.next_clause_id;
    if (lits.size() == 2) kernel_add_binary_clause(k, lits[0], lits[1], id);
    else kernel_add_large_clause(k, lits.data(), lits.size(), clause.redundant, clause.glue, id);
  }
  return cnf.str();
}

static uint64_t productive(const Kernel &k, unsigned family) {
  const auto &s = k.stats;
  switch (family) {
  case 0: return s.vivify_subsumed + s.vivify_shrunken;
  case 1: return s.congruence_merges + s.congruence_units;
  case 2: return s.backbone_failed;
  case 3: return s.sweep_units + s.sweep_equivalences;
  case 4: return s.substitute_vars;
  default: return s.bve_eliminated;
  }
}

static void signature(const Kernel &k, const DefaultInprocessScheduler &scheduler,
                      const char *boundary, unsigned family, bool unsat) {
  std::cout << std::setprecision(17) << boundary << ':' << family << ':' << unsat << ':'
            << serialize_observation(observe_kernel(k)) << ":session=" << k.bve_definition_session
            << ":wires=" << k.inprocessing << ':' << k.dense << ':' << k.large_watches_connected
            << ':' << k.work_budget << ':' << k.work_budget_offset << ':' << k.work_budget_bounded
            << ':' << k.phase_saving_suppressed << ":trail=";
  for (Lit lit : k.trail) std::cout << lit << ',';
  std::cout << ":retired=";
  for (Var v = 0; v < k.nvars; ++v) std::cout << kernel_var_retired(k, v);
  std::cout << ":saved=";
  for (auto value : k.h->saved) std::cout << int(value) << ',';
  std::cout << ":target=";
  for (auto value : k.h->target) std::cout << int(value) << ',';
  std::cout << ":best=";
  for (auto value : k.h->best) std::cout << int(value) << ',';
  std::cout << ":heap=";
  for (auto value : k.h->scores.stack) std::cout << value << ',';
  std::cout << ":heap-pos=";
  for (auto value : k.h->scores.pos) std::cout << value << ',';
  std::cout << ":scores=";
  for (auto value : k.h->scores.score) std::cout << value << ',';
  std::cout << ":queue=" << k.h->queue_first << ':' << k.h->queue_last << ':'
            << k.h->queue_stamp << ':' << k.h->search_idx << ':' << k.h->search_stamp;
  for (const auto &link : k.h->links) std::cout << ',' << link.prev << ':' << link.next << ':' << link.stamp;
  std::cout << ":slots=";
  for (const auto &entry : scheduler.entries) {
    const auto &tm = entry.tm;
    std::cout << entry.step_id << ':' << entry.rule << ':' << entry.limit << ':'
              << entry.applies << ':' << entry.cursor << ':' << entry.doses << ':'
              << entry.delay_count << ':' << entry.delay_current << ':'
              << tm.occasions << ':' << tm.admitted << ':' << tm.handed << ':'
              << tm.spent << ':' << tm.ok << ':' << tm.completed << ',';
  }
  std::cout << '\n';
}

int main(int argc, char **argv) {
  char temporary[] = "/tmp/tactix-inprocess-signature-XXXXXX";
  if (argc == 1) assert(mkdtemp(temporary));
  const std::string mode = argc == 3 ? argv[2] : "";
  assert(argc == 1 || argc == 2 || (argc == 3 &&
      (mode == "feedback" || mode == "diagnostics" || mode == "diagnostics-feedback")));
  const bool feedback = mode == "feedback" || mode == "diagnostics-feedback";
  const bool diagnostics = mode == "diagnostics" || mode == "diagnostics-feedback";
  const std::string directory = argc > 1 ? argv[1] : temporary;
  constexpr const char *families[] = {"Vivify", "Congruence", "Backbone", "Sweep", "Substitute", "BVE"};
  for (unsigned family = 0; family < 6; ++family) for (bool unsat : {false, true}) {
    Kernel k;
    ScopedHeuristics h(k);
    unsigned variables;
    const auto clauses = fixture(family, unsat, variables);
    const auto cnf = load(k, clauses, variables);
    const std::string stem = directory + "/family-" + std::to_string(family) + (unsat ? "-unsat" : "-sat");
    if (diagnostics) {
      rule_journal_close();
      assert(setenv("TACTIX_RULE_JOURNAL", (stem + ".journal").c_str(), 1) == 0);
      assert(setenv("TACTIX_EVENT_TRACE", (stem + ".events").c_str(), 1) == 0);
    }
    std::ofstream(stem + ".cnf") << cnf;
    Proof proof;
    assert(proof.open((stem + ".dsr").c_str(), ProofFormat::DsrAscii));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    k.stats.conflicts = 1; // An explicit post-startup phase fixture.
    k.stats.search_ticks = 1000000000;
    DefaultStrategyConfig config;
    auto &ip = config.inprocess;
    ip.append_unlisted_rules = false;
    ip.structured_root_stages.clear();
    ip.structured_root_bounded_backtracking_stages.clear();
    ip.structured_root_large_domain_stages.clear();
    ip.initial_fast_elimination = false;
    ip.minimum_effort_ticks = 1000000;
    ip.phase_rounds = 3;
    ip.vivify_permille = ip.backbone_permille = ip.sweep_permille = ip.substitute_permille = ip.bve_permille = 1000;
    const SlotGroup group = family == 5 ? SlotGroup::Eliminate : SlotGroup::Probe;
    ip.program = {{1, families[family], family == 5 ? InprocessPhase::Eliminate : InprocessPhase::Probe}};
    if (family == 3) {
      // The builder stops admitting clauses at the cap, even when their
      // variables are already connected. Allow one spare slot for this
      // four-variable component, while keeping the disjoint PHP truncated.
      ip.sweep_parameters.max_env_vars = ip.sweep_parameters.max_env_vars_ceiling = 5;
    }
    DefaultPolicy policy(config);
    policy.inprocess().build(k);
    FeedbackTrace phase_trace;
    if (feedback) assert(phase_trace.write((stem + ".phase.feedback.jsonl").c_str(), "phase-fixture"));
    execution::PolicyCompletion *active = nullptr;
    execution::PolicyCompletion completion(k, feedback ? &phase_trace : nullptr,
        [](const Kernel &kernel, const void *owner) {
          auto value = observe_kernel(kernel);
          const auto *current = *static_cast<execution::PolicyCompletion *const *>(owner);
          if (current) { value.sequence = current->sequence(); value.last_action = current->last_action(); }
          return value;
        }, &active);
    active = &completion;
    completion.begin();
    const int phase_result = policy.inprocess().run_phase(k, group, nullptr, &completion);
    completion.finish(phase_result, phase_result == 20 ? StopReason::Unsat : StopReason::Completed);
    if (feedback) assert(phase_trace.terminal() && phase_trace.error().empty());
    signature(k, policy.inprocess(), "phase", family, unsat);
    if (!productive(k, family)) std::cerr << "unproductive family=" << families[family] << " unsat=" << unsat << '\n';
    assert(productive(k, family));
    assert(k.bve_definition_session == (family == 5 ? 1u : 0u));
    assert(!k.pending.valid && !k.factor_selection && !k.dense && !k.inprocessing);
    kernel_check_invariants(k);
    FeedbackTrace solve_trace;
    if (feedback) {
      assert(solve_trace.write((stem + ".solve.feedback.jsonl").c_str(), "completion-fixture"));
      policy.set_feedback(&solve_trace);
    }
    const int verdict = policy.solve(k);
    assert(verdict == (unsat ? 20 : 10));
    if (feedback) assert(solve_trace.terminal() && solve_trace.error().empty());
    signature(k, policy.inprocess(), "terminal", family, unsat);
    kernel_check_invariants(k);
    if (verdict == 10) {
      kernel_extend_model(k, false);
      std::ofstream model(stem + ".model");
      model << "v ";
      for (unsigned v = 0; v < variables; ++v)
        model << (k.model_values[internalize(v + 1)] > 0 ? int(v + 1) : -int(v + 1)) << ' ';
      model << "0\n";
    }
    proof.flush();
    if (diagnostics) rule_journal_close();
    if (argc == 1) {
      assert(unlink((stem + ".cnf").c_str()) == 0);
      assert(unlink((stem + ".dsr").c_str()) == 0);
      if (verdict == 10) assert(unlink((stem + ".model").c_str()) == 0);
    }
  }
  if (argc == 1) assert(rmdir(temporary) == 0);
}

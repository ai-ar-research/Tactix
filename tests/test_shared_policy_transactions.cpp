#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/action_execution.hpp"
#include "strategy/conflict_analysis_execution.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>

using namespace tactix;
using namespace tactix::execution;

static std::string read_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input);
  return {std::istreambuf_iterator<char>(input), {}};
}

static void parse(Kernel &k, const char *cnf) {
  FILE *input = std::tmpfile();
  assert(input);
  std::fputs(cnf, input);
  std::rewind(input);
  assert(!parse_dimacs(input, k));
  std::fclose(input);
}

static void test_certificate_boundary() {
  auto propagate = tactix_rules().at(tactix_rule_id("Propagate"));
  auto lucky = tactix_rules().at(tactix_rule_id("Lucky"));
  // These families do not register semantic callbacks. Fixture adapters make
  // all four dispatcher paths legal without changing their public surfaces.
  propagate.apply_instance = [](Kernel &k, uint64_t instance) {
    assert(instance == INSTANCE_CANONICAL);
    return propagate_apply(k);
  };
  propagate.apply_semantic_instance = propagate.apply_instance;
  lucky.apply_semantic_instance = lucky.apply_instance;
  for (unsigned dispatch = 0; dispatch < 5; ++dispatch) {
    const std::string path = "/tmp/tactix-certificate-" +
                             std::to_string(getpid()) + "-" +
                             std::to_string(dispatch);
    {
      Kernel k;
      Proof proof;
      assert(proof.open(path.c_str(), ProofFormat::Automatic));
      k.proof = &proof;
      proof.bind_variable_map(&k.internal_to_external);
      parse(k, "p cnf 2 1\n1 2 0\n");
      assert(k.startup_certificate_open && !k.startup_input.empty());
      assert(proof.startup_pb_available());
      // A declining explicit startup instance must leave its opportunity open
      // through both public instance surfaces, before any ordinary dispatch.
      for (bool semantic : {false, true}) {
        const auto result = semantic
            ? apply_rule_semantic_instance(lucky, k, LUCKY_STARTUP_COUNT)
            : apply_rule_instance(lucky, k, LUCKY_STARTUP_COUNT);
        assert(result == ApplyResult::NoOp);
        assert(k.startup_certificate_open && proof.startup_pb_available());
      }
      switch (dispatch) {
      case 0: execute_action(k, CloseStartupCertificateRequest{}); break;
      case 1: apply_rule(propagate, k); break;
      case 2: apply_rule_instance(propagate, k, INSTANCE_CANONICAL); break;
      case 3: apply_rule_semantic_instance(propagate, k, INSTANCE_CANONICAL); break;
      case 4:
        apply_rule_structured(propagate, k, "boundary-fixture", [&] {
          assert(!k.startup_certificate_open && !proof.startup_pb_available());
          return propagate_apply(k);
        });
        break;
      }
      assert(!k.startup_certificate_open && k.startup_input.empty());
      assert(!proof.startup_pb_available() && !proof.pb());
      assert(proof.dsr() && !proof.begin_pb());
      execute_action(k, CloseStartupCertificateRequest{}); // idempotent
      assert(!k.startup_certificate_open);
      kernel_check_invariants(k);
    }
    assert(read_file(path).empty()); // no clausal event was invented
    assert(std::remove(path.c_str()) == 0);
  }
  Kernel proofless;
  proofless.startup_input = {1, 0};
  execute_action(proofless, CloseStartupCertificateRequest{});
  assert(!proofless.startup_certificate_open && proofless.startup_input.empty());
}

enum class Fixture { Learned, UnitBatch, Reused, RootConflict };

static void fixture(Kernel &k, Fixture type) {
  kernel_reserve_vars(k, 6);
  k.conflict_reuse = type == Fixture::Reused;
  if (type == Fixture::Learned) {
    kernel_add_binary_clause(k, internalize(-1), internalize(6), 1);
    kernel_add_binary_clause(k, internalize(-3), internalize(4), 2);
    kernel_add_binary_clause(k, internalize(-3), internalize(5), 3);
    const Lit conflict[] = {internalize(-6), internalize(-4), internalize(-5)};
    kernel_add_large_clause(k, conflict, 3, false, 0, 4);
    k.next_clause_id = 4;
    kernel_assign_decision(k, internalize(1));
    assert(propagate_apply(k) == ApplyResult::Ok);
    kernel_assign_decision(k, internalize(2));
    assert(propagate_apply(k) == ApplyResult::Ok);
    kernel_assign_decision(k, internalize(3));
    assert(propagate_apply(k) == ApplyResult::Conflict);
  } else if (type == Fixture::UnitBatch) {
    kernel_add_binary_clause(k, internalize(-1), internalize(2), 1);
    kernel_add_binary_clause(k, internalize(-1), internalize(-2), 2);
    k.next_clause_id = 2;
    kernel_assign_decision(k, internalize(1));
    assert(propagate_apply(k) == ApplyResult::Conflict);
  } else {
    kernel_add_binary_clause(k, internalize(1), internalize(2), 1);
    k.next_clause_id = 1;
    if (type == Fixture::RootConflict) {
      k.unit_ids[var_of(internalize(1))] = 2;
      k.unit_ids[var_of(internalize(2))] = 3;
      k.next_clause_id = 3;
      kernel_assign_unit(k, internalize(-1));
      kernel_assign_unit(k, internalize(-2));
    } else {
      kernel_assign_decision(k, internalize(-1));
      kernel_assign_decision(k, internalize(3));
      kernel_assign_decision(k, internalize(-2));
    }
    k.conflict_ref = INVALID_REF;
    k.conflict_bin[0] = internalize(1);
    k.conflict_bin[1] = internalize(2);
    k.conflict_valid = true;
  }
  assert(analyze_guard(k));
}

// Exact ordered native state and work, excluding wall-clock counters. Proof
// bytes are compared separately. This intentionally covers more than verdicts.
static std::string snapshot(const Kernel &k) {
  std::ostringstream out;
  out << k.level << ':' << k.propagate_head << ':' << k.inconsistent << ':'
      << k.conflict_valid << ':' << k.pending.valid << ':' << k.next_clause_id;
  for (Lit lit : k.trail) out << ":t" << lit;
  for (Var v = 0; v < k.nvars; ++v) {
    const auto &a = k.assigned[v];
    out << ":v" << int(k.values[make_lit(v, false)]) << ':' << a.level << ':'
        << a.trail << ':' << a.reason << ':' << a.binary << ':' << a.jumped;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    out << ":c" << c->id() << ':' << c->garbage() << ':' << c->redundant()
        << ':' << c->glue();
    for (Lit lit : *c) out << ':' << lit;
    ref += clause_actual_words(c);
  }
  const auto &s = k.stats;
  out << ":s" << s.decisions << ':' << s.conflicts << ':' << s.propagations
      << ':' << s.ticks << ':' << s.search_ticks << ':' << s.root_assignments
      << ':' << s.learned << ':' << s.learned_units << ':' << s.conflict_reused
      << ':' << s.chronological << ':' << s.jumped_reasons;
  for (auto phase : k.h->saved) out << ":p" << int(phase);
  for (auto score : k.h->scores.score) out << ":h" << score;
  out << ':' << k.h->scinc;
  return out.str();
}

static std::string run(Fixture type, bool shared, bool stable, uint32_t target,
                       ProofFormat format, const std::string &path) {
  Kernel k;
  ScopedHeuristics h(k);
  Proof proof;
  assert(proof.open(path.c_str(), format));
  proof.bind_variable_map(&k.internal_to_external);
  k.proof = &proof;
  k.h->stable = stable;
  fixture(k, type);
  unsigned scope_calls = 0, advice_calls = 0;
  bool scope_done = false;
  auto scope = [&](auto &&apply) {
    ++scope_calls;
    assert(k.conflict_valid && !scope_done);
    const auto result = apply();
    scope_done = true;
    return result;
  };
  auto advice = [&](ApplyResult status) {
    ++advice_calls;
    assert(scope_done && !k.conflict_valid);
    assert((status == ApplyResult::Unsat) == (type == Fixture::RootConflict));
    if (k.pending.valid != (type == Fixture::Learned))
      std::fprintf(stderr, "fixture=%u shared=%u stable=%u target=%u pending=%u learned=%llu reused=%llu\n",
                   static_cast<unsigned>(type), shared, stable, target,
                   k.pending.valid, static_cast<unsigned long long>(k.stats.learned),
                   static_cast<unsigned long long>(k.stats.conflict_reused));
    assert(k.pending.valid == (type == Fixture::Learned));
    return target;
  };
  ConflictAnalysisExecutionResult result;
  if (shared) result = execute_action(k, NativeConflictAnalysisRequest{}, scope, advice);
  else {
    const auto analysis = scope([&] { return analyze_apply(k); });
    const auto selected = advice(analysis);
    const bool jump = analysis == ApplyResult::Ok && k.pending.valid;
    const auto outcome = jump ? backjump_apply_to(k, selected) : analysis;
    result = {analysis, outcome, jump};
  }
  assert(scope_calls == 1 && advice_calls == 1);
  assert(!k.pending.valid && !k.conflict_valid);
  assert(result.backjumped == (type == Fixture::Learned));
  assert((result.result == ApplyResult::Unsat) == (type == Fixture::RootConflict));
  kernel_check_invariants(k);
  proof.flush();
  return snapshot(k);
}

int main() {
  test_certificate_boundary();
  unsigned pairs = 0;
  for (Fixture type : {Fixture::Learned, Fixture::UnitBatch, Fixture::Reused,
                       Fixture::RootConflict})
    for (bool stable : {false, true})
      for (uint32_t target : {0u, 1u, 2u, UINT32_MAX})
        for (ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat}) {
          const auto stem = "/tmp/tactix-shared-analysis-" + std::to_string(getpid());
          const auto before = run(type, false, stable, target, format, stem + "-old");
          const auto after = run(type, true, stable, target, format, stem + "-new");
          assert(before == after);
          assert(read_file(stem + "-old") == read_file(stem + "-new"));
          assert(std::remove((stem + "-old").c_str()) == 0);
          assert(std::remove((stem + "-new").c_str()) == 0);
          ++pairs;
        }
  assert(pairs == 64);
  std::printf("PASS: five certificate paths and %u native state/proof pairs\n", pairs);
}

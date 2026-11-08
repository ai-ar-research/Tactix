#undef NDEBUG
#include "query_test_support.hpp"
#include "frontend/parse.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/bce/bce.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"
#include "session/weakening_history.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>

using namespace tactix;
using namespace tactix::session;
using query_test::clause;
using query_test::drive;

static NativeQueryLimits limits() {
  return {{256, 4096, 1024}, {64, 4096, 1024, 8192, 64, 1024, 8192}, 1000000};
}

static void eliminate(Kernel &k, int32_t external) {
  const auto &rule = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule_guard(rule, k));
  assert(rule.apply_instance(k, lit_instance(k, kernel_internalize(k, external))) == ApplyResult::Ok);
  heuristics_consume_retirements(k);
}

static std::unique_ptr<Proof> writer(const std::string &prefix, const std::string &mode) {
  if (mode == "none") return {};
  auto proof = std::make_unique<Proof>();
  assert(proof->open((prefix+".proof").c_str(),
      mode == "pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
  return proof;
}

static void export_query(QueryHistory &h, const std::string &prefix) {
  FILE *file = std::fopen((prefix+".cnf").c_str(), "w");
  assert(file && h.write_query_dimacs(h.active_query()->id, file));
  assert(std::fclose(file) == 0);
}

static void result(QueryKernel &s, QueryHistory &h, int expected,
                   const std::string &prefix, bool dynamic) {
  assert(drive(s, h, dynamic) == expected);
  const auto ticket = s.ticket();
  Kernel &k = *s.kernel(ticket);
  assert(!k.inconsistent);
  if (expected == 10) {
    assert(s.check_model(ticket).status == ModelStatus::Valid);
    if (!prefix.empty()) {
      FILE *file = std::fopen((prefix+".model").c_str(), "w");
      assert(file);
      for (uint32_t v = 1; v <= h.active_query()->query_variables; ++v)
        assert(std::fprintf(file, "%d ", k.model_values[internalize(v)] > 0 ? int(v) : -int(v)) > 0);
      assert(std::fprintf(file, "0\n") > 0 && std::fclose(file) == 0);
    }
  }
  assert(s.flush_proof(ticket));
  std::printf("result=%d vars=%u rho=%zu conflicts=%llu decisions=%llu restarts=%llu\n",
      expected, k.nvars, k.reconstruction.size(), (unsigned long long)k.stats.conflicts,
      (unsigned long long)k.stats.decisions, (unsigned long long)k.stats.restarts);
  assert(h.finish_query(h.active_query()->id, expected == 10 ? QueryResult::Sat : QueryResult::Unsat) == InputStatus::Ok);
}

static void signed_forward_planning() {
  Kernel k;
  WeakeningHistory history(k, limits().weakening);
  ScopedPopulationObserverBinding binding(k, history);
  declare_input(k, 5, 3);
  for (auto raw : {std::vector<Lit>{internalize(1), internalize(2)},
                   std::vector<Lit>{internalize(-2), internalize(3)},
                   std::vector<Lit>{internalize(4), internalize(5)}}) ingest_clause(k, raw);
  finish_input(k);
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(bce_try(k, internalize(-2)) == ApplyResult::Ok);
  assert(bce_try(k, internalize(4)) == ApplyResult::Ok);
  const auto rho = k.reconstruction;
  const int32_t same[] = {1, -2, 4};
  const auto untouched = history.plan_restoration(same);
  assert(untouched.status() == RestorationPlanStatus::Ready && untouched.selected().empty() && untouched.kept() == 3);
  const int32_t opposing[] = {-1};
  const auto plan = history.plan_restoration(opposing);
  assert(plan.status() == RestorationPlanStatus::Ready && plan.selected().size() == 2 && plan.kept() == 1);
  assert(plan.selected()[0].witness == std::vector<int32_t>{1});
  assert(plan.selected()[1].witness == std::vector<int32_t>{-2});
  const int32_t later[] = {2};
  const auto one = history.plan_restoration(later);
  assert(one.selected().size() == 1 && one.selected()[0].witness[0] == -2);
  const int32_t bad[] = {INT32_MIN};
  assert(history.plan_restoration(bad).status() == RestorationPlanStatus::InvalidLiteral);
  assert(k.reconstruction == rho && history.records().size() == 3 && history.capture_complete());
}

// Each scenario uses one Kernel/H and exports every exact query separately.
static void scenario(const std::string &name, const std::string &prefix,
                     const std::string &mode, bool dynamic) {
  QueryHistory h;
  QueryKernel s(limits());
  if (name == "chain") {
    clause(h, {1, 2}); clause(h, {-2, 3}); clause(h, {4, 5});
  } else if (name == "root") {
    clause(h, {1, 2}); clause(h, {-2, 3}); clause(h, {-2, -3});
  } else if (name == "large") {
    clause(h, {1, 2, 3}); clause(h, {1, 2, 3}); clause(h, {4, 5}); clause(h, {6, -6});
  } else if (name == "shrink") {
    clause(h, {1, 2, 3}); clause(h, {-3}); clause(h, {4, 5});
  } else if (name == "archive") {
    clause(h, {1}); clause(h, {-1, 2}); clause(h, {3, 4});
  } else {
    assert(name == "compact");
    clause(h, {1, 2}); clause(h, {3, 4});
    assert(h.assume(1) == InputStatus::Ok);
  }
  assert(h.begin_query() == InputStatus::Ok);
  const std::string first = prefix.empty() ? "" : prefix+"-1";
  if (!first.empty()) export_query(h, first);
  assert(s.advance(h, writer(first, mode)).status == NativeAdvanceStatus::Replayed);
  auto old = s.ticket();
  Kernel *kernel = s.kernel(old);
  Heuristics *guidance = kernel->h;
  if (name == "chain") {
    assert(bce_try(*kernel, internalize(1)) == ApplyResult::Ok);
    assert(bce_try(*kernel, internalize(-2)) == ApplyResult::Ok);
    assert(bce_try(*kernel, internalize(4)) == ApplyResult::Ok);
  } else if (name == "root") {
    eliminate(*kernel, 1); eliminate(*kernel, 3);
    assert(kernel_external_value(*kernel, internalize(-2)) > 0);
  } else if (name == "archive") {
    assert(kernel_compact_variables(*kernel, true));
  } else {
    eliminate(*kernel, 1);
    assert(kernel_compact_variables(*kernel));
    assert(kernel_internalize(*kernel, 1) == INVALID_LIT);
  }
  result(s, h, 10, first, dynamic);
  if (name == "chain") {
    assert(h.assume(-1) == InputStatus::Ok && h.assume(-3) == InputStatus::Ok);
  } else if (name == "root") assert(h.assume(-1) == InputStatus::Ok);
  else if (name == "archive") assert(h.assume(-2) == InputStatus::Ok);
  else {
    assert(h.assume(-1) == InputStatus::Ok && h.assume(-2) == InputStatus::Ok);
    if (name == "large") assert(h.assume(-3) == InputStatus::Ok);
  }
  assert(h.begin_query() == InputStatus::Ok);
  const std::string second = prefix.empty() ? "" : prefix+"-2";
  if (!second.empty()) export_query(h, second);
  const auto retained_scores = guidance->scores.score;
  const auto retained_phases = guidance->saved;
  const auto advanced = s.advance(h, writer(second, mode));
  assert(advanced.status == NativeAdvanceStatus::Ok && advanced.checked);
  assert(advanced.restored == (name == "chain" || name == "large" ? 2 : name == "archive" ? 0 : 1));
  assert(!s.kernel(old) && s.kernel(s.ticket()) == kernel && kernel->h == guidance);
  assert(std::equal(retained_scores.begin(), retained_scores.end(), guidance->scores.score.begin()));
  for (Var v = 0; v < retained_phases.size(); ++v)
    if (!kernel->values[make_lit(v, false)])
      assert(retained_phases[v] == guidance->saved[v]);
  result(s, h, 20, second, dynamic);
  if (name == "chain") {
    assert(h.assume(-4) == InputStatus::Ok && h.assume(-5) == InputStatus::Ok);
  } else assert(h.assume(2) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  const std::string third = prefix.empty() ? "" : prefix+"-3";
  if (!third.empty()) export_query(h, third);
  assert(s.advance(h, writer(third, mode)).status == NativeAdvanceStatus::Ok);
  result(s, h, name == "chain" || name == "root" ? 20 : 10, third, dynamic);
  if (name == "chain") {
    // Capture again after two consumptions, using native IDs imported by an
    // earlier bridge. An empty cube must also replace the previous projection.
    assert(h.begin_query() == InputStatus::Ok);
    const std::string fourth = prefix.empty() ? "" : prefix+"-4";
    if (!fourth.empty()) export_query(h, fourth);
    assert(s.advance(h, writer(fourth, mode)).status == NativeAdvanceStatus::Ok);
    assert(s.effective_assumptions(s.ticket()).empty());
    eliminate(*kernel, 1);
    result(s, h, 10, fourth, dynamic);
    assert(h.assume(-1) == InputStatus::Ok && h.assume(-3) == InputStatus::Ok);
    assert(h.begin_query() == InputStatus::Ok);
    const std::string fifth = prefix.empty() ? "" : prefix+"-5";
    if (!fifth.empty()) export_query(h, fifth);
    const auto again = s.advance(h, writer(fifth, mode));
    assert(again.status == NativeAdvanceStatus::Ok && again.restored == 1);
    assert(s.kernel(s.ticket()) == kernel && kernel->h == guidance);
    result(s, h, 20, fifth, dynamic);
  }
}

static void refused_before_mutation() {
  QueryHistory h, other;
  QueryKernel s(limits());
  assert(s.advance(h).status == NativeAdvanceStatus::NoActiveQuery);
  clause(h, {1, 2});
  assert(h.begin_query() == InputStatus::Ok && s.replay(h) == ReplayStatus::Ok);
  Kernel &k = *s.kernel(s.ticket());
  auto old = s.ticket();
  assert(other.begin_query() == InputStatus::Ok);
  assert(s.advance(other).status == NativeAdvanceStatus::WrongHistory && s.kernel(old) == &k);
  assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  assert(h.assume(-1) == InputStatus::Ok && h.begin_query() == InputStatus::Ok);
  const auto rho = k.reconstruction;
  const auto id = k.next_clause_id;
  Proof *proof = k.proof;
  k.warming = true;
  assert(s.advance(h).status == NativeAdvanceStatus::Busy);
  k.warming = false;
  volatile sig_atomic_t stop = 1;
  assert(s.advance(h, {}, &stop).status == NativeAdvanceStatus::Interrupted);
  assert(s.advance(h, std::make_unique<Proof>()).status == NativeAdvanceStatus::ProofUnavailable);
  auto used = std::make_unique<Proof>();
  assert(used->open_nowhere());
  const Lit literal = internalize(1);
  used->seed_population(1, &literal, 1);
  assert(s.advance(h, std::move(used)).status == NativeAdvanceStatus::ProofUnavailable);
  assert(k.next_clause_id == id && k.reconstruction == rho && k.proof == proof);
  assert(s.advance(h).status == NativeAdvanceStatus::Ok);
  assert(h.finish_query(2, QueryResult::Unknown) == InputStatus::Ok);
  assert(h.assume(-9) == InputStatus::Ok && h.begin_query() == InputStatus::Ok);
  const auto admitted = s.advance(h);
  assert(admitted.status == NativeAdvanceStatus::Ok && admitted.created == 1);
  assert(k.nvars == 3 && k.external_to_internal.size() == 9);
  assert(drive(s, h, true) == 10);

  QueryHistory restricted;
  QueryKernel r(limits());
  clause(restricted, {1, 2});
  assert(restricted.begin_query() == InputStatus::Ok && r.replay(restricted) == ReplayStatus::Ok);
  Kernel &rk = *r.kernel(r.ticket());
  SubstitutionRedundancyInstance sr;
  sr.clause = {internalize(1)}; sr.witness = {internalize(1)};
  assert(substitution_redundancy_apply_detailed(rk, sr).rule_result == ApplyResult::Ok);
  assert(restricted.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  assert(restricted.assume(-1) == InputStatus::Ok && restricted.begin_query() == InputStatus::Ok);
  const auto *rh = rk.h;
  const auto repaired = r.advance(restricted);
  assert(repaired.status == NativeAdvanceStatus::Ok && repaired.retracted == 1);
  assert(r.kernel(r.ticket()) == &rk && rk.h == rh && !kernel_external_value(rk, internalize(1)));
  assert(drive(r, restricted, true) == 10);

  auto tight = limits();
  tight.proof.maximum_clauses = 1;
  QueryHistory bounded;
  clause(bounded, {1, 2}); clause(bounded, {3, 4});
  assert(bounded.begin_query() == InputStatus::Ok);
  QueryKernel b(tight);
  assert(b.replay(bounded) == ReplayStatus::Ok);
  Kernel &bk = *b.kernel(b.ticket());
  const auto bid = bk.next_clause_id;
  assert(bounded.finish_query(1, QueryResult::Unknown) == InputStatus::Ok && bounded.begin_query() == InputStatus::Ok);
  assert(b.advance(bounded).status == NativeAdvanceStatus::Exhausted && bk.next_clause_id == bid);
}

static void default_continues() {
  QueryHistory h;
  clause(h, {1, 2});
  QueryKernel s(limits());
  DefaultPolicy policy;
  Kernel *kernel = nullptr;
  for (const auto &assumptions : {std::vector<int32_t>{1, 2}, {-1, 2}, {-1, -2}, {1, -2}}) {
    for (int32_t lit : assumptions) assert(h.assume(lit) == InputStatus::Ok);
    assert(h.begin_query() == InputStatus::Ok);
    assert(s.advance(h).status == (kernel ? NativeAdvanceStatus::Ok : NativeAdvanceStatus::Replayed));
    if (kernel) assert(s.kernel(s.ticket()) == kernel);
    kernel = s.kernel(s.ticket());
    const auto solved = policy.solve_query(s);
    const bool unsat = assumptions[0] == -1 && assumptions[1] == -2;
    assert(solved.status == QuerySolveStatus::Complete && solved.result == (unsat ? 20 : 10));
    assert(!kernel->inconsistent);
    assert(h.finish_query(h.active_query()->id, unsat ? QueryResult::Unsat : QueryResult::Sat) == InputStatus::Ok);
  }
}

static void unsupported_and_id_capacity() {
  for (int kind = 0; kind != 5; ++kind) {
    QueryHistory h;
    QueryKernel s(limits());
    if (kind == 4) clause(h, {1, 2, 3});
    else clause(h, {1, 2});
    assert(h.begin_query() == InputStatus::Ok && s.replay(h) == ReplayStatus::Ok);
    Kernel &k = *s.kernel(s.ticket());
    if (kind == 0) kernel_new_extension_vars(k, 1);
    if (kind == 1) {
      kernel_bind_population_observer(k, nullptr);
      assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
    }
    if (kind == 2) k.next_clause_id = UINT64_MAX-1;
    if (kind == 4) {
      assert(decide_apply(k, internalize(-2), false) == ApplyResult::Ok);
      assert(decide_apply(k, internalize(-3), false) == ApplyResult::Ok);
      const Lit lits[] = {internalize(1), internalize(2), internalize(3)};
      const Ref reason = kernel_add_detached_large_reason(k, lits, 3, ++k.next_clause_id);
      kernel_assign(k, internalize(1), k.level, reason, false);
    }
    assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
    if (kind == 3) clause(h, {-1});
    assert(h.begin_query() == InputStatus::Ok);
    const auto rho = k.reconstruction;
    const auto next = k.next_clause_id;
    Proof *proof = k.proof;
    const auto refused = s.advance(h);
    if (kind == 3) {
      assert(refused.status == NativeAdvanceStatus::Ok && refused.appended == 1);
      assert(drive(s, h, true) == 10);
      continue;
    }
    assert(refused.status == (kind == 2 ? NativeAdvanceStatus::Exhausted : NativeAdvanceStatus::ReplayRequired));
    assert(k.reconstruction == rho && k.next_clause_id == next && k.proof == proof);
  }
}

int main(int argc, char **argv) {
  if (argc == 5) { scenario(argv[1], argv[2], argv[3], std::string(argv[4]) == "dynamic"); return 0; }
  assert(argc == 1);
  signed_forward_planning();
  for (bool dynamic : {false, true})
    for (const char *name : {"chain", "compact", "large", "shrink", "root", "archive"}) scenario(name, "", "none", dynamic);
  refused_before_mutation();
  unsupported_and_id_capacity();
  default_continues();
}

#undef NDEBUG
#include "query_test_support.hpp"
#include "proof/proof.hpp"
#include "rules/substitution_redundancy/substitution_redundancy.hpp"

#include <cassert>
#include <string>

using namespace tactix;
using namespace tactix::session;
using query_test::clause;
using query_test::drive;

static RetentionLimits limits() { return {32, 4096, 64, 1024, 32, 128, 2048}; }
static void formula(QueryHistory &h) {
  clause(h, {1, 2});
  clause(h, {1, -2});
  clause(h, {-1, 3, 4});
  clause(h, {-1, 3, -4});
}

static void learn_across_queries(bool dynamic, const std::string &prefix = {},
                                  const std::string &mode = "none") {
  QueryHistory h;
  formula(h);
  QueryKernel s(limits());
  for (unsigned q = 1; q <= 3; ++q) {
    if (q <= 2) assert(h.assume(q == 1 ? -1 : -3) == InputStatus::Ok);
    else clause(h, {-3});
    assert(h.begin_query() == InputStatus::Ok);
    const std::string stem = prefix + "-" + std::to_string(q);
    std::unique_ptr<Proof> writer;
    if (!prefix.empty()) {
      FILE *cnf = std::fopen((stem + ".cnf").c_str(), "w");
      assert(cnf && h.write_query_dimacs(q, cnf));
      assert(std::fclose(cnf) == 0);
      if (mode != "none") {
        writer = std::make_unique<Proof>();
        assert(writer->open((stem + ".proof").c_str(),
                            mode == "pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
      }
    }
    assert(s.replay(h, std::move(writer)) == ReplayStatus::Ok);
    const auto t = s.ticket();
    Kernel &k = *s.kernel(t);
    if (q == 2) {
      assert(k.value(internalize(1)) > 0 && k.var_level(0) == 0);
      assert(k.stats.conflicts == 0); // retained knowledge, no repeat search
    }
    if (q == 3) assert(k.inconsistent); // base UNSAT after new permanent input
    assert(drive(s, h, dynamic) == 20);
    const auto retained = s.retain_pending(t, UINT64_MAX);
    assert(!retained.stale && !retained.exhausted && !retained.interrupted);
    assert(s.retained_clauses().size() == (q == 1 ? 1 : 2));
    assert(s.retained_clauses()[0].literals == std::vector<int32_t>{1});
    if (q > 1) assert(s.retained_clauses()[1].literals == std::vector<int32_t>{3});
    assert(s.flush_proof(t));
    if (!prefix.empty()) {
      std::printf("query=%u pool=%zu accepted=%llu rejected=%llu conflicts=%llu decisions=%llu roots=%llu\n",
                  q, s.retained_clauses().size(),
                  (unsigned long long)retained.accepted,
                  (unsigned long long)retained.rejected,
                  (unsigned long long)k.stats.conflicts,
                  (unsigned long long)k.stats.decisions,
                  (unsigned long long)k.stats.root_assignments);
    }
    assert(h.finish_query(q, QueryResult::Unsat) == InputStatus::Ok);
  }
}

static void reject_symmetry_restrictions_and_preserve_budget_cursor() {
  QueryHistory h;
  QueryKernel s(limits());
  clause(h, {1, 2});
  assert(h.assume(-1) == InputStatus::Ok);
  assert(h.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  const auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  SubstitutionRedundancyInstance sr;
  sr.clause = {internalize(1)};
  sr.witness = sr.clause;
  assert(substitution_redundancy_apply_detailed(k, sr).rule_result == ApplyResult::Ok);
  const auto paused = s.retain_pending(t, 0);
  assert(paused.exhausted && paused.pending == 1 && s.retained_clauses().empty());
  volatile sig_atomic_t stop = 1;
  const auto interrupted = s.retain_pending(t, UINT64_MAX, &stop);
  assert(interrupted.interrupted && interrupted.pending == 1);
  const auto checked = s.retain_pending(t, UINT64_MAX);
  assert(checked.rejected == 1 && checked.pending == 0);
  assert(s.retained_clauses().empty()); // SR restriction is not a base consequence
  assert(s.replay(h) == ReplayStatus::Ok);
  assert(drive(s, h, false) == 10);
  assert(s.retain_pending(t, UINT64_MAX).stale);
}

static void bounded_capture_and_disabled_owner() {
  QueryHistory h;
  formula(h);
  assert(h.begin_query() == InputStatus::Ok);
  auto bounds = limits();
  bounds.maximum_pending_clauses = 1;
  QueryKernel s(bounds);
  assert(s.replay(h) == ReplayStatus::Ok);
  auto t = s.ticket();
  Kernel &k = *s.kernel(t);
  // Untrusted caller additions must still be validated from the raw prefix.
  kernel_add_binary_clause(k, internalize(1), internalize(3));
  kernel_add_binary_clause(k, internalize(1), internalize(4));
  auto kept = s.retain_pending(t, UINT64_MAX);
  assert(kept.accepted == 1 && kept.capture_dropped == 1 && kept.pending == 0);
  assert(s.retained_clauses().size() == 1);
  QueryKernel disabled;
  assert(disabled.replay(h) == ReplayStatus::Ok);
  t = disabled.ticket();
  assert(!disabled.kernel(t)->population_observer);
  assert(drive(disabled, h, false) == 10);
  assert(disabled.retain_pending(t, UINT64_MAX).accepted == 0);
  assert(disabled.retained_clauses().empty());
}

static void replay_clause_shapes(bool dynamic, const std::string &prefix = {},
                                 const std::string &mode = "none") {
  QueryHistory h;
  clause(h, {1, 2, 3});
  clause(h, {1, 2, -3});
  clause(h, {4, 5, 6, 7});
  clause(h, {4, 5, 6, -7});
  QueryKernel s(limits());
  assert(h.begin_query() == InputStatus::Ok);
  assert(s.replay(h) == ReplayStatus::Ok);
  auto t = s.ticket();
  Kernel &first = *s.kernel(t);
  // Candidate publication is untrusted. Admission independently establishes
  // these two consequences from the permanent prefix, without a source proof.
  kernel_add_binary_clause(first, internalize(1), internalize(2));
  const Lit ternary[] = {internalize(4), internalize(5), internalize(6)};
  kernel_add_large_clause(first, ternary, 3, true, 3);
  assert(s.retain_pending(t, UINT64_MAX).accepted == 2);
  assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
  for (unsigned q = 2; q <= 4; ++q) {
    if (q == 2) {
      assert(h.assume(-1) == InputStatus::Ok);
      assert(h.assume(-2) == InputStatus::Ok);
    } else if (q == 3) {
      for (int32_t a : {-4, -5, -6}) assert(h.assume(a) == InputStatus::Ok);
    } else {
      clause(h, {-1});
      clause(h, {-4});
      clause(h, {-5});
      assert(h.assume(-6) == InputStatus::Ok);
    }
    assert(h.begin_query() == InputStatus::Ok);
    const std::string stem = prefix + "-" + std::to_string(q);
    std::unique_ptr<Proof> writer;
    if (!prefix.empty()) {
      FILE *cnf = std::fopen((stem + ".cnf").c_str(), "w");
      assert(cnf && h.write_query_dimacs(q, cnf));
      assert(std::fclose(cnf) == 0);
      if (mode != "none") {
        writer = std::make_unique<Proof>();
        assert(writer->open((stem + ".proof").c_str(),
                            mode == "pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
      }
    }
    assert(s.replay(h, std::move(writer)) == ReplayStatus::Ok);
    t = s.ticket();
    Kernel &k = *s.kernel(t);
    if (q < 4) {
      assert(kernel_binary_live(k, internalize(1), internalize(2)));
      unsigned imported = 0;
      for (const auto [ref, c] : arena_records(k.arena)) {
        (void)ref;
        if (!c->garbage() && c->redundant() && c->size == 3) {
          assert(std::equal(c->begin(), c->end(), std::begin(ternary)));
          ++imported;
        }
      }
      assert(imported == 1);
    } else {
      assert(k.value(internalize(2)) > 0 && k.value(internalize(6)) > 0);
      assert(k.var_level(1) == 0 && k.var_level(5) == 0);
    }
    assert(drive(s, h, dynamic) == 20);
    assert(s.flush_proof(t));
    if (!prefix.empty())
      std::printf("shape-query=%u conflicts=%llu decisions=%llu roots=%llu\n", q,
                  (unsigned long long)k.stats.conflicts,
                  (unsigned long long)k.stats.decisions,
                  (unsigned long long)k.stats.root_assignments);
    assert(h.finish_query(q, QueryResult::Unsat) == InputStatus::Ok);
  }
}

int main(int argc, char **argv) {
  if (argc == 5) {
    if (std::string(argv[4]) == "shapes")
      replay_clause_shapes(std::string(argv[3]) == "registry", argv[1], argv[2]);
    else learn_across_queries(std::string(argv[3]) == "registry", argv[1], argv[2]);
    return 0;
  }
  assert(argc == 1);
  learn_across_queries(false);
  learn_across_queries(true);
  reject_symmetry_restrictions_and_preserve_budget_cursor();
  bounded_capture_and_disabled_owner();
  replay_clause_shapes(false);
  replay_clause_shapes(true);
}

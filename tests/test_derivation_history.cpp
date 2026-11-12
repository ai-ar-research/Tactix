#include "unit_test.hpp"
#include "query_test_support.hpp"
#include "proof/proof.hpp"
#include "policies/heuristics.hpp"

#include <memory>
#include <type_traits>
using namespace tactix::session;
static_assert(!std::is_move_constructible_v<DerivationHistory>);

static const auto limits = query_test::small_limits;
static void input(QueryHistory &h) {
  for (const auto &c : {std::vector<int>{1,2,3},{1,2,-3},{1,-2,4},{1,-2,-4}}) {
    for (int l : c) assert(h.add(l) == InputStatus::Ok);
    assert(h.add(0) == InputStatus::Ok);
  }
}
static void derive(Kernel &k) {
  const Lit a[] = {internalize(1),internalize(2)};
  const Lit b[] = {internalize(1),internalize(-2)};
  const auto a_id = ++k.next_clause_id;
  k.proof->add_rup(a_id, a, 2);
  const auto b_id = ++k.next_clause_id;
  k.proof->add_rup(b_id, b, 2);
  const auto unit_id = ++k.next_clause_id;
  k.proof->add_derived_unit(unit_id, internalize(1));
  k.unit_ids[0] = unit_id;
  kernel_assign_unit(k, internalize(1));
  heuristics_deactivate_var(k, 0);
  k.proof->del(++k.next_clause_id, b_id, b, 2);
  k.proof->del(++k.next_clause_id, a_id, a, 2);
}

static void fixture(const std::string &prefix, const std::string &mode, bool dynamic) {
  QueryHistory h;
  input(h);
  QueryKernel s(limits());
  Kernel *identity = nullptr;
  Heuristics *guidance = nullptr;
  for (unsigned q = 1; q <= 5; ++q) {
    if (q == 2) assert(h.assume(-1) == InputStatus::Ok);
    if (q == 3 || q == 5) assert(h.assume(1) == InputStatus::Ok);
    if (q == 4) {
      query_test::clause(h, {5});
      assert(h.assume(-5) == InputStatus::Ok);
    }
    assert(h.begin_query() == InputStatus::Ok);
    const std::string stem = prefix.empty() ? "" : prefix+"-"+std::to_string(q);
    query_test::export_query(h, stem);
    auto writer = query_test::writer(stem, mode);
    if (q == 1) {
      assert(s.replay(h, std::move(writer)) == ReplayStatus::Ok);
      identity = s.kernel(s.ticket());
      guidance = identity->h;
      derive(*identity);
    } else {
      const auto next = s.advance(h, std::move(writer));
      assert(next.status == NativeAdvanceStatus::Ok && next.derivations >= 3);
      assert(s.kernel(s.ticket()) == identity && identity->h == guidance);
      assert(identity->value(internalize(1)) > 0 && identity->var_level(0) == 0);
    }
    const int answer = q == 2 || q == 4 ? 20 : 10;
    assert(query_test::drive(s, h, dynamic) == answer && !identity->inconsistent);
    if (answer == 10) query_test::export_model(h, *identity, stem);
    assert(s.flush_proof(s.ticket()));
    const auto captured = s.derivation_state(s.ticket());
    assert(captured.records && !captured.dropped);
    std::printf("q=%u result=%d records=%zu words=%zu ids=%llu conflicts=%llu decisions=%llu\n",
        q, answer, captured.records, captured.literals,
        (unsigned long long)identity->next_clause_id,
        (unsigned long long)identity->stats.conflicts,
        (unsigned long long)identity->stats.decisions);
    assert(h.finish_query(q, answer == 10 ? QueryResult::Sat : QueryResult::Unsat) == InputStatus::Ok);
  }
}

static void assumptions_are_not_permanent_premises() {
  for (bool install : {false, true}) {
    QueryHistory h;
    query_test::clause(h, {-1,2});
    assert(h.assume(1) == InputStatus::Ok && h.begin_query() == InputStatus::Ok);
    QueryKernel s(limits());
    assert(s.replay(h) == ReplayStatus::Ok);
    Kernel &k = *s.kernel(s.ticket());
    // This proof-only unit is RUP from F+A_old, but not from permanent F.
    const auto id = ++k.next_clause_id;
    k.proof->add_derived_unit(id, internalize(2));
    if (install) {
      k.unit_ids[1] = id;
      kernel_assign_unit(k, internalize(2));
      heuristics_deactivate_var(k, 1);
    }
    assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
    assert(h.assume(-2) == InputStatus::Ok && h.begin_query() == InputStatus::Ok);
    const auto old_id = k.next_clause_id;
    const auto advanced = s.advance(h);
    if (install) {
      assert(advanced.status == NativeAdvanceStatus::ReplayRequired && k.next_clause_id == old_id);
      assert(s.replay(h) == ReplayStatus::Ok);
    } else assert(advanced.status == NativeAdvanceStatus::Ok && !advanced.derivations);
    assert(query_test::drive(s, h, false) == 10);
  }
}

static void quota_and_identity_preflight() {
  for (bool ids : {false, true}) {
    QueryHistory h;
    input(h);
    assert(h.begin_query() == InputStatus::Ok);
    auto bounds = limits();
    if (!ids) bounds.proof.maximum_pending_clauses = 4; // input fills the capture
    QueryKernel s(bounds);
    assert(s.replay(h) == ReplayStatus::Ok);
    Kernel &k = *s.kernel(s.ticket());
    derive(k);
    assert(s.derivation_state(s.ticket()).dropped > 0 || ids);
    assert(h.finish_query(1, QueryResult::Unknown) == InputStatus::Ok);
    assert(h.assume(-1) == InputStatus::Ok && h.begin_query() == InputStatus::Ok);
    // Leave one ID: even the exact input prefix needs more. This tests real
    // exhaustion independently of the certificate bridge's helper count.
    if (ids) k.next_clause_id = UINT64_MAX-1;
    const auto id = k.next_clause_id;
    const auto rho = k.reconstruction;
    volatile sig_atomic_t stop = 1;
    assert(s.advance(h, {}, &stop).status == NativeAdvanceStatus::Interrupted);
    const auto advanced = s.advance(h);
    assert(advanced.status == (ids ? NativeAdvanceStatus::Exhausted : NativeAdvanceStatus::ReplayRequired));
    assert(k.next_clause_id == id && k.reconstruction == rho && !k.inconsistent);
  }
}

int main(int argc, char **argv) {
  if (argc == 4) { fixture(argv[1], argv[2], std::string(argv[3]) == "dynamic"); return 0; }
  assert(argc == 1);
  assumptions_are_not_permanent_premises();
  quota_and_identity_preflight();
  for (bool dynamic : {false,true}) fixture("", "none", dynamic);
}

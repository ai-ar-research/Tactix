#include "unit_test.hpp"
#include "session/derivation_history.hpp"
#include "proof/proof.hpp"
extern "C" void tactix_test_fail_allocations(bool);
using namespace tactix::session;
int main() {
  DerivationHistory history(1, 8);
  history.start_query(7);
  const int32_t body[] = {1,2};
  DerivationEvent event;
  event.clause = PopulationClauseView::external_lits(body, 2);
  tactix_test_fail_allocations(true);
  history.observe(event);
  tactix_test_fail_allocations(false);
  assert(history.records().empty() && history.snapshot().literals == 0 && history.snapshot().dropped == 1);
  history.observe(event);
  assert(history.records().size() == 1 && history.records()[0].query == 7);
  history.observe(event);
  assert(history.records().size() == 1 && history.snapshot().dropped == 2);

  DerivationHistory mapped(10, 32);
  mapped.start_query(3);
  std::vector<Var> map = {4,1,8};
  Proof writer;
  assert(writer.open_nowhere());
  writer.bind_variable_map(&map);
  writer.bind_derivation_observer(&mapped);
  const Lit clause[] = {make_lit(0,false), make_lit(1,true)};
  const Lit witness = make_lit(2,false);
  const DsrLiteralMapping mapping{make_lit(1,false), make_lit(2,true)};
  assert(writer.add_sr(19, clause, 2, &witness, 1, &mapping, 1, 9));
  writer.del(20,19,clause,2);
  map.clear(); // callback-local views were copied, not retained
  assert(mapped.records().size() == 2 && mapped.snapshot().literals == 7);
  const auto saved = mapped.records()[0];
  assert(saved.kind == DerivationKind::SubstitutionRedundancy && saved.id == 19);
  assert(saved.clause == std::vector<int32_t>({5,-2}));
  assert(saved.witness == std::vector<int32_t>({9}));
  assert(saved.mappings.size() == 1 && saved.mappings[0] == std::make_pair(2,-9));
  assert(mapped.records()[1].kind == DerivationKind::Delete && mapped.records()[1].source_id == 19);
  map = {4,1,8};
  const int32_t external[] = {5,-2};
  const uint64_t wide = uint64_t(UINT32_MAX)+17;
  assert(writer.add_external_rup(wide, external, 2));
  assert(writer.add_external_rat(wide+1, external, 2));
  assert(writer.del_external(wide+2, wide+1, external, 2));
  writer.add_rat(wide+3, clause, 2);
  writer.seed_population(wide+4, clause, 2);
  writer.add_rup(wide+5, clause, 2);
  writer.add_derived_unit(wide+6, clause[0]);
  writer.add_empty(wide+7);
  assert(mapped.records().size() == 10 && mapped.snapshot().literals == 20 && !mapped.snapshot().dropped);
  assert(mapped.records()[2].kind == DerivationKind::Rup && mapped.records()[2].id == wide);
  assert(mapped.records()[2].clause == saved.clause);
  assert(mapped.records()[3].kind == DerivationKind::Rat);
  assert(mapped.records()[4].kind == DerivationKind::Delete && mapped.records()[4].source_id == wide+1);
  assert(mapped.records()[5].kind == DerivationKind::Rat);
  assert(mapped.records()[6].kind == DerivationKind::Input);
  assert(mapped.records()[7].kind == DerivationKind::Rup && mapped.records()[7].id == wide+5 && mapped.records()[7].clause == saved.clause);
  assert(mapped.records()[8].kind == DerivationKind::Rup && mapped.records()[8].id == wide+6 && mapped.records()[8].clause == std::vector<int32_t>({5}));
  assert(mapped.records()[9].kind == DerivationKind::Rup && mapped.records()[9].clause.empty());

  std::vector<PbpParityEquation> rows{{{1,2,3},false},{{1,2,4},false}};
  DerivationEvent parity;
  parity.kind=DerivationKind::ParityReason;
  parity.clause=PopulationClauseView::external_lits(body,2);
  parity.parity_equations=rows;
  DerivationHistory algebraic(4,20);
  algebraic.start_query(9);
  tactix_test_fail_allocations(true);
  algebraic.observe(parity);
  tactix_test_fail_allocations(false);
  assert(algebraic.records().empty() && algebraic.snapshot().dropped==1);
  algebraic.observe(parity);
  assert(algebraic.records().size()==1 && algebraic.snapshot().literals==10);
  DerivationHistory narrow(4,9);narrow.start_query(9);narrow.observe(parity);
  assert(narrow.records().empty() && narrow.snapshot().dropped==1);
  rows[0].variables[0]=7;rows[1].rhs=true;
  assert(algebraic.records()[0].kind==DerivationKind::ParityReason);
  assert(algebraic.records()[0].parity_equations[0].variables==std::vector<int32_t>({1,2,3}));
  assert(!algebraic.records()[0].parity_equations[1].rhs);
}

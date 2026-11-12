#undef NDEBUG
#include "dimacs_text.hpp"
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/heuristics.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/bce/bce.hpp"
#include "rules/bve/bve.hpp"
#include "rules/propagate/propagate.hpp"
#include "session/weakening_history.hpp"
#include "session/query_history.hpp"
#include "session/query_kernel.hpp"

#include <algorithm>
#include <cassert>
#include <set>
#include <memory>
#include <string>
#include <type_traits>

using namespace tactix;
using namespace tactix::session;
static_assert(!std::is_copy_constructible_v<WeakeningHistory>);
static_assert(!std::is_move_constructible_v<WeakeningHistory>);

static constexpr WeakeningLimits bounds{256, 4096, 1024};

static const auto input = input_clauses;

static bool equal(std::vector<int32_t> actual, std::initializer_list<int32_t> expected) {
  std::vector<int32_t> sorted(expected);
  std::sort(actual.begin(), actual.end());
  std::sort(sorted.begin(), sorted.end());
  return actual == sorted;
}

static void eliminate(Kernel &k, int32_t lit) {
  const auto &rule = tactix_rules()[tactix_rule_id("BVE")];
  assert(rule_guard(rule, k));
  assert(rule.apply_instance(k, lit_instance(k, kernel_internalize(k, lit))) == ApplyResult::Ok);
}

static void full_large_sources_survive_compaction() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 4, 0, -1, 3, 5, 0, -4, 0});
  assert(history.capture_complete());
  eliminate(k, 1);
  assert(history.capture_complete() && history.pending() == 0);
  assert(history.records().size() == 2);
  const auto &positive = history.records()[0];
  const auto &negative = history.records()[1];
  assert(equal(positive.clause, {1, 2, 4}) && equal(positive.witness, {1}));
  assert(equal(negative.clause, {-1, 3, 5}) && equal(negative.witness, {-1}));
  assert(positive.sources.size() == 1 && positive.sources[0].proof_id == 1);
  assert(negative.sources.size() == 1 && negative.sources[0].proof_id == 2);
  assert(positive.family == WeakeningFamily::Bve && positive.order < negative.order);
  // The saved first clause has three literals, but its rho residual has two.
  assert(k.reconstruction.size() == 7);
  assert(kernel_compact_variables(k, true));
  assert(history.capture_complete());
  assert(equal(history.records()[0].clause, {1, 2, 4}));
  assert(history.records()[0].sources[0].proof_id == 1);
  kernel_check_invariants(k);
}

static void newly_false_resolvent_literals_preserve_capture() {
  Kernel k;WeakeningHistory history(k,bounds);
  ScopedPopulationObserverBinding binding(k,history);
  input(k,{-3,-6,0,3,6,0,-6,3,0,-7,2,0});
  eliminate(k,3); // Resolving learns -6 after the source (3,6) was gathered.
  assert(k.value(internalize(-6))>0 && history.capture_complete());
  assert(k.reconstruction==std::vector<Lit>({internalize(6),internalize(3),2}));
  const int32_t touched[]{-3};
  const auto plan=history.plan_restoration(touched);
  assert(plan.status()==RestorationPlanStatus::Ready && !plan.kept());
  assert(plan.selected().size()==1 && equal(plan.selected()[0].clause,{3,6}));
  assert(history.records().size()>0 && equal(history.records()[0].clause,{3,6}));
  assert(propagate_apply(k)==ApplyResult::Ok);
  eliminate(k,2);
  assert(history.capture_complete() && !history.pending());
  assert(default_solve(k)==10);
  kernel_extend_model(k);
  for(const auto &clause:std::vector<std::vector<int32_t>>{{-3,-6},{3,6},{-6,3},{-7,2}}) {
    bool satisfied=false;for(int32_t lit:clause)satisfied|=k.model_values[internalize(lit)]>0;
    assert(satisfied);
  }
}

static void false_literals_do_not_license_arbitrary_repairs() {
  struct Case { std::vector<int32_t> roots,rest;bool accepted; };
  const std::vector<Case> cases{
    {{-4},{2,4},true}, {{-4},{2},true}, // Full and root-shortened valid twins.
    {{-3,-4},{2,3},false}, // A foreign false literal replaces a false source literal.
    {{-3},{2},false}, // An unfixed source literal is missing.
    {{-1},{2,4},false}, // The chosen witness is false.
    {{-4},{2,4,4},false}, {{-4},{2,1},false} // Extra false copy or witness copy.
  };
  for(const auto &test:cases) {
    Kernel k;WeakeningHistory history(k,bounds);
    ScopedPopulationObserverBinding binding(k,history);
    std::vector<Lit> raw{internalize(1),internalize(2),internalize(4)};
    ingest_clause(k,raw);
    for(int32_t literal:test.roots) {raw={internalize(literal)};ingest_clause(k,raw);}
    finish_input(k);assert(history.capture_complete());
    const Lit clause[]{internalize(1),internalize(2),internalize(4)},witness=clause[0];
    std::vector<Lit> rest;for(int32_t literal:test.rest)rest.push_back(internalize(literal));
    const uint64_t token=kernel_prepare_weakening(k,WeakeningFamily::Bce,
        kernel_population_clause(k,clause,3),kernel_population_clause(k,&witness,1),
        1,false,false,rest.size()+2);
    assert(token && history.pending()==1 && history.records().empty());
    bool deleted=false;
    for(const auto &[ref,c]:arena_records(k.arena)) {
      (void)ref;if(c->id()!=1)continue;
      kernel_delete_clause(k,c);deleted=true;break;
    }
    assert(deleted);
    kernel_push_witness(k,witness,rest.data(),rest.size());
    assert(k.reconstruction.size()==rest.size()+2);
    kernel_finish_weakening(k,token,true);
    assert(history.capture_complete()==test.accepted && !history.pending());
    assert(history.records().size()==(test.accepted?1u:0u));
  }
}

static void full_witness_is_consumed_by_native_restoration() {
  QueryHistory history;
  for(int literal:{-3,-6,0,3,6,0,-6,3,0,-7,2,0})assert(history.add(literal)==InputStatus::Ok);
  assert(history.begin_query()==InputStatus::Ok);
  QueryKernel owner(NativeQueryLimits{bounds,{64,4096,1024,8192,64,1024,8192},1000000});
  assert(owner.advance(history).status==NativeAdvanceStatus::Replayed);
  auto *k=owner.kernel(owner.ticket());assert(k);
  eliminate(*k,3);heuristics_consume_retirements(*k);
  assert(propagate_apply(*k)==ApplyResult::Ok);
  assert(k->reconstruction==std::vector<Lit>({internalize(6),internalize(3),2}));
  assert(history.finish_query(1,QueryResult::Unknown)==InputStatus::Ok);
  assert(history.add(-3)==InputStatus::Ok && history.add(0)==InputStatus::Ok);
  assert(history.begin_query()==InputStatus::Ok);
  const auto restored=owner.advance(history);
  assert(restored.status==NativeAdvanceStatus::Ok && restored.restored==1);
  assert(owner.kernel(owner.ticket())==k && k->reconstruction.empty() && k->inconsistent);
  assert(owner.flush_proof(owner.ticket()));
}

static void forwarding_capture_and_real_gap() {
  struct Layer final : ForwardingPopulationObserver {
    explicit Layer(PopulationObserver *p):ForwardingPopulationObserver(p) {}
  };
  Kernel k;WeakeningHistory history(k,bounds);
  ScopedPopulationObserverBinding primary(k,history);
  input(k,{1,2,4,0,-1,3,5,0});
  {
    Layer layer(&history);ScopedPopulationObserverBinding extra(k,layer);
    assert(history.capture_complete());
    eliminate(k,1);
    assert(history.capture_complete()&&history.records().size()==2);
    assert(equal(history.records()[0].clause,{1,2,4}));
    assert(equal(history.records()[1].clause,{-1,3,5}));
  }
  assert(history.capture_complete());
  {
    PopulationObserver silent;ScopedPopulationObserverBinding gap(k,silent);
    assert(!history.capture_complete());
  }
  assert(!history.capture_complete()); // Rebinding cannot repair missed provenance.
}

static void bce_batch_and_duplicate_sources() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 3, 0, 1, 4, 0});
  const uint64_t extra_id = ++k.next_clause_id;
  kernel_add_binary_clause(k, internalize(1), internalize(4), extra_id);
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(history.capture_complete() && history.records().size() == 2);
  std::set<uint64_t> ids;
  size_t source_count = 0;
  for (const auto &record : history.records()) {
    assert(record.family == WeakeningFamily::Bce && equal(record.witness, {1}));
    source_count += record.sources.size();
    for (const auto &source : record.sources) ids.insert(source.proof_id);
  }
  assert(source_count == 3 && ids == std::set<uint64_t>({1, 2, extra_id}));
  assert(history.live_binary_copies() == 0 && history.pending() == 0);
  assert(bce_try(k, internalize(1)) == ApplyResult::NoOp);
  assert(history.records().size() == 2 && history.capture_complete());
  kernel_check_invariants(k);
}

static void many_reserved_copies_survive_storage_compaction() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0, -1, 3, 0});
  for (unsigned copy = 0; copy < 100; ++copy)
    kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  assert(kernel_binary_id(k, internalize(-1), internalize(3)) == 1);
  eliminate(k, 1);
  assert(history.capture_complete() && history.records().size() == 102);
  std::set<uint64_t> ids, occurrences;
  for (const auto &record : history.records()) {
    assert(record.sources.size() == 1);
    ids.insert(record.sources[0].proof_id);
    occurrences.insert(record.sources[0].occurrence);
  }
  assert(ids.size() == 102 && *ids.begin() == 1 && *ids.rbegin() == 102);
  assert(occurrences.size() == 102);
  kernel_check_invariants(k);
}

static void cancelled_preparation_and_uncommitted_delete() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0});
  const Lit clause[2] = {internalize(1), internalize(2)};
  {
    ScopedWeakeningCapture capture(k, WeakeningFamily::Bce, clause, 2,
        internalize(1), 0, true, true);
    assert(history.pending() == 1 && history.records().empty());
  }
  assert(history.capture_complete() && history.pending() == 0 && history.records().empty());
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok && history.capture_complete());
  assert(history.records().size() == 1);

  Kernel other;
  WeakeningHistory missing(other, bounds);
  ScopedPopulationObserverBinding other_binding(other, missing);
  input(other, {1, 2, 0});
  {
    ScopedWeakeningCapture capture(other, WeakeningFamily::Bce, clause, 2,
        internalize(1), 0, true, true);
    // A dishonest/missing deletion cannot turn staging into a durable record.
    capture.commit();
  }
  assert(!missing.capture_complete() && missing.records().empty());
}

static void bce_semantic_choice() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0, 1, 3, 0});
  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  const auto &rule = tactix_rules()[tactix_rule_id("BCE")];
  uint64_t choices[8];
  assert(rule.semantic_instances(k, choices, 8) == 4);
  assert(rule.apply_semantic_instance(k, choices[0]) == ApplyResult::Ok);
  assert(history.capture_complete() && history.records().size() == 1);
  assert(history.records()[0].sources.size() == 2);
  assert(kernel_binary_live(k, internalize(1), internalize(3)));
}

static void staged_binary_replacement() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 3, 0, -3, 0});
  Ref source = INVALID_REF;
  for (const auto [ref, clause] : arena_records(k.arena))
    if (!clause->garbage()) source = ref;
  assert(source != INVALID_REF);
  const Lit replacement[2] = {internalize(1), internalize(2)};
  // Root -3 licenses this shortening. Exercise the already audited native
  // replacement transaction used by the rules, then weaken its binary image.
  const uint64_t replacement_id = ++k.next_clause_id;
  assert(kernel_replace_clause(k, clause_at(k.arena, source), replacement_id,
      replacement, 2, false, 0, nullptr) == ReplaceOutcome::Binary);
  assert(history.capture_complete() && history.live_binary_copies() == 1);
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(history.capture_complete() && history.records().size() == 1);
  assert(history.records()[0].sources[0].proof_id == replacement_id);
}

static void full_width_id_and_capture_gaps() {
  for (uint64_t id : {uint64_t(INT32_MAX), uint64_t(UINT32_MAX),
                      uint64_t(UINT32_MAX) + 1, uint64_t(INT64_MAX)}) {
    Kernel k;
    WeakeningHistory history(k, bounds);
    ScopedPopulationObserverBinding binding(k, history);
    k.next_clause_id = id - 1;
    input(k, {1, 2, 3, 0});
    assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
    assert(history.capture_complete() && history.records().size() == 1);
    assert(history.records()[0].sources[0].proof_id == id);
  }
  Kernel k;
  WeakeningHistory bounded(k, {1, 8, 8});
  ScopedPopulationObserverBinding binding(k, bounded);
  input(k, {1, 2, 0, 1, 3, 0});
  assert(bce_try(k, internalize(1)) == ApplyResult::Ok);
  assert(!bounded.capture_complete()); // exhaustion never narrows the legal rule
  assert(k.stats.bce_deleted == 2 && !k.reconstruction.empty());

  Kernel unsupported;
  WeakeningHistory history(unsupported, bounds);
  ScopedPopulationObserverBinding unsupported_binding(unsupported, history);
  input(unsupported, {1, 2, 0});
  const Lit rest = internalize(2);
  kernel_push_witness(unsupported, internalize(1), &rest, 1);
  assert(!history.capture_complete());
  assert(bce_try(unsupported, internalize(1)) == ApplyResult::Ok);
  assert(!history.capture_complete() && history.records().empty());
}

static void mismatched_reconstruction_is_a_gap() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0});
  const Lit clause[2] = {internalize(1), internalize(2)};
  ScopedWeakeningCapture capture(k, WeakeningFamily::Bce, clause, 2, clause[0], 0, true, true);
  assert(kernel_delete_binary_all_copies(k, clause[0], clause[1]));
  kernel_push_witness(k, clause[1], clause, 1); // different repair, same word count
  capture.commit();
  assert(!history.capture_complete() && history.records().empty());
}

static void lost_binding_is_a_permanent_gap() {
  Kernel k;
  WeakeningHistory history(k, bounds);
  ScopedPopulationObserverBinding binding(k, history);
  input(k, {1, 2, 0});
  assert(history.capture_complete());
  kernel_bind_population_observer(k, nullptr);
  kernel_add_binary_clause(k, internalize(1), internalize(2), ++k.next_clause_id);
  kernel_bind_population_observer(k, &history);
  assert(!history.capture_complete());
}

static void proof_fixture(const std::string &prefix, const std::string &mode,
                          bool capture, bool unsat) {
  QueryHistory input_history;
  const std::vector<int32_t> raw = {1,2,0,-1,3,0,4,5,0,6,7,0,-6,7,0,6,-7,0};
  for (auto literal : raw) assert(input_history.add(literal) == InputStatus::Ok);
  if (unsat) for (int literal : {-6,-7,0}) assert(input_history.add(literal) == InputStatus::Ok);
  assert(input_history.begin_query() == InputStatus::Ok);
  FILE *cnf = std::fopen((prefix + ".cnf").c_str(), "w");
  assert(cnf && input_history.write_query_dimacs(1, cnf) && std::fclose(cnf) == 0);
  Kernel k;
  ScopedHeuristics guidance(k);
  WeakeningHistory history(k, bounds);
  Proof writer;
  assert(mode == "none" ? writer.open_nowhere() : writer.open((prefix+".proof").c_str(),
      mode == "pbp" ? ProofFormat::PbpStreaming : ProofFormat::DsrAscii));
  k.proof = &writer;
  writer.bind_variable_map(&k.internal_to_external);
  writer.bind_external_variable_map(&k.external_to_internal);
  std::unique_ptr<ScopedPopulationObserverBinding> binding;
  if (capture) binding = std::make_unique<ScopedPopulationObserverBinding>(k, history);
  declare_input(k, 7, input_history.clauses());
  std::vector<Lit> clause;
  for (int32_t literal : input_history.input()) {
    if (literal) clause.push_back(internalize(literal));
    else { ingest_clause(k, clause); clause.clear(); }
  }
  finish_input(k);
  eliminate(k, 1);
  assert(bce_try(k, internalize(4)) == ApplyResult::Ok);
  if (capture) assert(history.capture_complete() && history.records().size() == 3);
  const int result = default_solve(k);
  assert(result == (unsat ? 20 : 10));
  if (!unsat) {
    kernel_extend_model(k);
    assert(input_history.check_model(1, [&](int32_t variable) {
      return k.model_values[internalize(variable)];
    }).status == ModelStatus::Valid);
    FILE *model = std::fopen((prefix+".model").c_str(), "w");
    assert(model);
    for (int variable = 1; variable <= 7; ++variable)
      assert(std::fprintf(model, "%d ", k.model_values[internalize(variable)] > 0
          ? variable : -variable) > 0);
    assert(std::fprintf(model, "0\n") > 0 && std::fclose(model) == 0);
  }
  writer.flush();
  std::printf("result=%d conflicts=%llu decisions=%llu bve=%llu bce=%llu vars=%u rho=%zu\n",
      result, (unsigned long long)k.stats.conflicts, (unsigned long long)k.stats.decisions,
      (unsigned long long)k.stats.bve_eliminated, (unsigned long long)k.stats.bce_deleted,
      k.nvars, k.reconstruction.size());
  binding.reset();
  k.proof = nullptr;
}

int main(int argc, char **argv) {
  if (argc == 5) {
    proof_fixture(argv[1], argv[2], std::string(argv[3]) == "capture",
                  std::string(argv[4]) == "unsat");
    return 0;
  }
  assert(argc == 1);
  newly_false_resolvent_literals_preserve_capture();
  false_literals_do_not_license_arbitrary_repairs();
  full_witness_is_consumed_by_native_restoration();
  forwarding_capture_and_real_gap();
  full_large_sources_survive_compaction();
  bce_batch_and_duplicate_sources();
  many_reserved_copies_survive_storage_compaction();
  cancelled_preparation_and_uncommitted_delete();
  bce_semantic_choice();
  staged_binary_replacement();
  full_width_id_and_capture_gaps();
  mismatched_reconstruction_is_a_gap();
  lost_binding_is_a_permanent_gap();
}

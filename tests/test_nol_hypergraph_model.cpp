#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/lucky.hpp"
#include "rules/lucky/nol_hypergraph_model.hpp"
#include "rules/lucky/model.hpp"
#include "rules/propagate/propagate.hpp"
#include "strategy/native_feedback.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

static Kernel parse(const std::string &text) {
  FILE *input = std::tmpfile();
  assert(input);
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel kernel;
  assert(parse_dimacs(input, kernel) == nullptr);
  std::fclose(input);
  return kernel;
}

static std::string hypergraph(
    int vertices, const std::vector<std::array<int, 3>> &edges,
    bool malformed = false, int active_vertices = -1) {
  if (active_vertices < 0) active_vertices = vertices;
  std::ostringstream body;
  uint64_t clauses = 0;
  for (int vertex = 0; vertex < active_vertices; ++vertex) {
    body << 3 * vertex + 1 << ' ' << 3 * vertex + 2 << ' '
         << 3 * vertex + 3 << " 0\n";
    ++clauses;
  }
  for (const auto &edge : edges)
    for (int color = 0; color < 3; ++color) {
      body << -(3 * edge[0] + color + 1) << ' '
           << -(3 * edge[1] + color + 1) << ' '
           << -(3 * edge[2] + color + 1) << " 0\n";
      ++clauses;
    }
  if (malformed) {
    body << "1 -5 9 0\n";
    ++clauses;
  }
  std::ostringstream formula;
  formula << "p cnf " << 3 * vertices << ' ' << clauses << '\n'
          << body.str();
  return formula.str();
}

int main() {
  {
    // A path of hyperedges is greedily 3-colorable. The published assignment
    // is total and independently checked against every live clause.
    Kernel kernel = parse(hypergraph(5, {{{0, 1, 2}}, {{1, 2, 3}},
                                          {{2, 3, 4}}}));
    assert(nol_hypergraph_model_guard(kernel));
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::Sat);
    assert(kernel.stats.nol_hypergraph_model_calls == 1);
    assert(kernel.stats.nol_hypergraph_model_recognized == 1);
    assert(kernel.stats.nol_hypergraph_model_vertices == 5);
    assert(kernel.stats.nol_hypergraph_model_edges == 3);
    assert(kernel.stats.nol_hypergraph_model_sat == 1);
    assert(kernel.trail.size() == kernel.nvars);
  }
  {
    // A mixed-sign ternary is outside the exact noL language. Recognition is
    // observational: it neither assigns nor publishes guidance.
    Kernel kernel = parse(hypergraph(3, {{{0, 1, 2}}}, true));
    const uint64_t offered = kernel.stats.local_search_hints_offered;
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.nol_hypergraph_model_recognized);
    assert(kernel.trail.empty());
    assert(kernel.stats.local_search_hints_offered == offered);
  }
  {
    // One same-color clause is not a hypergraph edge: all three colors must
    // occur for the detector to attach the monochromatic-edge objective.
    Kernel kernel = parse(
        "p cnf 9 4\n"
        "1 2 3 0\n4 5 6 0\n7 8 9 0\n-1 -4 -7 0\n");
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::NoOp);
    assert(!kernel.stats.nol_hypergraph_model_recognized);
    assert(!kernel.stats.local_search_hints_offered);
  }
  {
    // A complete seven-vertex 3-uniform hypergraph has no 3-coloring. A small
    // generic rule-work allowance stops the search at an atomic boundary and
    // exposes the best complete coloring only as phase guidance.
    std::vector<std::array<int, 3>> edges;
    for (int a = 0; a < 7; ++a)
      for (int b = a + 1; b < 7; ++b)
        for (int c = b + 1; c < 7; ++c)
          edges.push_back({a, b, c});
    // The eighth variable triple is intentionally inactive. It must remain
    // unknown in a partial hint instead of receiving invented false phases.
    Kernel kernel = parse(hypergraph(8, edges, false, 7));
    kernel.work_budget_bounded = true;
    kernel.work_budget = 8000;
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::Ok);
    assert(kernel.stats.nol_hypergraph_model_recognized == 1);
    assert(kernel.stats.nol_hypergraph_model_budget_exhausted == 1);
    assert(kernel.trail.empty());
    const auto &hint = kernel.local_search_hints[
        size_t(strategy::LocalSearchKind::HypergraphColoring)];
    assert(hint.available() && hint.objective > 0 && hint.objective_exact);
    assert(hint.known_phases == 21);
    assert(hint.external_phases[21] == 0);
    assert(hint.external_phases[22] == 0);
    assert(hint.external_phases[23] == 0);
    assert(hint.effort.amount <= kernel.work_budget);
  }
  {
    // Stop polling occurs before any publication. An interrupted constructor
    // can retain private best state, but must not leak a model or hint.
    Kernel kernel = parse(hypergraph(4, {{{0, 1, 2}}, {{1, 2, 3}}}));
    volatile sig_atomic_t stop = 1;
    kernel.stop = &stop;
    assert(nol_hypergraph_model_apply(kernel) == ApplyResult::NoOp);
    assert(kernel.stats.nol_hypergraph_model_interrupted == 1);
    assert(kernel.trail.empty() && !kernel.stats.local_search_hints_offered);
  }
  {
    // Stable public instance 25 participates in ordinary registry/replay
    // dispatch and reaches the same validated constructor.
    Kernel kernel = parse(hypergraph(4, {{{0, 1, 2}}, {{1, 2, 3}}}));
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    uint64_t instances[LUCKY_INSTANCE_COUNT];
    assert(lucky.instances(kernel, instances, LUCKY_INSTANCE_COUNT) ==
           LUCKY_ACTIVE_INSTANCE_COUNT);
    assert(instances[LUCKY_NOL_HYPERGRAPH] == LUCKY_NOL_HYPERGRAPH);
    kernel.work_budget_bounded = true;
    kernel.work_budget = 1234;
    const auto feedback = execution::native_rule_feedback(
        kernel, "Lucky", LUCKY_NOL_HYPERGRAPH, "noL hypergraph coloring");
    assert(feedback.meter && feedback.meter(kernel) == 0);
    assert(feedback.description.effort_currency == "hypergraph-work");
    assert(feedback.description.effort_allowance.amount() == 1234);
    kernel.work_budget_bounded = false;
    kernel.work_budget = 0;
    assert(lucky.apply_instance(kernel, LUCKY_NOL_HYPERGRAPH) ==
           ApplyResult::Sat);
  }
  {
    // Authentication is atomic at the measured work boundary. One work unit
    // below the exact successful cost retains no partial source image, starts
    // no trajectory and publishes no logical or phase claim.
    const auto input = hypergraph(5, {{{0, 1, 2}}, {{1, 2, 3}},
                                      {{2, 3, 4}}});
    Kernel measured = parse(input);
    NolHypergraphWalkSession measured_session;
    measured.nol_hypergraph_walk_session = &measured_session;
    const auto measurement = nol_hypergraph_model_authenticate(measured);
    assert(measurement.status == NolHypergraphAuthenticationStatus::Authenticated);
    assert(measurement.recognized && measurement.work > 0);

    Kernel short_kernel = parse(input);
    NolHypergraphWalkSession short_session;
    short_kernel.nol_hypergraph_walk_session = &short_session;
    short_kernel.work_budget_bounded = true;
    short_kernel.work_budget = measurement.work - 1;
    const auto short_result = nol_hypergraph_model_authenticate(short_kernel);
    assert(short_result.status == NolHypergraphAuthenticationStatus::Exhausted);
    assert(!short_result.recognized && !short_session.authenticated(short_kernel));
    assert(short_kernel.stats.nol_hypergraph_model_calls == 0);
    assert(short_session.steps() == 0 && short_session.trajectories() == 0);
    assert(short_result.work <= short_kernel.work_budget);
    assert(short_kernel.trail.empty() &&
           !short_kernel.stats.local_search_hints_offered);

    Kernel exact_kernel = parse(input);
    NolHypergraphWalkSession exact_session;
    exact_kernel.nol_hypergraph_walk_session = &exact_session;
    exact_kernel.work_budget_bounded = true;
    exact_kernel.work_budget = measurement.work;
    const auto exact_result = nol_hypergraph_model_authenticate(exact_kernel);
    assert(exact_result.status == NolHypergraphAuthenticationStatus::Authenticated);
    assert(exact_result.recognized && exact_session.authenticated(exact_kernel));
    assert(exact_result.work == measurement.work);
    assert(!exact_session.initialized() && exact_session.steps() == 0 &&
           exact_session.trajectories() == 0);
    assert(exact_kernel.stats.nol_hypergraph_model_calls == 0);
    assert(exact_kernel.trail.empty() &&
           !exact_kernel.stats.local_search_hints_offered);
  }
  {
    std::vector<std::array<int,3>> edges;
    for(int a=0;a<7;++a)for(int b=a+1;b<7;++b)for(int c=b+1;c<7;++c)edges.push_back({a,b,c});
    const auto input=hypergraph(7,edges);
    Kernel k=parse(input),replay=parse(input);
    NolHypergraphWalkSession session,replay_session;
    k.nol_hypergraph_walk_session=&session;replay.nol_hypergraph_walk_session=&replay_session;
    k.work_budget_bounded=replay.work_budget_bounded=true;
    k.work_budget=replay.work_budget=8000;
    const auto authenticated=nol_hypergraph_model_authenticate(k);
    const auto replay_authenticated=nol_hypergraph_model_authenticate(replay);
    assert(authenticated.status==NolHypergraphAuthenticationStatus::Authenticated);
    assert(replay_authenticated.status==NolHypergraphAuthenticationStatus::Authenticated);
    assert(authenticated.recognized&&replay_authenticated.recognized);
    assert(session.authenticated(k)&&replay_session.authenticated(replay));
    assert(!session.initialized()&&!replay_session.initialized());
    assert(session.steps()==0&&session.trajectories()==0);
    assert(replay_session.steps()==0&&replay_session.trajectories()==0);
    assert(k.stats.nol_hypergraph_model_calls==0&&
           replay.stats.nol_hypergraph_model_calls==0);
    assert(k.trail.empty()&&replay.trail.empty());
    assert(!k.stats.local_search_hints_offered&&
           !replay.stats.local_search_hints_offered);
    const auto first_slice=nol_hypergraph_model_slice(k);
    assert(first_slice.result==ApplyResult::Ok);
    assert(first_slice.status==NolHypergraphSliceStatus::Pending);
    assert(!first_slice.recognized&&first_slice.hint_published&&first_slice.useful_hint);
    assert(first_slice.work<=k.work_budget);
    assert(nol_hypergraph_model_apply(replay)==ApplyResult::Ok);
    assert(session.initialized()&&session.steps()>0&&session.trajectories()==1);
    assert(session.steps()==replay_session.steps()&&session.candidate_fingerprint()==replay_session.candidate_fingerprint());
    // Learned/redundant clauses, semantic helper allocation and collection
    // can change live representation epochs without invalidating this
    // external-coordinate, SAT-only source image. Resumption must not scan the
    // original clauses again.
    const Lit redundant[] = {kernel_internalize(k, 1),
                             kernel_internalize(k, 2),
                             kernel_internalize(k, 3)};
    kernel_add_large_clause(k, redundant, 3, true, 0, ++k.next_clause_id);
    kernel_affine_note_namespace_change(k);
    kernel_collect_garbage(k);
    const auto recognized=k.stats.nol_hypergraph_model_recognized;
    const auto steps=session.steps(),trajectory=session.trajectories();
    const auto arena=k.arena;const auto watches=k.watches.arena;const auto id=k.next_clause_id;
    const auto cumulative=k.stats.nol_hypergraph_model_work;
    k.work_budget=replay.work_budget=3000;
    const auto resumed=nol_hypergraph_model_slice(k);
    assert(resumed.result==ApplyResult::Ok);
    assert(resumed.status==NolHypergraphSliceStatus::Pending);
    assert(!resumed.recognized);
    assert(nol_hypergraph_model_apply(replay)==ApplyResult::Ok);
    assert(k.stats.nol_hypergraph_model_recognized==recognized&&session.steps()>steps&&session.trajectories()==trajectory);
    assert(k.stats.nol_hypergraph_model_work-cumulative<=k.work_budget);
    assert(session.steps()==replay_session.steps()&&session.candidate_fingerprint()==replay_session.candidate_fingerprint());
    assert(k.arena==arena&&k.watches.arena==watches&&k.next_clause_id==id&&k.trail.empty()&&!k.inconsistent&&!k.conflict_valid&&!k.pending.valid);
    const auto hints=k.stats.local_search_hints_offered,fingerprint=session.candidate_fingerprint(),stopped_steps=session.steps();
    volatile sig_atomic_t stop=1;k.stop=&stop;
    assert(nol_hypergraph_model_apply(k)==ApplyResult::NoOp);
    assert(k.stats.local_search_hints_offered==hints&&session.candidate_fingerprint()==fingerprint&&session.steps()==stopped_steps);
    k.stop=nullptr;
    // Root masks update without recognizing the unchanged original clauses.
    kernel_assign_unit(k,kernel_internalize(k,1));assert(propagate_apply(k)==ApplyResult::Ok);
    const auto root_trail=k.trail.size();
    assert(nol_hypergraph_model_apply(k)==ApplyResult::Ok);
    assert(k.stats.nol_hypergraph_model_recognized==recognized&&k.trail.size()==root_trail&&k.next_clause_id==id);
    const auto &hint=k.local_search_hints[size_t(strategy::LocalSearchKind::HypergraphColoring)];
    assert(hint.available()&&hint.external_phases[0]==1&&hint.external_phases[1]==-1&&hint.external_phases[2]==-1);
    assert(!k.inconsistent&&!k.conflict_valid&&!k.pending.valid);
  }
  {
    // A theory-satisfying coloring is not a model of arbitrary live redundant
    // CNF. Reproduce the deterministic first coloring then forbid it with one
    // redundant large clause: the full validator must reject SAT publication.
    const auto input=hypergraph(4,{{{0,1,2}},{{1,2,3}}});
    Kernel baseline=parse(input);assert(nol_hypergraph_model_apply(baseline)==ApplyResult::Sat);
    Kernel k=parse(input);NolHypergraphWalkSession session;k.nol_hypergraph_walk_session=&session;
    std::vector<Lit> reject;
    for(Var v=0;v<3;++v){Var chosen=3*v;while(baseline.values[make_lit(chosen,false)]<0)++chosen;reject.push_back(make_lit(chosen,true));}
    kernel_add_large_clause(k,reject.data(),reject.size(),true,0,++k.next_clause_id);
    const auto id=k.next_clause_id;const auto arena=k.arena;
    assert(nol_hypergraph_model_apply(k)!=ApplyResult::Sat);
    assert(k.trail.empty()&&k.next_clause_id==id&&k.arena==arena&&!k.inconsistent&&!k.conflict_valid&&!k.pending.valid);
    assert(!k.stats.nol_hypergraph_model_sat);
  }
  std::puts("test_nol_hypergraph_model OK");
  return 0;
}

#undef NDEBUG
#include "diagnostics/event_trace.hpp"
#include "policies/heuristics.hpp"

#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

using namespace tactix;

// Every artifact below lands in the machine-wide temporary directory, while a
// change is validated by running ctest over several build directories at once.
// A fixed name makes those runs race: one process unlinks and re-creates the
// file another is about to read back, so the loser reports a missing record
// instead of the collision.  Name each artifact after the process that writes
// it, as test_backbone.cpp already does.
static std::string trace_path(const char *stem) {
  return "/tmp/tactix-test-" + std::string(stem) + "-" +
         std::to_string(static_cast<long>(getpid())) + ".tsv";
}

int main() {
  const std::string imply_path = trace_path("imply-only");
  setenv("TACTIX_EVENT_TRACE", imply_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_IMPLY_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_FROM", "77", 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_TO", "77", 1);
  setenv("SAT_EVENT_TRACE_IMPLY_LITERALS", "2", 1);
  {
    Kernel implied;
    kernel_reserve_vars(implied, 2);
    ScopedHeuristics ih(implied);
    implied.warming = true;
    kernel_assign_decision(implied, internalize(1));
    const size_t begin = implied.trail.size();
    kernel_assign_binary(implied, internalize(2), internalize(-1));
    EventTrace trace;
    assert(trace.open_from_env(implied));
    for (uint64_t conflict : {76, 77, 78}) {
      implied.stats.conflicts = conflict;
      trace.implications(implied, begin);
      trace.detail(implied, "unrelated", 42);
    }
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_IMPLY_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_FROM");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_TO");
  unsetenv("SAT_EVENT_TRACE_IMPLY_LITERALS");
  std::ifstream imply_input(imply_path);
  const std::string imply_text((std::istreambuf_iterator<char>(imply_input)),
                               std::istreambuf_iterator<char>());
  assert(imply_text.find("\timply\t77\t") != std::string::npos);
  assert(imply_text.find("lit=2|level=1|reason=-1,2") != std::string::npos);
  assert(imply_text.find("\timply\t76\t") == std::string::npos);
  assert(imply_text.find("\timply\t78\t") == std::string::npos);
  assert(imply_text.find("unrelated") == std::string::npos);
  std::remove(imply_path.c_str());

  const std::string macro_path = trace_path("macro-boundary");
  const std::string macro_state_path = trace_path("macro-state");
  const std::string macro_active_set_path = trace_path("macro-active-set");
  const std::string inprocess_score_path = trace_path("inprocess-score");
  std::remove(macro_path.c_str());
  std::remove(macro_state_path.c_str());
  std::remove(macro_active_set_path.c_str());
  std::remove(inprocess_score_path.c_str());
  const pid_t macro_child = fork();
  assert(macro_child >= 0);
  if (!macro_child) {
    Kernel child;
    kernel_reserve_vars(child, 2);
    ScopedHeuristics child_h(child);
    child.values[make_lit(1, false)] = 1;
    child.values[make_lit(1, true)] = -1;
    child.assigned[1].level = 0;
    child.stats.conflicts = 17;
    child.stats.decisions = 23;
    child.stats.propagations = 29;
    child.stats.restarts = 3;
    child.stats.reductions = 2;
    child.stats.switched = 1;
    child.stats.rephased = 1;
    setenv("TACTIX_MACRO_BOUNDARY_TRACE", macro_path.c_str(), 1);
    setenv("SAT_MACRO_BOUNDARY_CONFLICT_FROM", "17", 1);
    setenv("SAT_MACRO_BOUNDARY_EXIT_EVENT", "probe_begin", 1);
    setenv("SAT_SEARCH_CHECKPOINT_INTERVAL", "4", 1);
    setenv("SAT_SEARCH_CHECKPOINT_CONFLICT_FROM", "17", 1);
    setenv("TACTIX_MACRO_BOUNDARY_STATE", macro_state_path.c_str(), 1);
    setenv("SAT_MACRO_BOUNDARY_STATE_EVENT", "probe_begin", 1);
    setenv("TACTIX_MACRO_ACTIVE_SET", macro_active_set_path.c_str(), 1);
    setenv("SAT_MACRO_ACTIVE_SET_EVENT", "probe_begin", 1);
    setenv("SAT_MACRO_ACTIVE_SET_CONFLICT", "17", 1);
    setenv("TACTIX_INPROCESS_SCORE_TRACE", inprocess_score_path.c_str(), 1);
    setenv("SAT_INPROCESS_SCORE_EXTERNAL", "1", 1);
    setenv("SAT_INPROCESS_SCORE_CONFLICT", "17", 1);
    EventTrace trace;
    trace.macro(child, "restart_begin");
    trace.macro(child, "restart_end");
    trace.checkpoint(child);
    child.stats.conflicts = 20;
    trace.checkpoint(child);
    child.stats.conflicts = 21;
    child.stats.decisions = 31;
    trace.checkpoint(child);
    child.stats.conflicts = 17;
    child.stats.decisions = 23;
    trace.macro(child, "probe_begin");
    std::_Exit(99);
  }
  int macro_status = 0;
  assert(waitpid(macro_child, &macro_status, 0) == macro_child);
  assert(WIFEXITED(macro_status) && WEXITSTATUS(macro_status) == 0);
  std::ifstream macro_input(macro_path);
  const std::string macro_text(
      (std::istreambuf_iterator<char>(macro_input)),
      std::istreambuf_iterator<char>());
  assert(macro_text.find("# tactix-macro-boundary\t1") !=
         std::string::npos);
  assert(macro_text.find("\trestart_begin\t17\t23\t29\t") !=
         std::string::npos);
  assert(macro_text.find("\trestart_end\t17\t23\t29\t") !=
         std::string::npos);
  assert(macro_text.find("\tsearch_checkpoint\t17\t23\t29\t") !=
         std::string::npos);
  assert(macro_text.find("\tsearch_checkpoint\t21\t31\t29\t") !=
         std::string::npos);
  assert(macro_text.find("\tsearch_checkpoint\t20\t") ==
         std::string::npos);
  assert(macro_text.find("\tprobe_begin\t17\t23\t29\t") !=
         std::string::npos);
  std::ifstream macro_state_input(macro_state_path);
  const std::string macro_state_text(
      (std::istreambuf_iterator<char>(macro_state_input)),
      std::istreambuf_iterator<char>());
  assert(macro_state_text.find("# tactix-macro-state\t1") !=
         std::string::npos);
  assert(macro_state_text.find("tactix\tprobe_begin\t17\t23\t29\t1\t2\t"
                               "0\t0\t0\t0") != std::string::npos);
  std::ifstream macro_active_set_input(macro_active_set_path);
  const std::string macro_active_set_text(
      (std::istreambuf_iterator<char>(macro_active_set_input)),
      std::istreambuf_iterator<char>());
  assert(macro_active_set_text.find("# tactix\tprobe_begin\t17\t1") !=
         std::string::npos);
  assert(macro_active_set_text.find("# external\n1\n") !=
         std::string::npos);
  std::ifstream inprocess_score_input(inprocess_score_path);
  const std::string inprocess_score_text(
      (std::istreambuf_iterator<char>(inprocess_score_input)),
      std::istreambuf_iterator<char>());
  assert(inprocess_score_text.find("# tactix-inprocess-score\t1") !=
         std::string::npos);
  assert(inprocess_score_text.find("tactix\tprobe_begin\t17\t23\t1\t0\t-\t"
                                   "0\t1") != std::string::npos);
  std::remove(macro_path.c_str());
  std::remove(macro_state_path.c_str());
  std::remove(macro_active_set_path.c_str());
  std::remove(inprocess_score_path.c_str());

  const std::string path = trace_path("event-trace");
  std::remove(path.c_str());
  Kernel k;
  kernel_reserve_vars(k, 3);
  ScopedHeuristics h(k);

  unsetenv("TACTIX_EVENT_TRACE");
  {
    EventTrace disabled;
    assert(!disabled.open_from_env(k));
    disabled.macro(k, "must_not_exist");
  }
  std::ifstream absent(path);
  assert(!absent.good());

  setenv("TACTIX_EVENT_TRACE", path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_FORMULA_MACRO", "1", 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_POSE", "1", 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_COST", "1", 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_WITHIN_VIVIFY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_MOVE_CLAUSE_SET", "-3,-2,1", 1);
  setenv("TACTIX_EVENT_TRACE_VIVIFY_CANDIDATE_FROM", "7", 1);
  setenv("TACTIX_EVENT_TRACE_VIVIFY_CANDIDATE_TO", "7", 1);
  setenv("TACTIX_EVENT_TRACE_MODE_CLOCK", "1", 1);
  setenv("TACTIX_EVENT_TRACE_HEAP_REMOVE_DECISION", "0", 1);
  setenv("TACTIX_EVENT_TRACE_FACTOR_MARK", "-2", 1);
  setenv("SAT_EVENT_TRACE_VIVIFY_LITERALS", "-3,1", 1);
  setenv("SAT_EVENT_TRACE_VIVIFY_CLAUSES", "1", 1);
  setenv("SAT_EVENT_TRACE_SUBSTITUTE_ALL", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.macro(k, "lucky_early_begin");
    trace.macro(k, "lucky_early_end", "unknown");
    trace.detail(k, "decision", 3);
    k.h->stable = true;
    trace.heap_insert(k, 1, 17, 4);
    trace.heap_remove(k, 1, 2, 17, 20, 4);
    k.h->stable = false;
    trace.slot_boundary(k, "Sweep", "begin");
    trace.sweep_seed(k, 1);
    trace.sweep_environment(k, 1, 3, 4, 0x1234);
    trace.sweep_effort(k, 2, 17, 31, 5);
    trace.sweep_equivalence(k, internalize(1), internalize(-2));
    trace.sweep_unit(k, internalize(-3));
    trace.factor_seed(k, internalize(-2), 17, 9);
    setenv("TACTIX_EVENT_TRACE_FACTOR_SEED", "1", 1);
    trace.factor_seed(k, internalize(-2), 18, 10);
    trace.factor_seed(k, internalize(1), 19, 11);
    unsetenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
    trace.factor_grid(k, internalize(-2), 3, 4, 5);
    trace.factor_grid_member(k, 0, internalize(-2));
    const Lit factor_members[3] = {internalize(-2), internalize(1),
                                   internalize(3)};
    k.stats.factorings = 7;
    trace.factor_fresh(k, internalize(-2), 2, factor_members, 3, 4, 5, 0, 0,
                       0, 0, 0);
    kernel_mark_factor_literal(k, internalize(-2), "fixture");
    trace.substitute_map(k, internalize(2), internalize(2));
    trace.substitute_map(k, internalize(2), internalize(-3));
    trace.detail(k, "analysis_begin", 2);
    k.stats.search_ticks = 17;
    k.diagnostic_search_propagation_ticks = 11;
    trace.mode_clock(k, "pre_analyze", 23, 5, 3);
    kernel_assign_decision(k, internalize(1));
    const size_t trail_begin = k.trail.size();
    kernel_assign_binary(k, internalize(2), internalize(-1));
    trace.implications(k, trail_begin);
    const Lit conflict[3] = {internalize(-3), internalize(1), internalize(-2)};
    k.conflict_ref = kernel_add_large_clause(k, conflict, 3, false, 0);
    k.conflict_valid = true;
    clause_at(k.arena, k.conflict_ref)->searched = 3;
    trace.watch_move(k, internalize(3), internalize(2), internalize(1),
                     clause_at(k.arena, k.conflict_ref));
    const uint32_t vivify_counts[6] = {11, 12, 13, 14, 15, 16};
    trace.vivify_counts(k, vivify_counts, 6);
    setenv("SAT_EVENT_TRACE_CLAUSE_SET", "-3,1,-2", 1);
    setenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_FROM", "0", 1);
    setenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_TO", "0", 1);
    trace.vivify_candidate(k, "vivify_candidate", 6, k.conflict_ref, 10, 99);
    trace.watch_cost(k, internalize(1), 33, 31, 3, 1, 2, 1, 0, 7);
    trace.vivify_candidate(k, "vivify_candidate", 7, k.conflict_ref, 10, 99);
    trace.watch_cost(k, internalize(1), 33, 31, 3, 1, 2, 1, 0, 7);
    trace.watch_scan(k, internalize(3));
    trace.vivify_candidate(k, "vivify_candidate_end", 7, k.conflict_ref, 17,
                           99);
    trace.watch_cost(k, internalize(1), 1, 1, 2, 0, 0, 0, 0, 2);
    trace.conflict(k);
    trace.learned(k, {internalize(3), internalize(-1), internalize(2)});
    trace.clause_glue(k, "fixture", clause_at(k.arena, k.conflict_ref), 3, 2);
    trace.clause_used(k, clause_at(k.arena, k.conflict_ref), 0);
    trace.detail(k, "analysis_end", 1);
    k.tier1 = 3;
    k.tier2 = 8;
    k.tier1_by_mode[0] = 3;
    k.tier2_by_mode[0] = 8;
    k.stats.used_glue[0][3] = 5;
    trace.retier(k, 0, k.stats.used_glue[0]);
    trace.reorder_begin(k);
    trace.reorder_end(k);
    trace.macro(k, "terminate", "10");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_FORMULA_MACRO");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_POSE");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_COST");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_WITHIN_VIVIFY");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_MOVE_CLAUSE_SET");
  unsetenv("TACTIX_EVENT_TRACE_VIVIFY_CANDIDATE_FROM");
  unsetenv("TACTIX_EVENT_TRACE_VIVIFY_CANDIDATE_TO");
  unsetenv("TACTIX_EVENT_TRACE_MODE_CLOCK");
  unsetenv("TACTIX_EVENT_TRACE_HEAP_REMOVE_DECISION");
  unsetenv("TACTIX_EVENT_TRACE_FACTOR_MARK");
  unsetenv("SAT_EVENT_TRACE_VIVIFY_LITERALS");
  unsetenv("SAT_EVENT_TRACE_VIVIFY_CLAUSES");
  unsetenv("SAT_EVENT_TRACE_SUBSTITUTE_ALL");
  unsetenv("SAT_EVENT_TRACE_CLAUSE_SET");
  unsetenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_FROM");
  unsetenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_TO");
  std::ifstream input(path);
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  assert(text.find("# tactix-event-trace\t1") != std::string::npos);
  assert(text.find("\treorder_begin\t") != std::string::npos);
  assert(text.find("\treorder_end\t") != std::string::npos);
  assert(text.find("\tlucky_early_begin\t") != std::string::npos);
  assert(text.find("\tlucky_early_end\t") != std::string::npos);
  assert(text.find("\tdecision\t") != std::string::npos);
  assert(text.find("stage=pre_analyze|ticks=17|propagate_ticks=11|"
                   "analysis_ticks=6|limit=23|anchor=5|count=3") !=
         std::string::npos);
  assert(text.find("\twatch_move\t") != std::string::npos);
  assert(text.find("\theap_insert\t") != std::string::npos);
  assert(text.find("var=2|score=0.5|from=17|to=4") != std::string::npos);
  assert(text.find("\theap_remove\t") != std::string::npos);
  assert(text.find("removed=2:0.5|removed_pos=17|last=3:0.66666666666666674|"
                   "from=20|to=4") !=
         std::string::npos);
  assert(text.find("\tslot_boundary\t") != std::string::npos);
  assert(text.find("name=Sweep|stage=begin") != std::string::npos);
  assert(text.find("|formula_binary_hash=") != std::string::npos);
  assert(text.find("|root_fixed=") != std::string::npos);
  assert(text.find("|root_fixed_hash=") != std::string::npos);
  assert(text.find("\tsweep_seed\t") != std::string::npos);
  assert(text.find("external=2") != std::string::npos);
  assert(text.find("\tsweep_environment\t") != std::string::npos);
  assert(text.find("\tsweep_effort\t") != std::string::npos);
  assert(text.find("vars=3|clauses=4|hash=0000000000001234") !=
         std::string::npos);
  assert(text.find("\tsweep_equivalence\t") != std::string::npos);
  assert(text.find("a=1|b=-2") != std::string::npos);
  assert(text.find("\tsweep_unit\t") != std::string::npos);
  assert(text.find("lit=-3") != std::string::npos);
  assert(text.find("\tfactor_seed\t") != std::string::npos);
  assert(text.find("external=-2|score=17|remainders=9") !=
         std::string::npos);
  assert(text.find("external=1|score=19|remainders=11") !=
         std::string::npos);
  assert(text.find("external=-2|score=18|remainders=10") ==
         std::string::npos);
  assert(text.find("\tfactor_grid\t") != std::string::npos);
  assert(text.find("\tfactor_grid_member\t") != std::string::npos);
  assert(text.find("\tfactor_fresh\t") != std::string::npos);
  assert(text.find("fresh=3|ordinal=7|seed=-2|factors=3|remainders=4|"
                   "delta=5|members=-2,1,3") != std::string::npos);

  const std::string factor_fresh_only_path =
      trace_path("event-factor-fresh-only");
  std::remove(factor_fresh_only_path.c_str());

  const std::string factor_update_only_path =
      trace_path("event-factor-update-only");
  std::remove(factor_update_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", factor_update_only_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_FACTOR_UPDATE_ONLY", "1", 1);
  setenv("SAT_EVENT_TRACE_FACTOR_UPDATE_ORDINAL", "7", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.macro(k, "restart_begin");
    trace.factor_seed(k, internalize(-2), 17, 9);
    trace.factor_update(k, internalize(-2), 3, internalize(1), 11);
    const Lit factor_members[2] = {internalize(-2), internalize(1)};
    k.stats.factorings = 8;
    trace.factor_fresh(k, internalize(-2), 2, factor_members, 2, 3, 1, 0, 0,
                       0, 0, 0);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_FACTOR_UPDATE_ONLY");
  unsetenv("SAT_EVENT_TRACE_FACTOR_UPDATE_ORDINAL");
  std::ifstream factor_update_only_input(factor_update_only_path);
  const std::string factor_update_only_text(
      (std::istreambuf_iterator<char>(factor_update_only_input)),
      std::istreambuf_iterator<char>());
  assert(factor_update_only_text.find("\tfactor_update\t") !=
         std::string::npos);
  assert(factor_update_only_text.find("\tfactor_fresh\t") !=
         std::string::npos);
  assert(factor_update_only_text.find(
             "factor=7|grid=-2|ordinal=3|external=1|score=11") !=
         std::string::npos);
  assert(factor_update_only_text.find("\tfactor_seed\t") ==
         std::string::npos);
  assert(factor_update_only_text.find("\trestart_begin\t") ==
         std::string::npos);
  std::remove(factor_update_only_path.c_str());

  const std::string factor_literal_only_path =
      trace_path("event-factor-literal-only");
  std::remove(factor_literal_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", factor_literal_only_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_FACTOR_LITERAL_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_FACTOR_LITERAL", "-2", 1);
  setenv("TACTIX_EVENT_TRACE_FACTOR_MARK", "-2", 1);
  setenv("SAT_EVENT_TRACE_BINARY_ADD", "-2", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.macro(k, "restart_begin");
    trace.factor_seed(k, internalize(-2), 17, 9);
    trace.factor_literal_state(k, internalize(-2), true, false, 0, 2);
    trace.factor_literal_entry(k, internalize(-2), 0, true,
                               internalize(1), 0, nullptr);
    trace.factor_mark_transition(k, internalize(-2), true, "test_source");
    trace.binary_add(k, internalize(-2), internalize(3));
    const Lit factor_members[2] = {internalize(-2), internalize(1)};
    trace.factor_fresh(k, internalize(-2), 2, factor_members, 2, 3, 1, 0, 0,
                       0, 0, 0);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_FACTOR_LITERAL_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_FACTOR_LITERAL");
  unsetenv("TACTIX_EVENT_TRACE_FACTOR_MARK");
  unsetenv("SAT_EVENT_TRACE_BINARY_ADD");
  std::ifstream factor_literal_only_input(factor_literal_only_path);
  const std::string factor_literal_only_text(
      (std::istreambuf_iterator<char>(factor_literal_only_input)),
      std::istreambuf_iterator<char>());
  assert(factor_literal_only_text.find("\tfactor_literal_state\t") !=
         std::string::npos);
  assert(factor_literal_only_text.find("\tfactor_literal_entry\t") !=
         std::string::npos);
  assert(factor_literal_only_text.find("\tfactor_fresh\t") !=
         std::string::npos);
  assert(factor_literal_only_text.find(
             "\tfactor_mark_transition\t") != std::string::npos);
  assert(factor_literal_only_text.find(
             "external=-2|marked=1|source=test_source") !=
         std::string::npos);
  assert(factor_literal_only_text.find("\tbinary_add\t") !=
         std::string::npos);
  assert(factor_literal_only_text.find("a=-2|b=3") != std::string::npos);
  assert(factor_literal_only_text.find("\tfactor_seed\t") ==
         std::string::npos);
  assert(factor_literal_only_text.find("\trestart_begin\t") ==
         std::string::npos);
  std::remove(factor_literal_only_path.c_str());

  const std::string clause_lifecycle_path =
      trace_path("event-clause-lifecycle-only");
  std::remove(clause_lifecycle_path.c_str());
  setenv("TACTIX_EVENT_TRACE", clause_lifecycle_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_CLAUSE_LIFECYCLE_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_FACTOR_LITERAL", "-2", 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_FROM", "10", 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_TO", "10", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail_text(k, "watch", "discarded");
    trace.macro(k, "restart_begin");
    trace.slot_boundary(k, "Outside", "begin");
    k.stats.conflicts = 10;
    trace.macro(k, "probe_begin");
    trace.slot_boundary(k, "Factor", "begin");
    trace.factor_literal_state(k, internalize(-2), true, false, 0, 2);
  }
  k.stats.conflicts = 0;
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_CLAUSE_LIFECYCLE_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_FACTOR_LITERAL");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_FROM");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_TO");
  std::ifstream clause_lifecycle_input(clause_lifecycle_path);
  const std::string clause_lifecycle_text(
      (std::istreambuf_iterator<char>(clause_lifecycle_input)),
      std::istreambuf_iterator<char>());
  assert(clause_lifecycle_text.find("\tprobe_begin\t") !=
         std::string::npos);
  assert(clause_lifecycle_text.find("\tslot_boundary\t") !=
         std::string::npos);
  assert(clause_lifecycle_text.find("name=Factor|stage=begin") !=
         std::string::npos);
  assert(clause_lifecycle_text.find("\tfactor_literal_state\t") !=
         std::string::npos);
  assert(clause_lifecycle_text.find("\twatch\t") == std::string::npos);
  assert(clause_lifecycle_text.find("\trestart_begin\t") ==
         std::string::npos);
  assert(clause_lifecycle_text.find("name=Outside") == std::string::npos);
  std::remove(clause_lifecycle_path.c_str());

  setenv("TACTIX_EVENT_TRACE", factor_fresh_only_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_FACTOR_FRESH_ONLY", "1", 1);
  setenv("SAT_EVENT_TRACE_FACTOR_METER", "1", 1);
  setenv("SAT_EVENT_TRACE_FACTOR_METER_COMPONENTS", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.macro(k, "restart_begin");
    trace.factor_seed(k, internalize(-2), 17, 9);
    const Lit factor_members[2] = {internalize(-2), internalize(1)};
    trace.factor_fresh(k, internalize(-2), 2, factor_members, 2, 3, 1, 123,
                       7, 11, 13, 17);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_FACTOR_FRESH_ONLY");
  unsetenv("SAT_EVENT_TRACE_FACTOR_METER");
  unsetenv("SAT_EVENT_TRACE_FACTOR_METER_COMPONENTS");
  std::ifstream factor_fresh_only_input(factor_fresh_only_path);
  const std::string factor_fresh_only_text(
      (std::istreambuf_iterator<char>(factor_fresh_only_input)),
      std::istreambuf_iterator<char>());
  assert(factor_fresh_only_text.find("\tfactor_fresh\t") !=
         std::string::npos);
  assert(factor_fresh_only_text.find("|ticks=123") != std::string::npos);
  assert(factor_fresh_only_text.find(
             "|pops=7|first_ticks=11|next_ticks=13|match_ticks=17") !=
         std::string::npos);
  assert(factor_fresh_only_text.find("\tfactor_seed\t") ==
         std::string::npos);
  assert(factor_fresh_only_text.find("\trestart_begin\t") ==
         std::string::npos);
  std::remove(factor_fresh_only_path.c_str());

  const std::string factor_seed_only_path =
      trace_path("event-factor-seed-only");
  std::remove(factor_seed_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", factor_seed_only_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_FACTOR_SEED_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_FACTOR_SEED", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.macro(k, "restart_begin");
    trace.factor_seed(k, internalize(-2), 17, 9);
    trace.factor_seed(k, internalize(1), 19, 11);
    const Lit factor_members[2] = {internalize(1), internalize(-2)};
    trace.factor_fresh(k, internalize(-2), 2, factor_members, 2, 3, 1, 0, 0,
                       0, 0, 0);
    trace.factor_fresh(k, internalize(1), 2, factor_members, 2, 3, 1, 0, 0,
                       0, 0, 0);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_FACTOR_SEED_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_FACTOR_SEED");
  std::ifstream factor_seed_only_input(factor_seed_only_path);
  const std::string factor_seed_only_text(
      (std::istreambuf_iterator<char>(factor_seed_only_input)),
      std::istreambuf_iterator<char>());
  assert(factor_seed_only_text.find("\ttrace_begin\t") != std::string::npos);
  assert(factor_seed_only_text.find("\tfactor_seed\t") !=
         std::string::npos);
  assert(factor_seed_only_text.find(
             "external=1|score=19|remainders=11") != std::string::npos);
  assert(factor_seed_only_text.find("external=-2|score=17") ==
         std::string::npos);
  assert(factor_seed_only_text.find("\tfactor_fresh\t") !=
         std::string::npos);
  assert(factor_seed_only_text.find("seed=1|factors=2") !=
         std::string::npos);
  assert(factor_seed_only_text.find("seed=-2|factors=2") ==
         std::string::npos);
  assert(factor_seed_only_text.find("\trestart_begin\t") ==
         std::string::npos);
  std::remove(factor_seed_only_path.c_str());
  assert(text.find("ordinal=0|external=-2") != std::string::npos);
  assert(text.find("external=-2|factors=3|remainders=4|delta=5") !=
         std::string::npos);
  assert(text.find("\tfactor_mark_transition\t") != std::string::npos);
  assert(text.find("external=-2|marked=1|source=fixture") !=
         std::string::npos);
  assert(text.find("\tsubstitute_map\t") != std::string::npos);
  assert(text.find("external=2|representative=-3") != std::string::npos);
  assert(text.find("external=2|representative=2") == std::string::npos);
  assert(text.find("\tanalysis_begin\t") != std::string::npos);
  assert(text.find("\timply\t") != std::string::npos);
  assert(text.find("\tlit=2|level=1|reason=-1,2\n") != std::string::npos);
  assert(text.find("\tconflict\t") != std::string::npos);
  const size_t watch_cost_pos = text.find("\twatch_cost\t");
  assert(watch_cost_pos != std::string::npos);
  assert(text.find("ordinal=7|scan=1|words_before=33|words_after=31|base=3"
                   "|binary_assign=1|large_deref=2|relocate=1"
                   "|large_assign=0|total=7",
                   watch_cost_pos) != std::string::npos);
  assert(text.find("ordinal=6|scan=1") == std::string::npos);
  assert(text.find("|clause=-3,-2,1|pose=-3,1,-2|priority=") !=
         std::string::npos);
  assert(text.find("\tvivify_counts\t") != std::string::npos);
  assert(text.find("\tvivify_occurrence_clause\t") != std::string::npos);
  assert(text.find("literal=-3|hash=") != std::string::npos);
  assert(text.find("literals=-3:16@raw=5@import=5@compact=5,"
                   "1:11@raw=0@import=0@compact=0") != std::string::npos);
  assert(text.find("ordinal=7|ticks=10|limit=99|size=3") !=
             std::string::npos &&
         text.find("|clause_poses=id=0,ref=0,order=0,searched=3,redundant=0,glue=0,used=0,priority=0,"
                   "pose=-3,1,-2") != std::string::npos);
  assert(text.find("|searched=3|pose=-3,1,-2\n") != std::string::npos);
  assert(text.find("\torder=-3,1,-2|set=-3,-2,1|") != std::string::npos);
  assert(text.find("|ref=0|id=0|glue=0|used=0|redundant=0") !=
         std::string::npos);
  assert(text.find("\tlearned\t") != std::string::npos);
  assert(text.find("\tretier\t") != std::string::npos);
  assert(text.find("focused=3,8|stable=2,6|posed=3,8|used="
                   "0:0,1:0,2:0,3:5,") !=
         std::string::npos);
  assert(text.find("\torder=3,-1,2|set=-1,2,3|levels=0,1,1"
                   "|nonasserting_glue=1\n") != std::string::npos);
  assert(text.find("\treason_glue\t") != std::string::npos);
  assert(text.find("source=fixture|id=0|old=3|new=2|pose=-3,1,-2"
                   "|levels=0,1,1|set=-3,-2,1") != std::string::npos);
  assert(text.find("mode=0|glue=0|pose=-3,1,-2|levels=0,1,1"
                   "|set=-3,-2,1") != std::string::npos);
  const size_t analysis = text.find("\tanalysis_end\t");
  assert(analysis != std::string::npos);
  const size_t line_end = text.find('\n', analysis);
  const std::string analysis_line = text.substr(analysis, line_end - analysis);
  assert(analysis_line.find("\tfocused\t3\t1\t2\t3\t0\t") !=
         std::string::npos &&
         "analysis_end must carry the post-analysis ranking snapshot");
  assert(analysis_line.find("\t3\t3\t1") != std::string::npos &&
         "focused top32 must retain queue-tail decision order");
  assert(analysis_line.find("|clause_set=-3,1,-2|clause_poses="
                            "id=0,ref=0,order=0,searched=3,redundant=0,glue=0,used=0,priority=0,"
                            "pose=-3,1,-2") != std::string::npos);
  std::remove(path.c_str());

  const std::string heap_vars_path = trace_path("event-heap-vars-only");
  std::remove(heap_vars_path.c_str());
  k.h->stable = true;
  setenv("TACTIX_EVENT_TRACE", heap_vars_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_HEAP_VARS_ONLY", "1", 1);
  setenv("SAT_EVENT_TRACE_HEAP_VARS", "2", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "decision", 99);
    trace.heap_insert(k, 0, 8, 4);
    trace.heap_insert(k, 1, 7, 3);
    trace.heap_remove(k, 1, 2, 3, 9, 5);
    trace.heap_pop(k, "begin", 1, 0, 7, 0);
    trace.substitute_map(k, internalize(1), internalize(1));
    trace.substitute_map(k, internalize(2), internalize(-1));
    Kernel root;
    kernel_reserve_vars(root, 2);
    ScopedHeuristics root_h(root);
    kernel_assign_unit(root, internalize(1));
    kernel_assign_unit(root, internalize(2));
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_HEAP_VARS_ONLY");
  unsetenv("SAT_EVENT_TRACE_HEAP_VARS");
  std::ifstream heap_vars_input(heap_vars_path);
  const std::string heap_vars_text(
      (std::istreambuf_iterator<char>(heap_vars_input)),
      std::istreambuf_iterator<char>());
  assert(heap_vars_text.find("\tdecision\t") == std::string::npos);
  assert(heap_vars_text.find("\theap_insert\t") != std::string::npos);
  assert(heap_vars_text.find("var=2|") != std::string::npos);
  assert(heap_vars_text.find("var=1|") == std::string::npos);
  assert(heap_vars_text.find("\theap_remove\t") != std::string::npos);
  assert(heap_vars_text.find("removed=2:") != std::string::npos);
  assert(heap_vars_text.find("\theap_pop\t") != std::string::npos);
  assert(heap_vars_text.find("stage=begin|held=2:") != std::string::npos);
  assert(heap_vars_text.find("\troot_assign\t") != std::string::npos);
  assert(heap_vars_text.find("lit=2|warming=0|trail_after=") !=
         std::string::npos);
  assert(heap_vars_text.find("|reason=unit") != std::string::npos);
  assert(heap_vars_text.find("lit=1|warming=") == std::string::npos);
  assert(heap_vars_text.find("\tsubstitute_map\t") != std::string::npos);
  assert(heap_vars_text.find("external=2|representative=-1|") !=
         std::string::npos);
  assert(heap_vars_text.find("external=1|representative=1|") ==
         std::string::npos);
  std::remove(heap_vars_path.c_str());
  k.h->stable = false;

  const std::string heap_window_path = trace_path("event-heap-vars-window");
  std::remove(heap_window_path.c_str());
  k.h->stable = true;
  const uint64_t heap_window_saved_conflicts = k.stats.conflicts;
  const std::string selected_conflict =
      std::to_string(heap_window_saved_conflicts + 1);
  setenv("TACTIX_EVENT_TRACE", heap_window_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_HEAP_VARS_ONLY", "1", 1);
  setenv("SAT_EVENT_TRACE_HEAP_VARS", "2", 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_FROM", selected_conflict.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_TO", selected_conflict.c_str(), 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.heap_insert(k, 1, 7, 3);
    k.stats.conflicts++;
    trace.heap_insert(k, 1, 7, 3);
  }
  k.stats.conflicts = heap_window_saved_conflicts;
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_HEAP_VARS_ONLY");
  unsetenv("SAT_EVENT_TRACE_HEAP_VARS");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_FROM");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_TO");
  std::ifstream heap_window_input(heap_window_path);
  const std::string heap_window_text(
      (std::istreambuf_iterator<char>(heap_window_input)),
      std::istreambuf_iterator<char>());
  const size_t selected_insert = heap_window_text.find("\theap_insert\t");
  assert(selected_insert != std::string::npos);
  assert(heap_window_text.find("\theap_insert\t", selected_insert + 1) ==
         std::string::npos);
  std::remove(heap_window_path.c_str());
  k.h->stable = false;

  const std::string glue_only_path = trace_path("event-glue-only");
  std::remove(glue_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", glue_only_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_GLUE_ONLY", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.macro(k, "must_be_filtered");
    trace.clause_glue(k, "fixture", clause_at(k.arena, k.conflict_ref), 3, 2);
    trace.detail(k, "analysis_end", 1);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_GLUE_ONLY");
  std::ifstream glue_only_input(glue_only_path);
  const std::string glue_only_text(
      (std::istreambuf_iterator<char>(glue_only_input)),
      std::istreambuf_iterator<char>());
  assert(glue_only_text.find("\treason_glue\t") != std::string::npos);
  assert(glue_only_text.find("\tanalysis_end\t") != std::string::npos);
  assert(glue_only_text.find("must_be_filtered") == std::string::npos);
  assert(glue_only_text.find("\ttrace_begin\t") == std::string::npos);
  std::remove(glue_only_path.c_str());

  const std::string formula_only_path = trace_path("event-formula-only");
  const std::string heap_dump_path = trace_path("event-heap-dump");
  std::remove(formula_only_path.c_str());
  std::remove(heap_dump_path.c_str());
  setenv("TACTIX_EVENT_TRACE", formula_only_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_FORMULA_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_FORMULA_THRESHOLDS", "1,10", 1);
  setenv("TACTIX_EVENT_TRACE_HEAP_DUMP", heap_dump_path.c_str(), 1);
  k.h->scores.stack = {0};
  k.h->scores.pos[0] = 0;
  k.h->scores.score[0] = 3.25;
  const uint64_t saved_conflicts = k.stats.conflicts;
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "decision", 1);
    trace.macro(k, "must_be_filtered");
    k.stats.conflicts = 2; // first analysis end after threshold 1
    trace.detail(k, "analysis_end", 1);
    k.stats.conflicts = 12; // conflict counters may skip threshold 10
    trace.detail(k, "analysis_end", 1);
    trace.macro(k, "terminate", "0");
  }
  k.stats.conflicts = saved_conflicts;
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_FORMULA_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_FORMULA_THRESHOLDS");
  unsetenv("TACTIX_EVENT_TRACE_HEAP_DUMP");
  std::ifstream formula_only_input(formula_only_path);
  const std::string formula_only_text(
      (std::istreambuf_iterator<char>(formula_only_input)),
      std::istreambuf_iterator<char>());
  assert(formula_only_text.find("\ttrace_begin\t") != std::string::npos);
  const size_t first_formula = formula_only_text.find("\tformula_checkpoint\t");
  assert(first_formula != std::string::npos);
  assert(formula_only_text.find("\tformula_checkpoint\t", first_formula + 1) !=
         std::string::npos);
  assert(formula_only_text.find("\tterminate\t") != std::string::npos);
  assert(formula_only_text.find("\tdecision\t") == std::string::npos);
  assert(formula_only_text.find("must_be_filtered") == std::string::npos);
  std::ifstream heap_dump_input(heap_dump_path);
  const std::string heap_dump_text(
      (std::istreambuf_iterator<char>(heap_dump_input)),
      std::istreambuf_iterator<char>());
  assert(heap_dump_text.find("tactix\t2\t0\t1\t0\t0\t0\t3.25\n") !=
         std::string::npos);
  assert(heap_dump_text.find("tactix\t12\t0\t1\t0\t0\t0\t3.25\n") !=
         std::string::npos);
  std::remove(formula_only_path.c_str());
  std::remove(heap_dump_path.c_str());

  const std::string conflict_only_path = trace_path("event-conflict-only");
  std::remove(conflict_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", conflict_only_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_ONLY", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "decision", 1);
    trace.conflict(k);
    trace.macro(k, "must_be_filtered");
    trace.macro(k, "terminate", "0");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_ONLY");
  std::ifstream conflict_only_input(conflict_only_path);
  const std::string conflict_only_text(
      (std::istreambuf_iterator<char>(conflict_only_input)),
      std::istreambuf_iterator<char>());
  assert(conflict_only_text.find("\ttrace_begin\t") != std::string::npos);
  assert(conflict_only_text.find("\tconflict\t") != std::string::npos);
  assert(conflict_only_text.find("\tterminate\t") != std::string::npos);
  assert(conflict_only_text.find("\tdecision\t") == std::string::npos);
  assert(conflict_only_text.find("must_be_filtered") == std::string::npos);
  std::remove(conflict_only_path.c_str());

  const std::string learned_only_path = trace_path("event-learned-only");
  std::remove(learned_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", learned_only_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_LEARNED_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_FROM", "2", 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_TO", "2", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    k.stats.conflicts = 1;
    trace.learned(k, {internalize(1), internalize(-2)});
    k.stats.conflicts = 2;
    trace.detail(k, "decision", 1);
    trace.conflict(k);
    trace.macro(k, "must_be_filtered");
    trace.learned_stage(k, "learned_raw",
                        {internalize(-1), internalize(3)});
    trace.learned(k, {internalize(1), internalize(-2)});
    k.stats.conflicts = 3;
    trace.learned(k, {internalize(2), internalize(-3)});
    trace.macro(k, "terminate", "0");
  }
  k.stats.conflicts = saved_conflicts;
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_LEARNED_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_FROM");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_TO");
  std::ifstream learned_only_input(learned_only_path);
  const std::string learned_only_text(
      (std::istreambuf_iterator<char>(learned_only_input)),
      std::istreambuf_iterator<char>());
  assert(learned_only_text.find("\ttrace_begin\t") != std::string::npos);
  assert(learned_only_text.find("\tlearned\t2\t") != std::string::npos);
  assert(learned_only_text.find("size=2|order_hash=") != std::string::npos);
  assert(learned_only_text.find("\tterminate\t3\t") != std::string::npos);
  assert(learned_only_text.find("\tlearned\t1\t") == std::string::npos);
  assert(learned_only_text.find("\tlearned\t3\t") == std::string::npos);
  assert(learned_only_text.find("\tdecision\t") == std::string::npos);
  assert(learned_only_text.find("\tconflict\t") == std::string::npos);
  assert(learned_only_text.find("\tlearned_raw\t") == std::string::npos);
  assert(learned_only_text.find("must_be_filtered") == std::string::npos);
  std::remove(learned_only_path.c_str());

  // The generic post-Reorder detail budget bounds verbose internals, but a
  // trajectory-only observer must retain every portable analysis boundary.
  // Otherwise 1024 filtered detail calls manufacture a Tactix/Kissat stream
  // divergence even while decisions and learned clauses remain identical.
  const std::string trajectory_reorder_path =
      trace_path("event-trajectory-reorder");
  std::remove(trajectory_reorder_path.c_str());
  setenv("TACTIX_EVENT_TRACE", trajectory_reorder_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.reorder_begin(k);
    trace.reorder_end(k);
    for (unsigned i = 0; i != 1024; ++i)
      trace.detail(k, "analysis_mark", i);
    trace.detail(k, "analysis_begin", 2);
    trace.detail(k, "analysis_end", 1);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY");
  std::ifstream trajectory_reorder_input(trajectory_reorder_path);
  const std::string trajectory_reorder_text(
      (std::istreambuf_iterator<char>(trajectory_reorder_input)),
      std::istreambuf_iterator<char>());
  assert(trajectory_reorder_text.find("\tanalysis_begin\t") !=
         std::string::npos);
  assert(trajectory_reorder_text.find("\tanalysis_end\t") !=
         std::string::npos);
  assert(trajectory_reorder_text.find("\tanalysis_mark\t") ==
         std::string::npos);
  std::remove(trajectory_reorder_path.c_str());

  // Pop localization must not inherit BVE_HEAP_ONLY's candidate/plan volume.
  // Both implementations expose this same diagnostic-only filter so a full
  // first schedule costs one summary plus one row per selected/checkpoint pop.
  const std::string bve_pop_only_path = trace_path("event-bve-pop-only");
  std::remove(bve_pop_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", bve_pop_only_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_BVE_POP_ONLY", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    const Var members[2] = {0, 1};
    const double scores[3] = {3.0, 2.0, 1.0};
    trace.bve_schedule(k, members, scores, 2, 0, 17);
    trace.bve_candidate(k, 0, "begin", 1, 2, false);
    trace.bve_plan(k, 0, 1, 1, 0, 0, 0, 1, false);
    trace.bve_pop(k, 4096, 23, 0, 3.0, 1, 1);
    trace.detail(k, "decision", 1);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_BVE_POP_ONLY");
  std::ifstream bve_pop_only_input(bve_pop_only_path);
  const std::string bve_pop_only_text(
      (std::istreambuf_iterator<char>(bve_pop_only_input)),
      std::istreambuf_iterator<char>());
  assert(bve_pop_only_text.find("\tbve_schedule\t") != std::string::npos);
  assert(bve_pop_only_text.find("\tbve_pop_checkpoint\t") !=
         std::string::npos);
  assert(bve_pop_only_text.find("\tbve_candidate\t") == std::string::npos);
  assert(bve_pop_only_text.find("\tbve_plan\t") == std::string::npos);
  assert(bve_pop_only_text.find("\tdecision\t") == std::string::npos);
  std::remove(bve_pop_only_path.c_str());

  // Compact inprocessing traces are the matched-cost surface for all
  // candidate-oriented rules.  In particular BVE and its phase cleanup must
  // survive this filter in both Tactix and the Kissat observer overlay; losing
  // either silently turns an exact parity campaign into a one-sided trace.
  const std::string compact_path = trace_path("event-compact-inprocess");
  std::remove(compact_path.c_str());
  setenv("TACTIX_EVENT_TRACE", compact_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_COMPACT_PROBE", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.bve_candidate(k, 0, "begin", 11, 19, false);
    trace.bve_plan(k, 0, 2, 3, 0, 0, 0, 6, false);
    trace.eliminate_cleanup(k, "before");
    trace.detail(k, "decision", 1);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_COMPACT_PROBE");
  std::ifstream compact_input(compact_path);
  const std::string compact_text(
      (std::istreambuf_iterator<char>(compact_input)),
      std::istreambuf_iterator<char>());
  assert(compact_text.find("\tbve_candidate\t") != std::string::npos);
  assert(compact_text.find("pivot_value=") != std::string::npos);
  assert(compact_text.find("\tbve_plan\t") != std::string::npos);
  assert(compact_text.find("\teliminate_cleanup\t") != std::string::npos);
  assert(compact_text.find("\tdecision\t") == std::string::npos);
  std::remove(compact_path.c_str());

  const std::string boundary_path = trace_path("event-boundary-only");
  std::remove(boundary_path.c_str());
  setenv("TACTIX_EVENT_TRACE", boundary_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_BOUNDARY_ONLY", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.slot_boundary(k, "Vivify", "begin");
    trace.factor_seed(k, 0, 3, 2);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_BOUNDARY_ONLY");
  std::ifstream boundary_input(boundary_path);
  const std::string boundary_text(
      (std::istreambuf_iterator<char>(boundary_input)),
      std::istreambuf_iterator<char>());
  assert(boundary_text.find("\tslot_boundary\t") != std::string::npos);
  assert(boundary_text.find("name=Vivify|stage=begin") != std::string::npos);
  assert(boundary_text.find("\tfactor_seed\t") == std::string::npos);
  std::remove(boundary_path.c_str());

  // Stable top32 is a portable reference comparison, not a rendering of
  // Tactix's dense variable indices.  Kissat assigns those indices in first
  // import order, which can disagree with both external and current compact
  // order.  Equal scores must therefore retain that permanent permutation.
  const std::string import_tie_path = trace_path("event-import-tie");
  const std::string heap_set_path = trace_path("event-heap-set");
  std::remove(import_tie_path.c_str());
  std::remove(heap_set_path.c_str());
  Kernel import_tie_kernel;
  kernel_reserve_vars(import_tie_kernel, 3);
  kernel_record_import(import_tie_kernel, 1);
  kernel_record_import(import_tie_kernel, 0);
  kernel_record_import(import_tie_kernel, 2);
  ScopedHeuristics import_tie_h(import_tie_kernel);
  for (Var v = 0; v < import_tie_kernel.nvars; ++v)
    heuristics_score_set(import_tie_kernel, v, 0.0);
  import_tie_kernel.h->stable = true;
  heuristics_update_scores(import_tie_kernel);
  setenv("TACTIX_EVENT_TRACE", import_tie_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_HEAP_SET_PATH", heap_set_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_HEAP_SET_CONFLICT", "0", 1);
  setenv("SAT_EVENT_TRACE_HEAP_SET_DECISION", "0", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(import_tie_kernel));
    trace.heap_scores(import_tie_kernel);
    trace.detail(import_tie_kernel, "analysis_end", 0);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_HEAP_SET_PATH");
  unsetenv("SAT_EVENT_TRACE_HEAP_SET_CONFLICT");
  unsetenv("SAT_EVENT_TRACE_HEAP_SET_DECISION");
  std::ifstream import_tie_input(import_tie_path);
  const std::string import_tie_text(
      (std::istreambuf_iterator<char>(import_tie_input)),
      std::istreambuf_iterator<char>());
  const size_t import_tie_analysis =
      import_tie_text.find("\tanalysis_end\t");
  assert(import_tie_analysis != std::string::npos);
  const size_t import_tie_end =
      import_tie_text.find('\n', import_tie_analysis);
  const std::string import_tie_line =
      import_tie_text.substr(import_tie_analysis,
                             import_tie_end - import_tie_analysis);
  assert(import_tie_line.find("\t2,1,3\t0") != std::string::npos &&
         "stable score ties must follow Kissat's first-import index order");
  std::ifstream heap_set_input(heap_set_path);
  const std::string heap_set_text(
      (std::istreambuf_iterator<char>(heap_set_input)),
      std::istreambuf_iterator<char>());
  assert(heap_set_text.find("# tactix-heap-set\t1\n") != std::string::npos);
  assert(heap_set_text.find("2\t0\t0\t0\t1\n") != std::string::npos);
  assert(heap_set_text.find("1\t1\t0\t0\t1\n") != std::string::npos);
  assert(heap_set_text.find("3\t2\t0\t0\t1\n") != std::string::npos);
  std::remove(import_tie_path.c_str());
  std::remove(heap_set_path.c_str());

  const std::string watch_positions_path =
      trace_path("event-watch-positions");
  std::remove(watch_positions_path.c_str());
  Kernel watch_kernel;
  kernel_reserve_vars(watch_kernel, 3);
  ScopedHeuristics watch_h(watch_kernel);
  setenv("TACTIX_EVENT_TRACE", watch_positions_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_WATCH_LIST", "-1", 1);
  setenv("SAT_EVENT_TRACE_WATCH_TARGETS", "2,3", 1);
  setenv("SAT_EVENT_TRACE_BINARY_ADD", "-1", 1);
  setenv("SAT_EVENT_TRACE_SWEEP_REWRITE", "-1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(watch_kernel));
    trace.binary_add(watch_kernel, internalize(-1), internalize(2));
    kernel_add_binary_clause(watch_kernel, internalize(-1), internalize(2));
    trace.binary_add(watch_kernel, internalize(-1), internalize(-3));
    kernel_add_binary_clause(watch_kernel, internalize(-1), internalize(-3));
    trace.sweep_rewrite_binary(watch_kernel, internalize(-2),
                               internalize(-1), 7, internalize(3));
    trace.macro(watch_kernel, "watch_positions");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_WATCH_LIST");
  unsetenv("SAT_EVENT_TRACE_WATCH_TARGETS");
  unsetenv("SAT_EVENT_TRACE_BINARY_ADD");
  unsetenv("SAT_EVENT_TRACE_SWEEP_REWRITE");
  std::ifstream watch_positions_input(watch_positions_path);
  const std::string watch_positions_text(
      (std::istreambuf_iterator<char>(watch_positions_input)),
      std::istreambuf_iterator<char>());
  assert(watch_positions_text.find(
             "|watch_lit=-1|binary_count=2|binary_hash=") !=
         std::string::npos);
  assert(watch_positions_text.find(
             "|target_a=2|target_a_pos=1|target_a_lit=2"
             "|target_b=3|target_b_pos=2|target_b_lit=-3") !=
         std::string::npos);
  assert(watch_positions_text.find("\tbinary_add\t") != std::string::npos);
  assert(watch_positions_text.find("\ta=-1|b=2\n") != std::string::npos);
  assert(watch_positions_text.find("\ta=-1|b=-3\n") != std::string::npos);
  assert(watch_positions_text.find(
             "\tsweep_rewrite_binary\t") != std::string::npos);
  assert(watch_positions_text.find(
             "\tsource=-2|repr=-1|position=7|other=3\n") !=
         std::string::npos);
  assert(watch_positions_text.find("|binary=") == std::string::npos);
  std::remove(watch_positions_path.c_str());

  const std::string binary_set_path = trace_path("event-binary-set");
  const std::string binary_set_event_path =
      trace_path("event-binary-set-events");
  std::remove(binary_set_path.c_str());
  std::remove(binary_set_event_path.c_str());
  setenv("TACTIX_EVENT_TRACE", binary_set_event_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_BINARY_SET_PATH", binary_set_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_BINARY_SET_MATCH", "selected-snapshot", 1);
  setenv("SAT_EVENT_TRACE_BINARY_SET_CONFLICT", "0", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(watch_kernel));
    trace.macro(watch_kernel, "other-snapshot");
    trace.macro(watch_kernel, "selected-snapshot");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_BINARY_SET_PATH");
  unsetenv("SAT_EVENT_TRACE_BINARY_SET_MATCH");
  unsetenv("SAT_EVENT_TRACE_BINARY_SET_CONFLICT");
  std::ifstream binary_set_input(binary_set_path);
  const std::string binary_set_text(
      (std::istreambuf_iterator<char>(binary_set_input)),
      std::istreambuf_iterator<char>());
  assert(binary_set_text.find(
             "# begin\t0\ttactix\tselected-snapshot\t0\t2\t-") !=
         std::string::npos);
  assert(binary_set_text.find("0\t-3\t-1\n") != std::string::npos);
  assert(binary_set_text.find("0\t-1\t2\n") != std::string::npos);
  assert(binary_set_text.find("other-snapshot") == std::string::npos);
  std::remove(binary_set_path.c_str());
  std::remove(binary_set_event_path.c_str());

  // The standalone BVE schedule observer must not require a global event
  // stream: Rank-17 preprocessing made even target-only global tracing too
  // expensive to reach this boundary.  It emits one row and exits before the
  // first candidate, leaving production and ordinary Trace behavior untouched.
  const std::string bve_schedule_path =
      trace_path("standalone-bve-schedule");
  std::remove(bve_schedule_path.c_str());
  const pid_t bve_schedule_child = fork();
  assert(bve_schedule_child >= 0);
  if (!bve_schedule_child) {
    unsetenv("TACTIX_EVENT_TRACE");
    setenv("TACTIX_BVE_SCHEDULE_TRACE", bve_schedule_path.c_str(), 1);
    setenv("SAT_BVE_SCHEDULE_CONFLICT", "1", 1);
    setenv("SAT_BVE_SCHEDULE_ORDINAL", "1", 1);
    setenv("SAT_BVE_SCHEDULE_MEMBERS", "1", 1);
    setenv("SAT_BVE_SCHEDULE_SELECTED", "1,2", 1);
    std::vector<uint32_t> occurrence_counts(2 * watch_kernel.nvars, 0);
    occurrence_counts[make_lit(0, false)] = 7;
    occurrence_counts[make_lit(0, true)] = 11;
    occurrence_counts[make_lit(2, false)] = 13;
    occurrence_counts[make_lit(2, true)] = 17;
    watch_kernel.bve_occurrence_count_image = &occurrence_counts;
    const Var members[2] = {0, 2};
    const double scores[3] = {-3.0, 0.0, -5.0};
    // A non-matching schedule must neither write nor terminate.  The same
    // standalone observer selects the requested later conflict exactly.
    event_trace_bve_schedule(watch_kernel, members, scores, 2, 0,
                             0x123456789abcdef0ull);
    watch_kernel.stats.conflicts = 1;
    event_trace_bve_schedule(watch_kernel, members, scores, 2, 0,
                             0x123456789abcdef0ull);
    event_trace_bve_schedule(watch_kernel, members, scores, 2, 0,
                             0x123456789abcdef0ull);
    _exit(99);
  }
  int bve_schedule_status = 0;
  assert(waitpid(bve_schedule_child, &bve_schedule_status, 0) ==
         bve_schedule_child);
  assert(WIFEXITED(bve_schedule_status));
  assert(WEXITSTATUS(bve_schedule_status) == 0);
  std::ifstream bve_schedule_input(bve_schedule_path);
  const std::string bve_schedule_text(
      (std::istreambuf_iterator<char>(bve_schedule_input)),
      std::istreambuf_iterator<char>());
  assert(bve_schedule_text.find("# bve-schedule-observer\t1\n") !=
         std::string::npos);
  assert(bve_schedule_text.find("tactix\tbound=0|scheduled=2|") !=
         std::string::npos);
  assert(bve_schedule_text.find("|conflicts=1|decisions=0|propagations=") !=
         std::string::npos);
  assert(bve_schedule_text.find("|active=3|vars=3|trail=") !=
         std::string::npos);
  assert(bve_schedule_text.find(
             "|insertion_hash=123456789abcdef0|") != std::string::npos);
  assert(bve_schedule_text.find(
             "member\texternal=1\tposition=0\tpositive=7\tnegative=11\t") !=
         std::string::npos);
  assert(bve_schedule_text.find(
             "member\texternal=3\tposition=1\tpositive=13\tnegative=17\t") !=
         std::string::npos);
  assert(bve_schedule_text.find(
             "selected\texternal=1\tposition=0\tpositive=7\tnegative=11\t"
             "value=0\tlevel=0\tretired=0\n") != std::string::npos);
  std::remove(bve_schedule_path.c_str());

  const std::string bve_candidate_path =
      trace_path("standalone-bve-candidate");
  std::remove(bve_candidate_path.c_str());
  const pid_t bve_candidate_child = fork();
  assert(bve_candidate_child >= 0);
  if (!bve_candidate_child) {
    unsetenv("TACTIX_EVENT_TRACE");
    setenv("TACTIX_BVE_CANDIDATE_TRACE", bve_candidate_path.c_str(), 1);
    setenv("SAT_BVE_CANDIDATE_CONFLICT", "1", 1);
    setenv("SAT_BVE_CANDIDATE_EXTERNAL", "1", 1);
    setenv("SAT_BVE_CANDIDATE_EXIT_AFTER_END", "1", 1);
    watch_kernel.stats.conflicts = 1;
    watch_kernel.bve_retry.resize(watch_kernel.nvars, 0);
    watch_kernel.bve_retry[0] = 1;
    event_trace_bve_candidate(watch_kernel, 0, "begin", 7, 11, false);
    watch_kernel.bve_retry[0] = 0;
    event_trace_bve_candidate(watch_kernel, 0, "end", 9, 11, false);
    _exit(99);
  }
  int bve_candidate_status = 0;
  assert(waitpid(bve_candidate_child, &bve_candidate_status, 0) ==
         bve_candidate_child);
  assert(WIFEXITED(bve_candidate_status));
  assert(WEXITSTATUS(bve_candidate_status) == 0);
  std::ifstream bve_candidate_input(bve_candidate_path);
  const std::string bve_candidate_text(
      (std::istreambuf_iterator<char>(bve_candidate_input)),
      std::istreambuf_iterator<char>());
  assert(bve_candidate_text.find("# bve-candidate-observer\t1\n") !=
         std::string::npos);
  assert(bve_candidate_text.find(
             "external=1\tstage=begin\tconflicts=1\tticks=7\tlimit=11\t"
             "applied=0\tretry=1\tvalue=0\tretired=0\troots=0\ttrail=0\t"
             "propagate=0\n") !=
         std::string::npos);
  assert(bve_candidate_text.find(
             "external=1\tstage=end\tconflicts=1\tticks=9\tlimit=11\t"
             "applied=0\tretry=0\tvalue=0\tretired=0\troots=0\ttrail=0\t"
             "propagate=0\n") !=
         std::string::npos);
  std::remove(bve_candidate_path.c_str());

  const std::string bve_retry_path = trace_path("standalone-bve-retry");
  std::remove(bve_retry_path.c_str());
  const pid_t bve_retry_child = fork();
  assert(bve_retry_child >= 0);
  if (!bve_retry_child) {
    unsetenv("TACTIX_EVENT_TRACE");
    setenv("TACTIX_BVE_RETRY_TRACE", bve_retry_path.c_str(), 1);
    setenv("SAT_BVE_RETRY_CONFLICT", "1", 1);
    setenv("SAT_BVE_RETRY_EXTERNAL", "1", 1);
    setenv("SAT_BVE_RETRY_EXIT_AFTER_MARK", "1", 1);
    watch_kernel.stats.conflicts = 1;
    watch_kernel.bve_retry.resize(watch_kernel.nvars, 0);
    watch_kernel.bve_retry[0] = 0;
    event_trace_bve_candidate(watch_kernel, 0, "end", 9, 11, false);
    kernel_mark_bve_retry(watch_kernel, 0, "unit-test-source");
    _exit(99);
  }
  int bve_retry_status = 0;
  assert(waitpid(bve_retry_child, &bve_retry_status, 0) == bve_retry_child);
  assert(WIFEXITED(bve_retry_status));
  assert(WEXITSTATUS(bve_retry_status) == 0);
  std::ifstream bve_retry_input(bve_retry_path);
  const std::string bve_retry_text(
      (std::istreambuf_iterator<char>(bve_retry_input)),
      std::istreambuf_iterator<char>());
  assert(bve_retry_text.find("# bve-retry-observer\t1\n") !=
         std::string::npos);
  assert(bve_retry_text.find(
             "external=1\tconflicts=1\tsource=unit-test-source\t"
             "trigger=1\tbefore=0\tafter=1\tvalue=0\tretired=0\t"
             "clause_id=0\tclause_size=0\tclause_hash=0000000000000000\t"
             "clause_lits=-\n") !=
         std::string::npos);
  std::remove(bve_retry_path.c_str());

  const std::string matched_exit_path = trace_path("event-matched-exit");
  std::remove(matched_exit_path.c_str());
  const pid_t matched_exit_child = fork();
  assert(matched_exit_child >= 0);
  if (!matched_exit_child) {
    setenv("TACTIX_EVENT_TRACE_LATE_PATH", matched_exit_path.c_str(), 1);
    // A zero cap plus target-only mode proves the selected boundary bypasses
    // both ordinary event work and the cap at its public entry point.
    setenv("TACTIX_EVENT_TRACE_LIMIT", "0", 1);
    setenv("SAT_EVENT_TRACE_TARGET_ONLY", "1", 1);
    setenv("SAT_EVENT_TRACE_EXIT_AFTER_MATCH",
           "slot_boundary|name=selected|stage=begin", 1);
    setenv("SAT_EVENT_TRACE_EXIT_CONFLICT", "0", 1);
    event_trace_slot_boundary(watch_kernel, "other", "begin");
    event_trace_slot_boundary(watch_kernel, "selected", "begin");
    _exit(99);
  }
  int matched_exit_status = 0;
  assert(waitpid(matched_exit_child, &matched_exit_status, 0) ==
         matched_exit_child);
  assert(WIFEXITED(matched_exit_status));
  assert(WEXITSTATUS(matched_exit_status) == 0);
  std::ifstream matched_exit_input(matched_exit_path);
  const std::string matched_exit_text(
      (std::istreambuf_iterator<char>(matched_exit_input)),
      std::istreambuf_iterator<char>());
  assert(matched_exit_text.find("\tother\t") == std::string::npos);
  assert(matched_exit_text.find(
             "\tslot_boundary\t") != std::string::npos);
  assert(matched_exit_text.find("name=selected|stage=begin") !=
         std::string::npos);
  std::remove(matched_exit_path.c_str());

  const std::string macro_exit_path = trace_path("event-macro-matched-exit");
  std::remove(macro_exit_path.c_str());
  const pid_t macro_exit_child = fork();
  assert(macro_exit_child >= 0);
  if (!macro_exit_child) {
    unsetenv("TACTIX_EVENT_TRACE");
    setenv("TACTIX_EVENT_TRACE_LATE_PATH", macro_exit_path.c_str(), 1);
    setenv("TACTIX_EVENT_TRACE_LIMIT", "0", 1);
    setenv("SAT_EVENT_TRACE_TARGET_ONLY", "1", 1);
    setenv("SAT_EVENT_TRACE_EXIT_AFTER_MATCH", "eliminate_end", 1);
    setenv("SAT_EVENT_TRACE_EXIT_CONFLICT", "0", 1);
    EventTrace late_macro;
    late_macro.macro(watch_kernel, "probe_end");
    late_macro.macro(watch_kernel, "eliminate_end");
    _exit(99);
  }
  int macro_exit_status = 0;
  assert(waitpid(macro_exit_child, &macro_exit_status, 0) == macro_exit_child);
  assert(WIFEXITED(macro_exit_status));
  assert(WEXITSTATUS(macro_exit_status) == 0);
  std::ifstream macro_exit_input(macro_exit_path);
  const std::string macro_exit_text(
      (std::istreambuf_iterator<char>(macro_exit_input)),
      std::istreambuf_iterator<char>());
  assert(macro_exit_text.find("\tprobe_end\t") == std::string::npos);
  assert(macro_exit_text.find("\teliminate_end\t") != std::string::npos);
  std::remove(macro_exit_path.c_str());

  const std::string factor_fresh_exit_path =
      trace_path("factor-fresh-matched-exit");
  std::remove(factor_fresh_exit_path.c_str());
  const pid_t factor_fresh_exit_child = fork();
  assert(factor_fresh_exit_child >= 0);
  if (!factor_fresh_exit_child) {
    unsetenv("TACTIX_EVENT_TRACE_LATE_PATH");
    unsetenv("SAT_EVENT_TRACE_TARGET_ONLY");
    setenv("TACTIX_EVENT_TRACE", factor_fresh_exit_path.c_str(), 1);
    setenv("SAT_EVENT_TRACE_FACTOR_FRESH_ONLY", "1", 1);
    setenv("TACTIX_EVENT_TRACE_LIMIT", "1", 1);
    setenv("SAT_EVENT_TRACE_EXIT_AFTER_MATCH",
           "slot_boundary|name=Factor|stage=end", 1);
    setenv("SAT_EVENT_TRACE_EXIT_CONFLICT", "0", 1);
    EventTrace trace;
    assert(trace.open_from_env(watch_kernel));
    trace.slot_boundary(watch_kernel, "Factor", "end");
    _exit(99);
  }
  int factor_fresh_exit_status = 0;
  assert(waitpid(factor_fresh_exit_child, &factor_fresh_exit_status, 0) ==
         factor_fresh_exit_child);
  assert(WIFEXITED(factor_fresh_exit_status));
  assert(WEXITSTATUS(factor_fresh_exit_status) == 0);
  std::ifstream factor_fresh_exit_input(factor_fresh_exit_path);
  const std::string factor_fresh_exit_text(
      (std::istreambuf_iterator<char>(factor_fresh_exit_input)),
      std::istreambuf_iterator<char>());
  assert(factor_fresh_exit_text.find("\tslot_boundary\t") !=
         std::string::npos);
  assert(factor_fresh_exit_text.find("name=Factor|stage=end") !=
         std::string::npos);
  std::remove(factor_fresh_exit_path.c_str());

  const std::string randec_exit_path = trace_path("event-randec-late-exit");
  std::remove(randec_exit_path.c_str());
  const pid_t randec_exit_child = fork();
  assert(randec_exit_child >= 0);
  if (!randec_exit_child) {
    setenv("TACTIX_EVENT_TRACE_LATE_PATH", randec_exit_path.c_str(), 1);
    setenv("TACTIX_EVENT_TRACE_LIMIT", "0", 1);
    setenv("SAT_EVENT_TRACE_TARGET_ONLY", "1", 1);
    setenv("SAT_EVENT_TRACE_EXIT_AFTER_MATCH", "randec_draw", 1);
    setenv("SAT_EVENT_TRACE_EXIT_CONFLICT", "0", 1);
    setenv("SAT_EVENT_TRACE_EXIT_DECISION", "42", 1);
    watch_kernel.stats.decisions = 41;
    event_trace_randec_draw(watch_kernel, 17, 0, 0, true);
    watch_kernel.stats.decisions = 42;
    event_trace_randec_draw(watch_kernel, 19, 1, 1, true);
    _exit(99);
  }
  int randec_exit_status = 0;
  assert(waitpid(randec_exit_child, &randec_exit_status, 0) ==
         randec_exit_child);
  assert(WIFEXITED(randec_exit_status));
  assert(WEXITSTATUS(randec_exit_status) == 0);
  std::ifstream randec_exit_input(randec_exit_path);
  const std::string randec_exit_text(
      (std::istreambuf_iterator<char>(randec_exit_input)),
      std::istreambuf_iterator<char>());
  assert(randec_exit_text.find("\trandec_draw\t0\t42\t") !=
         std::string::npos);
  assert(randec_exit_text.find("raw=19|slot=1|external=2|accepted=1") !=
         std::string::npos);
  assert(randec_exit_text.find("raw=17") == std::string::npos);
  std::remove(randec_exit_path.c_str());

  const std::string focused_stop_path = trace_path("event-focused-stop");
  std::remove(focused_stop_path.c_str());
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    Kernel stop_kernel;
    kernel_reserve_vars(stop_kernel, 1);
    ScopedHeuristics stop_h(stop_kernel);
    stop_kernel.h->stable = false;
    stop_kernel.stats.conflicts = 1;
    setenv("TACTIX_EVENT_TRACE", focused_stop_path.c_str(), 1);
    setenv("TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT", "1", 1);
    EventTrace trace;
    assert(trace.open_from_env(stop_kernel));
    trace.detail(stop_kernel, "analysis_end", 0);
    _exit(0);
  }
  int child_status = 0;
  assert(waitpid(child, &child_status, 0) == child);
  assert(WIFSIGNALED(child_status));
  assert(WTERMSIG(child_status) == SIGTERM);
  std::remove(focused_stop_path.c_str());

  const std::string keep_snapshots_path =
      trace_path("event-keep-snapshots-before-stop");
  std::remove(keep_snapshots_path.c_str());
  setenv("TACTIX_EVENT_TRACE", keep_snapshots_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT", "2", 1);
  setenv("TACTIX_EVENT_TRACE_KEEP_SNAPSHOTS", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    k.stats.conflicts = 1;
    trace.macro(k, "restart_begin");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_STOP_AFTER_CONFLICT");
  unsetenv("TACTIX_EVENT_TRACE_KEEP_SNAPSHOTS");
  std::ifstream keep_snapshots_input(keep_snapshots_path);
  const std::string keep_snapshots_text(
      (std::istreambuf_iterator<char>(keep_snapshots_input)),
      std::istreambuf_iterator<char>());
  assert(keep_snapshots_text.find("\trestart_begin\t1\t") !=
         std::string::npos);
  std::remove(keep_snapshots_path.c_str());

  const std::string window_path = trace_path("event-window");
  setenv("TACTIX_EVENT_TRACE", window_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_FROM", "2", 1);
  setenv("TACTIX_EVENT_TRACE_CONFLICT_TO", "2", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    k.stats.conflicts = 1;
    trace.detail(k, "decision", 11);
    k.stats.conflicts = 2;
    trace.detail(k, "decision", 22);
    k.stats.conflicts = 3;
    trace.detail(k, "decision", 33);
    trace.macro(k, "terminate", "20");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_FROM");
  unsetenv("TACTIX_EVENT_TRACE_CONFLICT_TO");
  std::ifstream window_input(window_path);
  const std::string window_text((std::istreambuf_iterator<char>(window_input)),
                                std::istreambuf_iterator<char>());
  assert(window_text.find("# detail-conflicts\t2\t2") != std::string::npos);
  assert(window_text.find("\tdecision\t2\t") != std::string::npos);
  assert(window_text.find("\tdecision\t1\t") == std::string::npos);
  assert(window_text.find("\tdecision\t3\t") == std::string::npos);
  std::remove(window_path.c_str());

  const std::string watch_only_path = trace_path("event-watch-only");
  std::remove(watch_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", watch_only_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_CLAUSE_SET", "2,3;-3,-2,1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "decision", 44);
    trace.detail(k, "analysis_begin", 2);
    trace.watch_scan(k, internalize(3));
    trace.macro(k, "restart_begin");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_CLAUSE_SET");
  std::ifstream watch_only_input(watch_only_path);
  const std::string watch_only_text(
      (std::istreambuf_iterator<char>(watch_only_input)),
      std::istreambuf_iterator<char>());
  assert(watch_only_text.find("\twatch\t") != std::string::npos);
  assert(watch_only_text.find("\trestart_begin\t") != std::string::npos);
  assert(watch_only_text.find("\tdecision\t") == std::string::npos);
  assert(watch_only_text.find("\tanalysis_begin\t") == std::string::npos);
  std::remove(watch_only_path.c_str());

  const std::string strict_watch_path =
      trace_path("event-watch-only-strict");
  std::remove(strict_watch_path.c_str());
  setenv("TACTIX_EVENT_TRACE", strict_watch_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_ONLY_STRICT", "1", 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_CLAUSE_SET", "2,3;-3,-2,1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "decision", 44);
    trace.watch_scan(k, internalize(3));
    trace.macro(k, "restart_begin");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_ONLY_STRICT");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_CLAUSE_SET");
  std::ifstream strict_watch_input(strict_watch_path);
  const std::string strict_watch_text(
      (std::istreambuf_iterator<char>(strict_watch_input)),
      std::istreambuf_iterator<char>());
  assert(strict_watch_text.find("\twatch\t") != std::string::npos);
  assert(strict_watch_text.find("\trestart_begin\t") == std::string::npos);
  assert(strict_watch_text.find("\tdecision\t") == std::string::npos);
  assert(strict_watch_text.find("\ttrace_begin\t") == std::string::npos);
  std::remove(strict_watch_path.c_str());

  const std::string garbage_formula_path =
      trace_path("event-formula-garbage");
  const std::string garbage_trace_path =
      trace_path("event-formula-garbage-trace");
  std::remove(garbage_formula_path.c_str());
  std::remove(garbage_trace_path.c_str());
  Kernel garbage_kernel;
  kernel_reserve_vars(garbage_kernel, 3);
  const Lit garbage_lits[3] = {internalize(1), internalize(-2),
                               internalize(3)};
  const Ref garbage_ref =
      kernel_add_large_clause(garbage_kernel, garbage_lits, 3, false, 0);
  kernel_delete_clause(garbage_kernel,
                       clause_at(garbage_kernel.arena, garbage_ref));
  setenv("TACTIX_EVENT_TRACE", garbage_trace_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_FORMULA_SET_PATH", garbage_formula_path.c_str(), 1);
  setenv("SAT_EVENT_TRACE_FORMULA_SET_MATCH", "fixture", 1);
  setenv("SAT_EVENT_TRACE_FORMULA_SET_CONFLICT", "0", 1);
  setenv("SAT_EVENT_TRACE_FORMULA_SET_INCLUDE_GARBAGE", "1", 1);
  setenv("SAT_EVENT_TRACE_FORMULA_SET_META", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(garbage_kernel));
    trace.macro(garbage_kernel, "fixture");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("SAT_EVENT_TRACE_FORMULA_SET_PATH");
  unsetenv("SAT_EVENT_TRACE_FORMULA_SET_MATCH");
  unsetenv("SAT_EVENT_TRACE_FORMULA_SET_CONFLICT");
  unsetenv("SAT_EVENT_TRACE_FORMULA_SET_INCLUDE_GARBAGE");
  unsetenv("SAT_EVENT_TRACE_FORMULA_SET_META");
  std::ifstream garbage_formula_input(garbage_formula_path);
  const std::string garbage_formula_text(
      (std::istreambuf_iterator<char>(garbage_formula_input)),
      std::istreambuf_iterator<char>());
  assert(garbage_formula_text.find(
             "-2,1,3\tglue=0\tpriority=0\tsearched=2\tused=0"
             "\treason=0\tshrunken=0\tgarbage=1") != std::string::npos);
  std::remove(garbage_formula_path.c_str());
  std::remove(garbage_trace_path.c_str());

  const std::string skip_ticks_path = trace_path("event-skip-analysis-ticks");
  std::remove(skip_ticks_path.c_str());
  setenv("TACTIX_EVENT_TRACE", skip_ticks_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_SKIP_ANALYSIS_TICKS", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "analysis_tick_reason", 1);
    trace.detail(k, "analysis_tick_shrink", 1);
    trace.detail(k, "decision", 7);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_SKIP_ANALYSIS_TICKS");
  std::ifstream skip_ticks_input(skip_ticks_path);
  const std::string skip_ticks_text(
      (std::istreambuf_iterator<char>(skip_ticks_input)),
      std::istreambuf_iterator<char>());
  assert(skip_ticks_text.find("\tanalysis_tick") == std::string::npos);
  assert(skip_ticks_text.find("\tdecision\t") != std::string::npos);
  std::remove(skip_ticks_path.c_str());

  const std::string skip_randec_path = trace_path("event-skip-randec");
  std::remove(skip_randec_path.c_str());
  setenv("TACTIX_EVENT_TRACE", skip_randec_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_SKIP_RANDEC_DRAWS", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.randec_draw(k, 17, 2, 0, false);
    trace.detail(k, "decision", 1);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_SKIP_RANDEC_DRAWS");
  std::ifstream skip_randec_input(skip_randec_path);
  const std::string skip_randec_text(
      (std::istreambuf_iterator<char>(skip_randec_input)),
      std::istreambuf_iterator<char>());
  assert(skip_randec_text.find("\trandec_draw\t") == std::string::npos);
  assert(skip_randec_text.find("\tdecision\t") != std::string::npos);
  std::remove(skip_randec_path.c_str());

  const std::string compact_bumps_path = trace_path("event-compact-bumps");
  std::remove(compact_bumps_path.c_str());
  setenv("TACTIX_EVENT_TRACE", compact_bumps_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_SEARCH_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_SKIP_HEAP", "1", 1);
  setenv("SAT_EVENT_TRACE_SWEEP_CLAUSES", "1", 1);
  setenv("SAT_EVENT_TRACE_SWEEP_QUERIES", "1", 1);
  setenv("SAT_EVENT_TRACE_SWEEP_COMMIT", "1,-2", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "score_bump", 2);
    trace.detail(k, "reason_bump_limit", 10);
    trace.detail(k, "heap_reinsert", 1);
    trace.randec_state(k, "pre", 523, 533, 4, 0x1234);
    trace.sweep_seed(k, 0);
    trace.sweep_environment(k, 0, 3, 4, 5);
    const Lit sweep_clause[2] = {internalize(1), internalize(-2)};
    trace.sweep_environment_clause(k, 0, 0, sweep_clause, 2);
    trace.sweep_query(k, 0, 0, "initial", INVALID_LIT, INVALID_LIT, 0, 7,
                      0);
    const Var sweep_vars[2] = {0, 1};
    const int8_t sweep_values[2] = {1, -1};
    trace.sweep_model(k, 0, 0, sweep_vars, sweep_values, 2);
    trace.sweep_equivalence(k, internalize(1), internalize(-2));
    trace.sweep_commit(k, internalize(-2), internalize(1), -1, 0, "entry");
    trace.detail(k, "diagnostic_noise", 9);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_SEARCH_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_SKIP_HEAP");
  unsetenv("SAT_EVENT_TRACE_SWEEP_CLAUSES");
  unsetenv("SAT_EVENT_TRACE_SWEEP_QUERIES");
  unsetenv("SAT_EVENT_TRACE_SWEEP_COMMIT");
  std::ifstream compact_bumps_input(compact_bumps_path);
  const std::string compact_bumps_text(
      (std::istreambuf_iterator<char>(compact_bumps_input)),
      std::istreambuf_iterator<char>());
  assert(compact_bumps_text.find("\tscore_bump\t") != std::string::npos);
  assert(compact_bumps_text.find("\treason_bump_limit\t") !=
         std::string::npos);
  assert(compact_bumps_text.find("\theap_reinsert\t") != std::string::npos);
  assert(compact_bumps_text.find("\trandec_state\t") != std::string::npos);
  assert(compact_bumps_text.find("\tsweep_seed\t") != std::string::npos);
  assert(compact_bumps_text.find("\tsweep_environment\t") !=
         std::string::npos);
  assert(compact_bumps_text.find("\tsweep_environment_clause\t") !=
         std::string::npos);
  assert(compact_bumps_text.find("\tsweep_query\t") != std::string::npos);
  assert(compact_bumps_text.find("\tsweep_query_model\t") !=
         std::string::npos);
  assert(compact_bumps_text.find("\tsweep_equivalence\t") !=
         std::string::npos);
  assert(compact_bumps_text.find("\tsweep_commit\t") != std::string::npos);
  assert(compact_bumps_text.find(
             "a=-2|b=1|av=-1|bv=0|proof=0|stage=entry") !=
         std::string::npos);
  assert(compact_bumps_text.find("\tdiagnostic_noise\t") ==
         std::string::npos);
  std::remove(compact_bumps_path.c_str());

  const std::string search_heap_path = trace_path("event-search-heap");
  std::remove(search_heap_path.c_str());
  setenv("TACTIX_EVENT_TRACE", search_heap_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_SEARCH_ONLY", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "heap_reinsert", 1);
    trace.detail(k, "diagnostic_noise", 9);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_SEARCH_ONLY");
  std::ifstream search_heap_input(search_heap_path);
  const std::string search_heap_text(
      (std::istreambuf_iterator<char>(search_heap_input)),
      std::istreambuf_iterator<char>());
  assert(search_heap_text.find("\theap_reinsert\t") != std::string::npos);
  assert(search_heap_text.find("\tdiagnostic_noise\t") == std::string::npos);
  std::remove(search_heap_path.c_str());

  const std::string trajectory_path = trace_path("event-trajectory-only");
  std::remove(trajectory_path.c_str());
  setenv("TACTIX_EVENT_TRACE", trajectory_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "decision", 3);
    trace.detail(k, "analysis_begin", 2);
    trace.learned_stage(k, "learned_shrunk",
                        {internalize(3), internalize(-1), internalize(2)});
    trace.detail(k, "analysis_tail_probe", 7);
    trace.detail(k, "analysis_end", 1);
    k.h->stable = true;
    trace.heap_scores(k);
    k.h->stable = false;
    trace.detail(k, "score_bump", 2);
    trace.detail(k, "analysis_mark", 1);
    trace.macro(k, "restart_begin");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY");
  std::ifstream trajectory_input(trajectory_path);
  const std::string trajectory_text(
      (std::istreambuf_iterator<char>(trajectory_input)),
      std::istreambuf_iterator<char>());
  assert(trajectory_text.find("\tdecision\t") != std::string::npos);
  assert(trajectory_text.find("\tanalysis_begin\t") != std::string::npos);
  assert(trajectory_text.find("\tlearned_shrunk\t") != std::string::npos);
  assert(trajectory_text.find("size=3|order_hash=") != std::string::npos);
  assert(trajectory_text.find("|set_hash=") != std::string::npos);
  assert(trajectory_text.find("\tanalysis_tail_probe\t") ==
         std::string::npos);
  assert(trajectory_text.find("\tanalysis_end\t") != std::string::npos);
  assert(trajectory_text.find("\theap_scores\t") == std::string::npos);
  assert(trajectory_text.find("\trestart_begin\t") != std::string::npos);
  assert(trajectory_text.find("\tscore_bump\t") == std::string::npos);
  assert(trajectory_text.find("\tanalysis_mark\t") == std::string::npos);
  std::remove(trajectory_path.c_str());

  const std::string sweep_only_path = trace_path("event-sweep-only");
  std::remove(sweep_only_path.c_str());
  setenv("TACTIX_EVENT_TRACE", sweep_only_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY", "1", 1);
  setenv("TACTIX_EVENT_TRACE_SWEEP_ONLY", "1", 1);
  setenv("SAT_EVENT_TRACE_SWEEP_CLAUSES", "2", 1);
  setenv("SAT_EVENT_TRACE_SWEEP_QUERIES", "2", 1);
  setenv("SAT_EVENT_TRACE_SWEEP_VARS", "2", 1);
  setenv("SAT_EVENT_TRACE_SWEEP_BINARIES", "2", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.detail(k, "analysis_begin", 2);
    trace.sweep_seed(k, 1);
    trace.sweep_environment_var(k, 1, 0, 2, 1);
    trace.sweep_binary_attempt(k, 1, 0, internalize(1), internalize(-2), 1,
                               2, "add");
    const Lit clause[2] = {internalize(1), internalize(-2)};
    trace.sweep_environment_clause(k, 1, 0, clause, 2);
    trace.sweep_query(k, 1, 0, "initial", INVALID_LIT, INVALID_LIT, 0, 7,
                      0);
    trace.sweep_effort(k, 2, 17, 31, 5);
    trace.macro(k, "sweep_end");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_TRAJECTORY_ONLY");
  unsetenv("TACTIX_EVENT_TRACE_SWEEP_ONLY");
  unsetenv("SAT_EVENT_TRACE_SWEEP_CLAUSES");
  unsetenv("SAT_EVENT_TRACE_SWEEP_QUERIES");
  unsetenv("SAT_EVENT_TRACE_SWEEP_VARS");
  unsetenv("SAT_EVENT_TRACE_SWEEP_BINARIES");
  std::ifstream sweep_only_input(sweep_only_path);
  const std::string sweep_only_text(
      (std::istreambuf_iterator<char>(sweep_only_input)),
      std::istreambuf_iterator<char>());
  assert(sweep_only_text.find("\tanalysis_begin\t") == std::string::npos);
  assert(sweep_only_text.find("\tsweep_seed\t") != std::string::npos);
  assert(sweep_only_text.find("\tsweep_environment_var\t") !=
         std::string::npos);
  assert(sweep_only_text.find("\tsweep_binary_attempt\t") !=
         std::string::npos);
  assert(sweep_only_text.find("lit_depth=1|other_depth=2|disposition=add") !=
         std::string::npos);
  assert(sweep_only_text.find("\tsweep_environment_clause\t") !=
         std::string::npos);
  assert(sweep_only_text.find("\tsweep_query\t") != std::string::npos);
  assert(sweep_only_text.find(
             "external=2|before=17|after=31|delta=14|solves=5") !=
         std::string::npos);
  assert(sweep_only_text.find("\tsweep_end\t") != std::string::npos);
  std::remove(sweep_only_path.c_str());

  const std::string search_watch_cost_path =
      trace_path("event-search-watch-cost-only");
  std::remove(search_watch_cost_path.c_str());
  setenv("TACTIX_EVENT_TRACE", search_watch_cost_path.c_str(), 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_COST", "1", 1);
  setenv("TACTIX_EVENT_TRACE_SEARCH_WATCH_COST_ONLY", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    k.inprocessing = true;
    trace.watch_scan(k, internalize(1));
    trace.watch_cost(k, internalize(1), 7, 7, 2, 1, 2, 0, 0, 5);
    k.inprocessing = false;
    trace.watch_scan(k, internalize(1));
    trace.watch_cost(k, internalize(1), 7, 7, 2, 1, 2, 0, 0, 5);
  }
  unsetenv("TACTIX_EVENT_TRACE");
  unsetenv("TACTIX_EVENT_TRACE_WATCH_COST");
  unsetenv("TACTIX_EVENT_TRACE_SEARCH_WATCH_COST_ONLY");
  std::ifstream search_watch_cost_input(search_watch_cost_path);
  const std::string search_watch_cost_text(
      (std::istreambuf_iterator<char>(search_watch_cost_input)),
      std::istreambuf_iterator<char>());
  const size_t first_watch_cost =
      search_watch_cost_text.find("\twatch_cost\t");
  assert(first_watch_cost != std::string::npos);
  assert(search_watch_cost_text.find("\twatch_cost\t", first_watch_cost + 1) ==
         std::string::npos);
  assert(search_watch_cost_text.find("\twatch\t") == std::string::npos);
  std::remove(search_watch_cost_path.c_str());

  const std::string reduce_path = trace_path("reduce-trace");
  std::remove(reduce_path.c_str());
  setenv("TACTIX_REDUCE_TRACE", reduce_path.c_str(), 1);
  {
    Kernel reduced;
    kernel_reserve_vars(reduced, 4);
    ScopedHeuristics rh(reduced);
    const Lit a[3] = {internalize(1), internalize(-2), internalize(3)};
    const Lit b[3] = {internalize(-1), internalize(2), internalize(4)};
    const Ref ra = kernel_add_large_clause(reduced, a, 3, true, 7);
    kernel_add_large_clause(reduced, b, 3, true, 2);
    reduced.tier1 = 2;
    reduced.tier2 = 6;
    EventTrace trace;
    trace.reduction(reduced, {ra}, 1);
  }
  unsetenv("TACTIX_REDUCE_TRACE");
  std::ifstream reduce_input(reduce_path);
  const std::string reduce_text(
      (std::istreambuf_iterator<char>(reduce_input)),
      std::istreambuf_iterator<char>());
  assert(reduce_text.find("# tactix-kissat-reduce\t1") != std::string::npos);
  assert(reduce_text.find("tactix\tsummary\t1\t0\tfocused\t2\t6\t2\t1\t1") !=
         std::string::npos);
  assert(reduce_text.find("\tcandidate\t0\t1\t-2,1,3\n") !=
         std::string::npos);
  std::remove(reduce_path.c_str());

  const std::string reduce_window_path = trace_path("reduce-window");
  std::remove(reduce_window_path.c_str());
  setenv("TACTIX_REDUCE_TRACE", reduce_window_path.c_str(), 1);
  setenv("TACTIX_REDUCE_TRACE_CONFLICT_FROM", "1000", 1);
  setenv("TACTIX_REDUCE_TRACE_CONFLICT_TO", "1000", 1);
  {
    Kernel reduced;
    kernel_reserve_vars(reduced, 3);
    ScopedHeuristics rh(reduced);
    const Lit clause[3] = {internalize(1), internalize(2), internalize(3)};
    const Ref ref = kernel_add_large_clause(reduced, clause, 3, true, 4);
    EventTrace trace;
    reduced.stats.conflicts = 999;
    trace.reduction(reduced, {ref}, 1);
    reduced.stats.conflicts = 1000;
    trace.reduction(reduced, {ref}, 1);
    reduced.stats.conflicts = 1001;
    trace.reduction(reduced, {ref}, 1);
  }
  unsetenv("TACTIX_REDUCE_TRACE");
  unsetenv("TACTIX_REDUCE_TRACE_CONFLICT_FROM");
  unsetenv("TACTIX_REDUCE_TRACE_CONFLICT_TO");
  std::ifstream reduce_window_input(reduce_window_path);
  const std::string reduce_window_text(
      (std::istreambuf_iterator<char>(reduce_window_input)),
      std::istreambuf_iterator<char>());
  assert(reduce_window_text.find("\t1000\tfocused\t") != std::string::npos);
  assert(reduce_window_text.find("\t999\tfocused\t") == std::string::npos);
  assert(reduce_window_text.find("\t1001\tfocused\t") == std::string::npos);
  std::remove(reduce_window_path.c_str());
  std::puts("test_event_trace OK");
  return 0;
}

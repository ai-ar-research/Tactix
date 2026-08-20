#undef NDEBUG
#include "policies/event_trace.hpp"
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
  setenv("TACTIX_EVENT_TRACE_WATCH_POSE", "1", 1);
  setenv("TACTIX_EVENT_TRACE_HEAP_REMOVE_DECISION", "0", 1);
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
    trace.factor_grid(k, internalize(-2), 3, 4, 5);
    trace.factor_grid_member(k, 0, internalize(-2));
    trace.detail(k, "analysis_begin", 2);
    kernel_assign_decision(k, internalize(1));
    const size_t trail_begin = k.trail.size();
    kernel_assign_binary(k, internalize(2), internalize(-1));
    trace.implications(k, trail_begin);
    const Lit conflict[3] = {internalize(-3), internalize(1), internalize(-2)};
    k.conflict_ref = kernel_add_large_clause(k, conflict, 3, false, 0);
    k.conflict_valid = true;
    clause_at(k.arena, k.conflict_ref)->searched = 3;
    trace.watch_scan(k, internalize(3));
    trace.conflict(k);
    trace.learned(k, {internalize(3), internalize(-1), internalize(2)});
    setenv("SAT_EVENT_TRACE_CLAUSE_SET", "-3,1,-2", 1);
    setenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_FROM", "0", 1);
    setenv("SAT_EVENT_TRACE_CLAUSE_SET_CONFLICT_TO", "0", 1);
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
  unsetenv("TACTIX_EVENT_TRACE_WATCH_POSE");
  unsetenv("TACTIX_EVENT_TRACE_HEAP_REMOVE_DECISION");
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
  assert(text.find("\theap_insert\t") != std::string::npos);
  assert(text.find("var=2|score=0|from=17|to=4") != std::string::npos);
  assert(text.find("\theap_remove\t") != std::string::npos);
  assert(text.find("removed=2:0|removed_pos=17|last=3:0|from=20|to=4") !=
         std::string::npos);
  assert(text.find("\tslot_boundary\t") != std::string::npos);
  assert(text.find("name=Sweep|stage=begin") != std::string::npos);
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
  assert(text.find("\tfactor_grid\t") != std::string::npos);
  assert(text.find("\tfactor_grid_member\t") != std::string::npos);
  assert(text.find("ordinal=0|external=-2") != std::string::npos);
  assert(text.find("external=-2|factors=3|remainders=4|delta=5") !=
         std::string::npos);
  assert(text.find("\tanalysis_begin\t") != std::string::npos);
  assert(text.find("\timply\t") != std::string::npos);
  assert(text.find("\tlit=2|level=1|reason=-1,2\n") != std::string::npos);
  assert(text.find("\tconflict\t") != std::string::npos);
  assert(text.find("|searched=3|pose=-3,1,-2\n") != std::string::npos);
  assert(text.find("\torder=-3,1,-2|set=-3,-2,1|") != std::string::npos);
  assert(text.find("\tlearned\t") != std::string::npos);
  assert(text.find("\tretier\t") != std::string::npos);
  assert(text.find("focused=3,8|stable=2,6|posed=3,8|used="
                   "0:0,1:0,2:0,3:5,") !=
         std::string::npos);
  assert(text.find("\torder=3,-1,2|set=-1,2,3\n") != std::string::npos);
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
                            "id=0,order=0,searched=3,redundant=0,"
                            "pose=-3,1,-2") != std::string::npos);
  std::remove(path.c_str());

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
  std::ifstream watch_only_input(watch_only_path);
  const std::string watch_only_text(
      (std::istreambuf_iterator<char>(watch_only_input)),
      std::istreambuf_iterator<char>());
  assert(watch_only_text.find("\twatch\t") != std::string::npos);
  assert(watch_only_text.find("\trestart_begin\t") != std::string::npos);
  assert(watch_only_text.find("\tdecision\t") == std::string::npos);
  assert(watch_only_text.find("\tanalysis_begin\t") == std::string::npos);
  std::remove(watch_only_path.c_str());

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
    trace.randec_state(k, "pre", 523, 533, 4);
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

#undef NDEBUG
#include "policies/event_trace.hpp"
#include "policies/heuristics.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

using namespace tactix;

int main() {
  static const char *path = "/tmp/tactix-test-event-trace.tsv";
  std::remove(path);
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

  setenv("TACTIX_EVENT_TRACE", path, 1);
  setenv("TACTIX_EVENT_TRACE_WATCH_POSE", "1", 1);
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.macro(k, "lucky_early_begin");
    trace.macro(k, "lucky_early_end", "unknown");
    trace.detail(k, "decision", 3);
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
  std::ifstream input(path);
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  assert(text.find("# tactix-event-trace\t1") != std::string::npos);
  assert(text.find("\treorder_begin\t") != std::string::npos);
  assert(text.find("\treorder_end\t") != std::string::npos);
  assert(text.find("\tlucky_early_begin\t") != std::string::npos);
  assert(text.find("\tlucky_early_end\t") != std::string::npos);
  assert(text.find("\tdecision\t") != std::string::npos);
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
  assert(text.find("\torder=-3,1,-2|set=-3,-2,1\n") != std::string::npos);
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
  std::remove(path);

  static const char *window_path = "/tmp/tactix-test-event-window.tsv";
  setenv("TACTIX_EVENT_TRACE", window_path, 1);
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
  std::remove(window_path);

  static const char *reduce_path = "/tmp/tactix-test-reduce-trace.tsv";
  std::remove(reduce_path);
  setenv("TACTIX_REDUCE_TRACE", reduce_path, 1);
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
  std::remove(reduce_path);

  static const char *reduce_window_path =
      "/tmp/tactix-test-reduce-window.tsv";
  std::remove(reduce_window_path);
  setenv("TACTIX_REDUCE_TRACE", reduce_window_path, 1);
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
  std::remove(reduce_window_path);
  std::puts("test_event_trace OK");
  return 0;
}

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
  {
    EventTrace trace;
    assert(trace.open_from_env(k));
    trace.macro(k, "lucky_early_begin");
    trace.macro(k, "lucky_early_end", "unknown");
    trace.detail(k, "decision", 3);
    trace.detail(k, "analysis_begin", 2);
    kernel_assign_decision(k, internalize(1));
    const size_t trail_begin = k.trail.size();
    kernel_assign_binary(k, internalize(2), internalize(-1));
    trace.implications(k, trail_begin);
    const Lit conflict[3] = {internalize(-3), internalize(1), internalize(-2)};
    k.conflict_ref = kernel_add_large_clause(k, conflict, 3, false, 0);
    k.conflict_valid = true;
    trace.conflict(k);
    trace.learned(k, {internalize(3), internalize(-1), internalize(2)});
    trace.detail(k, "analysis_end", 1);
    trace.reorder_begin(k);
    trace.reorder_end(k);
    trace.macro(k, "terminate", "10");
  }
  unsetenv("TACTIX_EVENT_TRACE");
  std::ifstream input(path);
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  assert(text.find("# tactix-event-trace\t1") != std::string::npos);
  assert(text.find("\treorder_begin\t") != std::string::npos);
  assert(text.find("\treorder_end\t") != std::string::npos);
  assert(text.find("\tlucky_early_begin\t") != std::string::npos);
  assert(text.find("\tlucky_early_end\t") != std::string::npos);
  assert(text.find("\tdecision\t") != std::string::npos);
  assert(text.find("\tanalysis_begin\t") != std::string::npos);
  assert(text.find("\timply\t") != std::string::npos);
  assert(text.find("\tlit=2|level=1|reason=-1,2\n") != std::string::npos);
  assert(text.find("\tconflict\t") != std::string::npos);
  assert(text.find("\torder=-3,1,-2|set=-3,-2,1\n") != std::string::npos);
  assert(text.find("\tlearned\t") != std::string::npos);
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
  std::puts("test_event_trace OK");
  return 0;
}

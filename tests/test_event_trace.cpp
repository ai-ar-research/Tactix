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
  assert(text.find("\tanalysis_end\t") != std::string::npos);
  std::remove(path);
  std::puts("test_event_trace OK");
  return 0;
}

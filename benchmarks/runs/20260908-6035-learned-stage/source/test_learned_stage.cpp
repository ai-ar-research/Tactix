// Real read-only callbacks: inclusive bounds, final stage, empty and long rows.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "diagnostics/event_trace.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>
using namespace tactix;
int main() {
  char path[] = "learned-stage-XXXXXX"; int fd = mkstemp(path); assert(fd >= 0); close(fd);
  setenv("SAT_LEARNED_STAGE_TRACE", path, 1);
  setenv("SAT_LEARNED_STAGE_FROM", "1", 1); setenv("SAT_LEARNED_STAGE_TO", "2", 1);
  Kernel k; kernel_reserve_vars(k, 2048); EventTrace trace;
  std::vector<Lit> lits{internalize(1), internalize(-2), internalize(3)};
  event_trace_learned_stage(k, "learned_raw", lits);
  k.stats.conflicts = 1; k.stats.decisions = 5;
  event_trace_learned_stage(k, "learned_raw", lits);
  std::swap(lits[0], lits[1]); event_trace_learned_stage(k, "learned_sorted", lits);
  k.stats.conflicts = 2; lits = {internalize(1), internalize(3)};
  event_trace_learned_stage(k, "learned_shrunk", lits); trace.learned(k, lits);
  lits.clear(); for (int i = 1; i <= 2048; ++i) lits.push_back(internalize(i));
  event_trace_learned_stage(k, "learned_raw", lits);
  lits.clear(); event_trace_learned_stage(k, "learned_shrunk", lits);
  k.stats.conflicts = 3; trace.learned(k, lits);
  std::ifstream in(path); std::string text(std::istreambuf_iterator<char>(in), {});
  assert(std::count(text.begin(), text.end(), '\n') == 8);
  assert(text.find("0\tlearned_raw\t1\t5\t0\t3\t1,-2,3\n") != std::string::npos);
  assert(text.find("3\tlearned\t2\t5\t0\t2\t1,3\n") != std::string::npos);
  assert(text.find(",2047,2048\n") != std::string::npos);
  assert(text.find("5\tlearned_shrunk\t2\t5\t0\t0\t-\n") != std::string::npos);
  kernel_check_invariants(k); assert(!std::remove(path));
  std::puts("PASS: inclusive learning window, stage order, final callback, empty and 2048-literal row");
}

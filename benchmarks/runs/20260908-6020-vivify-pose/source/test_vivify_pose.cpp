// Exercise real callback state, cache invalidation and round reset.
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
  char path[] = "vivify-pose-XXXXXX"; int fd = mkstemp(path); assert(fd >= 0); close(fd);
  setenv("SAT_VIVIFY_POSE_TRACE", path, 1);
  setenv("SAT_VIVIFY_POSE_LITS", "3,1,-2", 1);
  setenv("SAT_VIVIFY_POSE_CONFLICT", "1", 1);
  Kernel k; kernel_reserve_vars(k, 4); k.vivify_scope = 1;
  const Lit target[] = {internalize(1), internalize(-2), internalize(3)};
  const Lit other[] = {internalize(1), internalize(2), internalize(4)};
  Ref a = kernel_add_large_clause(k, target, 3, false, 0);
  Ref candidate = kernel_add_large_clause(k, other, 3, false, 0);
  event_trace_vivify_candidate(k, "vivify_candidate", 0, candidate, 4, 20);
  k.stats.conflicts = 1;
  event_trace_vivify_candidate(k, "vivify_candidate", 0, candidate, 4, 20);
  auto *c = clause_at(k.arena, a); std::swap(c->lits[0], c->lits[1]);
  event_trace_vivify_candidate(k, "vivify_candidate_end", 0, candidate, 5, 20);
  kernel_delete_clause(k, c);
  event_trace_vivify_candidate(k, "vivify_candidate", 1, candidate, 5, 20);
  const Lit replacement[] = {internalize(3), internalize(1), internalize(-2)};
  kernel_add_large_clause(k, replacement, 3, false, 0);
  event_trace_vivify_candidate(k, "vivify_candidate_end", 1, candidate, 7, 20);
  k.vivify_scope = 2;
  event_trace_vivify_candidate(k, "vivify_candidate", 0, candidate, 7, 25);
  std::ifstream in(path); std::string text(std::istreambuf_iterator<char>(in), {});
  assert(std::count(text.begin(), text.end(), '\n') == 7);
  assert(text.find("\t2\t1,-2,3\t0,0,0\t") != std::string::npos);
  assert(text.find("\t2\t-2,1,3\t0,0,0\t") != std::string::npos);
  assert(text.find("\t4294967295\t0\t-\t-\t") != std::string::npos);
  assert(text.find("\t2\t3,1,-2\t0,0,0\t") != std::string::npos);
  assert(text.find("4\tvivify_candidate\t1\t2\t0\t7\t25\t") != std::string::npos);
  kernel_check_invariants(k); assert(!std::remove(path));
  std::puts("PASS: candidate pose cache, garbage invalidation, replacement, scope reset and conflict filter");
}

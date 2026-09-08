// Actual boundary callback coverage, including duplicate clauses and disappearance.
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
  char path[] = "clause-pose-series-XXXXXX";
  const int fd = mkstemp(path); assert(fd >= 0); close(fd);
  setenv("SAT_CLAUSE_POSE_SERIES_PATH", path, 1);
  setenv("SAT_CLAUSE_POSE_SERIES_LITS", "3,1,-2", 1);
  setenv("SAT_CLAUSE_POSE_SERIES_FROM", "1", 1);
  setenv("SAT_CLAUSE_POSE_SERIES_TO", "1", 1);
  setenv("SAT_CLAUSE_POSE_SERIES_NAMES", "vivify", 1);
  Kernel k; kernel_reserve_vars(k, 4);
  const Lit lits[] = {internalize(1), internalize(-2), internalize(3)};
  const Ref a = kernel_add_large_clause(k, lits, 3, false, 0);
  const Ref b = kernel_add_large_clause(k, lits, 3, false, 0);
  const Lit other[] = {internalize(1), internalize(2), internalize(4)};
  kernel_add_large_clause(k, other, 3, false, 0);
  auto read = [&] { std::ifstream in(path); return std::string(std::istreambuf_iterator<char>(in), {}); };
  event_trace_slot_boundary(k, "Vivify", "begin");
  k.stats.conflicts = 1;
  event_trace_slot_boundary(k, "BVE", "begin");
  event_trace_slot_boundary(k, "Vivify", "begin");
  auto first = read();
  assert(first.find("# begin\t0\t1\t0\tvivify\tbegin\n") != std::string::npos);
  assert(first.find("# end\t0\t2\n") != std::string::npos);
  assert(std::count(first.begin(), first.end(), '\n') == 5);
  Clause *clause = clause_at(k.arena, a);
  std::swap(clause->lits[0], clause->lits[1]);
  event_trace_slot_boundary(k, "Vivify", "end");
  assert(read().find("\t-2,1,3\t0,0,0\n") != std::string::npos);
  kernel_delete_clause(k, clause_at(k.arena, a));
  kernel_delete_clause(k, clause_at(k.arena, b));
  event_trace_slot_boundary(k, "Vivify", "begin");
  const auto gone = read();
  assert(gone.find("# end\t2\t0\n") != std::string::npos);
  k.stats.conflicts = 2;
  event_trace_slot_boundary(k, "Vivify", "end");
  assert(read() == gone);
  kernel_check_invariants(k);
  assert(!std::remove(path));
  std::puts("PASS: actual boundary pose/multiplicity/disappearance and window/name selection");
}

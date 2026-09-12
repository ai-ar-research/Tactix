// Canonical binary disappearance must re-arm removal advice, independently
// of proof mode, duplicate copies and the caller's instance choice.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/substitute/substitute.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>
using namespace tactix;

static bool fixture(unsigned copies, int mode, bool single, bool reverse) {
  Kernel k;
  kernel_reserve_vars(k, 4);
  char path[] = "substitute-retry-proof-XXXXXX";
  const int fd = mkstemp(path); assert(fd >= 0); close(fd);
  Proof proof;
  if (mode) {
    assert(proof.open(path, mode == 1 ? ProofFormat::DratAscii : ProofFormat::Lrat));
    k.proof = &proof;
    proof.bind_variable_map(&k.internal_to_external);
  }
  for (Var v : reverse ? std::vector<Var>{1, 0, 2, 3}
                       : std::vector<Var>{0, 1, 2, 3})
    kernel_record_import(k, v);
  for (unsigned i = 0; i < copies; ++i) {
    kernel_add_binary_clause(k, internalize(-1), internalize(2), ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(1), internalize(-2), ++k.next_clause_id);
  }
  // A surviving rewrite is not a disappearance and must not re-arm x3/x4.
  kernel_add_binary_clause(k, internalize(2), internalize(3), ++k.next_clause_id);
  kernel_add_binary_clause(k, internalize(3), internalize(4), ++k.next_clause_id);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), 0);
  const auto removed = k.removed;
  const uint64_t before = k.stats.bve_marks;
  const auto &rule = tactix_rules()[tactix_rule_id("Substitute")];
  assert(rule.guard(k));
  uint64_t instances[8];
  const uint32_t count = rule.instances(k, instances, 8);
  assert(count > 0);
  const ApplyResult result = single ? rule.apply_instance(k, instances[0]) : rule.apply(k);
  assert(result == ApplyResult::Ok);
  assert(k.value(internalize(1)) == 0 && k.value(internalize(2)) == 0);
  assert(kernel_var_retired(k, reverse ? 0 : 1));
  assert(!kernel_var_retired(k, reverse ? 1 : 0));
  assert(!kernel_binary_live(k, internalize(-1), internalize(2)));
  assert(!kernel_binary_live(k, internalize(1), internalize(-2)));
  assert(kernel_binary_live(k, internalize(reverse ? 2 : 1), internalize(3)));
  assert(kernel_binary_live(k, internalize(3), internalize(4)));
  const bool retry = k.bve_retry[0] && k.bve_retry[1] &&
                     !k.bve_retry[2] && !k.bve_retry[3];
  const bool epochs = k.removed[0] > removed[0] && k.removed[1] > removed[1] &&
                      k.removed[2] == removed[2] && k.removed[3] == removed[3];
  const bool events = k.stats.bve_marks - before == 4 * copies;
  // A no-op reapplication does not synthesize fresh removal events.
  const uint64_t after = k.stats.bve_marks;
  assert(rule.apply(k) == ApplyResult::NoOp);
  assert(k.stats.bve_marks == after);
  k.level = 1; assert(!rule.guard(k)); k.level = 0;
  k.proof = nullptr;
  if (mode) proof.flush();
  assert(std::remove(path) == 0);
  if (!(retry && epochs && events))
    std::printf("missing retry: copies=%u mode=%d single=%d reverse=%d flags=%d%d%d%d marks=%llu epochs=%d\n",
                copies, mode, single, reverse, k.bve_retry[0], k.bve_retry[1],
                k.bve_retry[2], k.bve_retry[3],
                (unsigned long long)(after-before), epochs);
  return retry && epochs && events;
}

int main(int argc, char **argv) {
  bool ok = true;
  for (unsigned copies : {1u, 2u, 5u})
    for (int mode : {0, 1, 2})
      for (bool single : {false, true})
        for (bool reverse : {false, true})
          ok = fixture(copies, mode, single, reverse) && ok;
  if (argc == 2 && std::string(argv[1]) == "--expect-old-missing") {
    assert(!ok); // Negative control is a known missing state effect, not a crash.
    std::puts("PASS: actual parent reproduces missing Substitute removal advice");
    return 0;
  }
  assert(ok);
  std::puts("PASS: 36 canonical/public DRAT/LRAT retry fixtures");
}

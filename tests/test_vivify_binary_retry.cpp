// Binary Vivify retires a large source: surviving original endpoints also
// need retry advice. Cover public instances, both terminal paths and modes.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/vivify/vivify.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>
using namespace tactix;

static bool fixture(bool redundant, bool instantiate, bool reuse, bool preset, int mode) {
  Kernel k; kernel_reserve_vars(k, 4); k.vivify_reuse = reuse;
  const Lit candidate[] = {internalize(1), internalize(2), internalize(3)};
  const Ref ref = kernel_add_large_clause(k, candidate, 3, redundant, 2, ++k.next_clause_id);
  if (instantiate) {
    kernel_add_binary_clause(k, internalize(-3), internalize(4), ++k.next_clause_id);
    kernel_add_binary_clause(k, internalize(-3), internalize(-4), ++k.next_clause_id);
  } else kernel_add_binary_clause(k, internalize(1), internalize(-2), ++k.next_clause_id);
  std::fill(k.bve_retry.begin(), k.bve_retry.end(), uint8_t(preset));
  const uint64_t before = k.stats.bve_marks;
  char path[] = "vivify-binary-retry-XXXXXX";
  int fd = mkstemp(path); assert(fd >= 0); close(fd);
  Proof proof;
  if (mode) {
    assert(proof.open(path, mode == 1 ? ProofFormat::DratAscii : ProofFormat::Lrat));
    k.proof = &proof;
  }
  const auto &rule = tactix_rules()[tactix_rule_id("Vivify")];
  assert(rule.guard(k));
  uint64_t instances[8]; const uint32_t count = rule.instances(k, instances, 8);
  assert(std::find(instances, instances + count, ref) != instances + count);
  assert(rule.apply_instance(k, ref) == ApplyResult::Ok);
  assert(clause_at(k.arena, ref)->garbage());
  assert(kernel_binary_live(k, internalize(1), internalize(instantiate ? 2 : 3)));
  bool okay = k.stats.bve_marks - before == (redundant ? 0 : 3);
  for (Var v = 0; v != 3; ++v) okay &= bool(k.bve_retry[v]) == (preset || !redundant);
  okay &= bool(k.bve_retry[3]) == preset;
  const uint64_t after = k.stats.bve_marks;
  assert(rule.apply_instance(k, ref) == ApplyResult::NoOp && k.stats.bve_marks == after);
  assert(k.level == 0); k.level = 1; assert(!rule.guard(k)); k.level = 0;
  kernel_check_invariants(k);
  k.proof = nullptr; if (mode) proof.flush(); assert(std::remove(path) == 0);
  if (!okay) std::printf("missing: redundant=%d instantiate=%d reuse=%d preset=%d mode=%d events=%llu\n",
      redundant, instantiate, reuse, preset, mode, (unsigned long long)(after-before));
  return okay;
}
int main(int argc, char **argv) {
  unsigned missing = 0;
  for (bool redundant : {false, true}) for (bool instantiate : {false, true})
    for (bool reuse : {false, true}) for (bool preset : {false, true}) for (int mode : {0, 1, 2})
      missing += !fixture(redundant, instantiate, reuse, preset, mode);
  if (argc == 2 && std::string(argv[1]) == "--expect-old-missing") {
    assert(missing == 24); std::puts("PASS: actual parent misses advice in all24 irredundant cases");
  } else { assert(!missing); std::puts("PASS: 48 public binary-Vivify retry fixtures"); }
}

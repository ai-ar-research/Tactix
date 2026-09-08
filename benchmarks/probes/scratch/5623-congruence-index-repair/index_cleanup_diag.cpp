// Diagnostic variant of tests/test_congruence_index_cleanup.cpp (5619 prototype):
// identical fixture, but a proof mismatch is REPORTED (with the actual and
// expected bytes and the clause states) instead of aborting, so every failing
// case is visible in one run.  Probe only; never a test.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <unistd.h>
using namespace tactix;

static unsigned failures = 0, cases = 0;

static std::string esc(const std::string &s) {
  std::string r;
  for (char c : s) {
    if (c == '\n') r += "\\n";
    else r += c;
  }
  return r;
}

static void fixture(unsigned width, unsigned position, int value, bool negative,
                    bool redundant, unsigned copies, ProofFormat format) {
  cases++;
  Kernel k;
  kernel_reserve_vars(k, width);
  std::vector<Lit> lits;
  for (unsigned v = 1; v <= width; v++)
    lits.push_back(internalize(negative ? -static_cast<int>(v) : static_cast<int>(v)));
  std::vector<Ref> refs;
  for (unsigned i = 0; i < copies; i++)
    refs.push_back(kernel_add_large_clause(k, lits.data(), width, redundant, 0, ++k.next_clause_id));
  if (value) {
    const Lit unit = value > 0 ? lits[position] : negate(lits[position]);
    k.unit_ids[var_of(unit)] = ++k.next_clause_id;
    kernel_assign_unit(k, unit);
    assert(!kernel_propagate(k));
  }
  const auto original = k.arena;
  std::vector<CongGate> gates;
  congruence_extract_gates(k, gates, true);
  assert(gates.empty() && k.arena == original);
  k.level = 1;
  assert(!congruence_guard(k));
  k.level = 0;
  assert(congruence_guard(k));
  const int id = tactix_rule_id("Congruence");
  assert(id >= 0);
  const auto &rule = tactix_rules()[id];
  uint64_t instance = 0;
  assert(rule.instances(k, &instance, 1) == 1);
  const uint64_t proof_line = k.next_clause_id;
  bool deleted = !redundant && value == 1 && position <= 5;
  std::string expected;
  if (deleted)
    for (Ref ref : refs) {
      if (format == ProofFormat::Lrat)
        expected += std::to_string(proof_line) + " d " + std::to_string(clause_at(k.arena, ref)->id()) + " 0\n";
      else {
        expected += "d ";
        for (Lit lit : *clause_at(k.arena, ref)) expected += std::to_string(externalize(lit)) + " ";
        expected += "0\n";
      }
    }
  char path[] = "/tmp/tactix-congruence-index-diag-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  ApplyResult r1, r2;
  bool garbage_ok = true;
  uint64_t live_after = 0;
  {
    Proof proof;
    assert(proof.open(path, format));
    proof.bind_variable_map(&k.internal_to_external);
    k.proof = &proof;
    r1 = rule.apply_instance(k, instance);
    for (Ref ref : refs) garbage_ok = garbage_ok && (clause_at(k.arena, ref)->garbage() == deleted);
    live_after = k.live_irredundant_large;
    r2 = congruence_apply(k);
    proof.flush();
    k.proof = nullptr;
  }
  std::ifstream in(path);
  const std::string actual((std::istreambuf_iterator<char>(in)), {});
  in.close();
  std::remove(path);
  const bool ok = actual == expected && garbage_ok && r1 == ApplyResult::NoOp && r2 == ApplyResult::NoOp &&
                  !k.inconsistent && !k.stats.congruence_merges &&
                  live_after == (redundant || deleted ? 0 : copies);
  if (!ok) {
    failures++;
    std::printf("MISMATCH w=%u pos=%u val=%d neg=%d red=%d copies=%u fmt=%s r1=%d r2=%d garbage_ok=%d live=%llu next_id=%llu proof_line=%llu\n",
                width, position, value, (int)negative, (int)redundant, copies,
                format == ProofFormat::Lrat ? "lrat" : "drat", (int)r1, (int)r2, (int)garbage_ok,
                (unsigned long long)live_after, (unsigned long long)k.next_clause_id, (unsigned long long)proof_line);
    std::printf("  expected=[%s]\n  actual  =[%s]\n", esc(expected).c_str(), esc(actual).c_str());
  }
}

int main() {
  for (unsigned width = 3; width <= 7; width++)
    for (unsigned position = 0; position < width; position++)
      for (int value = -1; value <= 1; value++)
        for (bool negative : {false, true})
          for (bool redundant : {false, true})
            for (unsigned copies : {1u, 2u})
              for (ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat})
                fixture(width, position, value, negative, redundant, copies, format);
  std::printf("cases=%u failures=%u\n", cases, failures);
  return failures ? 1 : 0;
}

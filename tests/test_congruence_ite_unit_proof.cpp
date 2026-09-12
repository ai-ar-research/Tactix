// An ITE whose three inputs become equal up to sign can force a constant,
// but its original ternaries require a condition split to prove that unit.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "rules/congruence/congruence.hpp"
#include "rules/registry.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace tactix;

int main() {
  unsigned checked = 0;
  for (unsigned signs = 0; signs < 64; ++signs)
  for (bool truth : {false, true})
  for (bool reverse : {false, true})
  for (int mode = 0; mode < 4; ++mode) {
    std::vector<std::vector<int>> rows;
    for (int output : {3, 4, 5}) {
      rows.push_back({-output, 1});
      rows.push_back({-output, 2});
      rows.push_back({output, -1, -2});
    }
    const int c = 3, t = truth ? 4 : -4, e = truth ? -5 : 5, o = 6;
    rows.push_back({c, -e, o}); rows.push_back({c, e, -o});
    rows.push_back({-c, -t, o}); rows.push_back({-c, t, -o});
    const auto pose = [&](int lit) {
      return signs & (1u << (std::abs(lit) - 1)) ? -lit : lit;
    };
    for (auto &row : rows) for (int &lit : row) lit = pose(lit);
    if (reverse) {
      std::reverse(rows.begin(), rows.end());
      for (auto &row : rows) std::reverse(row.begin(), row.end());
    }
    unsigned models = 0;
    for (unsigned bits = 0; bits < 64; ++bits) {
      const auto value = [&](int lit) {
        return bool(bits & (1u << (std::abs(lit) - 1))) == (lit > 0);
      };
      bool model = true;
      for (const auto &row : rows) {
        bool sat = false;
        for (int lit : row) sat |= value(lit);
        model &= sat;
      }
      if (model) {
        ++models;
        assert(value(pose(6)) == truth);
      }
    }
    assert(models == 4);
    Kernel k;
    Proof proof;
    if (mode) {
      const auto format = mode == 1 ? ProofFormat::DratBinary :
                          mode == 2 ? ProofFormat::DratAscii : ProofFormat::Lrat;
      assert(proof.open("/dev/null", format));
      k.proof = &proof;
      proof.bind_variable_map(&k.internal_to_external);
    }
    FILE *input = std::tmpfile();
    assert(input);
    std::fprintf(input, "p cnf 6 %zu\n", rows.size());
    for (const auto &row : rows) {
      for (int lit : row) std::fprintf(input, "%d ", lit);
      std::fputs("0\n", input);
    }
    std::rewind(input);
    assert(!parse_dimacs(input, k));
    std::fclose(input);
    assert(!k.inconsistent && congruence_guard(k));
    const auto &rule = tactix_rules()[tactix_rule_id("Congruence")];
    uint64_t action = 0;
    assert(rule.instances(k, &action, 1) == 1 && action == INSTANCE_CANONICAL);
    assert(rule.apply_instance(k, action) == ApplyResult::Ok);
    assert(!k.inconsistent && !k.stats.congruence_unjustified);
    assert(k.value(internalize(pose(6))) == (truth ? 1 : -1));
    kernel_check_invariants(k);
    ++checked;
  }
  assert(checked == 1024);
  std::printf("ITE unit proof: %u signed/order/mode cases with exhaustive models\n", checked);
}

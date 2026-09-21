#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/lookup_circuit_model.hpp"
#include "rules/lucky/lucky.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;

static Kernel fixture(bool mixed_width = false) {
  std::ostringstream dimacs;
  dimacs << "p cnf 4 " << 14 + mixed_width << '\n';
  for (unsigned input = 0; input < 4; ++input)
    for (unsigned output = 0; output < 4; ++output) {
      if (output == input)
        continue;
      dimacs << ((input & 1) ? -1 : 1) << ' '
             << ((input & 2) ? -2 : 2) << ' '
             << ((output & 1) ? -3 : 3) << ' '
             << ((output & 2) ? -4 : 4) << " 0\n";
    }
  dimacs << "3 0\n-4 0\n";
  if (mixed_width)
    dimacs << "1 2 0\n";

  FILE *input = std::tmpfile();
  assert(input);
  const std::string text = dimacs.str();
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel k;
  assert(parse_dimacs(input, k) == nullptr);
  std::fclose(input);
  return k;
}

static Kernel wide_fixture() {
  // One 8-input/1-output lookup gate whose output is its highest input bit.
  // Fixing the output high forces enumeration beyond the first 128 input
  // assignments and exercises every AVX2 table-selection level.
  std::ostringstream dimacs;
  dimacs << "p cnf 9 257\n";
  for (unsigned input = 0; input < 256; ++input) {
    for (unsigned bit = 0; bit < 8; ++bit)
      dimacs << ((input >> bit) & 1 ? -int(bit + 1) : int(bit + 1))
             << ' ';
    const unsigned wanted = (input >> 7) & 1;
    const unsigned forbidden = wanted ^ 1;
    dimacs << (forbidden ? -9 : 9) << " 0\n";
  }
  dimacs << "9 0\n";

  FILE *input = std::tmpfile();
  assert(input);
  const std::string text = dimacs.str();
  assert(std::fwrite(text.data(), 1, text.size(), input) == text.size());
  std::rewind(input);
  Kernel k;
  assert(parse_dimacs(input, k) == nullptr);
  std::fclose(input);
  return k;
}

int main() {
  {
    Kernel k = fixture();
    assert(k.input_unit_clause_count == 2);
    assert(k.input_nonunit_clause_count == 12);
    assert(k.input_nonunit_uniform && k.input_uniform_nonunit_width == 4);
    assert(lookup_circuit_model_guard(k));
    assert(lookup_circuit_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.lookup_circuit_model_calls == 1);
    assert(k.stats.lookup_circuit_model_recognized == 1);
    assert(k.stats.lookup_circuit_model_gates == 1);
    assert(k.stats.lookup_circuit_model_support == 2);
    assert(k.stats.lookup_circuit_model_sat == 1);
#if defined(__x86_64__) || defined(__i386__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2"))
      assert(k.stats.lookup_circuit_model_vector_evaluations == 1);
#endif
    assert(k.unassigned_active == 0);
  }
  {
    Kernel k = fixture(true);
    const size_t trail = k.trail.size();
    assert(!k.input_nonunit_uniform);
    assert(lookup_circuit_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.lookup_circuit_model_recognized == 0);
    assert(k.trail.size() == trail);
  }
  {
    Kernel k = wide_fixture();
    assert(lookup_circuit_model_guard(k));
    assert(lookup_circuit_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.lookup_circuit_model_recognized == 1);
    assert(k.stats.lookup_circuit_model_gates == 1);
    assert(k.stats.lookup_circuit_model_support == 8);
#if defined(__x86_64__) || defined(__i386__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2"))
      assert(k.stats.lookup_circuit_model_vector_evaluations >= 3);
#endif
    assert(k.value(internalize(8)) > 0);
    assert(k.value(internalize(9)) > 0);
  }
  {
    Kernel k = fixture();
    strategy::LookupCircuitModelParameters parameters;
    parameters.maximum_fanin = 1;
    strategy::ScopedLookupCircuitModelParameters posed(k, parameters);
    assert(lookup_circuit_model_apply(k) == ApplyResult::NoOp);
  }
  {
    Kernel k = fixture();
    const RuleVTable &lucky = tactix_rules()[tactix_rule_id("Lucky")];
    assert(lucky.apply_instance(k, 13) == ApplyResult::Sat);
  }
  return 0;
}

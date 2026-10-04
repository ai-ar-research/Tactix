#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/simon_model.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;

static void xor_gate(std::ostringstream &out, uint32_t x, uint32_t y,
                     uint32_t z) {
  out << x << ' ' << y << " -" << z << " 0\n";
  out << x << " -" << y << ' ' << z << " 0\n";
  out << '-' << x << ' ' << y << ' ' << z << " 0\n";
  out << '-' << x << " -" << y << " -" << z << " 0\n";
}

static Kernel fixture(bool extra_binary = false) {
  std::ostringstream dimacs;
  dimacs << "p cnf 288 " << 736 + extra_binary << '\n';

  // Identity permutation from the primary to secondary key word.
  for (uint32_t bit = 0; bit < 32; ++bit) {
    const uint32_t primary = 1 + bit, secondary = 33 + bit;
    dimacs << primary << " -" << secondary << " 0\n";
    dimacs << '-' << primary << ' ' << secondary << " 0\n";
  }

  // One complete SIMON32 round. The output XOR uses the primary key half.
  for (uint32_t bit = 0; bit < 32; ++bit) {
    const uint32_t a = 65 + ((bit + 31) & 31);
    const uint32_t b = 65 + ((bit + 24) & 31);
    const uint32_t rotated = 65 + ((bit + 30) & 31);
    const uint32_t right = 97 + bit;
    const uint32_t conjunction = 129 + bit;
    const uint32_t first_xor = 161 + bit;
    const uint32_t second_xor = 193 + bit;
    const uint32_t output = 225 + bit;
    const uint32_t copy = 257 + bit;
    const uint32_t key = 1 + bit;
    const uint32_t left = 65 + bit;
    dimacs << a << " -" << conjunction << " 0\n";
    dimacs << b << " -" << conjunction << " 0\n";
    dimacs << '-' << a << " -" << b << ' ' << conjunction << " 0\n";
    xor_gate(dimacs, rotated, conjunction, first_xor);
    xor_gate(dimacs, right, first_xor, second_xor);
    xor_gate(dimacs, second_xor, key, output);
    dimacs << copy << " -" << left << " 0\n";
    dimacs << '-' << copy << ' ' << left << " 0\n";
  }

  // Zero initial state and the corresponding zero target for key zero.
  for (uint32_t variable = 65; variable <= 128; ++variable)
    dimacs << '-' << variable << " 0\n";
  for (uint32_t variable = 225; variable <= 288; ++variable)
    dimacs << '-' << variable << " 0\n";
  // Key zero forces every key bit false, so (1 v 2) would refute the formula
  // at parse and the rule's guard would not hold.  (-1 v -2) is satisfied by
  // that key and still breaks the exact binary population the shape expects.
  if (extra_binary)
    dimacs << "-1 -2 0\n";

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
    assert(k.input_unit_clause_count == 128);
    assert(k.input_binary_clause_count == 192);
    assert(k.input_ternary_clause_count == 416);
    assert(simon_model_guard(k));
    assert(simon_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.simon_model_calls == 1);
    assert(k.stats.simon_model_recognized == 1);
    assert(k.stats.simon_model_rounds == 1);
    assert(k.stats.simon_model_candidates == 1);
    assert(k.stats.simon_model_sat == 1);
    assert(k.unassigned_active == 0);
  }
  {
    Kernel k = fixture(true);
    const size_t trail = k.trail.size();
    assert(simon_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.simon_model_recognized == 0);
    assert(k.trail.size() == trail);
  }
  {
    Kernel k = fixture();
    strategy::SimonModelParameters parameters;
    parameters.maximum_rounds = 0;
    strategy::ScopedSimonModelParameters posed(k, parameters);
    assert(simon_model_apply(k) == ApplyResult::NoOp);
  }
  return 0;
}

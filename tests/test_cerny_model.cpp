#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/cerny_model.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;

static Kernel fixture(bool extra_positive_binary = false) {
  constexpr uint32_t states = 3, length = 4, base = states * length;
  const uint32_t clauses = 30 + extra_positive_binary;
  std::ostringstream dimacs;
  dimacs << "p cnf 20 " << clauses << '\n';
  for (uint32_t time = 0; time < length; ++time)
    dimacs << base + 2 * time + 1 << ' ' << base + 2 * time + 2
           << " 0\n";

  const uint32_t transition[states][2] = {{1, 0}, {2, 1}, {0, 0}};
  for (uint32_t letter = 0; letter < 2; ++letter) {
    bool emitted[states] = {};
    for (uint32_t source = 0; source < states; ++source) {
      const uint32_t destination = transition[source][letter];
      if (!emitted[destination]) {
        emitted[destination] = true;
        dimacs << destination + 1 << " -" << base + letter + 1 << " 0\n";
      }
    }
  }
  for (uint32_t time = 1; time < length; ++time)
    for (uint32_t source = 0; source < states; ++source)
      for (uint32_t letter = 0; letter < 2; ++letter)
        dimacs << '-' << (time - 1) * states + source + 1 << ' '
               << time * states + transition[source][letter] + 1 << " -"
               << base + 2 * time + letter + 1 << " 0\n";
  for (uint32_t first = 0; first < states; ++first)
    for (uint32_t second = first + 1; second < states; ++second)
      dimacs << '-' << (length - 1) * states + first + 1 << " -"
             << (length - 1) * states + second + 1 << " 0\n";
  if (extra_positive_binary)
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

int main() {
  {
    Kernel k = fixture();
    assert(k.input_positive_binary_count == 4);
    assert(k.input_ternary_clause_count == 18);
    assert(cerny_model_guard(k));
    assert(cerny_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.cerny_model_calls == 1);
    assert(k.stats.cerny_model_recognized == 1);
    assert(k.stats.cerny_model_states == 3);
    assert(k.stats.cerny_model_length == 4);
    assert(k.stats.cerny_model_sat == 1);
    assert(k.unassigned_active == 0);
  }
  {
    Kernel k = fixture(true);
    const size_t trail = k.trail.size();
    assert(cerny_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.cerny_model_recognized == 0);
    assert(k.trail.size() == trail);
  }
  {
    Kernel k = fixture();
    k.level = 1;
    assert(!cerny_model_guard(k));
  }
  return 0;
}

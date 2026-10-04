#undef NDEBUG
#include "frontend/parse.hpp"
#include "rules/lucky/fermat_model.hpp"

#include <cassert>
#include <cstdio>
#include <sstream>
#include <string>

using namespace tactix;

static Kernel fixture(bool third_long = false) {
  std::ostringstream dimacs;
  constexpr unsigned width = 11;
  const unsigned direct_bits = 6;
  const unsigned units = 2 * width + 1;
  const unsigned clauses = 2 + 2 * direct_bits + 2 + units + third_long;
  dimacs << "p cnf 122 " << clauses << '\n';
  for (unsigned i = 1; i <= width; ++i)
    dimacs << '-' << i << ' ';
  dimacs << "0\n";
  for (unsigned i = 1; i <= width; ++i)
    dimacs << '-' << width + i << ' ';
  dimacs << "0\n";

  // source_start=82, source_end=100.  Direct equivalences mark zero bits;
  // their absence marks bits 1..3, so the recovered odd constant is 15.
  for (unsigned bit = 4; bit <= 9; ++bit) {
    const unsigned source = 82 + 2 * bit;
    const unsigned auxiliary = 102 + bit;
    dimacs << '-' << source << ' ' << auxiliary << " 0\n";
    dimacs << source << " -" << auxiliary << " 0\n";
  }
  dimacs << "-100 -101 0\n100 101 0\n";

  // 15=3*5 gives a=(3+5)/2=4 and b=(5-3)/2=1.
  for (unsigned bit = 0; bit < width; ++bit)
    dimacs << (bit == 2 ? int(bit + 1) : -int(bit + 1)) << " 0\n";
  for (unsigned bit = 0; bit < width; ++bit)
    dimacs << (bit == 0 ? int(width + bit + 1)
                        : -int(width + bit + 1))
           << " 0\n";
  dimacs << "-101 0\n";
  if (third_long) {
    for (unsigned i = 1; i <= width; ++i)
      dimacs << i << ' ';
    dimacs << "0\n";
  }

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
    assert(k.input_long_clause_count == 2);
    assert(k.input_fermat_long_shape && k.input_fermat_width == 11);
    assert(fermat_model_guard(k));
    assert(fermat_model_apply(k) == ApplyResult::Sat);
    assert(k.stats.fermat_model_calls == 1);
    assert(k.stats.fermat_model_recognized == 1);
    assert(k.stats.fermat_model_candidates == 1);
    assert(k.stats.fermat_model_sat == 1);
    assert(k.unassigned_active == 0);
  }
  {
    Kernel k = fixture(true);
    const size_t trail = k.trail.size();
    assert(!k.input_fermat_long_shape);
    assert(fermat_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.fermat_model_recognized == 0);
    assert(k.trail.size() == trail);
  }
  {
    Kernel k = fixture();
    strategy::FermatModelParameters parameters;
    parameters.maximum_bits = 9;
    strategy::ScopedFermatModelParameters posed(k, parameters);
    assert(fermat_model_apply(k) == ApplyResult::NoOp);
    assert(k.stats.fermat_model_recognized == 0);
  }
  return 0;
}

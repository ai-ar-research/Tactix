// V6 typed ITE closure: exhaustive original/final models, all literal signs,
// both import orders, direct/public action surfaces; no DefaultPolicy solve.
#undef NDEBUG
#include "congruence_ite_support.hpp"

#include <cstdio>

using namespace tactix;

int main() {
  unsigned closed = 0;
  for (unsigned signs = 0; signs < 128; ++signs)
  for (bool reverse_import : {false, true})
  for (bool public_call : {false, true})
  for (unsigned scenario = 0; scenario < 7; ++scenario) {
    IteFixture f(signs, reverse_import);
    // 3=4 is discovered by Congruence, not present as an entry binary pair.
    f.and_gate(3, 1, 2); f.and_gate(4, 1, 2);
    switch (scenario) {
    case 0: f.ite(6, 3, 4, 5); f.and_gate(-7, -3, -5); break; // c==then: OR
    case 1: f.ite(6, 3, -4, 5); f.and_gate(7, -3, 5); break; // c==-then: AND
    case 2: f.ite(6, 3, 5, 4); f.and_gate(7, 3, 5); break; // c==else: AND
    case 3: f.ite(6, 3, 5, -4); f.and_gate(-7, 3, -5); break; // c==-else: OR
    case 4: f.ite(6, 5, 3, -4); f.xor_gate(-7, 5, 3); break; // then==-else: XOR
    case 5: f.ite(6, 3, 4, 5); f.ite(7, 3, 4, 5); break; // both ITEs degenerate
    case 6: f.ite(6, 1, 2, 5); f.ite(7, 1, 2, 5); break; // nondegenerate control
    }
    assert(f.equal_output_models() == 8);
    const auto equal = [&] {
      return kernel_binary_live(f.k, f.lit(-6), f.lit(7)) &&
             kernel_binary_live(f.k, f.lit(6), f.lit(-7));
    };
    assert(!equal());
    f.close(public_call);
    assert(equal());
    ++closed;
  }
  assert(closed == 3584);
  std::printf("ITE closure: %u cases; all 458752 original/final assignments agree\n", closed);
}

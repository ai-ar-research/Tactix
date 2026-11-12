// V6 Ite root-value rescheduling: root-cofactored ITE implications stay
// unmaterialized. Exhaustive original/final models, all literal signs,
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
  for (unsigned scenario = 0; scenario < 4; ++scenario) {
    IteFixture f(signs, reverse_import);
    // 3=4 is discovered by Congruence, not present as an entry binary pair.
    f.and_gate(3, 1, 2); f.and_gate(4, 1, 2);
    // The equal AND outputs force 5 only during closure, after extraction
    // retained the still-unvalued ITE relation.
    f.and_gate((scenario & 1) ? 5 : -5, 3, -4);
    if (scenario < 2) f.ite(6, 1, 5, 2);
    else f.ite(6, 1, 2, 5);
    switch (scenario) {
    case 0: f.and_gate(-7, -1, -2); break; // then true => OR
    case 1: f.and_gate(7, -1, 2); break;   // then false => AND
    case 2: f.and_gate(-7, 1, -2); break;  // else true => OR
    case 3: f.and_gate(7, 1, 2); break;    // else false => AND
    }
    assert(f.equal_output_models() == 4);
    // Whether from implies to in the binary implication graph.
    const auto implies = [&](Lit from, Lit to) {
      bool seen[14] = {};
      std::vector<Lit> todo{from};
      seen[from] = true;
      for (size_t i = 0; i < todo.size(); ++i) {
        if (todo[i] == to) return true;
        for (const WatchEntry w : f.k.watches.entries(negate(todo[i])))
          if (w.binary() && !seen[w.other()]) {
            seen[w.other()] = true;
            todo.push_back(w.other());
          }
      }
      return false;
    };
    const auto equal = [&] {
      return implies(f.lit(6), f.lit(7)) && implies(f.lit(7), f.lit(6));
    };
    assert(!equal());
    f.close(public_call);
    // The original ITE clauses with the derived root value of 5 already supply
    // these implications, so no redundant binary copies are added under 6.
    const int sides[4][3] = {{6, -1, -2}, {-6, -1, 2}, {6, 1, -2}, {-6, 1, 2}};
    for (unsigned side = 1; side <= 2; ++side)
      assert(!kernel_binary_live(f.k, f.lit(sides[scenario][0]),
                                 f.lit(sides[scenario][side])));
    assert(equal());
    ++closed;
  }
  assert(closed == 2048);
  std::printf("ITE valued-side representation: %u cases; all 262144 original/final assignments agree\n", closed);
}

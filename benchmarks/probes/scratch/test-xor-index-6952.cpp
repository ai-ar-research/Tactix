// Compiled with each actual congruence.cpp included into this translation unit;
// no copied ClauseIndex or XOR algorithm, and no production test-only API.
#undef NDEBUG
#include <cassert>
#include <cstdio>
using namespace tactix;
static uint64_t digest = 1469598103934665603ull;
static void mix(uint64_t v) { digest ^= v; digest *= 1099511628211ull; }
static void observe(const Kernel &k, const std::vector<CongGate> &gates) {
  mix(k.stats.congruence_ticks); mix(k.stats.rule_interrupted); mix(gates.size());
  for (const auto &g : gates) {
    mix(static_cast<unsigned>(g.kind)); mix(g.output); mix(g.base_id());
    mix(g.inputs.size()); for (Lit lit : g.inputs) mix(lit);
  }
}
int main() {
  unsigned cases = 0;
  for (unsigned mask = 0; mask < 256; mask++)
    for (bool finished : {false,true})
      for (bool reverse : {false,true}) {
        Kernel k; kernel_reserve_vars(k, 5); ClauseIndex index;
        for (unsigned i = 0; i < 8; i++) {
          const unsigned signs = reverse ? 7-i : i;
          if (!(mask & (1u << signs))) continue;
          Lit row[3]; for (unsigned v = 0; v < 3; v++) row[v] = make_lit(v,(signs >> v)&1);
          index.insert(small_key(row,3)); index.insert(small_key(row,3));
        }
        Lit repeated[] = {internalize(1),internalize(-1),internalize(2)};
        Lit larger[] = {internalize(1),internalize(2),internalize(3),internalize(4)};
        index.insert(small_key(repeated,3)); index.insert(small_key(larger,4));
        if (finished) index.finish();
        std::vector<CongGate> gates; congruence_extract_xor(k,index,index,gates);
        const bool complete = (mask & 0x69) == 0x69 || (mask & 0x96) == 0x96;
        assert(gates.size() == (finished && complete ? 3u : 0u));
        observe(k,gates); cases++;
      }
  // Real index construction returns before finish at its 256th arena record.
  // After removing the external cut, extracting that retained prefix must
  // still produce no gate, even though it contains many full parity sets.
  for (bool interrupt : {false,true}) {
    Kernel k; kernel_reserve_vars(k, 3);
    for (unsigned i = 0; i < 600; i++) {
      const unsigned signs = i % 8;
      Lit row[3]; for (unsigned v = 0; v < 3; v++) row[v] = make_lit(v,(signs >> v)&1);
      kernel_add_large_clause(k,row,3,false,0,++k.next_clause_id);
    }
    ClauseIndex index; volatile sig_atomic_t stop = 1;
    if (interrupt) k.stop = &stop; else cong_budget = {0,1,false};
    build_clause_index(k,index,nullptr);
    assert(index.size() == 255 && k.stats.congruence_ticks == 765);
    assert(interrupt ? k.stats.rule_interrupted == 1 : cong_budget.cut);
    cong_budget = {}; k.stop = nullptr;
    std::vector<CongGate> gates; congruence_extract_xor(k,index,index,gates);
    assert(gates.empty()); observe(k,gates); cases++;
  }
  std::printf("PASS %u actual index cases digest=%016llx\n",cases,(unsigned long long)digest);
}

// Include each actual congruence.cpp via -include. Exercise its private meter
// and immutable index without introducing a test-only production API.
#undef NDEBUG
#include <cassert>
#include <cstdio>
using namespace tactix;
static uint64_t digest = 1469598103934665603ull;
static void mix(uint64_t v) { digest ^= v; digest *= 1099511628211ull; }
static void observe(const Kernel &k, const std::vector<CongGate> &gates) {
  mix(k.stats.congruence_ticks); mix(k.stats.rule_interrupted); mix(cong_budget.cut);
  mix(gates.size());
  for (const auto &g : gates) {
    mix(static_cast<unsigned>(g.kind)); mix(g.output); mix(g.base_id());
    mix(g.inputs.size()); for (Lit lit : g.inputs) mix(lit);
    mix(g.input_bin_ids().size()); for (auto id : g.input_bin_ids()) mix(id);
  }
}
static void exercise(unsigned n, bool reverse, bool finished, uint64_t limit, bool stopped) {
  Kernel k; kernel_reserve_vars(k, 1+3*n); ClauseIndex index;
  std::vector<std::array<Lit, 3>> rows;
  const Lit out = make_lit(0, false);
  for (unsigned i = 0; i < n; i++) {
    const Lit c = make_lit(1+3*i, false), t = make_lit(2+3*i, i&1), e = make_lit(3+3*i, i&2);
    rows.push_back({c, negate(e), out}); rows.push_back({c, e, negate(out)});
    rows.push_back({negate(c), negate(t), out}); rows.push_back({negate(c), t, negate(out)});
  }
  if (reverse) std::reverse(rows.begin(), rows.end());
  for (const auto &row : rows) index.insert(small_key(row.data(), 3));
  if (finished) index.finish();
  const auto size = index.size();
  volatile sig_atomic_t stop = 1;
  if (stopped) k.stop = &stop;
  k.stats.congruence_ticks = 17;
  cong_budget = {17, limit, false};
  std::vector<CongGate> gates;
  congruence_extract_ite(k, index, index, gates);
  assert(index.size() == size);
  if (!finished) assert(gates.empty());
  if (stopped && n >= 64) assert(gates.empty() && k.stats.rule_interrupted == 1);
  if (finished && !limit && (!stopped || n < 64)) assert(gates.size() == (n <= 128 ? n : 0));
  for (const auto &g : gates) {
    assert(g.kind == GateKind::Ite && var_of(g.output) == 0);
    assert(g.inputs.size() == 3 && !negated(g.inputs[0]) && !negated(g.inputs[1]));
  }
  observe(k, gates);
  // Clear a cut/termination and extract from the same retained index. The
  // prior exit must not damage it or leak round state into a later caller.
  cong_budget = {}; k.stop = nullptr;
  std::vector<CongGate> resumed;
  congruence_extract_ite(k, index, index, resumed);
  assert(resumed.size() == (finished && n <= 128 ? n : 0));
  observe(k, resumed);
  cong_budget = {};
  mix(n); mix(reverse); mix(finished); mix(limit); mix(stopped);
}
int main() {
  unsigned cases = 0;
  for (unsigned n : {1u, 2u, 8u, 63u, 64u, 127u, 128u, 129u})
    for (bool reverse : {false, true})
      for (bool finished : {false, true})
        for (bool stopped : {false, true})
          for (uint64_t limit : {uint64_t{0}, uint64_t{1}, uint64_t{6}, uint64_t{24*n-1}, uint64_t{24*n}, uint64_t{24*n+1}, uint64_t{24*n+2*n*n+1}}) {
            exercise(n, reverse, finished, limit, stopped); cases++;
          }
  std::printf("PASS %u ITE budget/index cases digest=%016llx\n", cases, (unsigned long long)digest);
}

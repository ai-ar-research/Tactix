// Actual old/new objects run this fixed watch-compaction sequence. Reuse the
// existing exhaustive/model/LRAT oracles; the included helper main returns0.
#define main subsolver_existing_main
#include "test_subsolver.cpp"
#undef main

static uint64_t binary_digest = UINT64_C(1469598103934665603);
static unsigned binary_solves = 0, binary_sat = 0, binary_unsat = 0, binary_budget = 0;
static void mix(uint64_t v) { binary_digest = (binary_digest ^ v) * UINT64_C(1099511628211); }
template<class T> static void mix_vector(const std::vector<T> &v) {
  mix(v.size()); for (const auto &x : v) mix(x);
}
static void check_and_hash(SubSolver &s, const Formula &f,
                           const std::vector<SubLit> &assumptions, SubResult r) {
  ++binary_solves; mix(static_cast<unsigned>(r));
  mix(s.ticks()); mix(s.kitten_ticks()); mix(s.conflicts());
  mix(s.kitten_generator()); mix(s.kitten_phase_capacity()); mix(s.original_clauses());
  mix(s.has_model());
  for (SubLit l = 0; l < 2*f.nvars; ++l) mix(s.fixed_literal(l));
  if (r == SubResult::Budget) { ++binary_budget; return; }
  const bool expected = brute_force(f, assumptions);
  assert((r == SubResult::Sat) == expected);
  if (r == SubResult::Sat) {
    ++binary_sat; assert(s.has_model()); mix_vector(s.model());
    uint32_t bits = 0;
    for (SubVar v = 0; v < f.nvars; ++v) if (s.model()[v] > 0) bits |= 1u << v;
    assert(satisfied_by(f,bits));
    for (SubLit a : assumptions) assert(bool(bits & (1u << sub_var_of(a))) != bool(a & 1u));
    for (SubLit l = 0; l < 2*f.nvars; ++l) {
      mix(s.flip_model_literal(l)); mix_vector(s.model());
      uint32_t after = 0;
      for (SubVar v = 0; v < f.nvars; ++v) if (s.model()[v] > 0) after |= 1u << v;
      assert(satisfied_by(f,after));
    }
    return;
  }
  ++binary_unsat; assert(!s.has_model());
  SubDerivation d; const bool have = s.derivation(d,true); mix(have);
  if (!have) return;
  std::map<uint32_t,std::vector<SubLit>> clauses;
  for (uint32_t i = 0; i < f.clauses.size(); ++i) clauses[i+1] = f.clauses[i];
  mix(d.steps.size());
  for (const auto &step : d.steps) {
    assert(lrat_replay(clauses,step)); clauses[step.id] = step.lits;
    mix(step.id); mix_vector(step.lits); mix_vector(step.chain); mix_vector(step.core_antecedents());
  }
  assert(!d.steps.empty()); check_target_is_entailed(f,assumptions,d.steps.back().lits);
  mix_vector(d.core); mix(d.core_clauses.size());
  for (const auto &c : d.core_clauses) { mix(c.learned); mix(c.id); mix_vector(c.lits); }
}
int main() {
  unsigned cases = 0;
  for (unsigned copies : {0u,1u,15u,16u,17u,31u,32u,33u})
    for (unsigned pose = 0; pose < 5; ++pose)
      for (SubTickMode meter : {SubTickMode::Entries, SubTickMode::Kitten}) {
        Formula f; f.nvars = 8;
        std::vector<std::vector<int>> input = {
            {1,2},{1,-2},{-1,2},{-1,-2}, {3,4,5}, {-3,6,7,8}};
        for (unsigned i = 0; i < copies; ++i) input.push_back({1,2});
        std::rotate(input.begin(),input.begin()+pose,input.end());
        for (auto c : input) {
          std::rotate(c.begin(),c.begin()+pose%c.size(),c.end());
          std::vector<SubLit> lits;
          for (int l : c) lits.push_back(sub_lit(std::abs(l)-1,l < 0));
          f.clauses.push_back(lits);
        }
        SubSolver s; load(s,f,SubSearchMode::Kitten);
        for (unsigned pass = 0; pass < 4; ++pass) {
          s.randomize_phases();
          const auto result = s.solve({},0,meter);
          assert(result == SubResult::Unsat);
          check_and_hash(s,f,{},result);
          SubDerivation d; assert(s.derivation(d,true) && !d.core.empty());
          if (pass == 3) break;
          // Preserve non-core large clauses on the first rebuild as well, so
          // attachment regenerates both tags and following shuffles move them.
          auto keep = d.core;
          if (pass == 0) for (uint32_t id = 1; id <= f.clauses.size(); ++id)
            if (f.clauses[id-1].size() > 2) keep.push_back(id);
          s.shrink_to_clausal_core(keep);
          s.shuffle_clauses();
        }
        ++cases;
      }
  std::printf("subsolver-core-binary cases=%u solves=%u UNSAT=%u digest=%016llx\n",
      cases,binary_solves,binary_unsat,(unsigned long long)binary_digest);
}

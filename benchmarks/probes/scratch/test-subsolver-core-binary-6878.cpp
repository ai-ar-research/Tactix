// Fixed metadata-lifetime regression, compiled against each complete own arm.
#define main binary_fixture_main
#include "test_subsolver_binary.cpp"
#undef main
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

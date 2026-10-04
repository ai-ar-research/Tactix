#pragma once

#include <cstdint>
#include <vector>

namespace tactix::pigeonhole_certificate {

// Extended-resolution pigeonhole reduction using sequential AMO prefixes.
// x[pigeon][hole]: every pigeon has a cover, every hole has pairwise AMO.
// There are h+1 pigeons and h holes. Optional guard g means these premises
// hold under NOT g; append g to every emitted clause, including fresh RAT
// definitions, and discharge the refutation to the unit g. All input variable
// identities must be distinct and disjoint from g; the sink owns a fresh
// namespace and must stop permanently on failure. Zero guard means refutation.
// Sink: fresh(int32_t&), add(vector<int32_t>, bool rat, const char *role).
template<class Sink>
class Certificate {
  Sink &sink;
  int32_t guard;
  bool valid = true;
  bool add(std::vector<int32_t> clause, bool rat, const char *role) {
    if (!valid) return false;
    if (guard) clause.push_back(guard);
    return valid = sink.add(clause, rat, role);
  }
  int32_t fresh() {
    int32_t result = 0;
    if (valid) valid = sink.fresh(result) && result > 0;
    return result;
  }
  int32_t disjunction(int32_t a, int32_t b) {
    const int32_t q = fresh();
    add({q,-a}, true, "prefix-definition");
    add({q,-b}, true, "prefix-definition");
    add({-q,a,b}, true, "prefix-definition");
    return q;
  }
  int32_t move(int32_t a, int32_t b, int32_t c) {
    // y iff a OR (b AND c). Each positive-pivot clause precedes
    // negative-pivot clauses; all cross resolvents are tautological.
    const int32_t y = fresh();
    add({y,-a}, true, "move-definition");
    add({y,-b,-c}, true, "move-definition");
    add({-y,a,b}, true, "move-definition");
    add({-y,a,c}, true, "move-definition");
    return y;
  }
public:
  Certificate(Sink &output, int32_t condition = 0) : sink(output), guard(condition) {}
  bool prove(std::vector<std::vector<int32_t>> x) {
    return run(std::move(x),false,nullptr);
  }
  // Balanced form: n covers into n capacity-one holes imply that every hole
  // is occupied. Reduce to 1x1, then lift all occupancy units back through
  // the saved moved-prefix definitions. One shared cubic construction proves
  // every hole occupied; it does not repeat n separate pigeonhole refutations.
  bool derive_occupancy(std::vector<std::vector<int32_t>> x,
                        std::vector<int32_t> &occupied) {
    occupied.clear();
    return run(std::move(x),true,&occupied);
  }
private:
  bool run(std::vector<std::vector<int32_t>> x,bool balanced,
           std::vector<int32_t> *occupied) {
    if (x.empty() || x[0].empty() || x[0].size()+(balanced?0:1) != x.size()) return false;
    const size_t initial_holes = x[0].size();
    for (const auto &row : x) {
      if (row.size() != initial_holes) return false;
      for (int32_t lit : row) if (!lit || lit == INT32_MIN) return false;
    }
    // s[i][j] is the OR of x[0..i][j]. Establish its capacity using
    // existing pairwise exclusions, without adding a quadratic exclusion set.
    auto s = x;
    for (size_t j = 0; j < initial_holes && valid; ++j)
      for (size_t i = 1; i < x.size() && valid; ++i) {
        add({-s[i-1][j],-x[i][j]}, false, "initial-prefix-capacity");
        s[i][j] = disjunction(s[i-1][j],x[i][j]);
      }
    std::vector<std::vector<int32_t>> levels;
    while (valid && x[0].size() > 1) {
      if (balanced) levels.push_back(s.back());
      const size_t last = x.size()-1, hole = x[0].size()-1;
      std::vector<std::vector<int32_t>> y(last, std::vector<int32_t>(hole));
      auto next = y;
      for (size_t j = 0; j < hole && valid; ++j) {
        const int32_t displaced = x[last][j];
        for (size_t i = 0; i < last && valid; ++i) {
          y[i][j] = move(x[i][j],x[i][hole],displaced);
          if (!i) { next[i][j] = y[i][j]; continue; }
          next[i][j] = move(s[i][j],s[i][hole],displaced);
          // The compact moved prefix equals OR(previous moved prefix, y_i).
          add({-next[i-1][j],next[i][j]}, false, "moved-prefix-link");
          add({-y[i][j],next[i][j]}, false, "moved-prefix-link");
          add({-next[i][j],next[i-1][j],y[i][j]}, false, "moved-prefix-link");
          // First exclude an unmoved occupant, then both ways of selecting y_i.
          add({-next[i-1][j],-x[i][j]}, false, "moved-prefix-exclusion");
          add({-next[i-1][j],-y[i][j]}, false, "moved-prefix-capacity");
        }
      }
      // Under a negated new cover, every old non-last entry is false.
      // The old cover fixes the last entry; then the displaced pigeon's
      // cover and the last-hole capacity give the contradiction by UP.
      for (size_t i = 0; i < last && valid; ++i)
        add(y[i], false, "moved-pigeon-cover");
      x = std::move(y); s = std::move(next);
    }
    if (valid && balanced) {
      add({x[0][0]},false,"base-hole-occupied");
      for(auto level=levels.rbegin();level!=levels.rend()&&valid;++level)
        for(int32_t hole:*level) {
          if(!add({hole},false,"lift-hole-occupied"))break;
        }
      if(valid)*occupied=levels.empty()?x[0]:levels.front();
    } else if (valid) add({}, false, "pigeonhole-conclusion");
    return valid;
  }
};
} // namespace tactix::pigeonhole_certificate

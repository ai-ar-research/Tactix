#pragma once

#include "kernel/types.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace tactix::row_occupancy {

// Exact live clause occurrences. IDs identify this snapshot, not necessarily
// native LRAT IDs (binary native IDs need not be retained in every mode).
struct Premise { uint64_t occurrence; std::vector<int32_t> literals; };
struct Interval { uint32_t row, first, last; int32_t absence; };
struct Gate {
  Interval interval;
  int32_t left, right;
  uint64_t defining;
  std::vector<uint64_t> reverse_left, reverse_right;
};
struct Image {
  uint32_t width = 0;
  std::vector<int32_t> primary;
  std::vector<uint64_t> column_covers;
  std::vector<Gate> gates;
  // For each (row, first-label, second-label), an exact binary occurrence
  // excludes that pair via the certified interval meanings. Diagonal entries
  // are unused. Complete off-diagonal coverage certifies every row's AMO.
  std::vector<uint64_t> row_exclusions;
  // Signed disjuncts. Negative absence gates and positive primary indicators
  // both denote "some selected label in this block".
  std::vector<std::vector<int32_t>> rows;
};

inline uint32_t variable(int32_t literal) {
  return external_variable_of(literal);
}

// Indexed layout proposes a semantic interpretation; every reused gate is
// justified by exact clauses. Row AMO requires complete interval-exclusion
// coverage; no graph edge or row-cover unit is inferred from the layout.
// The caller bounds the snapshot storage and
// supplies a monotone work/termination meter. Failure never publishes an image.
template<class Charge>
bool recognize(const std::vector<Premise> &source, uint32_t maximum_width,
               Charge &&charge, Image &out) {
  out = {};
  Image image;
  std::map<std::vector<int32_t>, uint64_t> clauses;
  for (const auto &premise : source) {
    if (!charge(1 + premise.literals.size())) return false;
    auto key = premise.literals;
    for (int32_t lit : key) if (!lit || lit == INT32_MIN) return false;
    std::sort(key.begin(), key.end());
    clauses.emplace(key, premise.occurrence);
    const uint64_t n = key.size();
    if (n < 3 || n <= image.width || n > maximum_width || n*n > INT32_MAX) continue;
    uint32_t column = UINT32_MAX;
    bool indexed = true;
    std::vector<uint8_t> rows(n, 0);
    for (int32_t lit : key) {
      const uint32_t v = variable(lit) - 1;
      if (v >= n*n || rows[v/n] || (column != UINT32_MAX && v%n != column)) {
        indexed = false; break;
      }
      rows[v/n] = 1; column = v%n;
    }
    if (indexed) image.width = n;
  }
  const uint32_t n = image.width;
  if (!n || !charge(uint64_t(n)*n + n)) return false;
  image.primary.assign(size_t(n)*n, 0);
  image.column_covers.assign(n, 0);
  for (const auto &premise : source) {
    if (!charge(1 + premise.literals.size())) return false;
    if (premise.literals.size() != n) continue;
    uint32_t column = UINT32_MAX;
    std::vector<uint8_t> rows(n, 0);
    bool indexed = true;
    for (int32_t lit : premise.literals) {
      const uint32_t v = variable(lit)-1;
      if (v >= uint64_t(n)*n || rows[v/n] || (column != UINT32_MAX && v%n != column)) {
        indexed = false; break;
      }
      rows[v/n] = 1; column = v%n;
    }
    if (!indexed) continue;
    for (int32_t lit : premise.literals) {
      int32_t &entry = image.primary[variable(lit)-1];
      if (entry && entry != lit) return false;
      entry = lit;
    }
    image.column_covers[column] = premise.occurrence;
  }
  if (std::find(image.column_covers.begin(), image.column_covers.end(), 0) != image.column_covers.end()) return false;
  auto lookup = [&](std::initializer_list<int32_t> literals) -> uint64_t {
    std::vector<int32_t> key(literals); std::sort(key.begin(), key.end());
    const auto found = clauses.find(key);
    return found == clauses.end() ? 0 : found->second;
  };
  std::vector<const Premise *> triples;
  for (const auto &premise : source) {
    if (!charge(1)) return false;
    if (premise.literals.size() == 3) triples.push_back(&premise);
  }
  auto output = [](const Premise *p) {
    return *std::max_element(p->literals.begin(), p->literals.end(),
        [](int32_t a, int32_t b) { return variable(a) < variable(b); });
  };
  std::sort(triples.begin(), triples.end(), [&](auto a, auto b) {
    return std::pair(variable(output(a)), a->occurrence) < std::pair(variable(output(b)), b->occurrence);
  });
  std::map<int32_t, Interval> intervals;
  auto meaning = [&](int32_t lit, Interval &span) {
    const uint32_t v = variable(lit)-1;
    if (v < image.primary.size() && image.primary[v] == lit) {
      span = {v/n, v%n, v%n, 0}; return true;
    }
    const auto found = intervals.find(-lit);
    if (found == intervals.end()) return false;
    span = found->second; return true;
  };
  for (const Premise *premise : triples) {
    if (!charge(16)) return false;
    const int32_t z = output(premise);
    if (z <= 0 || uint64_t(z) <= uint64_t(n)*n) continue;
    std::vector<int32_t> inputs;
    for (int32_t l : premise->literals) if (l != z) inputs.push_back(l);
    if (inputs.size() != 2 || variable(inputs[0]) == variable(inputs[1])) continue;
    const int32_t a = inputs[0], b = inputs[1];
    Interval left, right;
    if (!meaning(a, left) || !meaning(b, right) || left.row != right.row ||
        uint64_t(left.last)+1 < right.first || uint64_t(right.last)+1 < left.first) continue;
    auto reverse = [&](int32_t x, int32_t other) {
      std::vector<uint64_t> parents;
      if (uint64_t direct = lookup({-x, -z})) parents.push_back(direct);
      else {
        const uint64_t last = lookup({other, -x, -z});
        uint64_t first = lookup({-other, -x});
        if (!first) first = lookup({-other, -z});
        // Under x=z=true, first forces other=false and last conflicts.
        if (first && last) parents = {first, last};
      }
      return parents;
    };
    auto ra = reverse(a,b), rb = reverse(b,a);
    if (ra.empty() || rb.empty()) continue;
    Interval span{left.row, std::min(left.first,right.first), std::max(left.last,right.last),z};
    if (auto previous = intervals.find(z); previous != intervals.end()) {
      if (previous->second.row != span.row || previous->second.first != span.first ||
          previous->second.last != span.last) return false;
      continue;
    }
    intervals.emplace(z,span);
    image.gates.push_back({span,a,b,premise->occurrence,std::move(ra),std::move(rb)});
  }
  const uint64_t exclusion_cells = uint64_t(n)*n*n;
  if (exclusion_cells > SIZE_MAX/sizeof(uint64_t) || !charge(exclusion_cells)) return false;
  image.row_exclusions.assign(static_cast<size_t>(exclusion_cells),0);
  auto absence = [&](int32_t lit, Interval &span) {
    const uint32_t v = variable(lit)-1;
    if (v < image.primary.size() && image.primary[v] == -lit) {
      span={v/n,v%n,v%n,lit}; return true;
    }
    const auto found=intervals.find(lit);
    if(found==intervals.end())return false;
    span=found->second;return true;
  };
  for(const auto &premise:source) {
    if(!charge(1))return false;
    if(premise.literals.size()!=2)continue;
    Interval a,b;
    if(!absence(premise.literals[0],a)||!absence(premise.literals[1],b)||a.row!=b.row)continue;
    for(uint32_t first=a.first;first<=a.last;++first)
      for(uint32_t second=b.first;second<=b.last;++second) {
        if(!charge(1))return false;
        image.row_exclusions[(size_t(a.row)*n+first)*n+second]=premise.occurrence;
        image.row_exclusions[(size_t(a.row)*n+second)*n+first]=premise.occurrence;
      }
  }
  for(uint32_t row=0;row<n;++row)for(uint32_t a=0;a<n;++a)for(uint32_t b=a+1;b<n;++b) {
    if(!charge(1)||!image.row_exclusions[(size_t(row)*n+a)*n+b])return false;
  }
  // Shortest exact row partition reuses certified intervals; singleton leaves
  // keep this complete even when no usable interval gate survives inprocessing.
  image.rows.resize(n);
  std::vector<std::vector<Interval>> by_row(n);
  for (const auto &[literal, span] : intervals) {
    if (!charge(1)) return false;
    by_row[span.row].push_back(span);
  }
  for (uint32_t row = 0; row < n; ++row) {
    if (!charge(n+1)) return false;
    std::vector<uint32_t> distance(n+1, UINT32_MAX), previous(n+1);
    std::vector<int32_t> chosen(n+1);
    distance[0] = 0;
    for (uint32_t first = 0; first < n; ++first) {
      if (!charge(1)) return false;
      auto relax = [&](uint32_t last, int32_t lit) {
        if (distance[first]+1 < distance[last+1]) {
          distance[last+1] = distance[first]+1; previous[last+1] = first; chosen[last+1] = lit;
        }
      };
      relax(first, image.primary[size_t(row)*n+first]);
      for (const Interval &span : by_row[row]) {
        if (!charge(1)) return false;
        if (span.first == first) relax(span.last,-span.absence);
      }
    }
    for (uint32_t end = n; end; end = previous[end]) image.rows[row].push_back(chosen[end]);
    std::reverse(image.rows[row].begin(), image.rows[row].end());
  }
  out = std::move(image);
  return true;
}
} // namespace tactix::row_occupancy

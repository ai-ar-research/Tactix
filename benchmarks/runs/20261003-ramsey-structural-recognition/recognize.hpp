#pragma once
// Isolated recognition research. Not installed in a production action.
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace ramsey_recovery {
using Clause = std::vector<int32_t>;
struct Limits {
  uint64_t clauses = 0, variables = 0, pair_cells = 0, work = 0;
};
struct Source {
  std::array<uint32_t, 4> vertices;
  uint64_t positive_clause, negative_clause; // zero-based input occurrences
};
struct Result {
  bool recognized = false, exhausted = false;
  uint64_t work = 0;
  // Signed external literals: true denotes one consistently chosen color.
  std::vector<std::vector<int32_t>> edges;
  std::vector<Source> sources;
};

inline Result recognize(const std::vector<Clause> &input, const Limits &limits) {
  Result out;
  const auto charge = [&](uint64_t amount = 1) {
    if (amount > limits.work - out.work) {
      out.exhausted = true;
      return false;
    }
    out.work += amount;
    return true;
  };
  if (input.empty() || input.size() > limits.clauses) return out;
  std::set<int32_t> variables;
  std::map<Clause, uint64_t> clauses;
  for (uint64_t occurrence = 0; occurrence < input.size(); ++occurrence) {
    if (!charge()) return out;
    Clause clause = input[occurrence];
    if (clause.size() != 6) return out;
    std::set<int32_t> distinct;
    for (const int32_t lit : clause) {
      if (!lit || lit == INT32_MIN) return out;
      const int32_t variable = lit < 0 ? -lit : lit;
      if (!distinct.insert(variable).second) return out;
      variables.insert(variable);
      if (variables.size() > limits.variables) return out;
    }
    std::sort(clause.begin(), clause.end());
    // Exact-input admission deliberately declines duplicate occurrences.
    if (!clauses.emplace(std::move(clause), occurrence).second) return out;
  }
  const uint64_t m = variables.size();
  if (!m || m > UINT32_MAX || m > limits.pair_cells / m) return out;
  uint64_t n = 2;
  while (n * (n - 1) / 2 < m) {
    if (!charge()) return out;
    ++n;
  }
  // For n>=6 the common-neighbor star clique has at least two vertices.
  if (n < 6 || n * (n - 1) / 2 != m ||
      input.size() != n * (n - 1) * (n - 2) * (n - 3) / 12) return out;
  std::vector<int32_t> identities(variables.begin(), variables.end());
  std::map<int32_t, uint32_t> index;
  for (uint32_t i = 0; i < m; ++i) index.emplace(identities[i], i);
  std::vector<uint32_t> counts(m * m, 0);
  std::vector<int8_t> signs(m * m, 0);
  for (const Clause &clause : input) {
    for (uint32_t a = 0; a < 6; ++a) for (uint32_t b = a + 1; b < 6; ++b) {
      if (!charge()) return out;
      const auto i = index.at(clause[a] < 0 ? -clause[a] : clause[a]);
      const auto j = index.at(clause[b] < 0 ? -clause[b] : clause[b]);
      const int8_t sign = (clause[a] < 0) == (clause[b] < 0) ? 1 : -1;
      if ((signs[i*m+j] && signs[i*m+j] != sign) ||
          counts[i*m+j] == UINT32_MAX) return out;
      signs[i*m+j] = signs[j*m+i] = sign;
      ++counts[i*m+j]; ++counts[j*m+i];
    }
  }
  const auto adjacent = [&](uint32_t i, uint32_t j) {
    return i != j && counts[i*m+j] == 2 * (n - 3);
  };
  for (uint32_t i = 0; i < m; ++i) for (uint32_t j = i + 1; j < m; ++j) {
    if (!charge()) return out;
    if (counts[i*m+j] != 2 && !adjacent(i, j)) return out;
  }
  std::vector<int8_t> orientation(m, 1);
  for (uint32_t i = 1; i < m; ++i) orientation[i] = signs[i];
  for (uint32_t i = 0; i < m; ++i) for (uint32_t j = i + 1; j < m; ++j) {
    if (!charge()) return out;
    if (signs[i*m+j] != orientation[i] * orientation[j]) return out;
  }
  std::set<std::vector<uint32_t>> stars;
  for (uint32_t i = 0; i < m; ++i) for (uint32_t j = i + 1; j < m; ++j) {
    if (!charge()) return out;
    if (!adjacent(i, j)) continue;
    std::vector<uint32_t> common, star{i, j};
    for (uint32_t k = 0; k < m; ++k) {
      if (!charge()) return out;
      if (adjacent(i, k) && adjacent(j, k)) common.push_back(k);
    }
    if (common.size() != n - 2) return out;
    // L(K_n): common neighbors are the n-3 other edges at the shared
    // endpoint, plus the isolated edge joining the two other endpoints.
    for (const uint32_t k : common) {
      bool linked = false;
      for (const uint32_t l : common) {
        if (!charge()) return out;
        linked |= adjacent(k, l);
      }
      if (linked) star.push_back(k);
    }
    if (star.size() != n - 1) return out;
    std::sort(star.begin(), star.end());
    for (size_t a = 0; a < star.size(); ++a) for (size_t b = a + 1; b < star.size(); ++b) {
      if (!charge()) return out;
      if (!adjacent(star[a], star[b])) return out;
    }
    stars.insert(std::move(star));
    if (stars.size() > n) return out;
  }
  if (stars.size() != n) return out;
  std::vector<std::vector<uint32_t>> endpoints(m);
  uint32_t vertex = 0;
  for (const auto &star : stars) {
    for (uint32_t edge : star) endpoints[edge].push_back(vertex);
    ++vertex;
  }
  std::vector<std::vector<int32_t>> edges(n, std::vector<int32_t>(n, 0));
  for (uint32_t i = 0; i < m; ++i) {
    if (!charge()) return out;
    if (endpoints[i].size() != 2) return out;
    const auto a = endpoints[i][0], b = endpoints[i][1];
    if (edges[a][b]) return out;
    edges[a][b] = edges[b][a] = orientation[i] * identities[i];
  }
  // Frequency recovery is only a candidate. Exact full-clause verification
  // below is the admission authority, including both signs and source IDs.
  std::vector<Source> sources;
  for (uint32_t a = 0; a < n; ++a) for (uint32_t b = a + 1; b < n; ++b)
    for (uint32_t c = b + 1; c < n; ++c) for (uint32_t d = c + 1; d < n; ++d) {
      if (!charge()) return out;
      Clause positive{edges[a][b], edges[a][c], edges[a][d], edges[b][c], edges[b][d], edges[c][d]};
      Clause negative = positive;
      for (int32_t &lit : negative) lit = -lit;
      std::sort(positive.begin(), positive.end());
      std::sort(negative.begin(), negative.end());
      const auto p = clauses.find(positive), q = clauses.find(negative);
      if (p == clauses.end() || q == clauses.end()) return out;
      sources.push_back({{a,b,c,d},p->second,q->second});
    }
  out.edges = std::move(edges);
  out.sources = std::move(sources);
  out.recognized = true;
  return out;
}
} // namespace ramsey_recovery

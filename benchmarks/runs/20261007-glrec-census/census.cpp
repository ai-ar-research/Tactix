// Offline cost census of graph_labeling::recognize (c1 source) on a raw DIMACS
// file. Approximation: externals = DIMACS-1, every input clause of width >= 3
// is an irredundant arena clause, binaries are watch-only, no root values.
// Reports the c1 charge per pass and the primary-region size (literals with
// external < M*M, M = min(maximum_vertices, isqrt(variables))).
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>
int main(int argc, char **argv) {
  const uint32_t maxv = argc > 1 ? atoi(argv[1]) : 140;
  std::vector<int> lits; std::vector<uint64_t> start; uint64_t vars = 0, bins = 0, units = 0;
  std::vector<uint64_t> sizehist(1024, 0);
  int c; int64_t cur = 0; bool neg = false, in = false; std::vector<int> cl;
  char buf[1 << 16]; size_t len; bool comment = false, bol = true;
  auto flush_lit = [&](int64_t v) {
    if (v == 0) {
      if (cl.size() >= 3) { start.push_back(lits.size()); lits.insert(lits.end(), cl.begin(), cl.end()); }
      else if (cl.size() == 2) ++bins; else if (cl.size() == 1) ++units;
      if (cl.size() < sizehist.size()) ++sizehist[cl.size()];
      cl.clear();
    } else { cl.push_back((int)v); vars = std::max<uint64_t>(vars, (uint64_t)std::llabs(v)); }
  };
  while ((len = fread(buf, 1, sizeof buf, stdin)) > 0) {
    for (size_t i = 0; i < len; ++i) {
      c = buf[i];
      if (bol && (c == 'c' || c == 'p')) comment = true;
      if (c == '\n') { bol = true; if (comment) { comment = false; continue; } }
      else bol = false;
      if (comment) continue;
      if (c == '-') { neg = true; continue; }
      if (c >= '0' && c <= '9') { cur = cur * 10 + (c - '0'); in = true; continue; }
      if (in) { flush_lit(neg ? -cur : cur); cur = 0; in = false; neg = false; }
    }
  }
  if (in) flush_lit(neg ? -cur : cur);
  start.push_back(lits.size());
  const uint64_t ncl = start.size() - 1;
  // pass 1
  uint64_t w1 = 0; uint32_t n = 0;
  for (uint64_t i = 0; i < ncl; ++i) {
    ++w1; uint32_t width = start[i + 1] - start[i];
    if (width >= 3 && width > n && width <= maxv && uint64_t(width) * width <= vars) {
      uint32_t column = UINT32_MAX; bool ok = true;
      for (uint64_t j = start[i]; j < start[i + 1]; ++j) {
        ++w1; uint64_t e = std::llabs(lits[j]) - 1;
        if (e >= uint64_t(width) * width || (column != UINT32_MAX && e % width != column)) { ok = false; break; }
        column = e % width;
      }
      if (ok) n = width;
    }
  }
  uint64_t w2 = 0, indexed = 0, cand = 0; std::vector<uint8_t> cols(n ? n : 1, 0);
  if (n) {
    const uint64_t sq = uint64_t(n) * n;
    w2 += sq + 3 * uint64_t(n);
    for (uint64_t i = 0; i < ncl; ++i) {
      ++w2; uint32_t size = start[i + 1] - start[i];
      if (size >= 3 && size <= n) {
        ++cand; w2 += n; std::vector<uint8_t> rows(n, 0); uint32_t column = UINT32_MAX; bool ok = true;
        for (uint64_t j = start[i]; j < start[i + 1]; ++j) {
          ++w2; uint64_t e = std::llabs(lits[j]) - 1;
          if (e >= sq || rows[e / n] || (column != UINT32_MAX && e % n != column)) { ok = false; break; }
          rows[e / n] = 1; column = e % n;
        }
        if (ok) {
          // no root values: a missing row fails at the first missing row
          for (uint32_t r = 0; r < n; ++r) { ++w2; if (!rows[r]) { ok = false; break; } }
        }
        if (ok) { ++indexed; w2 += n; cols[column] = 1; }
      }
    }
  }
  uint32_t covered = 0; for (auto x : cols) covered += x;
  // primary region size
  uint64_t M = std::min<uint64_t>(maxv, (uint64_t)std::sqrt((double)vars));
  while ((M + 1) * (M + 1) <= vars && M + 1 <= maxv) ++M;
  while (M * M > vars) --M;
  const uint64_t region = M * M;
  uint64_t region_large_w0 = 0, region_large_any = 0;
  for (uint64_t i = 0; i < ncl; ++i) {
    uint64_t a = std::llabs(lits[start[i]]) - 1, b = std::llabs(lits[start[i] + 1]) - 1;
    if (a < region || b < region) ++region_large_w0;
    bool any = false;
    for (uint64_t j = start[i]; j < start[i + 1]; ++j) if (uint64_t(std::llabs(lits[j]) - 1) < region) { any = true; break; }
    region_large_any += any;
  }
  int owner_n = 0;
  for (int k = 2; k < (int)sizehist.size(); ++k) if (sizehist[k] == (uint64_t)k && 1LL * k * k <= (long long)vars) owner_n = k;
  printf("vars=%llu large=%llu bins=%llu units=%llu lits=%llu pass1_n=%u pass1_work=%llu pass2_candidates=%llu pass2_indexed=%llu columns_covered=%u pass2_work=%llu M=%llu region_large_by_first_two=%llu region_large_any=%llu owner_infer_n=%d\n",
         (unsigned long long)vars, (unsigned long long)ncl, (unsigned long long)bins, (unsigned long long)units,
         (unsigned long long)lits.size(), n, (unsigned long long)w1, (unsigned long long)cand, (unsigned long long)indexed, covered,
         (unsigned long long)w2, (unsigned long long)M, (unsigned long long)region_large_w0, (unsigned long long)region_large_any, owner_n);
}
